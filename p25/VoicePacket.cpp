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
*   Copyright (C) 2017-2020 by Bryan Biedenkapp N2PLL
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
#include "p25/VoicePacket.h"
#include "p25/acl/AccessControl.h"
#include "p25/P25Utils.h"
#include "p25/Sync.h"
#include "edac/CRC.h"
#include "HostMain.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Resets the data states for the RF interface.
/// </summary>
void VoicePacket::resetRF()
{
    m_rfLC.reset();
    m_rfLastLDU1.reset();
    m_rfLastLDU2.reset();

    m_rfFrames = 0U;
    m_rfErrs = 0U;
    m_rfBits = 1U;
    m_rfUndecodableLC = 0U;
}

/// <summary>
/// Resets the data states for the network.
/// </summary>
void VoicePacket::resetNet()
{
    m_netLC.reset();
    m_netLastLDU1.reset();
    m_netFrames = 0U;
    m_netLost = 0U;
}

/// <summary>
/// Process a data frame from the RF interface.
/// </summary>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool VoicePacket::process(uint8_t* data, uint32_t len)
{
    assert(data != NULL);

    // Decode the NID
    bool valid = m_p25->m_nid.decode(data + 2U);

    if (m_p25->m_rfState == RS_RF_LISTENING && !valid)
        return false;

    uint8_t duid = m_p25->m_nid.getDUID();
    if (!valid) {
        switch (m_lastDUID) {
            case P25_DUID_HDU:
            case P25_DUID_LDU2:
                duid = P25_DUID_LDU1;
                break;
            case P25_DUID_LDU1:
                duid = P25_DUID_LDU2;
                break;
            default:
                break;
        }
    }

    // are we interrupting a running CC?
    if (m_p25->m_ccRunning) {
        g_interruptP25Control = true;
    }

    if (m_p25->m_rfState != RS_RF_LISTENING) {
        m_p25->m_rfTGHang.start();
    }

    if (duid == P25_DUID_HDU && m_lastDUID == P25_DUID_HDU) {
        duid = P25_DUID_LDU1;
    }

    // handle individual DUIDs
    if (duid == P25_DUID_HDU) {
        m_p25->m_trunk->resetStatusCommand();

        m_lastDUID = P25_DUID_HDU;

        if (m_p25->m_rfState == RS_RF_LISTENING && m_p25->m_ccRunning) {
            m_p25->m_modem->clearP25Data();
            m_p25->m_queue.clear();
            resetRF();
            resetNet();
        }

        if (m_p25->m_rfState == RS_RF_LISTENING || m_p25->m_rfState == RS_RF_AUDIO) {
            resetRF();
            resetNet();

            bool ret = m_rfLC.decodeHDU(data + 2U);
            if (!ret) {
                LogWarning(LOG_RF, P25_HDU_STR ", undecodable LC");
                m_rfUndecodableLC++;
                return false;
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_HDU_STR ", HDU_BSDWNACT, dstId = %u, algo = $%02X, kid = $%04X", m_rfLC.getDstId(), m_rfLC.getAlgId(), m_rfLC.getKId());
            }

            // don't process RF frames if the network isn't in a idle state and the RF destination is the network destination
            if (m_p25->m_netState != RS_NET_IDLE && m_rfLC.getDstId() == m_p25->m_netLastDstId) {
                LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing network traffic!");
                resetRF();
                return false;
            }

            // stop network frames from processing -- RF wants to transmit on a different talkgroup
            if (m_p25->m_netState != RS_NET_IDLE) {
                LogWarning(LOG_RF, "Traffic collision detect, preempting existing network traffic to new RF traffic, rfDstId = %u, netDstId = %u", m_rfLC.getDstId(),
                    m_p25->m_netLastDstId);
                resetNet();
                m_p25->writeRF_TDU(true);
            }

            m_p25->m_rfTGHang.start();
            m_p25->m_rfLastDstId = m_rfLC.getDstId();

            m_rfLastHDU.reset();
            m_rfLastHDU = m_rfLC;
        }

        return true;
    }
    else if (duid == P25_DUID_LDU1) {
        m_p25->m_trunk->resetStatusCommand();

        bool alreadyDecoded = false;
        m_lastDUID = P25_DUID_LDU1;

        if (m_p25->m_rfState == RS_RF_LISTENING) {
            if (!m_p25->m_ccRunning && m_p25->m_voiceOnControl) {
                m_p25->m_trunk->writeRF_ControlData(255U, 0U, false);
            }

            bool ret = m_rfLC.decodeLDU1(data + 2U);
            if (!ret) {
                return false;
            }

            m_rfLastLDU1 = m_rfLC;
            alreadyDecoded = true;

            uint32_t srcId = m_rfLC.getSrcId();
            uint32_t dstId = m_rfLC.getDstId();

            // don't process RF frames if the network isn't in a idle state and the RF destination is the network destination
            if (m_p25->m_netState != RS_NET_IDLE && dstId == m_p25->m_netLastDstId) {
                LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing network traffic!");
                resetRF();
                return false;
            }

            // stop network frames from processing -- RF wants to transmit on a different talkgroup
            if (m_p25->m_netState != RS_NET_IDLE) {
                if (m_netLC.getSrcId() == srcId && m_netLC.getDstId() == dstId) {
                    LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing RF traffic (Are we in a voting condition?), rfSrcId = %u, rfDstId = %u, netSrcId = %u, netDstId = %u", m_rfLC.getSrcId(), m_rfLC.getDstId(),
                        srcId, dstId);
                    resetRF();
                    return false;
                }
                else {
                    LogWarning(LOG_RF, "Traffic collision detect, preempting existing network traffic to new RF traffic, rfDstId = %u, netDstId = %u", m_rfLC.getDstId(),
                        m_p25->m_netLastDstId);
                    resetNet();
/*
                    m_p25->writeRF_TDU(true);
*/
                }
            }

            m_p25->m_trunk->setRFLC(m_rfLC);

            // validate the source RID
            if (!acl::AccessControl::validateSrcId(srcId)) {
                if (m_lastRejectId == 0U || m_lastRejectId != srcId) {
                    LogWarning(LOG_RF, P25_HDU_STR " denial, RID rejection, srcId = %u", srcId);
                    m_p25->m_trunk->writeRF_TSDU_Deny(P25_DENY_RSN_REQ_UNIT_NOT_VALID, (m_rfLC.getGroup() ? TSBK_IOSP_GRP_VCH : TSBK_IOSP_UU_VCH));
                    m_p25->m_trunk->denialInhibit(srcId);

                    ::ActivityLog("P25", true, "RF voice rejection from %u to %s%u ", srcId, m_rfLC.getGroup() ? "TG " : "", dstId);
                    m_lastRejectId = srcId;
                }

                m_p25->m_rfLastDstId = 0U;
                m_p25->m_rfTGHang.stop();
                m_p25->m_rfState = RS_RF_REJECTED;
                return false;
            }

            // is this a group or individual operation?
            if (!m_rfLC.getGroup()) {
                // validate the target RID
                if (!acl::AccessControl::validateSrcId(dstId)) {
                    if (m_lastRejectId == 0 || m_lastRejectId != dstId) {
                        LogWarning(LOG_RF, P25_HDU_STR " denial, RID rejection, dstId = %u", dstId);
                        m_p25->m_trunk->writeRF_TSDU_Deny(P25_DENY_RSN_TGT_UNIT_NOT_VALID, TSBK_IOSP_UU_VCH);

                        ::ActivityLog("P25", true, "RF voice rejection from %u to %s%u ", srcId, m_rfLC.getGroup() ? "TG " : "", dstId);
                        m_lastRejectId = dstId;
                    }

                    m_p25->m_rfLastDstId = 0U;
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
                        m_p25->m_trunk->writeRF_TSDU_Deny(P25_DENY_RSN_TGT_GROUP_NOT_VALID, TSBK_IOSP_GRP_VCH);

                        ::ActivityLog("P25", true, "RF voice rejection from %u to %s%u ", srcId, m_rfLC.getGroup() ? "TG " : "", dstId);
                        m_lastRejectId = dstId;
                    }

                    m_p25->m_rfLastDstId = 0U;
                    m_p25->m_rfTGHang.stop();
                    m_p25->m_rfState = RS_RF_REJECTED;
                    return false;
                }
            }

            // verify the source RID is affiliated to the group TGID; only if control data
            // is supported
            if (m_rfLC.getGroup() && m_p25->m_control) {
                if (!m_p25->m_trunk->hasSrcIdGrpAff(srcId, dstId) &&
                    m_p25->m_trunk->m_verifyAff) {
                    if (m_lastRejectId == 0 || m_lastRejectId != srcId) {
                        LogWarning(LOG_RF, P25_HDU_STR " denial, RID not affiliated to TGID, srcId = %u, dstId = %u", srcId, dstId);
                        m_p25->m_trunk->writeRF_TSDU_Deny(P25_DENY_RSN_REQ_UNIT_NOT_AUTH, TSBK_IOSP_GRP_VCH);
                        m_p25->m_trunk->writeRF_TSDU_U_Reg_Cmd(srcId);

                        ::ActivityLog("P25", true, "RF voice rejection from %u to %s%u ", srcId, m_rfLC.getGroup() ? "TG " : "", dstId);
                        m_lastRejectId = srcId;
                    }

                    m_p25->m_rfLastDstId = 0U;
                    m_p25->m_rfTGHang.stop();
                    m_p25->m_rfState = RS_RF_REJECTED;
                    return false;
                }
            }

            m_lastRejectId = 0U;
            ::ActivityLog("P25", true, "RF %svoice transmission from %u to %s%u", m_rfLC.getEncrypted() ? "encrypted ": "", srcId, m_rfLC.getGroup() ? "TG " : "", dstId);

            if (m_p25->m_control) {
                if (m_rfLC.getGroup() && (m_lastPatchGroup != dstId) &&
                    (dstId != m_p25->m_trunk->m_patchSuperGroup)) {
                    m_p25->m_trunk->writeRF_TSDU_Mot_Patch(dstId, 0U, 0U);
                    m_lastPatchGroup = dstId;
                }

                // if the group wasn't granted out -- explicitly grant the group
                if (!m_p25->m_trunk->hasDstIdGranted(dstId)) {
                    if (m_p25->m_legacyGroupGrnt) {
                        // are we auto-registering legacy radios to groups?
                        if (m_p25->m_legacyGroupReg && m_rfLC.getGroup()) {
                            if (!m_p25->m_trunk->hasSrcIdGrpAff(srcId, dstId)) {
                                if (!m_p25->m_trunk->writeRF_TSDU_Grp_Aff_Rsp(srcId, dstId)) {
                                    return false;
                                }
                            }
                        }                        

                        if (!m_p25->m_trunk->writeRF_TSDU_Grant(m_rfLC.getGroup(), false)) {
                            return false;
                        }
                    }
                    else {
                        return false;
                    }
                }
            }

            // single-channel trunking or voice on control support?
            if (m_p25->m_control && m_p25->m_voiceOnControl) {
                m_p25->m_ccRunning = false; // otherwise the grant will be bundled with other packets
                m_p25->m_trunk->writeRF_TSDU_Grant(m_rfLC.getGroup(), true);
            }

            m_p25->m_trunk->writeRF_TDULC_ChanGrant(m_rfLC.getGroup(), srcId, dstId);

            // perform lost/corrupt HDU checking
            if (m_rfLastHDU.getAlgId() != P25_ALGO_UNENCRYPT && m_rfLastHDU.getKId() != 0) {
                if ((m_rfLC.getAlgId() == P25_ALGO_UNENCRYPT && m_rfLC.getAlgId() != m_rfLastHDU.getAlgId()) &&
                    (m_rfLC.getKId() == 0 && m_rfLC.getKId() != m_rfLastHDU.getKId())) {
                    LogWarning(LOG_RF, P25_HDU_STR ", lost or changed data, using last HDU LC");
                    LogWarning(LOG_RF, P25_HDU_STR ", algo = $%02X, kid = $%04X doesn't match last HDU algo = $%02X, kid = $%04X, fixing",
                            m_rfLC.getAlgId(), m_rfLC.getKId(), m_rfLastHDU.getAlgId(), m_rfLastHDU.getKId());
                    m_rfLC.setAlgId(m_rfLastHDU.getAlgId());
                    m_rfLC.setKId(m_rfLastHDU.getKId());

                    uint8_t mi[P25_MI_LENGTH_BYTES];
                    m_rfLastHDU.getMI(mi);
                    m_rfLC.setMI(mi);

                    m_rfLastHDU.reset();
                }
            }

            m_hadVoice = true;

            m_p25->writeRF_Preamble();

            m_p25->m_rfState = RS_RF_AUDIO;

            m_p25->m_rfTGHang.start();
            m_p25->m_rfLastDstId = dstId;

            uint8_t buffer[P25_HDU_FRAME_LENGTH_BYTES + 2U];
            ::memset(buffer, 0x00U, P25_HDU_FRAME_LENGTH_BYTES + 2U);

            buffer[0U] = TAG_DATA;
            buffer[1U] = 0x00U;

            // Generate Sync
            Sync::addP25Sync(buffer + 2U);

            // Generate NID
            m_p25->m_nid.encode(buffer + 2U, P25_DUID_HDU);

            // Generate HDU
            m_rfLC.encodeHDU(buffer + 2U);

            // Add busy bits
            m_p25->addBusyBits(buffer + 2U, P25_HDU_FRAME_LENGTH_BITS, false, true);

            m_rfFrames = 0U;
            m_rfErrs = 0U;
            m_rfBits = 1U;
            m_rfUndecodableLC = 0U;
            m_p25->m_rfTimeout.start();
            m_lastDUID = P25_DUID_HDU;

            writeNetworkRF(buffer, P25_DUID_HDU);

            if (m_p25->m_duplex) {
                buffer[0U] = TAG_DATA;
                buffer[1U] = 0x00U;
                m_p25->writeQueueRF(buffer, P25_HDU_FRAME_LENGTH_BYTES + 2U);
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_HDU_STR ", dstId = %u, algo = $%02X, kid = $%04X", m_rfLC.getDstId(), m_rfLC.getAlgId(), m_rfLC.getKId());
            }

            if (m_debug) {
                Utils::dump(2U, "!!! *TX P25 Frame - P25_DUID_HDU", buffer + 2U, P25_HDU_FRAME_LENGTH_BYTES);
            }
        }

        if (m_p25->m_rfState == RS_RF_AUDIO) {
            if (!alreadyDecoded) {
                bool ret = m_rfLC.decodeLDU1(data + 2U);
                if (!ret) {
                    LogWarning(LOG_RF, P25_LDU1_STR ", undecodable LC, using last LDU1 LC");
                    m_rfLC = m_rfLastLDU1;
                    m_rfUndecodableLC++;
                }
                else {
                    m_rfLastLDU1 = m_rfLC;
                }
            }

            alreadyDecoded = false;

            if (m_p25->m_control) {
                m_p25->m_trunk->touchDstIdGrant(m_rfLC.getDstId());
            }

            // Generate Sync
            Sync::addP25Sync(data + 2U);

            // Generate NID
            m_p25->m_nid.encode(data + 2U, P25_DUID_LDU1);

            // Generate LDU1 Data
            m_rfLC.encodeLDU1(data + 2U);

            // Generate Low Speed Data
            m_rfLSD.process(data + 2U);

            // Regenerate Audio
            uint32_t errors = m_audio.process(data + 2U);

            // replace audio with silence in cases where the error rate
            // has exceeded the configured threshold
            if (errors > m_silenceThreshold) {
                // generate null audio
                uint8_t buffer[9U * 25U];
                ::memset(buffer, 0x00U, 9U * 25U);

                insertNullAudio(buffer);

                LogWarning(LOG_RF, P25_LDU1_STR ", exceeded lost audio threshold, filling in");

                // Add the Audio
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

            // Add busy bits
            m_p25->addBusyBits(data + 2U, P25_LDU_FRAME_LENGTH_BITS, false, true);

            writeNetworkRF(data + 2U, P25_DUID_LDU1);

            if (m_p25->m_duplex) {
                data[0U] = TAG_DATA;
                data[1U] = 0x00U;
                m_p25->writeQueueRF(data, P25_LDU_FRAME_LENGTH_BYTES + 2U);
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_LDU1_STR ", audio, srcId = %u, group = %u, emerg = %u, encrypt = %u, prio = %u, errs = %u/1233 (%.1f%%)",
                    m_rfLC.getSrcId(), m_rfLC.getGroup(), m_rfLC.getEmergency(), m_rfLC.getEncrypted(), m_rfLC.getPriority(), errors, float(errors) / 12.33F);
            }

            if (m_debug) {
                Utils::dump(2U, "!!! *TX P25 Frame - P25_DUID_LDU1", data + 2U, P25_LDU_FRAME_LENGTH_BYTES);
            }

            return true;
        }
    }
    else if (duid == P25_DUID_LDU2) {
        m_p25->m_trunk->resetStatusCommand();

        m_lastDUID = P25_DUID_LDU2;

        if (m_p25->m_rfState == RS_RF_LISTENING) {
            return false;
        }
        else if (m_p25->m_rfState == RS_RF_AUDIO) {
            bool ret = m_rfLC.decodeLDU2(data + 2U);
            if (!ret) {
                LogWarning(LOG_RF, P25_LDU2_STR ", undecodable LC, using last LDU2 LC");
                m_rfLC = m_rfLastLDU2;

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
                m_rfLastLDU2 = m_rfLC;
            }

            // Generate Sync
            Sync::addP25Sync(data + 2U);

            // Generate NID
            m_p25->m_nid.encode(data + 2U, P25_DUID_LDU2);

            // Generate LDU2 data
            m_rfLC.encodeLDU2(data + 2U);

            // Generate Low Speed Data
            m_rfLSD.process(data + 2U);

            // Regenerate Audio
            uint32_t errors = m_audio.process(data + 2U);

            // replace audio with silence in cases where the error rate
            // has exceeded the configured threshold
            if (errors > m_silenceThreshold) {
                // generate null audio
                uint8_t buffer[9U * 25U];
                ::memset(buffer, 0x00U, 9U * 25U);

                insertNullAudio(buffer);

                LogWarning(LOG_RF, P25_LDU2_STR ", exceeded lost audio threshold, filling in");

                // Add the Audio
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

            // Add busy bits
            m_p25->addBusyBits(data + 2U, P25_LDU_FRAME_LENGTH_BITS, false, true);

            writeNetworkRF(data + 2U, P25_DUID_LDU2);

            if (m_p25->m_duplex) {
                data[0U] = TAG_DATA;
                data[1U] = 0x00U;
                m_p25->writeQueueRF(data, P25_LDU_FRAME_LENGTH_BYTES + 2U);
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_LDU2_STR " audio, algo = $%02X, kid = $%04X, errs = %u/1233 (%.1f%%)", 
                    m_rfLC.getAlgId(), m_rfLC.getKId(), errors, float(errors) / 12.33F);
            }

            if (m_debug) {
                Utils::dump(2U, "!!! *TX P25 Frame - P25_DUID_LDU2", data + 2U, P25_LDU_FRAME_LENGTH_BYTES);
            }

            return true;
        }
    }
    else if (duid == P25_DUID_TDU || duid == P25_DUID_TDULC) {
        m_p25->m_trunk->resetStatusCommand();

        if (m_p25->m_control) {
            m_p25->m_trunk->releaseDstIdGrant(m_rfLC.getDstId(), false);
        }

        if (duid == P25_DUID_TDU) {
            m_p25->writeRF_TDU(false);

            m_lastDUID = duid;

            m_p25->m_rfTimeout.stop();
        }
        else
            m_p25->m_trunk->writeRF_TDULC(duid, false);

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

            if (m_p25->m_continuousControl) {
                m_p25->m_tailOnIdle = false;
                writeRF_EndOfVoice();
            }
            else {
                m_p25->m_tailOnIdle = true;
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
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="duid"></param>
/// <returns></returns>
bool VoicePacket::processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, uint8_t& duid)
{
    uint32_t count = 0U;

    switch (duid) {
        case P25_DUID_LDU1:
            // The '62', '63', '64', '65', '66', '67', '68', '69', '6A' records are LDU1
            if ((data[0U] == 0x62U) && (data[22U] == 0x63U) &&
                (data[36U] == 0x64U) && (data[53U] == 0x65U) &&
                (data[70U] == 0x66U) && (data[87U] == 0x67U) &&
                (data[104U] == 0x68U) && (data[121U] == 0x69U) &&
                (data[138U] == 0x6AU)) {
                // The '62' record
                ::memcpy(m_netLDU1 + 0U, data + count, 22U);
                count += 22U;

                // The '63' record
                ::memcpy(m_netLDU1 + 25U, data + count, 14U);
                count += 14U;

                // The '64' record
                ::memcpy(m_netLDU1 + 50U, data + count, 17U);
                count += 17U;

                // The '65' record
                ::memcpy(m_netLDU1 + 75U, data + count, 17U);
                count += 17U;

                // The '66' record
                ::memcpy(m_netLDU1 + 100U, data + count, 17U);
                count += 17U;

                // The '67' record
                ::memcpy(m_netLDU1 + 125U, data + count, 17U);
                count += 17U;

                // The '68' record
                ::memcpy(m_netLDU1 + 150U, data + count, 17U);
                count += 17U;

                // The '69' record
                ::memcpy(m_netLDU1 + 175U, data + count, 17U);
                count += 17U;

                // The '6A' record
                ::memcpy(m_netLDU1 + 200U, data + count, 16U);
                count += 17U;

                m_p25->m_trunk->resetStatusCommand();

                m_netLastLDU1 = control;

                checkNet_LDU2(control, lsd);
                if (m_p25->m_netState != RS_NET_IDLE) {
                    writeNet_LDU1(control, lsd);
                }
            }
            break;
        case P25_DUID_LDU2:
            // The '6B', '6C', '6D', '6E', '6F', '70', '71', '72', '73' records are LDU2
            if ((data[0U] == 0x6BU) && (data[22U] == 0x6CU) &&
                (data[36U] == 0x6DU) && (data[53U] == 0x6EU) &&
                (data[70U] == 0x6FU) && (data[87U] == 0x70U) &&
                (data[104U] == 0x71U) && (data[121U] == 0x72U) &&
                (data[138U] == 0x73U)) {
                // The '6B' record
                ::memcpy(m_netLDU2 + 0U, data + count, 22U);
                count += 22U;

                // The '6C' record
                ::memcpy(m_netLDU2 + 25U, data + count, 14U);
                count += 14U;

                // The '6D' record
                ::memcpy(m_netLDU2 + 50U, data + count, 17U);
                count += 17U;

                // The '6E' record
                ::memcpy(m_netLDU2 + 75U, data + count, 17U);
                count += 17U;

                // The '6F' record
                ::memcpy(m_netLDU2 + 100U, data + count, 17U);
                count += 17U;

                // The '70' record
                ::memcpy(m_netLDU2 + 125U, data + count, 17U);
                count += 17U;

                // The '71' record
                ::memcpy(m_netLDU2 + 150U, data + count, 17U);
                count += 17U;

                // The '72' record
                ::memcpy(m_netLDU2 + 175U, data + count, 17U);
                count += 17U;

                // The '73' record
                ::memcpy(m_netLDU2 + 200U, data + count, 16U);
                count += 17U;

                m_p25->m_trunk->resetStatusCommand();

                if (m_p25->m_netState == RS_NET_IDLE) {
                    m_p25->m_modem->clearP25Data();
                    m_p25->m_queue.clear();
                    
                    resetRF();
                    m_rfLastHDU.reset();

                    m_netLC.reset();
                    m_netFrames = 0U;
                    m_netLost = 0U;

                    m_p25->m_trunk->resetRF();
                    m_p25->m_trunk->resetNet();

                    writeNet_HDU(control, lsd);
                    if (m_p25->m_netState != RS_NET_IDLE) {
                        writeNet_LDU1(control, lsd);
                    }
                }
                else {
                    checkNet_LDU1(control, lsd);
                }

                if (m_p25->m_netState != RS_NET_IDLE) {
                    writeNet_LDU2(control, lsd);
                }
            }
            break;
        case P25_DUID_TDU:
        case P25_DUID_TDULC:
            // don't process network frames if the RF modem isn't in a listening state
            if (m_p25->m_rfState != RS_RF_LISTENING) {
                resetNet();
                return false;
            }

            if (m_p25->m_control) {
                m_p25->m_trunk->releaseDstIdGrant(m_netLC.getDstId(), false);
            }

            if (m_p25->m_netState != RS_NET_IDLE) {
                m_p25->m_trunk->resetStatusCommand();

                if (duid == P25_DUID_TDU)
                    writeNet_TDU();
                else
                    m_p25->m_trunk->writeNet_TDULC();
            }
            break;
    }

    return true;
}

/// <summary>
/// Helper to write end of frame data.
/// </summary>
/// <returns></returns>
bool VoicePacket::writeEndRF()
{
    if (m_p25->m_netState == RS_NET_IDLE && m_p25->m_rfState == RS_RF_LISTENING) {
        writeRF_EndOfVoice();
        
        // this should have been cleared by writeRF_EndOfVoice; but if it hasn't clear it
        // to prevent badness
        if (m_hadVoice) {
            m_hadVoice = false; 
        }

        if (!m_p25->m_ccRunning) {
            m_p25->m_trunk->writeRF_ControlData(255U, 0U, false);
            m_p25->writeControlEndRF();
        }

        m_p25->m_tailOnIdle = false;

        if (m_network != NULL)
            m_network->resetP25();

        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the VoicePacket class.
/// </summary>
/// <param name="p25">Instance of the Control class.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="debug">Flag indicating whether P25 debug is enabled.</param>
/// <param name="verbose">Flag indicating whether P25 verbose logging is enabled.</param>
VoicePacket::VoicePacket(Control* p25, network::BaseNetwork* network, bool debug, bool verbose) :
    m_p25(p25),
    m_network(network),
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
    m_rfLSD(),
    m_netLSD(),
    m_netLDU1(NULL),
    m_netLDU2(NULL),
    m_lastDUID(P25_DUID_TDU),
    m_lastIMBE(NULL),
    m_hadVoice(false),
    m_lastRejectId(0U),
    m_lastPatchGroup(0U),
    m_silenceThreshold(124U),
    m_verbose(verbose),
    m_debug(debug)
{
    m_netLDU1 = new uint8_t[9U * 25U];
    m_netLDU2 = new uint8_t[9U * 25U];

    ::memset(m_netLDU1, 0x00U, 9U * 25U);
    ::memset(m_netLDU2, 0x00U, 9U * 25U);

    m_lastIMBE = new uint8_t[11U];
    ::memcpy(m_lastIMBE, P25_NULL_IMBE, 11U);
}

/// <summary>
/// Finalizes a instance of the VoicePacket class.
/// </summary>
VoicePacket::~VoicePacket()
{
    delete[] m_netLDU1;
    delete[] m_netLDU2;
    delete[] m_lastIMBE;
}

/// <summary>
/// Write data processed from RF to the network.
/// </summary>
/// <param name="data"></param>
/// <param name="duid"></param>
void VoicePacket::writeNetworkRF(const uint8_t *data, uint8_t duid)
{
    assert(data != NULL);

    if (m_network == NULL)
        return;

    if (m_p25->m_rfTimeout.isRunning() && m_p25->m_rfTimeout.hasExpired())
        return;

    switch (duid) {
        case P25_DUID_HDU:
            // ignore HDU
            break;
        case P25_DUID_LDU1:
            m_network->writeP25LDU1(m_rfLC, m_rfLSD, data);
            break;
        case P25_DUID_LDU2:
            m_network->writeP25LDU2(m_rfLC, m_rfLSD, data);
            break;
        case P25_DUID_TDU:
        case P25_DUID_TDULC:
            m_network->writeP25TDU(m_rfLC, m_rfLSD);
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
void VoicePacket::writeRF_EndOfVoice()
{
    if (!m_hadVoice) {
        return;
    }

    bool grp = m_rfLC.getGroup();
    uint32_t srcId = m_rfLC.getSrcId();
    uint32_t dstId = m_rfLC.getDstId();

    resetRF();
    m_rfLastHDU.reset();

    resetNet();

    // transmit channelNo release burst
    m_p25->m_trunk->writeRF_TDULC_ChanRelease(grp, srcId, dstId);
/*
    if (grp && (m_lastPatchGroup != 0U)) {
        m_p25->m_trunk->writeRF_TSDU_Mot_Patch(m_p25->m_trunk->m_patchSuperGroup, 0U, 0U);
    }
*/
}

/// <summary>
/// Helper to write a network P25 HDU packet.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
void VoicePacket::writeNet_HDU(const lc::LC& control, const data::LowSpeedData& lsd)
{
    uint8_t lco = control.getLCO();
    uint8_t mfId = control.getMFId();
    uint32_t dstId = control.getDstId();
    uint32_t srcId = control.getSrcId();

    // don't process network frames if the destination ID's don't match and the network TG hang timer is running
    if (m_p25->m_rfLastDstId != 0U) {
        if (m_p25->m_rfLastDstId != dstId && (m_p25->m_rfTGHang.isRunning() && !m_p25->m_rfTGHang.hasExpired())) {
            resetNet();
            return;
        }

        if (m_p25->m_rfLastDstId == dstId && (m_p25->m_rfTGHang.isRunning() && !m_p25->m_rfTGHang.hasExpired())) {
            m_p25->m_rfTGHang.start();
        }
    }

    // ensure our srcId and dstId are sane from the last LDU1
    if (m_netLastLDU1.getDstId() != 0U) {
        if (dstId != m_netLastLDU1.getDstId()) {
            LogWarning(LOG_NET, P25_HDU_STR ", dstId = %u doesn't match last LDU1 dstId = %u, fixing",
                m_rfLC.getDstId(), m_rfLastLDU1.getDstId());
            dstId = m_netLastLDU1.getDstId();
        }
    }
    else {
        LogWarning(LOG_NET, P25_HDU_STR ", last LDU1 LC has bad data, dstId = 0");
    }

    if (m_netLastLDU1.getSrcId() != 0U) {
        if (srcId != m_netLastLDU1.getSrcId()) {
            LogWarning(LOG_NET, P25_HDU_STR ", srcId = %u doesn't match last LDU1 srcId = %u, fixing",
                m_rfLC.getSrcId(), m_rfLastLDU1.getSrcId());
            srcId = m_netLastLDU1.getSrcId();
        }
    }
    else {
        LogWarning(LOG_NET, P25_HDU_STR ", last LDU1 LC has bad data, srcId = 0");
    }

    // don't process network frames if the RF modem isn't in a listening state
    if (m_p25->m_rfState != RS_RF_LISTENING) {
        if (m_rfLC.getSrcId() == srcId && m_rfLC.getDstId() == dstId) {
            LogWarning(LOG_RF, "Traffic collision detect, preempting new network traffic to existing RF traffic (Are we in a voting condition?), rfSrcId = %u, rfDstId = %u, netSrcId = %u, netDstId = %u", m_rfLC.getSrcId(), m_rfLC.getDstId(),
                srcId, dstId);
            resetNet();
            return;
        }
        else {
            LogWarning(LOG_RF, "Traffic collision detect, preempting new network traffic to existing RF traffic, rfDstId = %u, netDstId = %u", m_rfLC.getDstId(),
                dstId);
            resetNet();
            return;
        }
    }

    uint8_t algId = m_netLDU2[126U];
    uint32_t kId = (m_netLDU2[127U] << 8) + m_netLDU2[128U];
    bool group = control.getLCO() == LC_GROUP;

    uint8_t mi[P25_MI_LENGTH_BYTES];
    ::memcpy(mi + 0U, m_netLDU2 + 51U, 3U);
    ::memcpy(mi + 3U, m_netLDU2 + 76U, 3U);
    ::memcpy(mi + 6U, m_netLDU2 + 101U, 3U);

    uint8_t serviceOptions = (uint8_t)(m_netLDU1[201U]);

    m_netLC.reset();
    m_netLC.setMI(mi);
    m_netLC.setAlgId(algId);
    m_netLC.setKId(kId);
    m_netLC.setLCO(lco);
    m_netLC.setMFId(mfId);
    m_netLC.setSrcId(srcId);
    m_netLC.setDstId(dstId);
    m_netLC.setGroup(group);
    m_netLC.setEmergency((serviceOptions & 0x80U) == 0x80U);
    m_netLC.setEncrypted((serviceOptions & 0x40U) == 0x40U);
    m_netLC.setPriority((serviceOptions & 0x07U));

    m_p25->m_trunk->setNetLC(m_netLC);

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

    if (!m_p25->m_ccRunning) {
        m_p25->m_trunk->writeRF_ControlData(255U, 0U, false);
    }

    ::ActivityLog("P25", false, "network %svoice transmission from %u to %s%u", m_netLC.getEncrypted() ? "encrypted " : "", srcId, group ? "TG " : "", dstId);

    m_rfLC.reset();
    m_rfLC.setMI(mi);
    m_rfLC.setAlgId(algId);
    m_rfLC.setKId(kId);
    m_rfLC.setMFId(mfId);
    m_rfLC.setSrcId(srcId);
    m_rfLC.setDstId(dstId);
    m_rfLC.setGroup(group);
    m_rfLC.setEmergency((serviceOptions & 0x80U) == 0x80U);
    m_rfLC.setEncrypted((serviceOptions & 0x40U) == 0x40U);
    m_rfLC.setPriority((serviceOptions & 0x07U));

    m_p25->m_trunk->setRFLC(m_rfLC);

    m_p25->writeRF_Preamble();

    if (m_p25->m_control) {
        if (group && (m_lastPatchGroup != dstId) &&
            (dstId != m_p25->m_trunk->m_patchSuperGroup)) {
            m_p25->m_trunk->writeRF_TSDU_Mot_Patch(dstId, 0U, 0U);
            m_lastPatchGroup = dstId;
        }

        if (!m_p25->m_trunk->writeRF_TSDU_Grant(group, false)) {
            if (m_network != NULL)
                m_network->resetP25();

            ::memset(m_netLDU1, 0x00U, 9U * 25U);
            ::memset(m_netLDU2, 0x00U, 9U * 25U);

            m_p25->m_netTimeout.stop();
            m_p25->m_networkWatchdog.stop();
            m_netLC.reset();
            m_netLastLDU1.reset();
            m_p25->m_netState = RS_NET_IDLE;
            m_p25->m_netLastDstId = 0U;
            if (m_p25->m_rfState == RS_RF_REJECTED) {
                m_p25->m_rfState = RS_RF_LISTENING;
            }
            return;
        }
    }

    // single-channel trunking or voice on control support?
    if (m_p25->m_control && m_p25->m_voiceOnControl) {
        m_p25->m_ccRunning = false; // otherwise the grant will be bundled with other packets
        m_p25->m_trunk->writeRF_TSDU_Grant(m_rfLC.getGroup(), true);
    }

    m_p25->m_trunk->writeRF_TDULC_ChanGrant(group, srcId, dstId);

    if (m_verbose) {
        LogMessage(LOG_NET, P25_HDU_STR ", dstId = %u, algo = $%02X, kid = $%04X", m_netLC.getDstId(), m_netLC.getAlgId(), m_netLC.getKId());
    }

    m_hadVoice = true;
    m_p25->m_netState = RS_NET_AUDIO;
    m_p25->m_netLastDstId = dstId;
    m_p25->m_netTimeout.start();
    m_netFrames = 0U;
    m_netLost = 0U;

    uint8_t buffer[P25_HDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_HDU_FRAME_LENGTH_BYTES + 2U);

    buffer[0U] = TAG_DATA;
    buffer[1U] = 0x00U;

    // Generate Sync
    Sync::addP25Sync(buffer + 2U);

    // Generate NID
    m_p25->m_nid.encode(buffer + 2U, P25_DUID_HDU);

    // Generate header
    m_netLC.encodeHDU(buffer + 2U);

    // Add busy bits
    m_p25->addBusyBits(buffer + 2U, P25_HDU_FRAME_LENGTH_BITS, false, true);

    m_p25->writeQueueNet(buffer, P25_HDU_FRAME_LENGTH_BYTES + 2U);

    if (m_debug) {
        Utils::dump(2U, "!!! *TX P25 Network Frame - P25_DUID_HDU", buffer + 2U, P25_HDU_FRAME_LENGTH_BYTES);
    }
}

/// <summary>
/// Helper to write a network P25 TDU packet.
/// </summary>
void VoicePacket::writeNet_TDU()
{
    if (m_p25->m_control) {
        m_p25->m_trunk->releaseDstIdGrant(m_netLC.getDstId(), false);
    }

    uint8_t buffer[P25_TDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_TDU_FRAME_LENGTH_BYTES + 2U);

    buffer[0U] = TAG_EOT;
    buffer[1U] = 0x00U;

    // Generate Sync
    Sync::addP25Sync(buffer + 2U);

    // Generate NID
    m_p25->m_nid.encode(buffer + 2U, P25_DUID_TDU);

    // Add busy bits
    m_p25->addBusyBits(buffer + 2U, P25_TDU_FRAME_LENGTH_BITS, true, true);

    m_p25->writeQueueNet(buffer, P25_TDU_FRAME_LENGTH_BYTES + 2U);

    if (m_verbose) {
        LogMessage(LOG_NET, P25_TDU_STR ", srcId = %u", m_netLC.getSrcId());
    }

    if (m_debug) {
        Utils::dump(2U, "!!! *TX P25 Network Frame - P25_DUID_TDU", buffer + 2U, P25_TDU_FRAME_LENGTH_BYTES);
    }

    if (m_netFrames > 0) {
        ::ActivityLog("P25", false, "network end of transmission, %.1f seconds, %u%% packet loss", 
            float(m_netFrames) / 50.0F, (m_netLost * 100U) / m_netFrames);
    }
    else {
        ::ActivityLog("P25", false, "network end of transmission, %u frames", m_netFrames);
    }

    if (m_network != NULL)
        m_network->resetP25();

    ::memset(m_netLDU1, 0x00U, 9U * 25U);
    ::memset(m_netLDU2, 0x00U, 9U * 25U);

    m_p25->m_netTimeout.stop();
    m_p25->m_networkWatchdog.stop();
    m_netLC.reset();
    m_netLastLDU1.reset();
    m_p25->m_netState = RS_NET_IDLE;
    m_p25->m_netLastDstId = 0U;
    m_p25->m_tailOnIdle = true;
}

/// <summary>
/// Helper to check for an unflushed LDU1 packet.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
void VoicePacket::checkNet_LDU1(const lc::LC& control, const data::LowSpeedData& lsd)
{
    if (m_p25->m_netState == RS_NET_IDLE)
        return;

    // Check for an unflushed LDU1
    if (m_netLDU1[0U] != 0x00U || m_netLDU1[25U] != 0x00U || m_netLDU1[50U] != 0x00U ||
        m_netLDU1[75U] != 0x00U || m_netLDU1[100U] != 0x00U || m_netLDU1[125U] != 0x00U ||
        m_netLDU1[150U] != 0x00U || m_netLDU1[175U] != 0x00U || m_netLDU1[200U] != 0x00U)
        writeNet_LDU1(control, lsd);
}

/// <summary>
/// Helper to write a network P25 LDU1 packet.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
void VoicePacket::writeNet_LDU1(const lc::LC& control, const data::LowSpeedData& lsd)
{
    uint8_t lco = control.getLCO();
    uint8_t mfId = control.getMFId();
    uint32_t dstId = control.getDstId();
    uint32_t srcId = control.getSrcId();

    // don't process network frames if the destination ID's don't match and the network TG hang timer is running
    if (m_p25->m_rfLastDstId != 0U) {
        if (m_p25->m_rfLastDstId != dstId && (m_p25->m_rfTGHang.isRunning() && !m_p25->m_rfTGHang.hasExpired())) {
            resetNet();
            return;
        }

        if (m_p25->m_rfLastDstId == dstId && (m_p25->m_rfTGHang.isRunning() && !m_p25->m_rfTGHang.hasExpired())) {
            m_p25->m_rfTGHang.start();
        }
    }

    uint8_t serviceOptions = (uint8_t)(m_netLDU1[201U]);

    if (m_p25->m_control) {
        m_p25->m_trunk->touchDstIdGrant(m_rfLC.getDstId());
    }

    m_netLC.setLCO(lco);
    m_netLC.setMFId(mfId);
    m_netLC.setSrcId(srcId);
    m_netLC.setDstId(dstId);
    m_netLC.setEmergency((serviceOptions & 0x80U) == 0x80U);
    m_netLC.setEncrypted((serviceOptions & 0x40U) == 0x40U);
    m_netLC.setPriority((serviceOptions & 0x07U));

    insertMissingAudio(m_netLDU1);

    uint8_t buffer[P25_LDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_LDU_FRAME_LENGTH_BYTES + 2U);

    buffer[0U] = TAG_DATA;
    buffer[1U] = 0x00U;

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
    m_p25->addBusyBits(buffer + 2U, P25_LDU_FRAME_LENGTH_BITS, false, true);

    m_p25->writeQueueNet(buffer, P25_LDU_FRAME_LENGTH_BYTES + 2U);

    if (m_verbose) {
        uint32_t loss = 0;
        if (m_netFrames != 0) {
            loss = (m_netLost * 100U) / m_netFrames;
        }
        else {
            loss = (m_netLost * 100U) / 1U;
            if (loss > 100) {
                loss = 100;
            }
        }

        LogMessage(LOG_NET, P25_LDU1_STR " audio, srcId = %u, group = %u, emerg = %u, encrypt = %u, prio = %u, %u%% packet loss",
            m_netLC.getSrcId(), m_netLC.getGroup(), m_netLC.getEmergency(), m_netLC.getEncrypted(), m_netLC.getPriority(), loss);
    }

    if (m_debug) {
        Utils::dump(2U, "!!! *TX P25 Network Frame - P25_DUID_LDU1", buffer + 2U, P25_LDU_FRAME_LENGTH_BYTES);
    }

    ::memset(m_netLDU1, 0x00U, 9U * 25U);

    m_netFrames += 9U;
}

/// <summary>
/// Helper to check for an unflushed LDU2 packet.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
void VoicePacket::checkNet_LDU2(const lc::LC& control, const data::LowSpeedData& lsd)
{
    if (m_p25->m_netState == RS_NET_IDLE)
        return;

    // Check for an unflushed LDU2
    if (m_netLDU2[0U] != 0x00U || m_netLDU2[25U] != 0x00U || m_netLDU2[50U] != 0x00U ||
        m_netLDU2[75U] != 0x00U || m_netLDU2[100U] != 0x00U || m_netLDU2[125U] != 0x00U ||
        m_netLDU2[150U] != 0x00U || m_netLDU2[175U] != 0x00U || m_netLDU2[200U] != 0x00U)
        writeNet_LDU2(control, lsd);
}

/// <summary>
/// Helper to write a network P25 LDU2 packet.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
void VoicePacket::writeNet_LDU2(const lc::LC& control, const data::LowSpeedData& lsd)
{
    uint8_t algId = m_netLDU2[126U];
    uint32_t kId = (m_netLDU2[127U] << 8) + m_netLDU2[128U];

    // don't process network frames if the destination ID's don't match and the network TG hang timer is running
    if (m_p25->m_rfLastDstId != 0U) {
        if (m_p25->m_rfLastDstId != m_netLastLDU1.getDstId() && (m_p25->m_rfTGHang.isRunning() && !m_p25->m_rfTGHang.hasExpired())) {
            resetNet();
            return;
        }
    }

    uint8_t mi[P25_MI_LENGTH_BYTES];
    ::memcpy(mi + 0U, m_netLDU2 + 51U, 3U);
    ::memcpy(mi + 3U, m_netLDU2 + 76U, 3U);
    ::memcpy(mi + 6U, m_netLDU2 + 101U, 3U);

    m_netLC.setMI(mi);
    m_netLC.setAlgId(algId);
    m_netLC.setKId(kId);

    insertMissingAudio(m_netLDU2);

    uint8_t buffer[P25_LDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_LDU_FRAME_LENGTH_BYTES + 2U);

    buffer[0U] = TAG_DATA;
    buffer[1U] = 0x00U;

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
    m_p25->addBusyBits(buffer + 2U, P25_LDU_FRAME_LENGTH_BITS, false, true);

    m_p25->writeQueueNet(buffer, P25_LDU_FRAME_LENGTH_BYTES + 2U);

    if (m_verbose) {
        uint32_t loss = 0;
        if (m_netFrames != 0) {
            loss = (m_netLost * 100U) / m_netFrames;
        }
        else {
            loss = (m_netLost * 100U) / 1U;
            if (loss > 100) {
                loss = 100;
            }
        }

        LogMessage(LOG_NET, P25_LDU2_STR " audio, algo = $%02X, kid = $%04X, %u%% packet loss", m_netLC.getAlgId(), m_netLC.getKId(), loss);
    }

    if (m_debug) {
        Utils::dump(2U, "!!! *TX P25 Network Frame - P25_DUID_LDU2", buffer + 2U, P25_LDU_FRAME_LENGTH_BYTES);
    }

    ::memset(m_netLDU2, 0x00U, 9U * 25U);

    m_netFrames += 9U;
}

/// <summary>
/// Helper to insert IMBE silence frames for missing audio.
/// </summary>
/// <param name="data"></param>
void VoicePacket::insertMissingAudio(uint8_t* data)
{
    if (data[0U] == 0x00U) {
        ::memcpy(data + 10U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 10U, 11U);
    }

    if (data[25U] == 0x00U) {
        ::memcpy(data + 26U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 26U, 11U);
    }

    if (data[50U] == 0x00U) {
        ::memcpy(data + 55U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 55U, 11U);
    }

    if (data[75U] == 0x00U) {
        ::memcpy(data + 80U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 80U, 11U);
    }

    if (data[100U] == 0x00U) {
        ::memcpy(data + 105U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 105U, 11U);
    }

    if (data[125U] == 0x00U) {
        ::memcpy(data + 130U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 130U, 11U);
    }

    if (data[150U] == 0x00U) {
        ::memcpy(data + 155U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 155U, 11U);
    }

    if (data[175U] == 0x00U) {
        ::memcpy(data + 180U, m_lastIMBE, 11U);
        m_netLost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 180U, 11U);
    }

    if (data[200U] == 0x00U) {
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
void VoicePacket::insertNullAudio(uint8_t* data)
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
