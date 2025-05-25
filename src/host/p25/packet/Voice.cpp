// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016,2017,2018 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/p25/P25Defines.h"
#include "common/p25/acl/AccessControl.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/p25/lc/tsbk/OSP_GRP_VCH_GRANT_UPD.h"
#include "common/p25/lc/tsbk/OSP_UU_VCH_GRANT_UPD.h"
#include "common/p25/lc/tdulc/TDULCFactory.h"
#include "common/p25/P25Utils.h"
#include "common/p25/Sync.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "p25/packet/Voice.h"
#include "ActivityLog.h"
#include "HostMain.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::dfsi::defines;
using namespace p25::packet;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t PKT_LDU1_COUNT = 3U;
const uint32_t ROAM_LDU1_COUNT = 1U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Resets the data states for the RF interface. */

void Voice::resetRF()
{
    lc::LC lc = lc::LC();

    m_rfLC = lc;
    //m_rfLastHDU = lc;
    m_rfLastHDUValid = false;
    m_rfLastLDU1 = lc;
    m_rfLastLDU2 = lc;
    m_rfFirstLDU2 = true;

    m_rfFrames = 0U;
    m_rfErrs = 0U;
    m_rfBits = 1U;
    m_rfUndecodableLC = 0U;
    m_pktLDU1Count = 0U;
    m_grpUpdtCount = 0U;
    m_roamLDU1Count = 0U;

    m_inbound = false;
}

/* Resets the data states for the network. */

void Voice::resetNet()
{
    lc::LC lc = lc::LC();

    m_netLC = lc;
    m_netLastLDU1 = lc;
    //m_netLastFrameType = P25_FT_DATA_UNIT;

    m_gotNetLDU1 = false;
    m_gotNetLDU2 = false;

    m_netFrames = 0U;
    m_netLost = 0U;
    m_pktLDU1Count = 0U;
    m_grpUpdtCount = 0U;
    m_roamLDU1Count = 0U;
    m_p25->m_networkWatchdog.stop();

    m_netLastDUID = DUID::TDU;
}

/* Process a data frame from the RF interface. */

bool Voice::process(uint8_t* data, uint32_t len)
{
    assert(data != nullptr);

    // Decode the NID
    bool valid = m_p25->m_nid.decode(data + 2U);
    if (!valid) {
        return false;
    }

    DUID::E duid = m_p25->m_nid.getDUID();

    if (m_p25->m_rfState != RS_RF_LISTENING) {
        m_p25->m_rfTGHang.start();
    }

    if (duid == DUID::HDU && m_lastDUID == DUID::HDU) {
        duid = DUID::LDU1;
    }

    // handle individual DUIDs
    if (duid == DUID::HDU) {
        m_lastDUID = DUID::HDU;

        if (m_p25->m_rfState == RS_RF_LISTENING || m_p25->m_rfState == RS_RF_AUDIO) {
            resetRF();

            m_inbound = true;

            lc::LC lc = lc::LC();
            bool ret = lc.decodeHDU(data + 2U);
            if (!ret) {
                LogWarning(LOG_RF, P25_HDU_STR ", undecodable LC");
                m_rfUndecodableLC++;
                return false;
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_HDU_STR ", HDU_BSDWNACT, dstId = %u, algo = $%02X, kid = $%04X", lc.getDstId(), lc.getAlgId(), lc.getKId());

                if (lc.getAlgId() != ALGO_UNENCRYPT) {
                    uint8_t mi[MI_LENGTH_BYTES];
                    ::memset(mi, 0x00U, MI_LENGTH_BYTES);

                    lc.getMI(mi);

                    LogMessage(LOG_RF, P25_HDU_STR ", Enc Sync, MI = %02X %02X %02X %02X %02X %02X %02X %02X %02X", 
                        mi[0U], mi[1U], mi[2U], mi[3U], mi[4U], mi[5U], mi[6U], mi[7U], mi[8U]);
                }
            }

            // don't process RF frames if this modem isn't authoritative
            if (!m_p25->m_authoritative && m_p25->m_permittedDstId != lc.getDstId()) {
                if (!g_disableNonAuthoritativeLogging)
                    LogWarning(LOG_RF, "[NON-AUTHORITATIVE] Ignoring RF traffic, destination not permitted!");
                resetRF();
                m_p25->m_rfState = RS_RF_LISTENING;
                return false;
            }

            // don't process RF frames if the network isn't in a idle state and the RF destination is the network destination
            if (m_p25->m_netState != RS_NET_IDLE && lc.getDstId() == m_p25->m_netLastDstId) {
                LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing network traffic!");
                resetRF();
                m_p25->m_rfState = RS_RF_LISTENING;
                return false;
            }

            // stop network frames from processing -- RF wants to transmit on a different talkgroup
            if (m_p25->m_netState != RS_NET_IDLE) {
                LogWarning(LOG_RF, "Traffic collision detect, preempting existing network traffic to new RF traffic, rfDstId = %u, netDstId = %u", lc.getDstId(),
                    m_p25->m_netLastDstId);
                if (!m_p25->m_dedicatedControl) {
                    m_p25->m_affiliations->releaseGrant(m_p25->m_netLastDstId, false);
                }

                resetNet();
                if (m_p25->m_network != nullptr)
                    m_p25->m_network->resetP25();

                if (m_p25->m_duplex) {
                    m_p25->writeRF_TDU(true);
                }
            }

            m_p25->writeRF_Preamble();

            m_p25->m_rfTGHang.start();
            m_p25->m_netTGHang.stop();
            m_p25->m_rfLastDstId = lc.getDstId();
            m_p25->m_rfLastSrcId = lc.getSrcId();

            m_rfLastHDU = lc;
            m_rfLastHDUValid = true;

            if (m_p25->m_rfState == RS_RF_LISTENING) {
                if (!m_p25->m_dedicatedControl) {
                    m_p25->m_modem->clearP25Frame();
                }
                m_p25->m_txQueue.clear();
            }
        }

        return true;
    }
    else if (duid == DUID::LDU1) {
        // prevent two xDUs of the same type from being sent consecutively
        if (m_lastDUID == DUID::LDU1) {
            return false;
        }
        m_lastDUID = DUID::LDU1;

        bool alreadyDecoded = false;
        bool hduEncrypt = false;
        FrameType::E frameType = FrameType::DATA_UNIT;
        ulong64_t rsValue = 0U;
        if (m_p25->m_rfState == RS_RF_LISTENING) {
            lc::LC lc = lc::LC();
            bool ret = lc.decodeLDU1(data + 2U);
            if (!ret) {
                m_inbound = false;
                return false;
            }

            m_inbound = true;

            rsValue = lc.getRS();

            uint32_t srcId = lc.getSrcId();
            uint32_t dstId = lc.getDstId();
            if (dstId == 0U && !lc.isStandardMFId() && m_rfLastHDUValid) {
                dstId = m_rfLastHDU.getDstId();
            }

            bool group = lc.getGroup();
            bool encrypted = lc.getEncrypted();

            alreadyDecoded = true;

            // don't process RF frames if this modem isn't authoritative
            if (!m_p25->m_authoritative && m_p25->m_permittedDstId != lc.getDstId()) {
                if (!g_disableNonAuthoritativeLogging)
                    LogWarning(LOG_RF, "[NON-AUTHORITATIVE] Ignoring RF traffic, destination not permitted!");
                resetRF();
                m_p25->m_rfState = RS_RF_LISTENING;
                return false;
            }

            // don't process RF frames if the network isn't in a idle state and the RF destination is the network destination
            if (m_p25->m_netState != RS_NET_IDLE && dstId == m_p25->m_netLastDstId) {
                LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing network traffic!");
                resetRF();
                m_p25->m_rfState = RS_RF_LISTENING;
                return false;
            }

            // stop network frames from processing -- RF wants to transmit on a different talkgroup
            if (m_p25->m_netState != RS_NET_IDLE) {
                if (m_netLC.getSrcId() == srcId && m_p25->m_netLastDstId == dstId) {
                    LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing network traffic (Are we in a voting condition?), rfSrcId = %u, rfDstId = %u, netSrcId = %u, netDstId = %u", srcId, dstId,
                        m_netLC.getSrcId(), m_p25->m_netLastDstId);
                    resetRF();
                    m_p25->m_rfState = RS_RF_LISTENING;
                    return false;
                }
                else {
                    LogWarning(LOG_RF, "Traffic collision detect, preempting existing network traffic to new RF traffic, rfDstId = %u, netDstId = %u", dstId,
                        m_p25->m_netLastDstId);
                    if (!m_p25->m_dedicatedControl) {
                        m_p25->m_affiliations->releaseGrant(m_p25->m_netLastDstId, false);
                    }

                    resetNet();
                    if (m_p25->m_network != nullptr)
                        m_p25->m_network->resetP25();

                    if (m_p25->m_duplex) {
                        m_p25->writeRF_TDU(true);
                    }

                    m_p25->m_netTGHang.stop();
                }

                // is control is enabled, and the group was granted by network already ignore RF traffic
                if (m_p25->m_enableControl && dstId == m_p25->m_netLastDstId) {
                    if (m_p25->m_affiliations->isNetGranted(dstId)) {
                        LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing granted network traffic (Are we in a voting condition?), rfSrcId = %u, rfDstId = %u, netSrcId = %u, netDstId = %u", srcId, dstId,
                            m_netLC.getSrcId(), m_p25->m_netLastDstId);
                        resetRF();
                        m_p25->m_rfState = RS_RF_LISTENING;
                        return false;
                    }
                }
            }

            // if this is a late entry call, clear states
            if (m_rfLastHDU.getDstId() == 0U) {
                if (!m_p25->m_dedicatedControl) {
                    m_p25->m_modem->clearP25Frame();
                }
                m_p25->m_txQueue.clear();

                resetRF();
            }

            if (m_p25->m_enableControl) {
                if (!m_p25->m_ccRunning && !m_p25->m_dedicatedControl) {
                    m_p25->m_control->writeRF_ControlData(255U, 0U, false);
                }
            }

            // validate the source RID
            if (!acl::AccessControl::validateSrcId(srcId)) {
                if (m_lastRejectId == 0U || m_lastRejectId != srcId) {
                    LogWarning(LOG_RF, P25_HDU_STR " denial, RID rejection, srcId = %u", srcId);
                    if (m_p25->m_enableControl) {
                        m_p25->m_control->writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_REQ_UNIT_NOT_VALID, (group ? TSBKO::IOSP_GRP_VCH : TSBKO::IOSP_UU_VCH), group, true);
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
                            m_p25->m_control->writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_TGT_UNIT_NOT_VALID, TSBKO::IOSP_UU_VCH, false, true);
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
                            m_p25->m_control->writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_TGT_GROUP_NOT_VALID, TSBKO::IOSP_GRP_VCH, true, true);
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
                if (!m_p25->m_affiliations->isGroupAff(srcId, dstId) && m_p25->m_control->m_verifyAff) {
                    if (m_lastRejectId == 0 || m_lastRejectId != srcId) {
                        LogWarning(LOG_RF, P25_HDU_STR " denial, RID not affiliated to TGID, srcId = %u, dstId = %u", srcId, dstId);
                        m_p25->m_control->writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_REQ_UNIT_NOT_AUTH, TSBKO::IOSP_GRP_VCH, true, true);
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

            // bryanb: due to moronic reasons -- if this case happens, default the RID to something sane
            if (srcId == 0U && !lc.isStandardMFId()) {
                LogMessage(LOG_RF, P25_HDU_STR " ** source RID was 0 with non-standard MFId defaulting source RID, dstId = %u, mfId = $%02X", dstId, lc.getMFId());
                srcId = WUID_FNE;
            }

            // send network grant demand TDU
            if (m_p25->m_network != nullptr) {
                if (!m_p25->m_dedicatedControl && m_p25->m_convNetGrantDemand) {
                    uint8_t controlByte = 0x80U;                                            // Grant Demand Flag
                    if (encrypted)
                        controlByte |= 0x08U;                                               // Grant Encrypt Flag
                    if (!group)
                        controlByte |= 0x01U;                                               // Unit-to-unit Flag

                    LogMessage(LOG_RF, P25_HDU_STR " remote grant demand, srcId = %u, dstId = %u", srcId, dstId);
                    m_p25->m_network->writeP25TDU(lc, m_rfLSD, controlByte);
                }
            }

            m_rfLC = lc;
            m_rfLastLDU1 = m_rfLC;
            hduEncrypt = encrypted;

            m_lastRejectId = 0U;
            ::ActivityLog("P25", true, "RF %svoice transmission from %u to %s%u", encrypted ? "encrypted ": "", srcId, group ? "TG " : "", dstId);

            uint8_t serviceOptions = (m_rfLC.getEmergency() ? 0x80U : 0x00U) +       // Emergency Flag
                (m_rfLC.getEncrypted() ? 0x40U : 0x00U) +                            // Encrypted Flag
                (m_rfLC.getPriority() & 0x07U);                                      // Priority

            if (m_p25->m_enableControl) {
                // if the group wasn't granted out -- explicitly grant the group
                if (!m_p25->m_affiliations->isGranted(dstId)) {
                    if (m_p25->m_legacyGroupGrnt) {
                        // are we auto-registering legacy radios to groups?
                        if (m_p25->m_legacyGroupReg && group) {
                            if (!m_p25->m_affiliations->isGroupAff(srcId, dstId)) {
                                if (m_p25->m_control->writeRF_TSDU_Grp_Aff_Rsp(srcId, dstId) != ResponseCode::ACCEPT) {
                                    LogWarning(LOG_RF, P25_HDU_STR " denial, conventional affiliation required, not affiliated to TGID, srcId = %u, dstId = %u", srcId, dstId);
                                    m_p25->m_rfLastDstId = 0U;
                                    m_p25->m_rfLastSrcId = 0U;
                                    m_p25->m_rfTGHang.stop();
                                    m_p25->m_rfState = RS_RF_REJECTED;
                                    return false;
                                }
                            }
                        }

                        if (!m_p25->m_control->writeRF_TSDU_Grant(srcId, dstId, serviceOptions, group)) {
                            return false;
                        }
                    }
                    else {
                        LogWarning(LOG_RF, P25_HDU_STR " denial, conventional affiliation required, and legacy group grant disabled, not affiliated to TGID, srcId = %u, dstId = %u", srcId, dstId);
                        m_p25->m_rfLastDstId = 0U;
                        m_p25->m_rfLastSrcId = 0U;
                        m_p25->m_rfTGHang.stop();
                        m_p25->m_rfState = RS_RF_REJECTED;
                        return false;
                    }
                }
            }

            // conventional registration or DVRS support?
            if ((m_p25->m_enableControl && !m_p25->m_dedicatedControl) || m_p25->m_voiceOnControl) {
                if (!m_p25->m_affiliations->isGranted(dstId)) {
                    m_p25->m_control->writeRF_TSDU_Grant(srcId, dstId, serviceOptions, group, false, true);
                }

                // if voice on control; insert grant updates before voice traffic
                if (m_p25->m_voiceOnControl) {
                    uint32_t chNo = m_p25->m_affiliations->getGrantedCh(dstId);
                    ::lookups::VoiceChData voiceChData = m_p25->m_affiliations->rfCh()->getRFChData(chNo);
                    bool grp = m_p25->m_affiliations->isGroup(dstId);

                    std::unique_ptr<lc::TSBK> osp;

                    if (grp) {
                        osp = std::make_unique<lc::tsbk::OSP_GRP_VCH_GRANT_UPD>();

                        // transmit group voice grant update
                        osp->setLCO(TSBKO::OSP_GRP_VCH_GRANT_UPD);
                        osp->setDstId(dstId);
                        osp->setGrpVchId(voiceChData.chId());
                        osp->setGrpVchNo(chNo);
                    }
                    else {
                        uint32_t srcId = m_p25->m_affiliations->getGrantedSrcId(dstId);

                        osp = std::make_unique<lc::tsbk::OSP_UU_VCH_GRANT_UPD>();

                        // transmit group voice grant update
                        osp->setLCO(TSBKO::OSP_UU_VCH_GRANT_UPD);
                        osp->setSrcId(srcId);
                        osp->setDstId(dstId);
                        osp->setGrpVchId(voiceChData.chId());
                        osp->setGrpVchNo(chNo);
                    }

                    if (!m_p25->m_ccHalted) {
                        m_p25->m_txQueue.clear();
                        m_p25->m_ccHalted = true;
                    }

                    for (int i = 0; i < 3; i++)
                        m_p25->m_control->writeRF_TSDU_SBF(osp.get(), true);
                }
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

                uint8_t mi[MI_LENGTH_BYTES];
                m_rfLastHDU.getMI(mi);
                m_rfLC.setMI(mi);

                uint8_t buffer[P25_HDU_FRAME_LENGTH_BYTES + 2U];
                ::memset(buffer, 0x00U, P25_HDU_FRAME_LENGTH_BYTES + 2U);

                // generate Sync
                Sync::addP25Sync(buffer + 2U);

                // generate NID
                m_p25->m_nid.encode(buffer + 2U, DUID::HDU);

                // generate HDU
                m_rfLC.encodeHDU(buffer + 2U);

                // add status bits
                P25Utils::addStatusBits(buffer + 2U, P25_HDU_FRAME_LENGTH_BITS, m_inbound, false);

                writeNetwork(buffer, DUID::HDU);

                if (m_p25->m_duplex && !m_p25->m_isModemDFSI) {
                    buffer[0U] = modem::TAG_DATA;
                    buffer[1U] = 0x00U;

                    m_p25->addFrame(buffer, P25_HDU_FRAME_LENGTH_BYTES + 2U);
                }

                frameType = FrameType::HDU_VALID;

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_HDU_STR ", dstId = %u, algo = $%02X, kid = $%04X", m_rfLC.getDstId(), m_rfLC.getAlgId(), m_rfLC.getKId());
                }
            }
            else {
                frameType = FrameType::HDU_LATE_ENTRY;
                LogWarning(LOG_RF, P25_HDU_STR ", not transmitted; possible late entry, dstId = %u, algo = $%02X, kid = $%04X", m_rfLastHDU.getDstId(), m_rfLastHDU.getAlgId(), m_rfLastHDU.getKId());

                m_p25->writeRF_Preamble();
            }

            // if voice on control; insert group voice channel updates directly after HDU but before LDUs
            if (m_p25->m_voiceOnControl) {
                uint32_t chNo = m_p25->m_affiliations->getGrantedCh(dstId);
                ::lookups::VoiceChData voiceChData = m_p25->m_affiliations->rfCh()->getRFChData(chNo);
                bool grp = m_p25->m_affiliations->isGroup(dstId);

                std::unique_ptr<lc::TSBK> osp;

                if (grp) {
                    osp = std::make_unique<lc::tsbk::OSP_GRP_VCH_GRANT_UPD>();

                    // transmit group voice grant update
                    osp->setLCO(TSBKO::OSP_GRP_VCH_GRANT_UPD);
                    osp->setDstId(dstId);
                    osp->setGrpVchId(voiceChData.chId());
                    osp->setGrpVchNo(chNo);
                }
                else {
                    uint32_t srcId = m_p25->m_affiliations->getGrantedSrcId(dstId);

                    osp = std::make_unique<lc::tsbk::OSP_UU_VCH_GRANT_UPD>();

                    // transmit group voice grant update
                    osp->setLCO(TSBKO::OSP_UU_VCH_GRANT_UPD);
                    osp->setSrcId(srcId);
                    osp->setDstId(dstId);
                    osp->setGrpVchId(voiceChData.chId());
                    osp->setGrpVchNo(chNo);
                }

                if (!m_p25->m_ccHalted) {
                    m_p25->m_txQueue.clear();
                    m_p25->m_ccHalted = true;
                }

                for (int i = 0; i < 3; i++)
                    m_p25->m_control->writeRF_TSDU_SBF(osp.get(), true);
            }

            m_rfFrames = 0U;
            m_rfErrs = 0U;
            m_rfBits = 1U;
            m_rfUndecodableLC = 0U;
            m_pktLDU1Count = 0U;
            m_grpUpdtCount = 0U;
            m_roamLDU1Count = 0U;
            m_p25->m_rfTimeout.start();
            m_lastDUID = DUID::HDU;

            m_rfLastHDU = lc::LC();
        }

        if (m_p25->m_rfState == RS_RF_AUDIO) {
            // don't process RF frames if this modem isn't authoritative
            if (!m_p25->m_authoritative && m_p25->m_permittedDstId != m_rfLC.getDstId()) {
                if (!g_disableNonAuthoritativeLogging)
                    LogWarning(LOG_RF, "[NON-AUTHORITATIVE] Ignoring RF traffic, destination not permitted!");
                resetRF();
                m_p25->m_rfState = RS_RF_LISTENING;
                return false;
            }

            // stop network frames from processing -- RF wants to transmit on a different talkgroup
            if (m_p25->m_netState != RS_NET_IDLE) {
                if (m_netLC.getSrcId() == m_rfLC.getSrcId() && m_p25->m_netLastDstId == m_rfLC.getDstId()) {
                    LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing network traffic (Are we in a voting condition?), rfSrcId = %u, rfDstId = %u, netSrcId = %u, netDstId = %u", m_rfLC.getSrcId(), m_rfLC.getDstId(),
                        m_netLC.getSrcId(), m_p25->m_netLastDstId);
                    resetRF();
                    m_p25->m_rfState = RS_RF_LISTENING;
                    return false;
                }

                // is control is enabled, and the group was granted by network already ignore RF traffic
                if (m_p25->m_enableControl && m_rfLC.getDstId() == m_p25->m_netLastDstId) {
                    if (m_p25->m_affiliations->isNetGranted(m_rfLC.getDstId())) {
                        LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing granted network traffic (Are we in a voting condition?), rfSrcId = %u, rfDstId = %u, netSrcId = %u, netDstId = %u", m_rfLC.getSrcId(), m_rfLC.getDstId(),
                            m_netLC.getSrcId(), m_p25->m_netLastDstId);
                        resetRF();
                        m_p25->m_rfState = RS_RF_LISTENING;
                        return false;
                    }
                }
            }

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
            else {
                // this might be the first LDU1 -- set the encryption flag if necessary
                m_rfLC.setEncrypted(hduEncrypt);
            }

            m_inbound = true;

            rsValue = m_rfLC.getRS();

            alreadyDecoded = false;

            if (m_p25->m_enableControl) {
                m_p25->m_affiliations->touchGrant(m_rfLC.getDstId());
            }

            if (m_p25->m_notifyCC) {
                m_p25->notifyCC_TouchGrant(m_rfLC.getDstId());
            }

            // are we swapping the LC out for the RFSS_STS_BCAST or LC_GROUP_UPDT?
            m_pktLDU1Count++;
            if (m_pktLDU1Count > PKT_LDU1_COUNT) {
                m_pktLDU1Count = 0U;

                // conventional registration or DVRS support?
                if ((m_p25->m_enableControl && !m_p25->m_dedicatedControl) || m_p25->m_voiceOnControl) {
                    // per TIA-102.AABD-B transmit RFSS_STS_BCAST every 3 superframes (e.g. every 3 LDU1s)
                    m_rfLC.setMFId(MFG_STANDARD);
                    m_rfLC.setLCO(LCO::RFSS_STS_BCAST);
                }
                else {
                    std::lock_guard<std::mutex> lock(m_p25->m_activeTGLock);
                    if (m_p25->m_activeTG.size() > 0) {
                        if (m_grpUpdtCount > m_p25->m_activeTG.size())
                            m_grpUpdtCount = 0U;

                        if (m_p25->m_activeTG.size() < 2) {
                            uint32_t dstId = m_p25->m_activeTG.at(0);
                            m_rfLC.setMFId(MFG_STANDARD);
                            m_rfLC.setLCO(LCO::GROUP_UPDT);
                            m_rfLC.setDstId(dstId);
                        }
                        else {
                            uint32_t dstId = m_p25->m_activeTG.at(m_grpUpdtCount);
                            uint32_t dstIdB = m_p25->m_activeTG.at(m_grpUpdtCount + 1U);
                            m_rfLC.setMFId(MFG_STANDARD);
                            m_rfLC.setLCO(LCO::GROUP_UPDT);
                            m_rfLC.setDstId(dstId);
                            m_rfLC.setDstIdB(dstIdB);

                            m_grpUpdtCount++;
                        }
                    }
                }
            }

            // generate Sync
            Sync::addP25Sync(data + 2U);

            // generate NID
            m_p25->m_nid.encode(data + 2U, DUID::LDU1);

            // generate LDU1 Data
            if (!m_rfLC.isStandardMFId()) {
                if (m_debug) {
                    LogDebug(LOG_RF, "P25, LDU1 LC, non-standard payload, lco = $%02X, mfId = $%02X", m_rfLC.getLCO(), m_rfLC.getMFId());
                }
                m_rfLC.setRS(rsValue);
            }

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

            // add status bits
            P25Utils::addStatusBits(data + 2U, P25_LDU_FRAME_LENGTH_BITS, m_inbound, false);

            writeNetwork(data + 2U, DUID::LDU1, frameType);

            if (m_p25->m_duplex && !m_p25->m_isModemDFSI) {
                data[0U] = modem::TAG_DATA;
                data[1U] = 0x00U;

                m_p25->addFrame(data, P25_LDU_FRAME_LENGTH_BYTES + 2U);
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_LDU1_STR ", audio, mfId = $%02X srcId = %u, dstId = %u, group = %u, emerg = %u, encrypt = %u, prio = %u, errs = %u/1233 (%.1f%%)",
                    m_rfLC.getMFId(), m_rfLC.getSrcId(), m_rfLC.getDstId(), m_rfLC.getGroup(), m_rfLC.getEmergency(), m_rfLC.getEncrypted(), m_rfLC.getPriority(), errors, float(errors) / 12.33F);
            }

            return true;
        }
    }
    else if (duid == DUID::LDU2) {
        // prevent two xDUs of the same type from being sent consecutively
        if (m_lastDUID == DUID::LDU2) {
            return false;
        }
        m_lastDUID = DUID::LDU2;

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
                uint8_t lastMI[MI_LENGTH_BYTES];
                ::memset(lastMI, 0x00U, MI_LENGTH_BYTES);

                uint8_t nextMI[MI_LENGTH_BYTES];
                ::memset(nextMI, 0x00U, MI_LENGTH_BYTES);

                if (m_rfFirstLDU2) {
                    m_rfFirstLDU2 = false;
                    if (m_rfLastHDUValid) {
                        m_rfLastHDU.getMI(lastMI);
                    }
                }
                else {
                    m_rfLastLDU2.getMI(lastMI);
                }

                getNextMI(lastMI, nextMI);
                if (m_verbose && m_debug) {
                    Utils::dump(1U, "Previous P25 MI", lastMI, MI_LENGTH_BYTES);
                    Utils::dump(1U, "Calculated next P25 MI", nextMI, MI_LENGTH_BYTES);
                }

                m_rfLC.setMI(nextMI);
                m_rfLastLDU2.setMI(nextMI);
            }
            else {
                m_rfLastLDU2 = m_rfLC;
                m_rfFirstLDU2 = false;
            }

            m_inbound = true;

            // generate Sync
            Sync::addP25Sync(data + 2U);

            // generate NID
            m_p25->m_nid.encode(data + 2U, DUID::LDU2);

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

            // add status bits
            P25Utils::addStatusBits(data + 2U, P25_LDU_FRAME_LENGTH_BITS, m_inbound, false);

            writeNetwork(data + 2U, DUID::LDU2);

            if (m_p25->m_duplex && !m_p25->m_isModemDFSI) {
                data[0U] = modem::TAG_DATA;
                data[1U] = 0x00U;

                m_p25->addFrame(data, P25_LDU_FRAME_LENGTH_BYTES + 2U);
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_LDU2_STR ", audio, algo = $%02X, kid = $%04X, errs = %u/1233 (%.1f%%)",
                    m_rfLC.getAlgId(), m_rfLC.getKId(), errors, float(errors) / 12.33F);

                if (m_rfLC.getAlgId() != ALGO_UNENCRYPT) {
                    uint8_t mi[MI_LENGTH_BYTES];
                    ::memset(mi, 0x00U, MI_LENGTH_BYTES);

                    m_rfLC.getMI(mi);

                    LogMessage(LOG_RF, P25_LDU2_STR ", Enc Sync, MI = %02X %02X %02X %02X %02X %02X %02X %02X %02X", 
                        mi[0U], mi[1U], mi[2U], mi[3U], mi[4U], mi[5U], mi[6U], mi[7U], mi[8U]);
                }
            }

            return true;
        }
    }
    else if (duid == DUID::VSELP1) {
        // prevent two xDUs of the same type from being sent consecutively
        if (m_lastDUID == DUID::VSELP1) {
            return false;
        }
        m_lastDUID = DUID::VSELP1;

        // VSELP has no decoding -- its just passed transparently

        if (m_p25->m_rfState == RS_RF_LISTENING) {
            // stop network frames from processing -- RF wants to transmit on a different talkgroup
            if (m_p25->m_netState != RS_NET_IDLE) {
                LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing network traffic");
                resetRF();
                m_p25->m_rfState = RS_RF_LISTENING;
                return false;
            }

            // if this is a late entry call, clear states
            if (m_rfLastHDU.getDstId() == 0U) {
                if (!m_p25->m_dedicatedControl) {
                    m_p25->m_modem->clearP25Frame();
                }
                m_p25->m_txQueue.clear();

                resetRF();
            }

            m_inbound = true;

            m_lastRejectId = 0U;
            ::ActivityLog("P25", true, "RF VSELP voice transmission");

            m_hadVoice = true;

            m_p25->m_rfState = RS_RF_AUDIO;

            // make sure we actually got a HDU -- otherwise treat the call as a late entry
            if (m_rfLastHDU.getDstId() != 0U) {
                m_p25->m_rfTGHang.start();
                m_p25->m_netTGHang.stop();
                m_p25->m_rfLastDstId = m_rfLastHDU.getDstId();

                m_rfLC = lc::LC();

                // copy destination and encryption parameters from the last HDU received (if possible)
                if (m_rfLC.getDstId() != m_rfLastHDU.getDstId()) {
                    m_rfLC.setDstId(m_rfLastHDU.getDstId());
                }

                m_rfLC.setAlgId(m_rfLastHDU.getAlgId());
                m_rfLC.setKId(m_rfLastHDU.getKId());

                uint8_t mi[MI_LENGTH_BYTES];
                m_rfLastHDU.getMI(mi);
                m_rfLC.setMI(mi);

                uint8_t buffer[P25_HDU_FRAME_LENGTH_BYTES + 2U];
                ::memset(buffer, 0x00U, P25_HDU_FRAME_LENGTH_BYTES + 2U);

                // generate Sync
                Sync::addP25Sync(buffer + 2U);

                // generate NID
                m_p25->m_nid.encode(buffer + 2U, DUID::HDU);

                // generate HDU
                m_rfLC.encodeHDU(buffer + 2U);

                // add status bits
                P25Utils::addStatusBits(buffer + 2U, P25_HDU_FRAME_LENGTH_BITS, m_inbound, false);

                writeNetwork(buffer, DUID::HDU);

                if (m_p25->m_duplex) {
                    buffer[0U] = modem::TAG_DATA;
                    buffer[1U] = 0x00U;

                    m_p25->addFrame(buffer, P25_HDU_FRAME_LENGTH_BYTES + 2U);
                }

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_HDU_STR ", dstId = %u, algo = $%02X, kid = $%04X", m_rfLC.getDstId(), m_rfLC.getAlgId(), m_rfLC.getKId());
                }
            }
            else {
                LogWarning(LOG_RF, P25_HDU_STR ", not transmitted; possible late entry, dstId = %u, algo = $%02X, kid = $%04X", m_rfLastHDU.getDstId(), m_rfLastHDU.getAlgId(), m_rfLastHDU.getKId());
            }

            m_rfFrames = 0U;
            m_rfErrs = 0U;
            m_rfBits = 1U;
            m_rfUndecodableLC = 0U;
            m_pktLDU1Count = 0U;
            m_grpUpdtCount = 0U;
            m_roamLDU1Count = 0U;
            m_p25->m_rfTimeout.start();
            m_lastDUID = DUID::HDU;

            m_rfLastHDU = lc::LC();
        }

        if (m_p25->m_rfState == RS_RF_AUDIO) {
            m_rfFrames++;

            m_inbound = true;

            // generate Sync
            Sync::addP25Sync(data + 2U);

            // generate NID
            m_p25->m_nid.encode(data + 2U, DUID::VSELP1);

            // add status bits
            P25Utils::addStatusBits(data + 2U, P25_LDU_FRAME_LENGTH_BITS, m_inbound, false);

            writeNetwork(data + 2U, DUID::VSELP1);

            if (m_p25->m_duplex) {
                data[0U] = modem::TAG_DATA;
                data[1U] = 0x00U;

                m_p25->addFrame(data, P25_LDU_FRAME_LENGTH_BYTES + 2U);
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_VSELP1_STR ", audio");
            }

            return true;
        }
    }
    else if (duid == DUID::VSELP2) {
        // prevent two xDUs of the same type from being sent consecutively
        if (m_lastDUID == DUID::VSELP2) {
            return false;
        }
        m_lastDUID = DUID::VSELP2;

        // VSELP has no decoding -- its just passed transparently

        if (m_p25->m_rfState == RS_RF_LISTENING) {
            return false;
        }
        else if (m_p25->m_rfState == RS_RF_AUDIO) {
            m_rfFrames++;

            m_inbound = true;

            // generate Sync
            Sync::addP25Sync(data + 2U);

            // generate NID
            m_p25->m_nid.encode(data + 2U, DUID::VSELP2);

            // add status bits
            P25Utils::addStatusBits(data + 2U, P25_LDU_FRAME_LENGTH_BITS, m_inbound, false);

            writeNetwork(data + 2U, DUID::VSELP2);

            if (m_p25->m_duplex) {
                data[0U] = modem::TAG_DATA;
                data[1U] = 0x00U;

                m_p25->addFrame(data, P25_LDU_FRAME_LENGTH_BYTES + 2U);
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_VSELP2_STR ", audio");
            }

            return true;
        }
    }
    else if (duid == DUID::TDU || duid == DUID::TDULC) {
        if (!m_p25->m_enableControl) {
            m_p25->m_affiliations->releaseGrant(m_rfLC.getDstId(), false);
        }

        if (m_p25->m_notifyCC) {
            m_p25->notifyCC_ReleaseGrant(m_rfLC.getDstId());
        }

        if (duid == DUID::TDU) {
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

        // if voice on control; and CC is halted restart CC
        if (m_p25->m_voiceOnControl && m_p25->m_ccHalted) {
            m_p25->m_ccHalted = false;
            m_p25->writeRF_ControlData();
        }

        m_inbound = false;
        m_p25->m_rfState = RS_RF_LISTENING;
        return true;
    }
    else {
        LogError(LOG_RF, "P25 unhandled voice DUID, duid = $%02X", duid);
    }

    return false;
}

/* Process a data frame from the network. */

bool Voice::processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, defines::DUID::E& duid, defines::FrameType::E& frameType)
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

    // bryanb: possible fix for a "tail ride" condition where network traffic immediately follows RF traffic *while*
    // the RF TG hangtimer is running
    if (m_p25->m_rfTGHang.isRunning() && !m_p25->m_rfTGHang.hasExpired()) {
        m_p25->m_rfTGHang.stop();
    }

    // perform authoritative network TG hangtimer and traffic preemption
    if (m_p25->m_authoritative) {
        // don't process network frames if the destination ID's don't match and the network TG hang timer is running
        if (m_p25->m_netLastDstId != 0U && dstId != 0U && (duid == DUID::LDU1 || duid == DUID::LDU2)) {
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
                LogWarning(LOG_NET, "Traffic collision detect, preempting new network traffic to existing RF traffic (Are we in a voting condition?), rfSrcId = %u, rfDstId = %u, netSrcId = %u, netDstId = %u", m_rfLC.getSrcId(), m_rfLC.getDstId(),
                    srcId, dstId);
                resetNet();
                if (m_p25->m_network != nullptr)
                    m_p25->m_network->resetP25();
                return false;
            }
            else {
                LogWarning(LOG_NET, "Traffic collision detect, preempting new network traffic to existing RF traffic, rfDstId = %u, netDstId = %u", m_rfLC.getDstId(),
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
        if (!g_disableNonAuthoritativeLogging)
            LogWarning(LOG_NET, "[NON-AUTHORITATIVE] Ignoring network traffic, destination not permitted, dstId = %u", dstId);
        resetNet();
        if (m_p25->m_network != nullptr)
            m_p25->m_network->resetP25();
        return false;
    }

    uint32_t count = 0U;
    switch (duid) {
        case DUID::LDU1:
            if ((data[0U] == DFSIFrameType::LDU1_VOICE1) && (data[22U] == DFSIFrameType::LDU1_VOICE2) &&
                (data[36U] == DFSIFrameType::LDU1_VOICE3) && (data[53U] == DFSIFrameType::LDU1_VOICE4) &&
                (data[70U] == DFSIFrameType::LDU1_VOICE5) && (data[87U] == DFSIFrameType::LDU1_VOICE6) &&
                (data[104U] == DFSIFrameType::LDU1_VOICE7) && (data[121U] == DFSIFrameType::LDU1_VOICE8) &&
                (data[138U] == DFSIFrameType::LDU1_VOICE9)) {

                m_dfsiLC = dfsi::LC(control, lsd);

                m_dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE1);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 10U);
                count += DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE2);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 26U);
                count += DFSI_LDU1_VOICE2_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE3);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 55U);
                count += DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE4);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 80U);
                count += DFSI_LDU1_VOICE4_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE5);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 105U);
                count += DFSI_LDU1_VOICE5_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE6);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 130U);
                count += DFSI_LDU1_VOICE6_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE7);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 155U);
                count += DFSI_LDU1_VOICE7_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE8);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 180U);
                count += DFSI_LDU1_VOICE8_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE9);
                m_dfsiLC.decodeLDU1(data + count, m_netLDU1 + 204U);
                count += DFSI_LDU1_VOICE9_FRAME_LENGTH_BYTES;

                m_gotNetLDU1 = true;

                // these aren't set by the DFSI decoder, so we'll manually
                // reset them
                m_dfsiLC.control()->setNetId(control.getNetId());
                m_dfsiLC.control()->setSysId(control.getSysId());

                // overwrite the destination ID if the network message header and
                // decoded DFSI data don't agree (this can happen if the network is dynamically
                // altering the destination ID in-flight)
                if (m_dfsiLC.control()->getDstId() != control.getDstId()) {
                    m_dfsiLC.control()->setDstId(control.getDstId());
                }

                m_netLastLDU1 = control;
                m_netLastFrameType = frameType;

                // save MI to member variable before writing to RF
                control.getMI(m_lastMI);

                if (m_p25->m_enableControl) {
                    lc::LC control = lc::LC(*m_dfsiLC.control());
                    m_p25->m_affiliations->touchGrant(control.getDstId());
                }

                if (m_p25->m_notifyCC) {
                    m_p25->notifyCC_TouchGrant(control.getDstId());
                }

                if (m_p25->m_dedicatedControl && !m_p25->m_voiceOnControl) {
                    return true;
                }

                // see if we've somehow missed the previous LDU2, and if we have insert null audio
                if (m_netLastDUID == DUID::LDU1) {
                    LogWarning(LOG_NET, P25_LDU2_STR " audio, missed LDU2 for superframe, filling in lost audio");
                    resetWithNullAudio(m_netLDU2, m_netLC.getAlgId() != P25DEF::ALGO_UNENCRYPT);
                    writeNet_LDU2();
                } else {
                    checkNet_LDU2();
                }

                if (m_p25->m_netState != RS_NET_IDLE) {
                    m_p25->m_netTGHang.start();
                    writeNet_LDU1();
                }

                m_netLastDUID = duid;
            }
            break;
        case DUID::LDU2:
            if ((data[0U] == DFSIFrameType::LDU2_VOICE10) && (data[22U] == DFSIFrameType::LDU2_VOICE11) &&
                (data[36U] == DFSIFrameType::LDU2_VOICE12) && (data[53U] == DFSIFrameType::LDU2_VOICE13) &&
                (data[70U] == DFSIFrameType::LDU2_VOICE14) && (data[87U] == DFSIFrameType::LDU2_VOICE15) &&
                (data[104U] == DFSIFrameType::LDU2_VOICE16) && (data[121U] == DFSIFrameType::LDU2_VOICE17) &&
                (data[138U] == DFSIFrameType::LDU2_VOICE18)) {
                m_dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE10);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 10U);
                count += DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE11);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 26U);
                count += DFSI_LDU2_VOICE11_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE12);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 55U);
                count += DFSI_LDU2_VOICE12_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE13);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 80U);
                count += DFSI_LDU2_VOICE13_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE14);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 105U);
                count += DFSI_LDU2_VOICE14_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE15);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 130U);
                count += DFSI_LDU2_VOICE15_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE16);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 155U);
                count += DFSI_LDU2_VOICE16_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE17);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 180U);
                count += DFSI_LDU2_VOICE17_FRAME_LENGTH_BYTES;

                m_dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE18);
                m_dfsiLC.decodeLDU2(data + count, m_netLDU2 + 204U);
                count += DFSI_LDU2_VOICE18_FRAME_LENGTH_BYTES;

                m_gotNetLDU2 = true;

                if (m_p25->m_enableControl) {
                    lc::LC control = lc::LC(*m_dfsiLC.control());
                    m_p25->m_affiliations->touchGrant(control.getDstId());
                }

                if (m_p25->m_notifyCC) {
                    m_p25->notifyCC_TouchGrant(control.getDstId());
                }

                if (m_p25->m_dedicatedControl && !m_p25->m_voiceOnControl) {
                    return true;
                }

                if (m_p25->m_netState == RS_NET_IDLE) {
                    m_p25->m_modem->clearP25Frame();
                    m_p25->m_txQueue.clear();

                    resetRF();
                    resetNet();

                    writeNet_LDU1();
                }
                else {
                    // see if we've somehow missed the previous LDU1, and if we have insert null audio
                    if (m_netLastDUID == DUID::LDU2) {
                        LogWarning(LOG_NET, P25_LDU1_STR " audio, missed LDU1 for superframe, filling in lost audio");
                        resetWithNullAudio(m_netLDU1, m_netLC.getAlgId() != P25DEF::ALGO_UNENCRYPT);
                        writeNet_LDU1();
                    } else {
                        checkNet_LDU1();
                    }
                }

                if (m_p25->m_netState != RS_NET_IDLE) {
                    m_p25->m_netTGHang.start();
                    writeNet_LDU2();
                }

                m_netLastDUID = duid;
            }
            break;
        case DUID::VSELP1:
        case DUID::VSELP2:
            // currently ignored -- this is a TODO
            break;
        case DUID::TDU:
        case DUID::TDULC:
            if (duid == DUID::TDULC) {
                std::unique_ptr<lc::TDULC> tdulc = lc::tdulc::TDULCFactory::createTDULC(data);
                if (tdulc == nullptr) {
                    LogWarning(LOG_NET, P25_TDULC_STR ", undecodable TDULC");
                }
                else {
                    if (tdulc->getLCO() != LCO::CALL_TERM)
                        break;
                }
            }

            // ignore a TDU that doesn't contain our destination ID
            if (control.getDstId() != m_p25->m_netLastDstId) {
                return false;
            }

            // don't process network frames if the RF modem isn't in a listening state
            if (m_p25->m_rfState != RS_RF_LISTENING) {
                resetNet();
                return false;
            }

            m_netLastDUID = duid;

            if (!m_p25->m_enableControl) {
                m_p25->m_affiliations->releaseGrant(m_netLC.getDstId(), false);
            }

            if (m_p25->m_notifyCC) {
                m_p25->notifyCC_ReleaseGrant(m_netLC.getDstId());
            }

            if (m_p25->m_netState != RS_NET_IDLE) {
                if (duid == DUID::TDU)
                    writeNet_TDU();

                resetNet();
            }
            break;

        default:
            break;
    }

    return true;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the Voice class. */

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
    m_rfLastHDUValid(false),
    m_rfLastLDU1(),
    m_rfLastLDU2(),
    m_rfFirstLDU2(true),
    m_netLC(),
    m_netLastLDU1(),
    m_netLastFrameType(FrameType::DATA_UNIT),
    m_rfLSD(),
    m_netLSD(),
    m_dfsiLC(),
    m_gotNetLDU1(false),
    m_netLDU1(nullptr),
    m_gotNetLDU2(false),
    m_netLDU2(nullptr),
    m_netLastDUID(DUID::TDU),
    m_lastDUID(DUID::TDU),
    m_lastMI(nullptr),
    m_hadVoice(false),
    m_lastRejectId(0U),
    m_silenceThreshold(DEFAULT_SILENCE_THRESHOLD),
    m_pktLDU1Count(0U),
    m_grpUpdtCount(0U),
    m_roamLDU1Count(0U),
    m_inbound(false),
    m_verbose(verbose),
    m_debug(debug)
{
    m_netLDU1 = new uint8_t[9U * 25U];
    m_netLDU2 = new uint8_t[9U * 25U];

    ::memset(m_netLDU1, 0x00U, 9U * 25U);
    resetWithNullAudio(m_netLDU1, false);
    ::memset(m_netLDU2, 0x00U, 9U * 25U);
    resetWithNullAudio(m_netLDU2, false);

    m_lastMI = new uint8_t[MI_LENGTH_BYTES];
    ::memset(m_lastMI, 0x00U, MI_LENGTH_BYTES);
}

/* Finalizes a instance of the Voice class. */

Voice::~Voice()
{
    delete[] m_netLDU1;
    delete[] m_netLDU2;
    delete[] m_lastMI;
}

/* Write data processed from RF to the network. */

void Voice::writeNetwork(const uint8_t *data, defines::DUID::E duid, defines::FrameType::E frameType)
{
    assert(data != nullptr);

    if (m_p25->m_network == nullptr)
        return;

    if (m_p25->m_rfTimeout.isRunning() && m_p25->m_rfTimeout.hasExpired())
        return;

    switch (duid) {
        case DUID::HDU:
            // ignore HDU
            break;
        case DUID::LDU1:
            m_p25->m_network->writeP25LDU1(m_rfLC, m_rfLSD, data, frameType);
            break;
        case DUID::LDU2:
            m_p25->m_network->writeP25LDU2(m_rfLC, m_rfLSD, data);
            break;
        case DUID::TDU:
        case DUID::TDULC:
            m_p25->m_network->writeP25TDU(m_rfLC, m_rfLSD);
            break;
        default:
            LogError(LOG_NET, "P25 unhandled voice DUID, duid = $%02X", duid);
            break;
    }
}

/* Helper to write end of frame data. */

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
    m_p25->writeRF_TDU(true, true);
    m_p25->m_control->writeRF_TDULC_ChanRelease(grp, srcId, dstId);
}

/* Helper to write a network P25 TDU packet. */

void Voice::writeNet_TDU()
{
    uint8_t buffer[P25_TDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_TDU_FRAME_LENGTH_BYTES + 2U);

    buffer[0U] = modem::TAG_EOT;
    buffer[1U] = 0x00U;

    // generate Sync
    Sync::addP25Sync(buffer + 2U);

    // generate NID
    m_p25->m_nid.encode(buffer + 2U, DUID::TDU);

    // add status bits
    P25Utils::addStatusBits(buffer + 2U, P25_TDU_FRAME_LENGTH_BITS, false, false);

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

    resetWithNullAudio(m_netLDU1, false);
    resetWithNullAudio(m_netLDU2, false);

    m_p25->m_netTimeout.stop();
    m_p25->m_networkWatchdog.stop();
    resetNet();
    m_p25->m_netState = RS_NET_IDLE;
    m_p25->m_tailOnIdle = true;

    // if voice on control; and CC is halted restart CC
    if (m_p25->m_voiceOnControl && m_p25->m_ccHalted) {
        m_p25->m_ccHalted = false;
        m_p25->writeRF_ControlData();
    }
}

/* Helper to check for an unflushed LDU1 packet. */

void Voice::checkNet_LDU1()
{
    if (m_p25->m_netState == RS_NET_IDLE)
        return;

    // check for an unflushed LDU1
    if ((m_netLDU1[10U] != 0x00U || m_netLDU1[26U] != 0x00U || m_netLDU1[55U] != 0x00U ||
        m_netLDU1[80U] != 0x00U || m_netLDU1[105U] != 0x00U || m_netLDU1[130U] != 0x00U ||
        m_netLDU1[155U] != 0x00U || m_netLDU1[180U] != 0x00U || m_netLDU1[204U] != 0x00U) &&
        m_gotNetLDU1)
        writeNet_LDU1();
}

/* Helper to write a network P25 LDU1 packet. */

void Voice::writeNet_LDU1()
{
    lc::LC control = lc::LC(*m_dfsiLC.control());

    // because the lc::LC internal copy routine will reset the encrypted flag -- lets force it
    control.setEncrypted(m_dfsiLC.control()->getEncrypted());

    data::LowSpeedData lsd = data::LowSpeedData(*m_dfsiLC.lsd());

    uint32_t dstId = control.getDstId();
    uint32_t srcId = control.getSrcId();
    bool group = control.getLCO() == LCO::GROUP;

    // ensure our dstId are sane from the last LDU1
    if (m_netLastLDU1.getDstId() != 0U) {
        if (dstId != m_netLastLDU1.getDstId() && control.isStandardMFId()) {
            if (m_verbose) {
                LogMessage(LOG_NET, P25_LDU1_STR ", dstId = %u doesn't match last LDU1 dstId = %u, fixing",
                    dstId, m_netLastLDU1.getDstId());
            }
            dstId = m_netLastLDU1.getDstId();
        }
    }

    // ensure our srcId are sane from the last LDU1
    if (m_netLastLDU1.getSrcId() != 0U) {
        if (srcId != m_netLastLDU1.getSrcId() && control.isStandardMFId()) {
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
    ulong64_t rsValue = control.getRS();

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
        uint8_t mi[MI_LENGTH_BYTES];
        ::memset(mi, 0x00U, MI_LENGTH_BYTES);

        /*if (control.getAlgId() != ALGO_UNENCRYPT && control.getKId() != 0) {
            control.getMI(m_lastMI);
        }*/

        if (m_netLastLDU1.getAlgId() != ALGO_UNENCRYPT && m_netLastLDU1.getKId() != 0) {
            control.setAlgId(m_netLastLDU1.getAlgId());
            control.setKId(m_netLastLDU1.getKId());
        }

        // restore MI from member variable
        ::memcpy(mi, m_lastMI, MI_LENGTH_BYTES);

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

        // conventional registration or DVRS support?
        if (((m_p25->m_enableControl && !m_p25->m_dedicatedControl) || m_p25->m_voiceOnControl) && !m_p25->m_disableNetworkGrant) {
            uint8_t serviceOptions = (m_netLC.getEmergency() ? 0x80U : 0x00U) +     // Emergency Flag
                (m_netLC.getEncrypted() ? 0x40U : 0x00U) +                          // Encrypted Flag
                (m_netLC.getPriority() & 0x07U);                                    // Priority

            if (!m_p25->m_affiliations->isGranted(dstId)) {
                if (!m_p25->m_control->writeRF_TSDU_Grant(srcId, dstId, serviceOptions, group, true)) {
                    LogError(LOG_NET, P25_HDU_STR " call rejected, network call not granted, dstId = %u", dstId);

                    if ((!m_p25->m_networkWatchdog.isRunning() || m_p25->m_networkWatchdog.hasExpired()) &&
                        m_p25->m_netLastDstId != 0U) {
                        if (m_p25->m_network != nullptr)
                            m_p25->m_network->resetP25();

                        resetWithNullAudio(m_netLDU1, false);
                        resetWithNullAudio(m_netLDU2, false);

                        m_p25->m_netTimeout.stop();
                        m_p25->m_networkWatchdog.stop();

                        m_netLC = lc::LC();
                        m_netLastLDU1 = lc::LC();
                        m_netLastFrameType = FrameType::DATA_UNIT;

                        m_p25->m_netState = RS_NET_IDLE;
                        m_p25->m_netLastDstId = 0U;
                        m_p25->m_netLastSrcId = 0U;

                        if (m_p25->m_rfState == RS_RF_REJECTED) {
                            m_p25->m_rfState = RS_RF_LISTENING;
                        }

                        return;
                    }
                }
            }

            m_p25->writeRF_Preamble(0, true);

            // if voice on control; insert grant updates before voice traffic
            if (m_p25->m_voiceOnControl) {
                uint32_t chNo = m_p25->m_affiliations->getGrantedCh(dstId);
                ::lookups::VoiceChData voiceChData = m_p25->m_affiliations->rfCh()->getRFChData(chNo);
                bool grp = m_p25->m_affiliations->isGroup(dstId);

                std::unique_ptr<lc::TSBK> osp;

                if (grp) {
                    osp = std::make_unique<lc::tsbk::OSP_GRP_VCH_GRANT_UPD>();

                    // transmit group voice grant update
                    osp->setLCO(TSBKO::OSP_GRP_VCH_GRANT_UPD);
                    osp->setDstId(dstId);
                    osp->setGrpVchId(voiceChData.chId());
                    osp->setGrpVchNo(chNo);
                }
                else {
                    uint32_t srcId = m_p25->m_affiliations->getGrantedSrcId(dstId);

                    osp = std::make_unique<lc::tsbk::OSP_UU_VCH_GRANT_UPD>();

                    // transmit group voice grant update
                    osp->setLCO(TSBKO::OSP_UU_VCH_GRANT_UPD);
                    osp->setSrcId(srcId);
                    osp->setDstId(dstId);
                    osp->setGrpVchId(voiceChData.chId());
                    osp->setGrpVchNo(chNo);
                }

                if (!m_p25->m_ccHalted) {
                    m_p25->m_txQueue.clear();
                    m_p25->m_ccHalted = true;
                }

                for (int i = 0; i < 6; i++)
                    m_p25->m_control->writeRF_TSDU_SBF_Imm(osp.get(), true);
            }
        }

        m_hadVoice = true;
        m_p25->m_netState = RS_NET_AUDIO;
        m_p25->m_netLastDstId = dstId;
        m_p25->m_netLastSrcId = srcId;
        m_p25->m_netTGHang.start();
        m_p25->m_netTimeout.start();
        m_netFrames = 0U;
        m_netLost = 0U;
        m_pktLDU1Count = 0U;
        m_grpUpdtCount = 0U;
        m_roamLDU1Count = 0U;

        if (!m_p25->m_disableNetworkHDU) {
            if (m_netLastFrameType != FrameType::HDU_LATE_ENTRY) {
                uint8_t buffer[P25_HDU_FRAME_LENGTH_BYTES + 2U];
                ::memset(buffer, 0x00U, P25_HDU_FRAME_LENGTH_BYTES + 2U);

                // generate Sync
                Sync::addP25Sync(buffer + 2U);

                // generate NID
                m_p25->m_nid.encode(buffer + 2U, DUID::HDU);

                // generate HDU
                m_netLC.encodeHDU(buffer + 2U);

                // add status bits
                P25Utils::addStatusBits(buffer + 2U, P25_HDU_FRAME_LENGTH_BITS, false, false);

                buffer[0U] = modem::TAG_DATA;
                buffer[1U] = 0x00U;

                m_p25->addFrame(buffer, P25_HDU_FRAME_LENGTH_BYTES + 2U, true);

                if (m_verbose) {
                    LogMessage(LOG_NET, P25_HDU_STR ", dstId = %u, algo = $%02X, kid = $%04X", m_netLC.getDstId(), m_netLC.getAlgId(), m_netLC.getKId());

                    if (control.getAlgId() != ALGO_UNENCRYPT) {
                        LogMessage(LOG_NET, P25_HDU_STR ", Enc Sync, MI = %02X %02X %02X %02X %02X %02X %02X %02X %02X", 
                            mi[0U], mi[1U], mi[2U], mi[3U], mi[4U], mi[5U], mi[6U], mi[7U], mi[8U]);
                    }
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
    else {
        if (m_p25->m_netTGHang.isRunning()) {
            if (m_p25->m_netLastDstId == 0U) {
                m_p25->m_netLastDstId = dstId;
                m_p25->m_netLastSrcId = srcId;
                LogWarning(LOG_NET, P25_LDU1_STR ", traffic in progress, with net TG hangtimer running and netLastDstId = 0, netLastDstId = %u", m_p25->m_netLastDstId);
            }

            m_p25->m_netTGHang.start();
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
                m_netLC.setLCO(LCO::EXPLICIT_SOURCE_ID);
            }
            else {
                // flag explicit block to follow in next LDU1
                if (m_netLC.getLCO() == LCO::GROUP) {
                    m_netLC.setExplicitId(true);
                }
            }
        }
    }
    else {
        netId = lc::LC::getSiteData().netId();
        sysId = lc::LC::getSiteData().sysId();
    }

    // are we swapping the LC out for the RFSS_STS_BCAST or LC_GROUP_UPDT?
    m_pktLDU1Count++;
    if (m_pktLDU1Count > PKT_LDU1_COUNT) {
        m_pktLDU1Count = 0U;

        // conventional registration or DVRS support?
        if ((m_p25->m_enableControl && !m_p25->m_dedicatedControl) || m_p25->m_voiceOnControl) {
            // per TIA-102.AABD-B transmit RFSS_STS_BCAST every 3 superframes (e.g. every 3 LDU1s)
            m_netLC.setMFId(MFG_STANDARD);
            m_netLC.setLCO(LCO::RFSS_STS_BCAST);
        }
        else {
            std::lock_guard<std::mutex> lock(m_p25->m_activeTGLock);
            if (m_p25->m_activeTG.size() > 0) {
                if (m_grpUpdtCount > m_p25->m_activeTG.size())
                    m_grpUpdtCount = 0U;

                if (m_p25->m_activeTG.size() < 2) {
                    uint32_t dstId = m_p25->m_activeTG.at(0);
                    m_netLC.setMFId(MFG_STANDARD);
                    m_netLC.setLCO(LCO::GROUP_UPDT);
                    m_netLC.setDstId(dstId);
                }
                else {
                    uint32_t dstId = m_p25->m_activeTG.at(m_grpUpdtCount);
                    uint32_t dstIdB = m_p25->m_activeTG.at(m_grpUpdtCount + 1U);
                    m_netLC.setMFId(MFG_STANDARD);
                    m_netLC.setLCO(LCO::GROUP_UPDT);
                    m_netLC.setDstId(dstId);
                    m_netLC.setDstIdB(dstIdB);

                    m_grpUpdtCount++;
                }
            }
        }
    }

    uint8_t buffer[P25_LDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_LDU_FRAME_LENGTH_BYTES + 2U);

    // generate Sync
    Sync::addP25Sync(buffer + 2U);

    // generate NID
    m_p25->m_nid.encode(buffer + 2U, DUID::LDU1);

    // generate LDU1 data
    if (!m_netLC.isStandardMFId()) {
        if (m_debug) {
            LogDebug(LOG_NET, "P25, LDU1 LC, non-standard payload, lco = $%02X, mfId = $%02X", m_netLC.getLCO(), m_netLC.getMFId());
        }
        m_netLC.setRS(rsValue);
    }

    m_netLC.encodeLDU1(buffer + 2U);

    // add the Audio
    m_audio.encode(buffer + 2U, m_netLDU1 + 10U, 0U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 26U, 1U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 55U, 2U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 80U, 3U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 105U, 4U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 130U, 5U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 155U, 6U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 180U, 7U);
    m_audio.encode(buffer + 2U, m_netLDU1 + 204U, 8U);

    // add the Low Speed Data
    m_netLSD.setLSD1(lsd.getLSD1());
    m_netLSD.setLSD2(lsd.getLSD2());
    m_netLSD.encode(buffer + 2U);

    // add status bits
    P25Utils::addStatusBits(buffer + 2U, P25_LDU_FRAME_LENGTH_BITS, false, false);

    buffer[0U] = modem::TAG_DATA;
    buffer[1U] = 0x00U;

    m_p25->addFrame(buffer, P25_LDU_FRAME_LENGTH_BYTES + 2U, true);

    if (m_verbose) {
        LogMessage(LOG_NET, P25_LDU1_STR " audio, mfId = $%02X, srcId = %u, dstId = %u, group = %u, emerg = %u, encrypt = %u, prio = %u, sysId = $%03X, netId = $%05X",
            m_netLC.getMFId(), m_netLC.getSrcId(), m_netLC.getDstId(), m_netLC.getGroup(), m_netLC.getEmergency(), m_netLC.getEncrypted(), m_netLC.getPriority(),
            sysId, netId);
    }

    resetWithNullAudio(m_netLDU1, m_netLC.getAlgId() != P25DEF::ALGO_UNENCRYPT);
    m_gotNetLDU1 = false;

    m_netFrames += 9U;
}

/* Helper to check for an unflushed LDU2 packet. */

void Voice::checkNet_LDU2()
{
    if (m_p25->m_netState == RS_NET_IDLE)
        return;

    // check for an unflushed LDU2
    if ((m_netLDU2[10U] != 0x00U || m_netLDU2[26U] != 0x00U || m_netLDU2[55U] != 0x00U ||
        m_netLDU2[80U] != 0x00U || m_netLDU2[105U] != 0x00U || m_netLDU2[130U] != 0x00U ||
        m_netLDU2[155U] != 0x00U || m_netLDU2[180U] != 0x00U || m_netLDU2[204U] != 0x00U) &&
        m_gotNetLDU2) {
        writeNet_LDU2();
    }
}

/* Helper to write a network P25 LDU2 packet. */

void Voice::writeNet_LDU2()
{
    lc::LC control = lc::LC(*m_dfsiLC.control());
    data::LowSpeedData lsd = data::LowSpeedData(*m_dfsiLC.lsd());

    uint32_t dstId = control.getDstId();

    // don't process network frames if this modem isn't authoritative
    if (!m_p25->m_authoritative && m_p25->m_permittedDstId != dstId) {
        if (!g_disableNonAuthoritativeLogging)
            LogWarning(LOG_NET, "[NON-AUTHORITATIVE] Ignoring network traffic (LDU2), destination not permitted!");
        resetNet();
        return;
    }

    uint8_t mi[MI_LENGTH_BYTES];
    control.getMI(mi);

    m_netLC.setMI(mi);
    m_netLC.setAlgId(control.getAlgId());
    m_netLC.setKId(control.getKId());

    uint8_t buffer[P25_LDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_LDU_FRAME_LENGTH_BYTES + 2U);

    // generate Sync
    Sync::addP25Sync(buffer + 2U);

    // generate NID
    m_p25->m_nid.encode(buffer + 2U, DUID::LDU2);

    // generate LDU2 data
    m_netLC.encodeLDU2(buffer + 2U);

    // add the Audio
    m_audio.encode(buffer + 2U, m_netLDU2 + 10U, 0U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 26U, 1U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 55U, 2U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 80U, 3U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 105U, 4U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 130U, 5U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 155U, 6U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 180U, 7U);
    m_audio.encode(buffer + 2U, m_netLDU2 + 204U, 8U);

    // add the Low Speed Data
    m_netLSD.setLSD1(lsd.getLSD1());
    m_netLSD.setLSD2(lsd.getLSD2());
    m_netLSD.encode(buffer + 2U);

    // add status bits
    P25Utils::addStatusBits(buffer + 2U, P25_LDU_FRAME_LENGTH_BITS, false, false);

    buffer[0U] = modem::TAG_DATA;
    buffer[1U] = 0x00U;

    m_p25->addFrame(buffer, P25_LDU_FRAME_LENGTH_BYTES + 2U, true);

    if (m_verbose) {
        LogMessage(LOG_NET, P25_LDU2_STR " audio, algo = $%02X, kid = $%04X", m_netLC.getAlgId(), m_netLC.getKId());

        if (control.getAlgId() != ALGO_UNENCRYPT) {
            LogMessage(LOG_NET, P25_LDU2_STR ", Enc Sync, MI = %02X %02X %02X %02X %02X %02X %02X %02X %02X", 
                mi[0U], mi[1U], mi[2U], mi[3U], mi[4U], mi[5U], mi[6U], mi[7U], mi[8U]);
        }
    }

    resetWithNullAudio(m_netLDU2, m_netLC.getAlgId() != P25DEF::ALGO_UNENCRYPT);
    m_gotNetLDU2 = false;

    m_netFrames += 9U;
}

/* Helper to insert IMBE null frames for missing audio. */

void Voice::insertNullAudio(uint8_t *data)
{
    if (data[0U] == 0x00U) {
        ::memcpy(data + 10U, NULL_IMBE, 11U);
    }

    if (data[25U] == 0x00U) {
        ::memcpy(data + 26U, NULL_IMBE, 11U);
    }

    if (data[50U] == 0x00U) {
        ::memcpy(data + 55U, NULL_IMBE, 11U);
    }

    if (data[75U] == 0x00U) {
        ::memcpy(data + 80U, NULL_IMBE, 11U);
    }

    if (data[100U] == 0x00U) {
        ::memcpy(data + 105U, NULL_IMBE, 11U);
    }

    if (data[125U] == 0x00U) {
        ::memcpy(data + 130U, NULL_IMBE, 11U);
    }

    if (data[150U] == 0x00U) {
        ::memcpy(data + 155U, NULL_IMBE, 11U);
    }

    if (data[175U] == 0x00U) {
        ::memcpy(data + 180U, NULL_IMBE, 11U);
    }

    if (data[200U] == 0x00U) {
        ::memcpy(data + 204U, NULL_IMBE, 11U);
    }
}

/* Helper to insert encrypted IMBE null frames for missing audio. */

void Voice::insertEncryptedNullAudio(uint8_t *data)
{
    if (data[0U] == 0x00U) {
        ::memcpy(data + 10U, ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[25U] == 0x00U) {
        ::memcpy(data + 26U, ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[50U] == 0x00U) {
        ::memcpy(data + 55U, ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[75U] == 0x00U) {
        ::memcpy(data + 80U, ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[100U] == 0x00U) {
        ::memcpy(data + 105U, ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[125U] == 0x00U) {
        ::memcpy(data + 130U, ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[150U] == 0x00U) {
        ::memcpy(data + 155U, ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[175U] == 0x00U) {
        ::memcpy(data + 180U, ENCRYPTED_NULL_IMBE, 11U);
    }

    if (data[200U] == 0x00U) {
        ::memcpy(data + 204U, ENCRYPTED_NULL_IMBE, 11U);
    }
}

/* Helper to reset IMBE buffer with null frames. */

void Voice::resetWithNullAudio(uint8_t* data, bool encrypted)
{
    if (data == nullptr)
        return;

    // clear buffer for next sequence
    ::memset(data, 0x00U, 9U * 25U);

    // fill with null
    if (!encrypted) {
        ::memcpy(data + 10U, P25DEF::NULL_IMBE, 11U);
        ::memcpy(data + 26U, P25DEF::NULL_IMBE, 11U);
        ::memcpy(data + 55U, P25DEF::NULL_IMBE, 11U);

        ::memcpy(data + 80U, P25DEF::NULL_IMBE, 11U);
        ::memcpy(data + 105U, P25DEF::NULL_IMBE, 11U);
        ::memcpy(data + 130U, P25DEF::NULL_IMBE, 11U);

        ::memcpy(data + 155U, P25DEF::NULL_IMBE, 11U);
        ::memcpy(data + 180U, P25DEF::NULL_IMBE, 11U);
        ::memcpy(data + 204U, P25DEF::NULL_IMBE, 11U);
    } else {
        ::memcpy(data + 10U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);
        ::memcpy(data + 26U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);
        ::memcpy(data + 55U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);

        ::memcpy(data + 80U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);
        ::memcpy(data + 105U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);
        ::memcpy(data + 130U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);

        ::memcpy(data + 155U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);
        ::memcpy(data + 180U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);
        ::memcpy(data + 204U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);
    }
}

/* Given the last MI, generate the next MI using LFSR. */

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
