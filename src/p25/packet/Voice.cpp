/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2016,2017,2018 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2023 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/packet/Voice.h"
#include "p25/packet/ControlSignaling.h"
#include "p25/acl/AccessControl.h"
#include "p25/dfsi/DFSIDefines.h"
#include "p25/lc/tdulc/TDULCFactory.h"
#include "p25/P25Utils.h"
#include "p25/Sync.h"
#include "edac/CRC.h"
#include "HostMain.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::packet;

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t VOC_LDU1_COUNT = 3U;
const uint32_t ROAM_LDU1_COUNT = 1U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Resets the data states for the RF interface.
/// </summary>
void Voice::resetRF()
{
    lc::LC lc = lc::LC();

    m_rfLC = lc;
    //m_rfLastHDU = lc;
    m_rfLastLDU1 = lc;
    m_rfLastLDU2 = lc;

    m_rfFrames = 0U;
    m_rfErrs = 0U;
    m_rfBits = 1U;
    m_rfUndecodableLC = 0U;
    m_vocLDU1Count = 0U;
    m_roamLDU1Count = 0U;
}

/// <summary>
/// Resets the data states for the network.
/// </summary>
void Voice::resetNet()
{
    lc::LC lc = lc::LC();

    m_netLC = lc;
    m_netLastLDU1 = lc;
    //m_netLastFrameType = P25_FT_DATA_UNIT;

    m_netFrames = 0U;
    m_netLost = 0U;
    m_vocLDU1Count = 0U;
    m_roamLDU1Count = 0U;
    m_p25->m_networkWatchdog.stop();
}

/// <summary>
/// Process a data frame from the RF interface.
/// </summary>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool Voice::process(uint8_t* data, uint32_t len)
{
    assert(data != nullptr);

    // Decode the NID
    bool valid = m_p25->m_nid.decode(data + 2U);
    if (!valid) {
        return false;
    }

    uint8_t duid = m_p25->m_nid.getDUID();

    // are we interrupting a running CC?
    if (m_p25->m_ccRunning) {
        m_p25->m_ccHalted = true;
    }

    if (m_p25->m_rfState != RS_RF_LISTENING) {
        m_p25->m_rfTGHang.start();
    }

    if (duid == P25_DUID_HDU && m_lastDUID == P25_DUID_HDU) {
        duid = P25_DUID_LDU1;
    }

    // handle individual DUIDs
    if (duid == P25_DUID_HDU) {
        m_lastDUID = P25_DUID_HDU;

        if (m_p25->m_rfState == RS_RF_LISTENING) {
            if (!m_p25->m_dedicatedControl) {
                m_p25->m_modem->clearP25Frame();
            }
            m_p25->m_txQueue.clear();
            resetRF();
            resetNet();
        }

        if (m_p25->m_rfState == RS_RF_LISTENING || m_p25->m_rfState == RS_RF_AUDIO) {
            resetRF();
            resetNet();

            lc::LC lc = lc::LC();
            bool ret = lc.decodeHDU(data + 2U);
            if (!ret) {
                LogWarning(LOG_RF, P25_HDU_STR ", undecodable LC");
                m_rfUndecodableLC++;
                return false;
            }

            if (m_verbose && m_debug) {
                uint8_t mi[P25_MI_LENGTH_BYTES];
                ::memset(mi, 0x00U, p25::P25_MI_LENGTH_BYTES);
                lc.getMI(mi);
                Utils::dump(1U, "P25 HDU MI read from RF", mi, P25_MI_LENGTH_BYTES);
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_HDU_STR ", HDU_BSDWNACT, dstId = %u, algo = $%02X, kid = $%04X", lc.getDstId(), lc.getAlgId(), lc.getKId());
            }

            // don't process RF frames if this modem isn't authoritative
            if (!m_p25->m_authoritative && m_p25->m_permittedDstId != lc.getDstId()) {
                LogWarning(LOG_RF, "[NON-AUTHORITATIVE] Ignoring RF traffic, destination not permitted!");
                resetRF();
                return false;
            }

            // don't process RF frames if the network isn't in a idle state and the RF destination is the network destination
            if (m_p25->m_netState != RS_NET_IDLE && lc.getDstId() == m_p25->m_netLastDstId) {
                LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing network traffic!");
                resetRF();
                return false;
            }

            // stop network frames from processing -- RF wants to transmit on a different talkgroup
            if (m_p25->m_netState != RS_NET_IDLE) {
                LogWarning(LOG_RF, "Traffic collision detect, preempting existing network traffic to new RF traffic, rfDstId = %u, netDstId = %u", lc.getDstId(),
                    m_p25->m_netLastDstId);
                if (!m_p25->m_dedicatedControl) {
                    m_p25->m_affiliations.releaseGrant(m_p25->m_netLastDstId, false);
                }

                resetNet();
                if (m_p25->m_network != nullptr)
                    m_p25->m_network->resetP25();

                if (m_p25->m_duplex) {
                    m_p25->writeRF_TDU(true);
                }
            }

            if (m_p25->m_duplex) {
                m_p25->writeRF_Preamble();
            }

            m_p25->m_rfTGHang.start();
            m_p25->m_netTGHang.stop();
            m_p25->m_rfLastDstId = lc.getDstId();
            m_p25->m_rfLastSrcId = lc.getSrcId();

            m_rfLastHDU = lc;
        }

        return true;
    }
    else if (duid == P25_DUID_LDU1) {

        // prevent two LDUs of the same type from being sent consecutively
        if (m_lastDUID == P25_DUID_LDU1) {
            return false;
        }
        m_lastDUID = P25_DUID_LDU1;

        bool alreadyDecoded = false;
        uint8_t frameType = P25_FT_DATA_UNIT;
        if (m_p25->m_rfState == RS_RF_LISTENING) {
            // if this is a late entry call, clear states
            if (m_rfLastHDU.getDstId() == 0U) {
                if (!m_p25->m_dedicatedControl) {
                    m_p25->m_modem->clearP25Frame();
                }
                m_p25->m_txQueue.clear();
                resetRF();
                resetNet();
            }

            if (m_p25->m_enableControl) {
                if (!m_p25->m_ccRunning && m_p25->m_voiceOnControl) {
                    m_p25->m_control->writeRF_ControlData(255U, 0U, false);
                }
            }

            lc::LC lc = lc::LC();
            bool ret = lc.decodeLDU1(data + 2U);
            if (!ret) {
                return false;
            }

            uint32_t srcId = lc.getSrcId();
            uint32_t dstId = lc.getDstId();
            bool group = lc.getGroup();
            bool encrypted = lc.getEncrypted();

            alreadyDecoded = true;

            // don't process RF frames if this modem isn't authoritative
            if (!m_p25->m_authoritative && m_p25->m_permittedDstId != lc.getDstId()) {
                LogWarning(LOG_RF, "[NON-AUTHORITATIVE] Ignoring RF traffic, destination not permitted!");
                resetRF();
                return false;
            }

            // don't process RF frames if the network isn't in a idle state and the RF destination is the network destination
            if (m_p25->m_netState != RS_NET_IDLE && dstId == m_p25->m_netLastDstId) {
                LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing network traffic!");
                resetRF();
                return false;
            }

            // stop network frames from processing -- RF wants to transmit on a different talkgroup
            if (m_p25->m_netState != RS_NET_IDLE) {
                if (m_netLC.getSrcId() == srcId && m_p25->m_netLastDstId == dstId) {
                    LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing RF traffic (Are we in a voting condition?), rfSrcId = %u, rfDstId = %u, netSrcId = %u, netDstId = %u", srcId, dstId,
                        m_netLC.getSrcId(), m_p25->m_netLastDstId);
                    resetRF();
                    return false;
                }
                else {
                    LogWarning(LOG_RF, "Traffic collision detect, preempting existing network traffic to new RF traffic, rfDstId = %u, netDstId = %u", dstId,
                        m_p25->m_netLastDstId);
                    if (!m_p25->m_dedicatedControl) {
                        m_p25->m_affiliations.releaseGrant(m_p25->m_netLastDstId, false);
                    }

                    resetNet();
                    if (m_p25->m_network != nullptr)
                        m_p25->m_network->resetP25();

                    if (m_p25->m_duplex) {
                        m_p25->writeRF_TDU(true);
                    }

                    m_p25->m_netTGHang.stop();
                }
            }

            // validate the source RID
            if (!acl::AccessControl::validateSrcId(srcId)) {
                if (m_lastRejectId == 0U || m_lastRejectId != srcId) {
                    LogWarning(LOG_RF, P25_HDU_STR " denial, RID rejection, srcId = %u", srcId);
                    if (m_p25->m_enableControl) {
                        m_p25->m_control->writeRF_TSDU_Deny(srcId, dstId, P25_DENY_RSN_REQ_UNIT_NOT_VALID, (group ? TSBK_IOSP_GRP_VCH : TSBK_IOSP_UU_VCH));
                        m_p25->m_control->denialInhibit(srcId);
                    }

                    ::ActivityLog("P25", true, "RF voice rejection from %u to %s%u ", srcId, group ? "TG " : "", dstId);
                    m_lastRejectId = srcId;
                }

                m_p25->m_rfLastDstId = 0U;
                m_p25->m_rfLastSrcId = 0U;
                m_p25->m_rfTGHang.stop();
                m_p25->m_rfState = RS_RF_REJECTED;
                return false;
            }

            // is this a group or individual operation?
            if (!group) {
                // validate the target RID
                if (!acl::AccessControl::validateSrcId(dstId)) {
                    if (m_lastRejectId == 0 || m_lastRejectId != dstId) {
                        LogWarning(LOG_RF, P25_HDU_STR " denial, RID rejection, dstId = %u", dstId);
                        if (m_p25->m_enableControl) {
                            m_p25->m_control->writeRF_TSDU_Deny(srcId, dstId, P25_DENY_RSN_TGT_UNIT_NOT_VALID, TSBK_IOSP_UU_VCH);
                        }

                        ::ActivityLog("P25", true, "RF voice rejection from %u to %s%u ", srcId, group ? "TG " : "", dstId);
                        m_lastRejectId = dstId;
                    }

                    m_p25->m_rfLastDstId = 0U;
                    m_p25->m_rfLastSrcId = 0U;
                    m_p25->m_rfTGHang.stop();
                    m_p25->m_rfState = RS_RF_REJECTED;
                    return false;
                }
            }
            else {
                // validate the target ID, if the target is a talkgroup
                if (!acl::AccessControl::validateTGId(dstId)) {
                    if (m_lastRejectId == 0 || m_lastRejectId != dstId) {
                        LogWarning(LOG_RF, P25_HDU_STR " denial, TGID rejection, dstId = %u", dstId);
                        if (m_p25->m_enableControl) {
                            m_p25->m_control->writeRF_TSDU_Deny(srcId, dstId, P25_DENY_RSN_TGT_GROUP_NOT_VALID, TSBK_IOSP_GRP_VCH);
                        }

                        ::ActivityLog("P25", true, "RF voice rejection from %u to %s%u ", srcId, group ? "TG " : "", dstId);
                        m_lastRejectId = dstId;
                    }

                    m_p25->m_rfLastDstId = 0U;
                    m_p25->m_rfLastSrcId = 0U;
                    m_p25->m_rfTGHang.stop();
                    m_p25->m_rfState = RS_RF_REJECTED;
                    return false;
                }
            }

            // verify the source RID is affiliated to the group TGID; only if control data
            // is supported
            if (group && m_p25->m_enableControl) {
                if (!m_p25->m_affiliations.isGroupAff(srcId, dstId) && m_p25->m_control->m_verifyAff) {
                    if (m_lastRejectId == 0 || m_lastRejectId != srcId) {
                        LogWarning(LOG_RF, P25_HDU_STR " denial, RID not affiliated to TGID, srcId = %u, dstId = %u", srcId, dstId);
                        m_p25->m_control->writeRF_TSDU_Deny(srcId, dstId, P25_DENY_RSN_REQ_UNIT_NOT_AUTH, TSBK_IOSP_GRP_VCH);
                        m_p25->m_control->writeRF_TSDU_U_Reg_Cmd(srcId);

                        ::ActivityLog("P25", true, "RF voice rejection from %u to %s%u ", srcId, group ? "TG " : "", dstId);
                        m_lastRejectId = srcId;
                    }

                    m_p25->m_rfLastDstId = 0U;
                    m_p25->m_rfLastSrcId = 0U;
                    m_p25->m_rfTGHang.stop();
                    m_p25->m_rfState = RS_RF_REJECTED;
                    return false;
                }
            }

            m_rfLC = lc;
            m_rfLastLDU1 = m_rfLC;

            m_lastRejectId = 0U;
            ::ActivityLog("P25", true, "RF %svoice transmission from %u to %s%u", encrypted ? "encrypted ": "", srcId, group ? "TG " : "", dstId);

            uint8_t serviceOptions = (m_rfLC.getEmergency() ? 0x80U : 0x00U) +       // Emergency Flag
                (m_rfLC.getEncrypted() ? 0x40U : 0x00U) +                            // Encrypted Flag
                (m_rfLC.getPriority() & 0x07U);                                      // Priority

            if (m_p25->m_enableControl) {
                // if the group wasn't granted out -- explicitly grant the group
                if (!m_p25->m_affiliations.isGranted(dstId)) {
                    if (m_p25->m_legacyGroupGrnt) {
                        // are we auto-registering legacy radios to groups?
                        if (m_p25->m_legacyGroupReg && group) {
                            if (!m_p25->m_affiliations.isGroupAff(srcId, dstId)) {
                                if (!m_p25->m_control->writeRF_TSDU_Grp_Aff_Rsp(srcId, dstId)) {
                                    return false;
                                }
                            }
                        }

                        if (!m_p25->m_control->writeRF_TSDU_Grant(srcId, dstId, serviceOptions, group)) {
                            return false;
                        }
                    }
                    else {
                        return false;
                    }
                }
            }

            // single-channel trunking or voice on control support?
            if (m_p25->m_enableControl && m_p25->m_voiceOnControl) {
                m_p25->m_control->writeRF_TSDU_Grant(srcId, dstId, serviceOptions, group, true, true);
            }

            m_hadVoice = true;

            m_p25->m_rfState = RS_RF_AUDIO;

            if (group) {
                m_p25->m_rfTGHang.start();
            }
            else {
                m_p25->m_rfTGHang.stop();
            }
            m_p25->m_netTGHang.stop();
            m_p25->m_rfLastDstId = dstId;
            m_p25->m_rfLastSrcId = srcId;

            // make sure we actually got a HDU -- otherwise treat the call as a late entry
            if (m_rfLastHDU.getDstId() != 0U) {
                // copy destination and encryption parameters from the last HDU received (if possible)
                if (m_rfLC.getDstId() != m_rfLastHDU.getDstId()) {
                    m_rfLC.setDstId(m_rfLastHDU.getDstId());
                }

                m_rfLC.setAlgId(m_rfLastHDU.getAlgId());
                m_rfLC.setKId(m_rfLastHDU.getKId());

                uint8_t mi[P25_MI_LENGTH_BYTES];
                m_rfLastHDU.getMI(mi);
                m_rfLC.setMI(mi);

                uint8_t buffer[P25_HDU_FRAME_LENGTH_BYTES + 2U];
                ::memset(buffer, 0x00U, P25_HDU_FRAME_LENGTH_BYTES + 2U);

                // Generate Sync
                Sync::addP25Sync(buffer + 2U);

                // Generate NID
                m_p25->m_nid.encode(buffer + 2U, P25_DUID_HDU);

                // Generate HDU
                m_rfLC.encodeHDU(buffer + 2U);

                // Add busy bits
                P25Utils::addBusyBits(buffer + 2U, P25_HDU_FRAME_LENGTH_BITS, false, true);

                writeNetwork(buffer, P25_DUID_HDU);

                if (m_p25->m_duplex) {
                    buffer[0U] = modem::TAG_DATA;
                    buffer[1U] = 0x00U;

                    m_p25->addFrame(buffer, P25_HDU_FRAME_LENGTH_BYTES + 2U);
                }

                frameType = P25_FT_HDU_VALID;

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_HDU_STR ", dstId = %u, algo = $%02X, kid = $%04X", m_rfLC.getDstId(), m_rfLC.getAlgId(), m_rfLC.getKId());
                }
            }
            else {
                frameType = P25_FT_HDU_LATE_ENTRY;
                LogWarning(LOG_RF, P25_HDU_STR ", not transmitted; possible late entry, dstId = %u, algo = $%02X, kid = $%04X", m_rfLastHDU.getDstId(), m_rfLastHDU.getAlgId(), m_rfLastHDU.getKId());
            }

            m_rfFrames = 0U;
            m_rfErrs = 0U;
            m_rfBits = 1U;
            m_rfUndecodableLC = 0U;
            m_vocLDU1Count = 0U;
            m_roamLDU1Count = 0U;
            m_p25->m_rfTimeout.start();
            m_lastDUID = P25_DUID_HDU;

            m_rfLastHDU = lc::LC();
        }

        if (m_p25->m_rfState == RS_RF_AUDIO) {
            if (!alreadyDecoded) {
                bool ret = m_rfLC.decodeLDU1(data + 2U);
                if (!ret) {
                    LogWarning(LOG_RF, P25_LDU1_STR ", undecodable LC, using last LDU1 LC");
                    m_rfLC = m_rfLastLDU1;

                    // ensure our srcId and dstId are sane from the last LDU1
                    if (m_rfLastLDU1.getDstId() != 0U) {
                        if (m_rfLC.getDstId() != m_rfLastLDU1.getDstId()) {
                            LogWarning(LOG_RF, P25_LDU2_STR ", dstId = %u doesn't match last LDU1 dstId = %u, fixing",
                                m_rfLC.getDstId(), m_rfLastLDU1.getDstId());
                            m_rfLC.setDstId(m_rfLastLDU1.getDstId());
                        }
                    }
                    else {
                        LogWarning(LOG_RF, P25_LDU2_STR ", last LDU1 LC has bad data, dstId = 0");
                    }

                    if (m_rfLastLDU1.getSrcId() != 0U) {
                        if (m_rfLC.getSrcId() != m_rfLastLDU1.getSrcId()) {
                            LogWarning(LOG_RF, P25_LDU2_STR ", srcId = %u doesn't match last LDU1 srcId = %u, fixing",
                                m_rfLC.getSrcId(), m_rfLastLDU1.getSrcId());
                            m_rfLC.setSrcId(m_rfLastLDU1.getSrcId());
                        }
                    }
                    else {
                        LogWarning(LOG_RF, P25_LDU2_STR ", last LDU1 LC has bad data, srcId = 0");
                    }

                    m_rfUndecodableLC++;
                }
                else {
                    m_rfLastLDU1 = m_rfLC;
                }
            }

            alreadyDecoded = false;

            if (m_p25->m_enableControl) {
                m_p25->m_affiliations.touchGrant(m_rfLC.getDstId());
            }

            if (m_p25->m_notifyCC) {
                m_p25->notifyCC_TouchGrant(m_rfLC.getDstId());
            }

            // single-channel trunking or voice on control support?
            if (m_p25->m_enableControl && m_p25->m_voiceOnControl) {
                // per TIA-102.AABD-B transmit RFSS_STS_BCAST every 3 superframes (e.g. every 3 LDU1s)
                m_vocLDU1Count++;
                if (m_vocLDU1Count > VOC_LDU1_COUNT) {
                    m_vocLDU1Count = 0U;
                    m_rfLC.setLCO(LC_RFSS_STS_BCAST);
                }
            }

            // generate Sync
            Sync::addP25Sync(data + 2U);

            // generate NID
            m_p25->m_nid.encode(data + 2U, P25_DUID_LDU1);

            // generate LDU1 Data
            m_rfLC.encodeLDU1(data + 2U);

            // generate Low Speed Data
            m_rfLSD.process(data + 2U);

            // regenerate audio
            uint32_t errors = m_audio.process(data + 2U);

            // replace audio with silence in cases where the error rate
            // has exceeded the configured threshold
            if (errors > m_silenceThreshold) {
                // generate null audio
                uint8_t buffer[9U * 25U];
                ::memset(buffer, 0x00U, 9U * 25U);

                if (m_rfLC.getEncrypted()) {
                    insertEncryptedNullAudio(buffer);
                }
                else {
                    insertNullAudio(buffer);
                }

                LogWarning(LOG_RF, P25_LDU1_STR ", exceeded lost audio threshold, filling in");

                // add the audio
                m_audio.encode(data + 2U, buffer + 10U, 0U);
                m_audio.encode(data + 2U, buffer + 26U, 1U);
                m_audio.encode(data + 2U, buffer + 55U, 2U);
                m_audio.encode(data + 2U, buffer + 80U, 3U);
                m_audio.encode(data + 2U, buffer + 105U, 4U);
                m_audio.encode(data + 2U, buffer + 130U, 5U);
                m_audio.encode(data + 2U, buffer + 155U, 6U);
                m_audio.encode(data + 2U, buffer + 180U, 7U);
                m_audio.encode(data + 2U, buffer + 204U, 8U);
            }

            m_rfBits += 1233U;
            m_rfErrs += errors;
            m_rfFrames++;

            // add busy bits
            P25Utils::addBusyBits(data + 2U, P25_LDU_FRAME_LENGTH_BITS, false, true);

            writeNetwork(data + 2U, P25_DUID_LDU1, frameType);

            if (m_p25->m_duplex) {
                data[0U] = modem::TAG_DATA;
                data[1U] = 0x00U;

                m_p25->addFrame(data, P25_LDU_FRAME_LENGTH_BYTES + 2U);
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_LDU1_STR ", audio, srcId = %u, dstId = %u, group = %u, emerg = %u, encrypt = %u, prio = %u, errs = %u/1233 (%.1f%%)",
                    m_rfLC.getSrcId(), m_rfLC.getDstId(), m_rfLC.getGroup(), m_rfLC.getEmergency(), m_rfLC.getEncrypted(), m_rfLC.getPriority(), errors, float(errors) / 12.33F);
            }

            return true;
        }
    }
    else if (duid == P25_DUID_LDU2) {
        // prevent two LDUs of the same type from being sent consecutively
        if (m_lastDUID == P25_DUID_LDU2) {
            return false;
        }
        m_lastDUID = P25_DUID_LDU2;

        if (m_p25->m_rfState == RS_RF_LISTENING) {
            return false;
        }
        else if (m_p25->m_rfState == RS_RF_AUDIO) {
            bool ret = m_rfLC.decodeLDU2(data + 2U);
            if (!ret) {
                LogWarning(LOG_RF, P25_LDU2_STR ", undecodable LC, using last LDU2 LC");
                m_rfLC = m_rfLastLDU2;
                m_rfUndecodableLC++;

                // regenerate the MI using LFSR
                uint8_t lastMI[P25_MI_LENGTH_BYTES];
                ::memset(lastMI, 0x00U, P25_MI_LENGTH_BYTES);

                uint8_t nextMI[P25_MI_LENGTH_BYTES];
                ::memset(nextMI, 0x00U, P25_MI_LENGTH_BYTES);

                m_rfLastLDU2.getMI(lastMI);
                getNextMI(lastMI, nextMI);

                if (m_verbose && m_debug) {
                    Utils::dump(1U, "Previous P25 HDU MI", lastMI, P25_MI_LENGTH_BYTES);
                    Utils::dump(1U, "Calculated next P25 HDU MI", nextMI, P25_MI_LENGTH_BYTES);
                }

                m_rfLC.setMI(nextMI);
                m_rfLastLDU2.setMI(nextMI);
            }
            else {
                m_rfLastLDU2 = m_rfLC;
            }

            // generate Sync
            Sync::addP25Sync(data + 2U);

            // generate NID
            m_p25->m_nid.encode(data + 2U, P25_DUID_LDU2);

            // generate LDU2 data
            m_rfLC.encodeLDU2(data + 2U);

            // generate Low Speed Data
            m_rfLSD.process(data + 2U);

            // regenerate audio
            uint32_t errors = m_audio.process(data + 2U);

            // replace audio with silence in cases where the error rate
            // has exceeded the configured threshold
            if (errors > m_silenceThreshold) {
                // generate null audio
                uint8_t buffer[9U * 25U];
                ::memset(buffer, 0x00U, 9U * 25U);

                if (m_rfLC.getEncrypted()) {
                    insertEncryptedNullAudio(buffer);
                }
                else {
                    insertNullAudio(buffer);
                }

                LogWarning(LOG_RF, P25_LDU2_STR ", exceeded lost audio threshold, filling in");

                // add the Audio
                m_audio.encode(data + 2U, buffer + 10U, 0U);
                m_audio.encode(data + 2U, buffer + 26U, 1U);
                m_audio.encode(data + 2U, buffer + 55U, 2U);
                m_audio.encode(data + 2U, buffer + 80U, 3U);
                m_audio.encode(data + 2U, buffer + 105U, 4U);
                m_audio.encode(data + 2U, buffer + 130U, 5U);
                m_audio.encode(data + 2U, buffer + 155U, 6U);
                m_audio.encode(data + 2U, buffer + 180U, 7U);
                m_audio.encode(data + 2U, buffer + 204U, 8U);
            }

            m_rfBits += 1233U;
            m_rfErrs += errors;
            m_rfFrames++;

            // add busy bits
            P25Utils::addBusyBits(data + 2U, P25_LDU_FRAME_LENGTH_BITS, false, true);

            writeNetwork(data + 2U, P25_DUID_LDU2);

            if (m_p25->m_duplex) {
                data[0U] = modem::TAG_DATA;
                data[1U] = 0x00U;

                m_p25->addFrame(data, P25_LDU_FRAME_LENGTH_BYTES + 2U);
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_LDU2_STR ", audio, algo = $%02X, kid = $%04X, errs = %u/1233 (%.1f%%)",
                    m_rfLC.getAlgId(), m_rfLC.getKId(), errors, float(errors) / 12.33F);
            }

            return true;
        }
    }
    else if (duid == P25_DUID_TDU || duid == P25_DUID_TDULC) {
        if (!m_p25->m_enableControl) {
            m_p25->m_affiliations.releaseGrant(m_rfLC.getDstId(), false);
            m_p25->notifyCC_ReleaseGrant(m_rfLC.getDstId());
        }

        if (duid == P25_DUID_TDU) {
            m_p25->writeRF_TDU(false);

            m_lastDUID = duid;

            m_p25->m_rfTimeout.stop();
        }
        else {
            std::unique_ptr<lc::TDULC> tdulc = lc::tdulc::TDULCFactory::createTDULC(data + 2U);
            if (tdulc == nullptr) {
                LogWarning(LOG_RF, P25_TDULC_STR ", undecodable TDULC");
            }
            else {
                m_p25->m_control->writeRF_TDULC(tdulc.get(), false);
            }
        }

        if (m_p25->m_rfState == RS_RF_AUDIO) {
            if (m_p25->m_rssi != 0U) {
                ::ActivityLog("P25", true, "RF end of transmission, %.1f seconds, BER: %.1f%%, RSSI : -%u / -%u / -%u dBm",
                    float(m_rfFrames) / 5.56F, float(m_rfErrs * 100U) / float(m_rfBits), m_p25->m_minRSSI, m_p25->m_maxRSSI,
                    m_p25->m_aveRSSI / m_p25->m_rssiCount);
            }
            else {
                ::ActivityLog("P25", true, "RF end of transmission, %.1f seconds, BER: %.1f%%",
                    float(m_rfFrames) / 5.56F, float(m_rfErrs * 100U) / float(m_rfBits));
            }

            LogMessage(LOG_RF, P25_TDU_STR ", total frames: %d, bits: %d, undecodable LC: %d, errors: %d, BER: %.4f%%",
                m_rfFrames, m_rfBits, m_rfUndecodableLC, m_rfErrs, float(m_rfErrs * 100U) / float(m_rfBits));

            if (m_p25->m_dedicatedControl) {
                m_p25->m_tailOnIdle = false;
                writeRF_EndOfVoice();
            }
            else {
                m_p25->m_tailOnIdle = true;
                m_p25->m_control->writeNet_TSDU_Call_Term(m_rfLC.getSrcId(), m_rfLC.getDstId());
            }
        }

        m_p25->m_rfState = RS_RF_LISTENING;
        return true;
    }
    else {
        LogError(LOG_RF, "P25 unhandled voice DUID, duid = $%02X", duid);
    }

    return false;
}

/// <summary>
/// Process a data frame from the network.
/// </summary>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <param name="control">Link Control Data.</param>
/// <param name="lsd">Low Speed Data.</param>
/// <param name="duid">Data Unit ID.</param>
/// <param name="frameType">Network Frame Type.</param>
/// <returns></returns>
bool Voice::processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, uint8_t& duid, uint8_t& frameType)
{
    uint32_t dstId = control.getDstId();
    uint32_t srcId = control.getSrcId();

    // don't process network frames if the destination ID's don't match and the RF TG hang timer is running
    if (m_p25->m_rfLastDstId != 0U && dstId != 0U) {
        if (m_p25->m_rfLastDstId != dstId && (m_p25->m_rfTGHang.isRunning() && !m_p25->m_rfTGHang.hasExpired())) {
            resetNet();
            if (m_p25->m_network != nullptr)
                m_p25->m_network->resetP25();
            return false;
        }

        if (m_p25->m_rfLastDstId == dstId && (m_p25->m_rfTGHang.isRunning() && !m_p25->m_rfTGHang.hasExpired())) {
            m_p25->m_rfTGHang.start();
        }
    }

    // perform authoritative network TG hangtimer and traffic preemption
    if (m_p25->m_authoritative) {
        // don't process network frames if the destination ID's don't match and the network TG hang timer is running
        if (m_p25->m_netLastDstId != 0U && dstId != 0U && (duid == P25_DUID_LDU1 || duid == P25_DUID_LDU2)) {
            if (m_p25->m_netLastDstId != dstId && (m_p25->m_netTGHang.isRunning() && !m_p25->m_netTGHang.hasExpired())) {
                return false;
            }

            if (m_p25->m_netLastDstId == dstId && (m_p25->m_netTGHang.isRunning() && !m_p25->m_netTGHang.hasExpired())) {
                m_p25->m_netTGHang.start();
            }
        }

        // don't process network frames if the RF modem isn't in a listening state
        if (m_p25->m_rfState != RS_RF_LISTENING) {
            if (m_rfLC.getSrcId() == srcId && m_rfLC.getDstId() == dstId) {
                LogWarning(LOG_RF, "Traffic collision detect, preempting new network traffic to existing RF traffic (Are we in a voting condition?), rfSrcId = %u, rfDstId = %u, netSrcId = %u, netDstId = %u", m_rfLC.getSrcId(), m_rfLC.getDstId(),
                    srcId, dstId);
                resetNet();
                if (m_p25->m_network != nullptr)
                    m_p25->m_network->resetP25();
                return false;
            }
            else {
                LogWarning(LOG_RF, "Traffic collision detect, preempting new network traffic to existing RF traffic, rfDstId = %u, netDstId = %u", m_rfLC.getDstId(),
                    dstId);
                resetNet();
                if (m_p25->m_network != nullptr)
                    m_p25->m_network->resetP25();
                return false;
            }
        }
    }

    // don't process network frames if this modem isn't authoritative
    if (!m_p25->m_authoritative && m_p25->m_permittedDstId != dstId) {
        LogWarning(LOG_NET, "[NON-AUTHORITATIVE] Ignoring network traffic, destination not permitted, dstId = %u", dstId);
        resetNet();
        if (m_p25->m_network != nullptr)
            m_p25->m_network->resetP25();
        return false;
    }

    uint32_t count = 0U;
    switch (duid) {
        case P25_DUID_LDU1:
            if ((data[0U] == dfsi::P25_DFSI_LDU1_VOICE1) && (data[22U] == dfsi::P25_DFSI_LDU1_VOICE2) &&
                (data[36U] == dfsi::P25_DFSI_LDU1_VOICE3) && (data[53U] == dfsi::P25_DFSI_LDU1_VOICE4) &&
                (data[70U] == dfsi::P25_DFSI_LDU1_VOICE5) && (data[87U] == dfsi::P25_DFSI_LDU1_VOICE6) &&
                (data[104U] == dfsi::P25_DFSI_LDU1_VOICE7) && (data[121U] == dfsi::P25_DFSI_LDU1_VOICE8) &&
                (data[138U] == dfsi::P25_DFSI_LDU1_VOICE9)) {

                m_dfsiLC = dfsi::LC(control, lsd);

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE1);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 10U);
                count += dfsi::P25_DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE2);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 26U);
                count += dfsi::P25_DFSI_LDU1_VOICE2_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE3);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 55U);
                count += dfsi::P25_DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE4);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 80U);
                count += dfsi::P25_DFSI_LDU1_VOICE4_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE5);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 105U);
                count += dfsi::P25_DFSI_LDU1_VOICE5_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE6);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 130U);
                count += dfsi::P25_DFSI_LDU1_VOICE6_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE7);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 155U);
                count += dfsi::P25_DFSI_LDU1_VOICE7_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE8);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 180U);
                count += dfsi::P25_DFSI_LDU1_VOICE8_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE9);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 204U);
                count += dfsi::P25_DFSI_LDU1_VOICE9_FRAME_LENGTH_BYTES;

                // these aren't set by the DFSI decoder, so we'll manually
                // reset them
                m_dfsiLC.control()->setNetId(control.getNetId());
                m_dfsiLC.control()->setSysId(control.getSysId());

                m_netLastLDU1 = control;
                m_netLastFrameType = frameType;

                // save MI to member variable before writing to RF
                control.getMI(m_lastMI);

                if (m_p25->m_enableControl) {
                    lc::LC control = lc::LC(*m_dfsiLC.control());
                    m_p25->m_affiliations.touchGrant(control.getDstId());
                }

                if (m_p25->m_notifyCC) {
                    m_p25->notifyCC_TouchGrant(control.getDstId());
                }

                if (m_p25->m_dedicatedControl && !m_p25->m_voiceOnControl) {
                    return true;
                }

                if (m_p25->m_netState == RS_NET_IDLE) {
                    // are we interrupting a running CC?
                    if (m_p25->m_ccRunning) {
                        m_p25->m_ccHalted = true;
                    }
                }

                checkNet_LDU2();
                if (m_p25->m_netState != RS_NET_IDLE) {
                    m_p25->m_netTGHang.start();
                    writeNet_LDU1();
                }
            }
            break;
        case P25_DUID_LDU2:
            if ((data[0U] == dfsi::P25_DFSI_LDU2_VOICE10) && (data[22U] == dfsi::P25_DFSI_LDU2_VOICE11) &&
                (data[36U] == dfsi::P25_DFSI_LDU2_VOICE12) && (data[53U] == dfsi::P25_DFSI_LDU2_VOICE13) &&
                (data[70U] == dfsi::P25_DFSI_LDU2_VOICE14) && (data[87U] == dfsi::P25_DFSI_LDU2_VOICE15) &&
                (data[104U] == dfsi::P25_DFSI_LDU2_VOICE16) && (data[121U] == dfsi::P25_DFSI_LDU2_VOICE17) &&
                (data[138U] == dfsi::P25_DFSI_LDU2_VOICE18)) {
                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE10);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 10U);
                count += dfsi::P25_DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE11);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 26U);
                count += dfsi::P25_DFSI_LDU2_VOICE11_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE12);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 55U);
                count += dfsi::P25_DFSI_LDU2_VOICE12_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE13);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 80U);
                count += dfsi::P25_DFSI_LDU2_VOICE13_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE14);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 105U);
                count += dfsi::P25_DFSI_LDU2_VOICE14_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE15);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 130U);
                count += dfsi::P25_DFSI_LDU2_VOICE15_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE16);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 155U);
                count += dfsi::P25_DFSI_LDU2_VOICE16_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE17);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 180U);
                count += dfsi::P25_DFSI_LDU2_VOICE17_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE18);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 204U);
                count += dfsi::P25_DFSI_LDU2_VOICE18_FRAME_LENGTH_BYTES;

                if (m_p25->m_enableControl) {
                    lc::LC control = lc::LC(*m_dfsiLC.control());
                    m_p25->m_affiliations.touchGrant(control.getDstId());
                }

                if (m_p25->m_notifyCC) {
                    m_p25->notifyCC_TouchGrant(control.getDstId());
                }

                if (m_p25->m_dedicatedControl && !m_p25->m_voiceOnControl) {
                    return true;
                }

                if (m_p25->m_netState == RS_NET_IDLE) {
                    if (!m_p25->m_voiceOnControl) {
                        m_p25->m_modem->clearP25Frame();
                    }
                    m_p25->m_txQueue.clear();

                    resetRF();
                    resetNet();

                    writeNet_LDU1();
                }
                else {
                    checkNet_LDU1();
                }

                if (m_p25->m_netState != RS_NET_IDLE) {
                    m_p25->m_netTGHang.start();
                    writeNet_LDU2();
                }
            }
            break;
        case P25_DUID_TDU:
        case P25_DUID_TDULC:
            // ignore a TDU that doesn't contain our destination ID
            if (control.getDstId() != m_p25->m_netLastDstId) {
                return false;
            }

            // don't process network frames if the RF modem isn't in a listening state
            if (m_p25->m_rfState != RS_RF_LISTENING) {
                resetNet();
                return false;
            }

            if (!m_p25->m_enableControl) {
                m_p25->m_affiliations.releaseGrant(m_netLC.getDstId(), false);
                m_p25->notifyCC_ReleaseGrant(m_netLC.getDstId());
            }

            if (m_p25->m_netState != RS_NET_IDLE) {
                if (duid == P25_DUID_TDU)
                    writeNet_TDU();

                resetNet();
            }
            break;
    }

    return true;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Voice class.
/// </summary>
/// <param name="p25">Instance of the Control class.</param>
/// <param name="debug">Flag indicating whether P25 debug is enabled.</param>
/// <param name="verbose">Flag indicating whether P25 verbose logging is enabled.</param>
Voice::Voice(Control* p25, bool debug, bool verbose) :
    m_p25(p25),
    m_rfFrames(0U),
    m_rfBits(0U),
    m_rfErrs(0U),
    m_rfUndecodableLC(0U),
    m_netFrames(0U),
    m_netLost(0U),
    m_audio(),
    m_rfLC(),
    m_rfLastHDU(),
    m_rfLastLDU1(),
    m_rfLastLDU2(),
    m_netLC(),
    m_netLastLDU1(),
    m_netLastFrameType(P25_FT_DATA_UNIT),
    m_rfLSD(),
    m_netLSD(),
    m_dfsiLC(),
    m_netLDU1(nullptr),
    m_netLDU2(nullptr),
    m_lastDUID(P25_DUID_TDU),
    m_lastIMBE(nullptr),
    m_lastMI(nullptr),
    m_hadVoice(false),
    m_lastRejectId(0U),
    m_silenceThreshold(DEFAULT_SILENCE_THRESHOLD),
    m_vocLDU1Count(0U),
    m_roamLDU1Count(0U),
    m_verbose(verbose),
    m_debug(debug)
{
    m_netLDU1 = new uint8_t[9U * 25U];
    m_netLDU2 = new uint8_t[9U * 25U];

    ::memset(m_netLDU1, 0x00U, 9U * 25U);
    ::memset(m_netLDU2, 0x00U, 9U * 25U);

    m_lastIMBE = new uint8_t[11U];
    ::memcpy(m_lastIMBE, P25_NULL_IMBE, 11U);

    m_lastMI = new uint8_t[P25_MI_LENGTH_BYTES];
    ::memset(m_lastMI, 0x00U, P25_MI_LENGTH_BYTES);
}

/// <summary>
/// Finalizes a instance of the Voice class.
/// </summary>
Voice::~Voice()
{
    delete[] m_netLDU1;
    delete[] m_netLDU2;
    delete[] m_lastIMBE;
    delete[] m_lastMI;
}

/// <summary>
/// Write data processed from RF to the network.
/// </summary>
/// <param name="data"></param>
/// <param name="duid"></param>
/// <param name="frameType"></param>
void Voice::writeNetwork(const uint8_t *data, uint8_t duid, uint8_t frameType)
{
    assert(data != nullptr);

    if (m_p25->m_network == nullptr)
        return;

    if (m_p25->m_rfTimeout.isRunning() && m_p25->m_rfTimeout.hasExpired())
        return;

    switch (duid) {
        case P25_DUID_HDU:
            // ignore HDU
            break;
        case P25_DUID_LDU1:
            m_p25->m_network->writeP25LDU1(m_rfLC, m_rfLSD, data, frameType);
            break;
        case P25_DUID_LDU2:
            m_p25->m_network->writeP25LDU2(m_rfLC, m_rfLSD, data);
            break;
        case P25_DUID_TDU:
        case P25_DUID_TDULC:
            m_p25->m_network->writeP25TDU(m_rfLC, m_rfLSD);
            break;
        default:
            LogError(LOG_NET, "P25 unhandled voice DUID, duid = $%02X", duid);
            break;
    }
}

/// <summary>
/// Helper to write end of frame data.
/// </summary>
/// <returns></returns>
void Voice::writeRF_EndOfVoice()
{
    if (!m_hadVoice) {
        return;
    }

    bool grp = m_rfLC.getGroup();
    uint32_t srcId = m_rfLC.getSrcId();
    uint32_t dstId = m_rfLC.getDstId();

    resetRF();
    resetNet();

    // transmit channelNo release burst
    m_p25->m_control->writeRF_TDULC_ChanRelease(grp, srcId, dstId);
}

/// <summary>
/// Helper to write a network P25 TDU packet.
/// </summary>
void Voice::writeNet_TDU()
{
    uint8_t buffer[P25_TDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_TDU_FRAME_LENGTH_BYTES + 2U);

    buffer[0U] = modem::TAG_EOT;
    buffer[1U] = 0x00U;

    // Generate Sync
    Sync::addP25Sync(buffer + 2U);

    // Generate NID
    m_p25->m_nid.encode(buffer + 2U, P25_DUID_TDU);

    // Add busy bits
    P25Utils::addBusyBits(buffer + 2U, P25_TDU_FRAME_LENGTH_BITS, true, true);

    m_p25->addFrame(buffer, P25_TDU_FRAME_LENGTH_BYTES + 2U, true);

    if (m_verbose) {
        LogMessage(LOG_NET, P25_TDU_STR ", srcId = %u", m_netLC.getSrcId());
    }

    if (m_netFrames > 0) {
        ::ActivityLog("P25", false, "network end of transmission, %.1f seconds, %u%% packet loss",
            float(m_netFrames) / 50.0F, (m_netLost * 100U) / m_netFrames);
    }
    else {
        ::ActivityLog("P25", false, "network end of transmission, %u frames", m_netFrames);
    }

    if (m_p25->m_network != nullptr)
        m_p25->m_network->resetP25();

    ::memset(m_netLDU1, 0x00U, 9U * 25U);
    ::memset(m_netLDU2, 0x00U, 9U * 25U);

    m_p25->m_netTimeout.stop();
    m_p25->m_networkWatchdog.stop();
    resetNet();
    m_p25->m_netState = RS_NET_IDLE;
    m_p25->m_tailOnIdle = true;
}

/// <summary>
/// Helper to check for an unflushed LDU1 packet.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
void Voice::checkNet_LDU1()
{
    if (m_p25->m_netState == RS_NET_IDLE)
        return;

    // Check for an unflushed LDU1
    if (m_netLDU1[10U] != 0x00U || m_netLDU1[26U] != 0x00U || m_netLDU1[55U] != 0x00U ||
        m_netLDU1[80U] != 0x00U || m_netLDU1[105U] != 0x00U || m_netLDU1[130U] != 0x00U ||
        m_netLDU1[155U] != 0x00U || m_netLDU1[180U] != 0x00U || m_netLDU1[204U] != 0x00U)
        writeNet_LDU1();
}

/// <summary>
/// Helper to write a network P25 LDU1 packet.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
void Voice::writeNet_LDU1()
{
    lc::LC control = lc::LC(*m_dfsiLC.control());

    // because the lc::LC internal copy routine will reset the encrypted flag -- lets force it
    control.setEncrypted(m_dfsiLC.control()->getEncrypted());

    data::LowSpeedData lsd = data::LowSpeedData(*m_dfsiLC.lsd());

    uint32_t dstId = control.getDstId();
    uint32_t srcId = control.getSrcId();
    bool group = control.getLCO() == LC_GROUP;

    // ensure our dstId are sane from the last LDU1
    if (m_netLastLDU1.getDstId() != 0U) {
        if (dstId != m_netLastLDU1.getDstId()) {
            if (m_verbose) {
                LogMessage(LOG_NET, P25_LDU1_STR ", dstId = %u doesn't match last LDU1 dstId = %u, fixing",
                    dstId, m_netLastLDU1.getDstId());
            }
            dstId = m_netLastLDU1.getDstId();
        }
    }

    // ensure our srcId are sane from the last LDU1
    if (m_netLastLDU1.getSrcId() != 0U) {
        if (srcId != m_netLastLDU1.getSrcId()) {
            if (m_verbose) {
                LogMessage(LOG_NET, P25_LDU1_STR ", srcId = %u doesn't match last LDU1 srcId = %u, fixing",
                    srcId, m_netLastLDU1.getSrcId());
            }
            srcId = m_netLastLDU1.getSrcId();
        }
    }

    if (m_debug) {
        LogMessage(LOG_NET, P25_LDU1_STR " service flags, emerg = %u, encrypt = %u, prio = %u, DFSI emerg = %u, DFSI encrypt = %u, DFSI prio = %u",
            control.getEmergency(), control.getEncrypted(), control.getPriority(),
            m_dfsiLC.control()->getEmergency(), m_dfsiLC.control()->getEncrypted(), m_dfsiLC.control()->getPriority());
    }

    // set network and RF link control states
    m_netLC = lc::LC();
    m_netLC.setLCO(control.getLCO());
    m_netLC.setMFId(control.getMFId());
    m_netLC.setSrcId(srcId);
    m_netLC.setDstId(dstId);
    m_netLC.setGroup(group);
    m_netLC.setEmergency(control.getEmergency());
    m_netLC.setEncrypted(control.getEncrypted());
    m_netLC.setPriority(control.getPriority());

    m_rfLC = lc::LC();
    m_rfLC.setLCO(control.getLCO());
    m_rfLC.setMFId(control.getMFId());
    m_rfLC.setSrcId(srcId);
    m_rfLC.setDstId(dstId);
    m_rfLC.setGroup(group);
    m_rfLC.setEmergency(control.getEmergency());
    m_rfLC.setEncrypted(control.getEncrypted());
    m_rfLC.setPriority(control.getPriority());

    // if we are idle lets generate HDU data
    if (m_p25->m_netState == RS_NET_IDLE) {
        uint8_t mi[P25_MI_LENGTH_BYTES];
        ::memset(mi, 0x00U, P25_MI_LENGTH_BYTES);

        if (m_netLastLDU1.getAlgId() != P25_ALGO_UNENCRYPT && m_netLastLDU1.getKId() != 0) {
            control.setAlgId(m_netLastLDU1.getAlgId());
            control.setKId(m_netLastLDU1.getKId());
        }

        // restore MI from member variable
        ::memcpy(mi, m_lastMI, P25_MI_LENGTH_BYTES);

        if (m_verbose && m_debug) {
            Utils::dump(1U, "P25 HDU MI from network to RF", mi, P25_MI_LENGTH_BYTES);
        }

        m_netLC.setMI(mi);
        m_rfLC.setMI(mi);
        m_netLC.setAlgId(control.getAlgId());
        m_rfLC.setAlgId(control.getAlgId());
        m_netLC.setKId(control.getKId());
        m_rfLC.setKId(control.getKId());

        // validate source RID
        if (!acl::AccessControl::validateSrcId(srcId)) {
            LogWarning(LOG_NET, P25_HDU_STR " denial, RID rejection, srcId = %u", srcId);
            return;
        }

        // is this a group or individual operation?
        if (!group) {
            // validate the target RID
            if (!acl::AccessControl::validateSrcId(dstId)) {
                LogWarning(LOG_NET, P25_HDU_STR " denial, RID rejection, dstId = %u", dstId);
                return;
            }
        }
        else {
            // validate the target ID, if the target is a talkgroup
            if (!acl::AccessControl::validateTGId(dstId)) {
                LogWarning(LOG_NET, P25_HDU_STR " denial, TGID rejection, dstId = %u", dstId);
                return;
            }
        }

        m_p25->writeRF_Preamble();

        ::ActivityLog("P25", false, "network %svoice transmission from %u to %s%u", m_netLC.getEncrypted() ? "encrypted " : "", srcId, group ? "TG " : "", dstId);

        // single-channel trunking or voice on control support?
        if (m_p25->m_enableControl && m_p25->m_voiceOnControl && !m_p25->m_disableNetworkGrant) {
            uint8_t serviceOptions = (m_netLC.getEmergency() ? 0x80U : 0x00U) +     // Emergency Flag
                (m_netLC.getEncrypted() ? 0x40U : 0x00U) +                          // Encrypted Flag
                (m_netLC.getPriority() & 0x07U);                                    // Priority

            if (!m_p25->m_control->writeRF_TSDU_Grant(srcId, dstId, serviceOptions, group, true)) {
                LogError(LOG_NET, P25_HDU_STR " call failure, network call not granted, dstId = %u", dstId);

                if ((!m_p25->m_networkWatchdog.isRunning() || m_p25->m_networkWatchdog.hasExpired()) &&
                    m_p25->m_netLastDstId != 0U) {
                    if (m_p25->m_network != nullptr)
                        m_p25->m_network->resetP25();

                    ::memset(m_netLDU1, 0x00U, 9U * 25U);
                    ::memset(m_netLDU2, 0x00U, 9U * 25U);

                    m_p25->m_netTimeout.stop();
                    m_p25->m_networkWatchdog.stop();

                    m_netLC = lc::LC();
                    m_netLastLDU1 = lc::LC();
                    m_netLastFrameType = P25_FT_DATA_UNIT;

                    m_p25->m_netState = RS_NET_IDLE;
                    m_p25->m_netLastDstId = 0U;
                    m_p25->m_netLastSrcId = 0U;

                    if (m_p25->m_rfState == RS_RF_REJECTED) {
                        m_p25->m_rfState = RS_RF_LISTENING;
                    }

                    return;
                }
            }

            m_p25->writeRF_Preamble(0, true);
        }

        m_hadVoice = true;
        m_p25->m_netState = RS_NET_AUDIO;
        m_p25->m_netLastDstId = dstId;
        m_p25->m_netLastSrcId = srcId;
        m_p25->m_netTGHang.start();
        m_p25->m_netTimeout.start();
        m_netFrames = 0U;
        m_netLost = 0U;
        m_vocLDU1Count = 0U;
        m_roamLDU1Count = 0U;

        if (!m_p25->m_disableNetworkHDU) {
            if (m_netLastFrameType != P25_FT_HDU_LATE_ENTRY) {
                uint8_t buffer[P25_HDU_FRAME_LENGTH_BYTES + 2U];
                ::memset(buffer, 0x00U, P25_HDU_FRAME_LENGTH_BYTES + 2U);

                // Generate Sync
                Sync::addP25Sync(buffer + 2U);

                // Generate NID
                m_p25->m_nid.encode(buffer + 2U, P25_DUID_HDU);

                // Generate header
                m_netLC.encodeHDU(buffer + 2U);

                // Add busy bits
                P25Utils::addBusyBits(buffer + 2U, P25_HDU_FRAME_LENGTH_BITS, false, true);

                buffer[0U] = modem::TAG_DATA;
                buffer[1U] = 0x00U;

                m_p25->addFrame(buffer, P25_HDU_FRAME_LENGTH_BYTES + 2U, true);

                if (m_verbose) {
                    LogMessage(LOG_NET, P25_HDU_STR ", dstId = %u, algo = $%02X, kid = $%04X", m_netLC.getDstId(), m_netLC.getAlgId(), m_netLC.getKId());
                }
            }
            else {
                if (m_verbose) {
                    LogMessage(LOG_NET, P25_HDU_STR ", not transmitted; network HDU late entry, dstId = %u, algo = $%02X, kid = $%04X", m_netLC.getDstId(), m_netLC.getAlgId(), m_netLC.getKId());
                }
            }
        }
        else {
            if (m_verbose) {
                LogMessage(LOG_NET, P25_HDU_STR ", not transmitted; network HDU disabled, dstId = %u, algo = $%02X, kid = $%04X", m_netLC.getDstId(), m_netLC.getAlgId(), m_netLC.getKId());
            }
        }
    }

    uint32_t netId = control.getNetId();
    uint32_t sysId = control.getSysId();

    // is the network peer a different WACN or system ID?
    if (m_p25->m_enableControl && m_p25->m_allowExplicitSourceId) {
        if (sysId != lc::LC::getSiteData().sysId()) {
            // per TIA-102.AABD-D transmit EXPLICIT_SOURCE_ID every other frame (e.g. every other LDU1)
            m_roamLDU1Count++;
            if (m_roamLDU1Count > ROAM_LDU1_COUNT) {
                m_roamLDU1Count = 0U;
                m_netLC.setNetId(netId);
                m_netLC.setSysId(sysId);
                m_netLC.setLCO(LC_EXPLICIT_SOURCE_ID);
            }
            else {
                // flag explicit block to follow in next LDU1
                if (m_netLC.getLCO() == LC_GROUP) {
                    m_netLC.setExplicitId(true);
                }
            }
        }
    }
    else {
        netId = lc::LC::getSiteData().netId();
        sysId = lc::LC::getSiteData().sysId();
    }

    // single-channel trunking or voice on control support?
    if (m_p25->m_enableControl && m_p25->m_voiceOnControl) {
        // per TIA-102.AABD-B transmit RFSS_STS_BCAST every 3 superframes (e.g. every 3 LDU1s)
        m_vocLDU1Count++;
        if (m_vocLDU1Count > VOC_LDU1_COUNT) {
            m_vocLDU1Count = 0U;
            m_netLC.setLCO(LC_RFSS_STS_BCAST);
        }
    }

    insertMissingAudio(m_netLDU1);

    uint8_t buffer[P25_LDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_LDU_FRAME_LENGTH_BYTES + 2U);

    // Generate Sync
    Sync::addP25Sync(buffer + 2U);

    // Generate NID
    m_p25->m_nid.encode(buffer + 2U, P25_DUID_LDU1);

    // Generate LDU1 data
    m_netLC.encodeLDU1(buffer + 2U);

    // Add the Audio
    m_audio.encode(buffer + 2U, m_netLDU1 + 10U, 0U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 26U, 1U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 55U, 2U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 80U, 3U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 105U, 4U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 130U, 5U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 155U, 6U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 180U, 7U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 204U, 8U);

    // Add the Low Speed Data
    m_netLSD.setLSD1(lsd.getLSD1());
    m_netLSD.setLSD2(lsd.getLSD2());
    m_netLSD.encode(buffer + 2U);

    // Add busy bits
    P25Utils::addBusyBits(buffer + 2U, P25_LDU_FRAME_LENGTH_BITS, false, true);

    buffer[0U] = modem::TAG_DATA;
    buffer[1U] = 0x00U;

    m_p25->addFrame(buffer, P25_LDU_FRAME_LENGTH_BYTES + 2U, true);

    if (m_verbose) {
        LogMessage(LOG_NET, P25_LDU1_STR " audio, srcId = %u, dstId = %u, group = %u, emerg = %u, encrypt = %u, prio = %u, sysId = $%03X, netId = $%05X",
            m_netLC.getSrcId(), m_netLC.getDstId(), m_netLC.getGroup(), m_netLC.getEmergency(), m_netLC.getEncrypted(), m_netLC.getPriority(),
            sysId, netId);
    }

    ::memset(m_netLDU1, 0x00U, 9U * 25U);

    m_netFrames += 9U;
}

/// <summary>
/// Helper to check for an unflushed LDU2 packet.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
void Voice::checkNet_LDU2()
{
    if (m_p25->m_netState == RS_NET_IDLE)
        return;

    // Check for an unflushed LDU2
    if (m_netLDU2[10U] != 0x00U || m_netLDU2[26U] != 0x00U || m_netLDU2[55U] != 0x00U ||
        m_netLDU2[80U] != 0x00U || m_netLDU2[105U] != 0x00U || m_netLDU2[130U] != 0x00U ||
        m_netLDU2[155U] != 0x00U || m_netLDU2[180U] != 0x00U || m_netLDU2[204U] != 0x00U)
        writeNet_LDU2();
}

/// <summary>
/// Helper to write a network P25 LDU2 packet.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
void Voice::writeNet_LDU2()
{
    lc::LC control = lc::LC(*m_dfsiLC.control());
    data::LowSpeedData lsd = data::LowSpeedData(*m_dfsiLC.lsd());

    uint32_t dstId = control.getDstId();

    // don't process network frames if this modem isn't authoritative
    if (!m_p25->m_authoritative && m_p25->m_permittedDstId != dstId) {
        LogWarning(LOG_NET, "[NON-AUTHORITATIVE] Ignoring network traffic (LDU2), destination not permitted!");
        resetNet();
        return;
    }

    uint8_t mi[P25_MI_LENGTH_BYTES];
    control.getMI(mi);

    if (m_verbose && m_debug) {
        Utils::dump(1U, "Network LDU2 MI", mi, P25_MI_LENGTH_BYTES);
    }

    m_netLC.setMI(mi);
    m_netLC.setAlgId(control.getAlgId());
    m_netLC.setKId(control.getKId());

    insertMissingAudio(m_netLDU2);

    uint8_t buffer[P25_LDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_LDU_FRAME_LENGTH_BYTES + 2U);

    // Generate Sync
    Sync::addP25Sync(buffer + 2U);

    // Generate NID
    m_p25->m_nid.encode(buffer + 2U, P25_DUID_LDU2);

    // Generate LDU2 data
    m_netLC.encodeLDU2(buffer + 2U);

    // Add the Audio
    m_audio.encode(buffer + 2U, m_netLDU2 + 10U, 0U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 26U, 1U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 55U, 2U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 80U, 3U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 105U, 4U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 130U, 5U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 155U, 6U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 180U, 7U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 204U, 8U);

    // Add the Low Speed Data
    m_netLSD.setLSD1(lsd.getLSD1());
    m_netLSD.setLSD2(lsd.getLSD2());
    m_netLSD.encode(buffer + 2U);

    // Add busy bits
    P25Utils::addBusyBits(buffer + 2U, P25_LDU_FRAME_LENGTH_BITS, false, true);

    buffer[0U] = modem::TAG_DATA;
    buffer[1U] = 0x00U;

    m_p25->addFrame(buffer, P25_LDU_FRAME_LENGTH_BYTES + 2U, true);

    if (m_verbose) {
        LogMessage(LOG_NET, P25_LDU2_STR " audio, algo = $%02X, kid = $%04X", m_netLC.getAlgId(), m_netLC.getKId());
    }

    ::memset(m_netLDU2, 0x00U, 9U * 25U);

    m_netFrames += 9U;
}

/// <summary>
/// Helper to insert IMBE silence frames for missing audio.
/// </summary>
/// <param name="data"></param>
void Voice::insertMissingAudio(uint8_t *data)
{
    if (data[10U] == 0x00U) {
        ::memcpy(data + 10U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 10U, 11U);
    }

    if (data[26U] == 0x00U) {
        ::memcpy(data + 26U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 26U, 11U);
    }

    if (data[55U] == 0x00U) {
        ::memcpy(data + 55U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 55U, 11U);
    }

    if (data[80U] == 0x00U) {
        ::memcpy(data + 80U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 80U, 11U);
    }

    if (data[105U] == 0x00U) {
        ::memcpy(data + 105U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 105U, 11U);
    }

    if (data[130U] == 0x00U) {
        ::memcpy(data + 130U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 130U, 11U);
    }

    if (data[155U] == 0x00U) {
        ::memcpy(data + 155U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 155U, 11U);
    }

    if (data[180U] == 0x00U) {
        ::memcpy(data + 180U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 180U, 11U);
    }

    if (data[204U] == 0x00U) {
        ::memcpy(data + 204U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 204U, 11U);
    }
}

/// <summary>
/// Helper to insert IMBE null frames for missing audio.
/// </summary>
/// <param name="data"></param>
void Voice::insertNullAudio(uint8_t *data)
{
    if (data[0U] == 0x00U) {
        ::memcpy(data + 10U, P25_NULL_IMBE, 11U);
    }

    if (data[25U] == 0x00U) {
        ::memcpy(data + 26U, P25_NULL_IMBE, 11U);
    }

    if (data[50U] == 0x00U) {
        ::memcpy(data + 55U, P25_NULL_IMBE, 11U);
    }

    if (data[75U] == 0x00U) {
        ::memcpy(data + 80U, P25_NULL_IMBE, 11U);
    }

    if (data[100U] == 0x00U) {
        ::memcpy(data + 105U, P25_NULL_IMBE, 11U);
    }

    if (data[125U] == 0x00U) {
        ::memcpy(data + 130U, P25_NULL_IMBE, 11U);
    }

    if (data[150U] == 0x00U) {
        ::memcpy(data + 155U, P25_NULL_IMBE, 11U);
    }

    if (data[175U] == 0x00U) {
        ::memcpy(data + 180U, P25_NULL_IMBE, 11U);
    }

    if (data[200U] == 0x00U) {
        ::memcpy(data + 204U, P25_NULL_IMBE, 11U);
    }
}

/// <summary>
/// Helper to insert encrypted IMBE null frames for missing audio.
/// </summary>
/// <param name="data"></param>
void Voice::insertEncryptedNullAudio(uint8_t *data)
{
    if (data[0U] == 0x00U) {
        ::memcpy(data + 10U, P25_ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[25U] == 0x00U) {
        ::memcpy(data + 26U, P25_ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[50U] == 0x00U) {
        ::memcpy(data + 55U, P25_ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[75U] == 0x00U) {
        ::memcpy(data + 80U, P25_ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[100U] == 0x00U) {
        ::memcpy(data + 105U, P25_ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[125U] == 0x00U) {
        ::memcpy(data + 130U, P25_ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[150U] == 0x00U) {
        ::memcpy(data + 155U, P25_ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[175U] == 0x00U) {
        ::memcpy(data + 180U, P25_ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[200U] == 0x00U) {
        ::memcpy(data + 204U, P25_ENCRYPTED_NULL_IMBE, 11U);
    }
}

/// <summary>
/// Given the last MI, generate the next MI using LFSR.
/// </summary>
/// <param name="lastMI"></param>
/// <param name="nextMI"></param>
void Voice::getNextMI(uint8_t lastMI[9U], uint8_t nextMI[9U])
{
    uint8_t carry, i;
    std::copy(lastMI, lastMI + 9, nextMI);

    for (uint8_t cycle = 0; cycle < 64; cycle++) {
        // calculate bit 0 for the next cycle
        carry = ((nextMI[0] >> 7) ^ (nextMI[0] >> 5) ^ (nextMI[2] >> 5) ^
                 (nextMI[3] >> 5) ^ (nextMI[4] >> 2) ^ (nextMI[6] >> 6)) &
                0x01;

        // shift all the list elements, except the last one
        for (i = 0; i < 7; i++) {

            // grab high bit from the next element and use it as our low bit
            nextMI[i] = ((nextMI[i] & 0x7F) << 1) | (nextMI[i + 1] >> 7);
        }

        // shift last element, then copy the bit 0 we calculated in
        nextMI[7] = ((nextMI[i] & 0x7F) << 1) | carry;
    }
}
