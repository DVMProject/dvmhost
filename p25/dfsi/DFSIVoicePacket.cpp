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
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
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
#include "p25/acl/AccessControl.h"
#include "p25/dfsi/DFSIDefines.h"
#include "p25/dfsi/DFSIVoicePacket.h"
#include "p25/P25Utils.h"
#include "p25/Sync.h"
#include "HostMain.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::dfsi;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t VOC_LDU1_COUNT = 3U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Resets the data states for the RF interface.
/// </summary>
void DFSIVoicePacket::resetRF()
{
    VoicePacket::resetRF();

    LC lc = LC();
    m_rfDFSILC = lc;
}

/// <summary>
/// Resets the data states for the network.
/// </summary>
void DFSIVoicePacket::resetNet()
{
    VoicePacket::resetNet();

    LC lc = LC();
    m_netDFSILC = lc;
}

/// <summary>
/// Process a data frame from the RF interface.
/// </summary>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool DFSIVoicePacket::process(uint8_t* data, uint32_t len)
{
    assert(data != NULL);

    bool valid = m_rfDFSILC.decodeNID(data + 2U);

    if (m_p25->m_rfState == RS_RF_LISTENING && !valid)
        return false;

    uint8_t frameType = m_rfDFSILC.getFrameType();
    if (frameType == P25_DFSI_VHDR2) {
        if (m_p25->m_rfState == RS_RF_LISTENING && m_p25->m_ccRunning) {
            m_p25->m_modem->clearP25Data();
            m_p25->m_queue.clear();
            resetRF();
            resetNet();
        }

        if (m_p25->m_rfState == RS_RF_LISTENING || m_p25->m_rfState == RS_RF_AUDIO) {
            resetRF();
            resetNet();

            bool ret = m_rfDFSILC.decodeVHDR2(data + 2U);
            if (!ret) {
                LogWarning(LOG_RF, P25_HDU_STR " DFSI, undecodable LC");
                m_rfUndecodableLC++;
                return false;
            }

            m_rfLC = m_rfDFSILC.control();

            if (m_verbose) {
                LogMessage(LOG_RF, P25_HDU_STR " DFSI, HDU_BSDWNACT, dstId = %u, algo = $%02X, kid = $%04X", m_rfLC.getDstId(), m_rfLC.getAlgId(), m_rfLC.getKId());
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
            }

            m_p25->m_rfTGHang.start();
            m_p25->m_rfLastDstId = m_rfLC.getDstId();

            m_rfLastHDU = m_rfLC;
        }

        return true;
    }
    else if (frameType >= P25_DFSI_LDU1_VOICE1 && frameType <= P25_DFSI_LDU1_VOICE9) {
        uint8_t n = 10U;
        switch (frameType) {
        case P25_DFSI_LDU1_VOICE1:
            n = 10U;
            break;
        case P25_DFSI_LDU1_VOICE2:
            n = 26U;
            break;
        case P25_DFSI_LDU1_VOICE3:
            n = 55U;
            break;
        case P25_DFSI_LDU1_VOICE4:
            n = 80U;
            break;
        case P25_DFSI_LDU1_VOICE5:
            n = 105U;
            break;
        case P25_DFSI_LDU1_VOICE6:
            n = 130U;
            break;
        case P25_DFSI_LDU1_VOICE7:
            n = 155U;
            break;
        case P25_DFSI_LDU1_VOICE8:
            n = 180U;
            break;
        case P25_DFSI_LDU1_VOICE9:
            n = 204U;
            break;
        }

        if (m_rfDFSILC.decodeLDU1(data + 2U, m_dfsiLDU1 + n)) {
            // if this is the last LDU1 frame process the full LDU1
            if (frameType == P25_DFSI_LDU1_VOICE9) {
                bool alreadyDecoded = false;
                m_lastDUID = P25_DUID_LDU1;

                if (m_p25->m_rfState == RS_RF_LISTENING) {
                    if (m_p25->m_control) {
                        if (!m_p25->m_ccRunning && m_p25->m_voiceOnControl) {
                            m_p25->m_trunk->writeRF_ControlData(255U, 0U, false);
                        }
                    }

                    lc::LC lc = m_rfDFSILC.control();

                    uint32_t srcId = lc.getSrcId();
                    uint32_t dstId = lc.getDstId();
                    bool group = lc.getGroup();
                    bool encrypted = lc.getEncrypted();

                    alreadyDecoded = true;

                    // don't process RF frames if the network isn't in a idle state and the RF destination is the network destination
                    if (m_p25->m_netState != RS_NET_IDLE && dstId == m_p25->m_netLastDstId) {
                        LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing network traffic!");
                        resetRF();
                        ::memset(m_dfsiLDU1, 0x00U, 9U * 25U);
                        return false;
                    }

                    // stop network frames from processing -- RF wants to transmit on a different talkgroup
                    if (m_p25->m_netState != RS_NET_IDLE) {
                        if (m_netLC.getSrcId() == srcId && m_p25->m_netLastDstId == dstId) {
                            LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing RF traffic (Are we in a voting condition?), rfSrcId = %u, rfDstId = %u, netSrcId = %u, netDstId = %u", srcId, dstId,
                                m_netLC.getSrcId(), m_p25->m_netLastDstId);
                            resetRF();
                            ::memset(m_dfsiLDU1, 0x00U, 9U * 25U);
                            return false;
                        }
                        else {
                            LogWarning(LOG_RF, "Traffic collision detect, preempting existing network traffic to new RF traffic, rfDstId = %u, netDstId = %u", dstId,
                                m_p25->m_netLastDstId);
                            resetNet();
                        }
                    }

                    m_p25->m_trunk->m_rfTSBK = lc::TSBK(&lc);
                    m_p25->m_trunk->m_rfTSBK.setVerbose(m_p25->m_trunk->m_dumpTSBK);

                    // validate the source RID
                    if (!acl::AccessControl::validateSrcId(srcId)) {
                        if (m_lastRejectId == 0U || m_lastRejectId != srcId) {
                            LogWarning(LOG_RF, P25_HDU_STR " denial, RID rejection, srcId = %u", srcId);
                            if (m_p25->m_control) {
                                m_p25->m_trunk->writeRF_TSDU_Deny(P25_DENY_RSN_REQ_UNIT_NOT_VALID, (group ? TSBK_IOSP_GRP_VCH : TSBK_IOSP_UU_VCH));
                                m_p25->m_trunk->denialInhibit(srcId);
                            }

                            ::ActivityLog("P25", true, "RF voice rejection from %u to %s%u ", srcId, group ? "TG " : "", dstId);
                            m_lastRejectId = srcId;
                        }

                        m_p25->m_rfLastDstId = 0U;
                        m_p25->m_rfTGHang.stop();
                        m_p25->m_rfState = RS_RF_REJECTED;
                        ::memset(m_dfsiLDU1, 0x00U, 9U * 25U);
                        return false;
                    }

                    // is this a group or individual operation?
                    if (!group) {
                        // validate the target RID
                        if (!acl::AccessControl::validateSrcId(dstId)) {
                            if (m_lastRejectId == 0 || m_lastRejectId != dstId) {
                                LogWarning(LOG_RF, P25_HDU_STR " denial, RID rejection, dstId = %u", dstId);
                                if (m_p25->m_control) {
                                    m_p25->m_trunk->writeRF_TSDU_Deny(P25_DENY_RSN_TGT_UNIT_NOT_VALID, TSBK_IOSP_UU_VCH);
                                }

                                ::ActivityLog("P25", true, "RF voice rejection from %u to %s%u ", srcId, group ? "TG " : "", dstId);
                                m_lastRejectId = dstId;
                            }

                            m_p25->m_rfLastDstId = 0U;
                            m_p25->m_rfTGHang.stop();
                            m_p25->m_rfState = RS_RF_REJECTED;
                            ::memset(m_dfsiLDU1, 0x00U, 9U * 25U);
                            return false;
                        }
                    }
                    else {
                        // validate the target ID, if the target is a talkgroup
                        if (!acl::AccessControl::validateTGId(dstId)) {
                            if (m_lastRejectId == 0 || m_lastRejectId != dstId) {
                                LogWarning(LOG_RF, P25_HDU_STR " denial, TGID rejection, dstId = %u", dstId);
                                if (m_p25->m_control) {
                                    m_p25->m_trunk->writeRF_TSDU_Deny(P25_DENY_RSN_TGT_GROUP_NOT_VALID, TSBK_IOSP_GRP_VCH);
                                }

                                ::ActivityLog("P25", true, "RF voice rejection from %u to %s%u ", srcId, group ? "TG " : "", dstId);
                                m_lastRejectId = dstId;
                            }

                            m_p25->m_rfLastDstId = 0U;
                            m_p25->m_rfTGHang.stop();
                            m_p25->m_rfState = RS_RF_REJECTED;
                            ::memset(m_dfsiLDU1, 0x00U, 9U * 25U);
                            return false;
                        }
                    }

                    // verify the source RID is affiliated to the group TGID; only if control data
                    // is supported
                    if (group && m_p25->m_control) {
                        if (!m_p25->m_trunk->hasSrcIdGrpAff(srcId, dstId) && m_p25->m_trunk->m_verifyAff) {
                            if (m_lastRejectId == 0 || m_lastRejectId != srcId) {
                                LogWarning(LOG_RF, P25_HDU_STR " denial, RID not affiliated to TGID, srcId = %u, dstId = %u", srcId, dstId);
                                m_p25->m_trunk->writeRF_TSDU_Deny(P25_DENY_RSN_REQ_UNIT_NOT_AUTH, TSBK_IOSP_GRP_VCH);
                                m_p25->m_trunk->writeRF_TSDU_U_Reg_Cmd(srcId);

                                ::ActivityLog("P25", true, "RF voice rejection from %u to %s%u ", srcId, group ? "TG " : "", dstId);
                                m_lastRejectId = srcId;
                            }

                            m_p25->m_rfLastDstId = 0U;
                            m_p25->m_rfTGHang.stop();
                            m_p25->m_rfState = RS_RF_REJECTED;
                            ::memset(m_dfsiLDU1, 0x00U, 9U * 25U);
                            return false;
                        }
                    }

                    m_rfLC = lc;
                    m_rfLastLDU1 = m_rfLC;

                    m_lastRejectId = 0U;
                    ::ActivityLog("P25", true, "RF %svoice transmission from %u to %s%u", encrypted ? "encrypted " : "", srcId, group ? "TG " : "", dstId);

                    if (m_p25->m_control) {
                        if (group && (m_lastPatchGroup != dstId) &&
                            (dstId != m_p25->m_trunk->m_patchSuperGroup)) {
                            m_p25->m_trunk->writeRF_TSDU_Mot_Patch(dstId, 0U, 0U);
                            m_lastPatchGroup = dstId;
                        }

                        // if the group wasn't granted out -- explicitly grant the group
                        if (!m_p25->m_trunk->hasDstIdGranted(dstId)) {
                            if (m_p25->m_legacyGroupGrnt) {
                                // are we auto-registering legacy radios to groups?
                                if (m_p25->m_legacyGroupReg && group) {
                                    if (!m_p25->m_trunk->hasSrcIdGrpAff(srcId, dstId)) {
                                        if (!m_p25->m_trunk->writeRF_TSDU_Grp_Aff_Rsp(srcId, dstId)) {
                                            ::memset(m_dfsiLDU1, 0x00U, 9U * 25U);
                                            return false;
                                        }
                                    }
                                }

                                if (!m_p25->m_trunk->writeRF_TSDU_Grant(group)) {
                                    ::memset(m_dfsiLDU1, 0x00U, 9U * 25U);
                                    return false;
                                }
                            }
                            else {
                                ::memset(m_dfsiLDU1, 0x00U, 9U * 25U);
                                return false;
                            }
                        }
                    }

                    // single-channel trunking or voice on control support?
                    if (m_p25->m_control && m_p25->m_voiceOnControl) {
                        m_p25->m_ccRunning = false; // otherwise the grant will be bundled with other packets
                        m_p25->m_trunk->writeRF_TSDU_Grant(group, true);
                    }

                    m_hadVoice = true;

                    m_p25->m_rfState = RS_RF_AUDIO;

                    m_p25->m_rfTGHang.start();
                    m_p25->m_rfLastDstId = dstId;

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
                        m_p25->addBusyBits(buffer + 2U, P25_HDU_FRAME_LENGTH_BITS, false, true);

                        writeNetworkRF(buffer, P25_DUID_HDU);

                        if (m_verbose) {
                            LogMessage(LOG_RF, P25_HDU_STR " DFSI, dstId = %u, algo = $%02X, kid = $%04X", m_rfLC.getDstId(), m_rfLC.getAlgId(), m_rfLC.getKId());
                        }
                    }
                    else {
                        LogWarning(LOG_RF, P25_HDU_STR " DFSI, not transmitted; possible late entry, dstId = %u, algo = $%02X, kid = $%04X", m_rfLastHDU.getDstId(), m_rfLastHDU.getAlgId(), m_rfLastHDU.getKId());
                    }

                    m_rfFrames = 0U;
                    m_rfErrs = 0U;
                    m_rfBits = 1U;
                    m_rfUndecodableLC = 0U;
                    m_vocLDU1Count = 0U;
                    m_p25->m_rfTimeout.start();
                    m_lastDUID = P25_DUID_HDU;

                    m_rfLastHDU = lc::LC(m_p25->m_siteData);
                }

                if (m_p25->m_rfState == RS_RF_AUDIO) {
                    if (!alreadyDecoded) {
                        m_rfLC = m_rfDFSILC.control();
                        m_rfLastLDU1 = m_rfLC;
                    }

                    if (m_p25->m_control) {
                        m_p25->m_trunk->touchDstIdGrant(m_rfLC.getDstId());
                    }

                    // single-channel trunking or voice on control support?
                    if (m_p25->m_control && m_p25->m_voiceOnControl) {
                        // per TIA-102.AABD-B transmit RFSS_STS_BCAST every 3 superframes (e.g. every 3 LDU1s)
                        m_vocLDU1Count++;
                        if (m_vocLDU1Count > VOC_LDU1_COUNT) {
                            m_vocLDU1Count = 0U;
                            m_rfLC.setLCO(LC_RFSS_STS_BCAST);
                        }
                    }

                    uint8_t buffer[P25_LDU_FRAME_LENGTH_BYTES + 2U];
                    ::memset(buffer, 0x00U, P25_LDU_FRAME_LENGTH_BYTES + 2U);

                    // Generate Sync
                    Sync::addP25Sync(buffer + 2U);

                    // Generate NID
                    m_p25->m_nid.encode(buffer + 2U, P25_DUID_LDU1);

                    // Generate LDU1 Data
                    m_rfLC.encodeLDU1(buffer + 2U);

                    // Generate Low Speed Data
                    m_rfLSD.process(buffer + 2U);

                    insertMissingAudio(m_dfsiLDU1);

                    // Add the Audio
                    m_audio.encode(buffer + 2U, m_dfsiLDU1 + 10U, 0U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU1 + 26U, 1U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU1 + 55U, 2U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU1 + 80U, 3U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU1 + 105U, 4U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU1 + 130U, 5U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU1 + 155U, 6U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU1 + 180U, 7U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU1 + 204U, 8U);

                    m_rfBits += 1233U;
                    m_rfFrames++;

                    // Add busy bits
                    m_p25->addBusyBits(buffer + 2U, P25_LDU_FRAME_LENGTH_BITS, false, true);

                    writeNetworkRF(buffer + 2U, P25_DUID_LDU1);

                    if (m_verbose) {
                        LogMessage(LOG_RF, P25_LDU1_STR " DFSI, audio, srcId = %u, dstId = %u, group = %u, emerg = %u, encrypt = %u, prio = %u",
                            m_rfLC.getSrcId(), m_rfLC.getDstId(), m_rfLC.getGroup(), m_rfLC.getEmergency(), m_rfLC.getEncrypted(), m_rfLC.getPriority());
                    }

                    ::memset(m_dfsiLDU1, 0x00U, 9U * 25U);
                    return true;
                }
            }
        }
    }
    else if (frameType >= P25_DFSI_LDU2_VOICE10 && frameType <= P25_DFSI_LDU2_VOICE18) {
        uint8_t n = 10U;
        switch (frameType) {
        case P25_DFSI_LDU2_VOICE10:
            n = 10U;
            break;
        case P25_DFSI_LDU2_VOICE11:
            n = 26U;
            break;
        case P25_DFSI_LDU2_VOICE12:
            n = 55U;
            break;
        case P25_DFSI_LDU2_VOICE13:
            n = 80U;
            break;
        case P25_DFSI_LDU2_VOICE14:
            n = 105U;
            break;
        case P25_DFSI_LDU2_VOICE15:
            n = 130U;
            break;
        case P25_DFSI_LDU2_VOICE16:
            n = 155U;
            break;
        case P25_DFSI_LDU2_VOICE17:
            n = 180U;
            break;
        case P25_DFSI_LDU2_VOICE18:
            n = 204U;
            break;
        }

        if (m_rfDFSILC.decodeLDU2(data + 2U, m_dfsiLDU2 + n)) {
            // if this is the last LDU2 frame process the full LDU2
            if (frameType == P25_DFSI_LDU2_VOICE18) {
                m_lastDUID = P25_DUID_LDU2;

                if (m_p25->m_rfState == RS_RF_LISTENING) {
                    ::memset(m_dfsiLDU2, 0x00U, 9U * 25U);
                    return false;
                }
                else if (m_p25->m_rfState == RS_RF_AUDIO) {
                    m_rfLC = m_rfDFSILC.control();
                    m_rfLastLDU2 = m_rfLC;

                    uint8_t buffer[P25_LDU_FRAME_LENGTH_BYTES + 2U];
                    ::memset(buffer, 0x00U, P25_LDU_FRAME_LENGTH_BYTES + 2U);

                    // Generate Sync
                    Sync::addP25Sync(buffer + 2U);

                    // Generate NID
                    m_p25->m_nid.encode(buffer + 2U, P25_DUID_LDU2);

                    // Generate LDU2 data
                    m_rfLC.encodeLDU2(buffer + 2U);

                    // Generate Low Speed Data
                    m_rfLSD.process(buffer + 2U);

                    insertMissingAudio(m_dfsiLDU2);

                    // Add the Audio
                    m_audio.encode(buffer + 2U, m_dfsiLDU2 + 10U, 0U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU2 + 26U, 1U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU2 + 55U, 2U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU2 + 80U, 3U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU2 + 105U, 4U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU2 + 130U, 5U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU2 + 155U, 6U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU2 + 180U, 7U);
                    m_audio.encode(buffer + 2U, m_dfsiLDU2 + 204U, 8U);

                    m_rfBits += 1233U;
                    m_rfFrames++;

                    // Add busy bits
                    m_p25->addBusyBits(buffer + 2U, P25_LDU_FRAME_LENGTH_BITS, false, true);

                    writeNetworkRF(buffer + 2U, P25_DUID_LDU2);

                    if (m_verbose) {
                        LogMessage(LOG_RF, P25_LDU2_STR " DFSI, audio, algo = $%02X, kid = $%04X",
                            m_rfLC.getAlgId(), m_rfLC.getKId());
                    }

                    ::memset(m_dfsiLDU2, 0x00U, 9U * 25U);
                    return true;
                }
            }
        }
    }
    else if (frameType == P25_DFSI_START_STOP) {
        if (m_rfDFSILC.getType() == P25_DFSI_TYPE_VOICE && m_rfDFSILC.getStartStop() == P25_DFSI_STOP_FLAG) {
            if (m_p25->m_control) {
                m_p25->m_trunk->releaseDstIdGrant(m_rfLC.getDstId(), false);
            }

            uint8_t data[P25_TDU_FRAME_LENGTH_BYTES + 2U];
            ::memset(data + 2U, 0x00U, P25_TDU_FRAME_LENGTH_BYTES);

            // Generate Sync
            Sync::addP25Sync(data + 2U);

            // Generate NID
            m_p25->m_nid.encode(data + 2U, P25_DUID_TDU);

            // Add busy bits
            m_p25->addBusyBits(data + 2U, P25_TDU_FRAME_LENGTH_BITS, true, true);

            writeNetworkRF(data + 2U, P25_DUID_TDU);

            m_lastDUID = P25_DUID_TDU;

            m_p25->m_rfTimeout.stop();

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

                LogMessage(LOG_RF, P25_TDU_STR " DFSI, total frames: %d, bits: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", 
                    m_rfFrames, m_rfBits, m_rfUndecodableLC, m_rfErrs, float(m_rfErrs * 100U) / float(m_rfBits));

                if (m_p25->m_dedicatedControl) {
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
    }
    else {
        LogError(LOG_RF, "P25 unhandled DFSI frame type, frameType = $%02X", frameType);
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
bool DFSIVoicePacket::processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, uint8_t& duid)
{
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
            count += 22U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE2);
            m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 26U);
            count += 14U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE3);
            m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 55U);
            count += 17U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE4);
            m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 80U);
            count += 17U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE5);
            m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 105U);
            count += 17U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE6);
            m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 130U);
            count += 17U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE7);
            m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 155U);
            count += 17U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE8);
            m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 180U);
            count += 17U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE9);
            m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 204U);
            count += 16U;

            m_netLastLDU1 = control;

            if (m_p25->m_netState == RS_NET_IDLE) {
                // are we interrupting a running CC?
                if (m_p25->m_ccRunning) {
                    g_interruptP25Control = true;
                }

                // single-channel trunking or voice on control support?
                if (m_p25->m_control && m_p25->m_voiceOnControl) {
                    m_p25->m_ccRunning = false; // otherwise the grant will be bundled with other packets
                }
            }

            checkNet_LDU2();
            if (m_p25->m_netState != RS_NET_IDLE) {
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
            count += 22U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE11);
            m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 26U);
            count += 14U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE12);
            m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 55U);
            count += 17U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE13);
            m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 80U);
            count += 17U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE14);
            m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 105U);
            count += 17U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE15);
            m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 130U);
            count += 17U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE16);
            m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 155U);
            count += 17U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE17);
            m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 180U);
            count += 17U;

            m_dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE18);
            m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 204U);
            count += 16U;

            if (m_p25->m_netState == RS_NET_IDLE) {
                if (!m_p25->m_voiceOnControl) {
                    m_p25->m_modem->clearP25Data();
                }
                m_p25->m_queue.clear();

                resetRF();
                resetNet();

                m_p25->m_trunk->m_rfTSBK = lc::TSBK(m_p25->m_siteData, m_p25->m_idenEntry, m_p25->m_trunk->m_dumpTSBK);
                m_p25->m_trunk->m_netTSBK = lc::TSBK(m_p25->m_siteData, m_p25->m_idenEntry, m_p25->m_trunk->m_dumpTSBK);

                writeNet_LDU1();
            }
            else {
                checkNet_LDU1();
            }

            if (m_p25->m_netState != RS_NET_IDLE) {
                writeNet_LDU2();
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
/// Initializes a new instance of the DFSIVoicePacket class.
/// </summary>
/// <param name="p25">Instance of the Control class.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="debug">Flag indicating whether P25 debug is enabled.</param>
/// <param name="verbose">Flag indicating whether P25 verbose logging is enabled.</param>
DFSIVoicePacket::DFSIVoicePacket(Control* p25, network::BaseNetwork* network, bool debug, bool verbose) :
    VoicePacket(p25, network, debug, verbose),
    m_trunk(NULL),
    m_rfDFSILC(),
    m_netDFSILC(),
    m_dfsiLDU1(NULL),
    m_dfsiLDU2(NULL)
{
    m_dfsiLDU1 = new uint8_t[9U * 25U];
    m_dfsiLDU2 = new uint8_t[9U * 25U];

    ::memset(m_dfsiLDU1, 0x00U, 9U * 25U);
    ::memset(m_dfsiLDU2, 0x00U, 9U * 25U);

    // hmmm...this should hopefully be a safe cast...right?
    m_trunk = (DFSITrunkPacket *)p25->m_trunk;
}

/// <summary>
/// Finalizes a instance of the DFSIVoicePacket class.
/// </summary>
DFSIVoicePacket::~DFSIVoicePacket()
{
    delete[] m_dfsiLDU1;
    delete[] m_dfsiLDU2;
}

/// <summary>
/// Helper to write a network P25 TDU packet.
/// </summary>
void DFSIVoicePacket::writeNet_TDU()
{
    if (m_p25->m_control) {
        m_p25->m_trunk->releaseDstIdGrant(m_netLC.getDstId(), false);
    }

    m_trunk->writeRF_DSFI_Stop(P25_DFSI_TYPE_VOICE);

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

    if (m_network != NULL)
        m_network->resetP25();

    ::memset(m_netLDU1, 0x00U, 9U * 25U);
    ::memset(m_netLDU2, 0x00U, 9U * 25U);

    m_p25->m_netTimeout.stop();
    m_p25->m_networkWatchdog.stop();
    resetNet();
    m_p25->m_netState = RS_NET_IDLE;
    m_p25->m_netLastDstId = 0U;
    m_p25->m_tailOnIdle = true;
}

/// <summary>
/// Helper to write a network P25 LDU1 packet.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
void DFSIVoicePacket::writeNet_LDU1()
{
    lc::LC control = lc::LC(m_dfsiLC.control());
    data::LowSpeedData lsd = data::LowSpeedData(m_dfsiLC.lsd());

    uint32_t dstId = control.getDstId();
    uint32_t srcId = control.getSrcId();
    bool group = control.getLCO() == LC_GROUP;

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

    if (m_p25->m_control) {
        m_p25->m_trunk->touchDstIdGrant(m_rfLC.getDstId());
    }

    // set network and RF link control states
    m_netLC = lc::LC(m_p25->m_siteData);
    m_netLC.setLCO(control.getLCO());
    m_netLC.setMFId(control.getMFId());
    m_netLC.setSrcId(srcId);
    m_netLC.setDstId(dstId);
    m_netLC.setGroup(group);
    m_netLC.setEmergency(control.getEmergency());
    m_netLC.setEncrypted(control.getEncrypted());
    m_netLC.setPriority(control.getPriority());

    m_rfLC = lc::LC(m_p25->m_siteData);
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
        control.getMI(mi);

        if (m_verbose && m_debug) {
            Utils::dump(1U, "Network HDU MI", mi, P25_MI_LENGTH_BYTES);
        }

        m_netLC.setMI(mi);
        m_rfLC.setMI(mi);
        m_netLC.setAlgId(control.getAlgId());
        m_rfLC.setAlgId(control.getAlgId());
        m_netLC.setKId(control.getKId());
        m_rfLC.setKId(control.getKId());

        m_p25->m_trunk->m_rfTSBK = lc::TSBK(&m_rfLC);
        m_p25->m_trunk->m_rfTSBK.setVerbose(m_p25->m_trunk->m_dumpTSBK);
        m_p25->m_trunk->m_netTSBK = lc::TSBK(&m_netLC);
        m_p25->m_trunk->m_netTSBK.setVerbose(m_p25->m_trunk->m_dumpTSBK);

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

        ::ActivityLog("P25", false, "network %svoice transmission from %u to %s%u", m_netLC.getEncrypted() ? "encrypted " : "", srcId, group ? "TG " : "", dstId);

        if (m_p25->m_control) {
            if (group && (m_lastPatchGroup != dstId) &&
                (dstId != m_p25->m_trunk->m_patchSuperGroup)) {
                m_p25->m_trunk->writeRF_TSDU_Mot_Patch(dstId, 0U, 0U);
                m_lastPatchGroup = dstId;
            }
        }

        // single-channel trunking or voice on control support?
        if (m_p25->m_control && m_p25->m_voiceOnControl) {
            m_p25->m_ccRunning = false; // otherwise the grant will be bundled with other packets
            if (!m_p25->m_trunk->writeRF_TSDU_Grant(group, false, true)) {
                if (m_network != NULL)
                    m_network->resetP25();

                ::memset(m_netLDU1, 0x00U, 9U * 25U);
                ::memset(m_netLDU2, 0x00U, 9U * 25U);

                m_p25->m_netTimeout.stop();
                m_p25->m_networkWatchdog.stop();

                m_netLC = lc::LC(m_p25->m_siteData);
                m_netLastLDU1 = lc::LC(m_p25->m_siteData);

                m_p25->m_netState = RS_NET_IDLE;
                m_p25->m_netLastDstId = 0U;

                if (m_p25->m_rfState == RS_RF_REJECTED) {
                    m_p25->m_rfState = RS_RF_LISTENING;
                }

                return;
            }

            m_p25->writeRF_Preamble(0, true);
        }

        m_hadVoice = true;
        m_p25->m_netState = RS_NET_AUDIO;
        m_p25->m_netLastDstId = dstId;
        m_p25->m_netTimeout.start();
        m_netFrames = 0U;
        m_netLost = 0U;
        m_vocLDU1Count = 0U;

        m_netDFSILC.control(m_netLC);
        m_netDFSILC.lsd(lsd);

        m_trunk->writeRF_DFSI_Start(P25_DFSI_TYPE_VOICE);

        uint8_t buffer[P25_DFSI_VHDR1_FRAME_LENGTH_BYTES + 2U];
        ::memset(buffer, 0x00U, P25_DFSI_VHDR1_FRAME_LENGTH_BYTES + 2U);

        // Generate Voice Header 1
        m_netDFSILC.setFrameType(P25_DFSI_VHDR1);
        m_netDFSILC.encodeVHDR1(buffer + 2U);

        buffer[0U] = modem::TAG_DATA;
        buffer[1U] = 0x00U;
        m_p25->writeQueueNet(buffer, P25_DFSI_VHDR1_FRAME_LENGTH_BYTES + 2U);

        // Generate Voice Header 2
        m_netDFSILC.setFrameType(P25_DFSI_VHDR2);
        m_netDFSILC.encodeVHDR2(buffer + 2U);

        buffer[0U] = modem::TAG_DATA;
        buffer[1U] = 0x00U;
        m_p25->writeQueueNet(buffer, P25_DFSI_VHDR2_FRAME_LENGTH_BYTES + 2U);

        if (m_verbose) {
            LogMessage(LOG_NET, P25_HDU_STR " DFSI, dstId = %u, algo = $%02X, kid = $%04X", m_netLC.getDstId(), m_netLC.getAlgId(), m_netLC.getKId());
        }
    }

    // single-channel trunking or voice on control support?
    if (m_p25->m_control && m_p25->m_voiceOnControl) {
        // per TIA-102.AABD-B transmit RFSS_STS_BCAST every 3 superframes (e.g. every 3 LDU1s)
        m_vocLDU1Count++;
        if (m_vocLDU1Count > VOC_LDU1_COUNT) {
            m_vocLDU1Count = 0U;
            m_netLC.setLCO(LC_RFSS_STS_BCAST);
        }
    }

    insertMissingAudio(m_netLDU1);

    uint8_t buffer[P25_DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES + 2U];
    for (uint8_t i = P25_DFSI_LDU1_VOICE1; i <= P25_DFSI_LDU1_VOICE9; i++) {
        uint8_t len = P25_DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;

        // frame 2
        if (i == P25_DFSI_LDU1_VOICE2) {
            len = P25_DFSI_LDU1_VOICE2_FRAME_LENGTH_BYTES;
        }

        // frames 3 - 8 are the same size
        if (i >= P25_DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES && i <= P25_DFSI_LDU1_VOICE8_FRAME_LENGTH_BYTES) {
            len = P25_DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES;
        }

        // frame 9
        if (i == P25_DFSI_LDU1_VOICE9) {
            len = P25_DFSI_LDU1_VOICE9_FRAME_LENGTH_BYTES;
        }

        uint8_t n = 10U;
        switch (i) {
        case P25_DFSI_LDU1_VOICE1:
            n = 10U;
            break;
        case P25_DFSI_LDU1_VOICE2:
            n = 26U;
            break;
        case P25_DFSI_LDU1_VOICE3:
            n = 55U;
            break;
        case P25_DFSI_LDU1_VOICE4:
            n = 80U;
            break;
        case P25_DFSI_LDU1_VOICE5:
            n = 105U;
            break;
        case P25_DFSI_LDU1_VOICE6:
            n = 130U;
            break;
        case P25_DFSI_LDU1_VOICE7:
            n = 155U;
            break;
        case P25_DFSI_LDU1_VOICE8:
            n = 180U;
            break;
        case P25_DFSI_LDU1_VOICE9:
            n = 204U;
            break;
        }

        ::memset(buffer, 0x00U, len + 2U);

        // Generate Voice Frame
        m_netDFSILC.setFrameType(i);
        m_netDFSILC.encodeLDU1(buffer + 2U, m_netLDU1 + n);
        
        buffer[0U] = modem::TAG_DATA;
        buffer[1U] = 0x00U;
        m_p25->writeQueueNet(buffer, len + 2U);
    }

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

        LogMessage(LOG_NET, P25_LDU1_STR " DFSI audio, srcId = %u, dstId = %u, group = %u, emerg = %u, encrypt = %u, prio = %u, %u%% packet loss",
            m_netLC.getSrcId(), m_netLC.getDstId(), m_netLC.getGroup(), m_netLC.getEmergency(), m_netLC.getEncrypted(), m_netLC.getPriority(), loss);
    }

    ::memset(m_netLDU1, 0x00U, 9U * 25U);

    m_netFrames += 9U;
}

/// <summary>
/// Helper to write a network P25 LDU2 packet.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
void DFSIVoicePacket::writeNet_LDU2()
{
    lc::LC control = lc::LC(m_dfsiLC.control());
    data::LowSpeedData lsd = data::LowSpeedData(m_dfsiLC.lsd());

    // don't process network frames if the destination ID's don't match and the network TG hang timer is running
    if (m_p25->m_rfLastDstId != 0U) {
        if (m_p25->m_rfLastDstId != m_netLastLDU1.getDstId() && (m_p25->m_rfTGHang.isRunning() && !m_p25->m_rfTGHang.hasExpired())) {
            resetNet();
            return;
        }
    }

    uint8_t mi[P25_MI_LENGTH_BYTES];
    control.getMI(mi);

    if (m_verbose && m_debug) {
        Utils::dump(1U, "Network LDU2 MI", mi, P25_MI_LENGTH_BYTES);
    }

    m_netLC.setMI(mi);
    m_netLC.setAlgId(control.getAlgId());
    m_netLC.setKId(control.getKId());

    m_netDFSILC.control(m_netLC);

    insertMissingAudio(m_netLDU2);

    uint8_t buffer[P25_DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES + 2U];
    for (uint8_t i = P25_DFSI_LDU2_VOICE10; i <= P25_DFSI_LDU2_VOICE18; i++) {
        uint8_t len = P25_DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;

        // frame 11
        if (i == P25_DFSI_LDU2_VOICE11) {
            len = P25_DFSI_LDU2_VOICE11_FRAME_LENGTH_BYTES;
        }

        // frames 12 - 17 are the same size
        if (i >= P25_DFSI_LDU2_VOICE12_FRAME_LENGTH_BYTES && i <= P25_DFSI_LDU2_VOICE17_FRAME_LENGTH_BYTES) {
            len = P25_DFSI_LDU2_VOICE12_FRAME_LENGTH_BYTES;
        }

        // frame 9
        if (i == P25_DFSI_LDU2_VOICE18) {
            len = P25_DFSI_LDU2_VOICE18_FRAME_LENGTH_BYTES;
        }

        uint8_t n = 10U;
        switch (i) {
        case P25_DFSI_LDU2_VOICE10:
            n = 10U;
            break;
        case P25_DFSI_LDU2_VOICE11:
            n = 26U;
            break;
        case P25_DFSI_LDU2_VOICE12:
            n = 55U;
            break;
        case P25_DFSI_LDU2_VOICE13:
            n = 80U;
            break;
        case P25_DFSI_LDU2_VOICE14:
            n = 105U;
            break;
        case P25_DFSI_LDU2_VOICE15:
            n = 130U;
            break;
        case P25_DFSI_LDU2_VOICE16:
            n = 155U;
            break;
        case P25_DFSI_LDU2_VOICE17:
            n = 180U;
            break;
        case P25_DFSI_LDU2_VOICE18:
            n = 204U;
            break;
        }

        ::memset(buffer, 0x00U, len + 2U);

        // Generate Voice Frame
        m_netDFSILC.setFrameType(i);
        m_netDFSILC.encodeLDU2(buffer + 2U, m_netLDU2 + n);

        buffer[0U] = modem::TAG_DATA;
        buffer[1U] = 0x00U;
        m_p25->writeQueueNet(buffer, len + 2U);
    }

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

    ::memset(m_netLDU2, 0x00U, 9U * 25U);

    m_netFrames += 9U;
}
