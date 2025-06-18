// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2017-2025 Bryan Biedenkapp, N2PLL
 *  Copyright (C) 2022 Jason-UWU
 *
 */
#include "Defines.h"
#include "common/p25/P25Defines.h"
#include "common/p25/acl/AccessControl.h"
#include "common/p25/lc/tsbk/TSBKFactory.h"
#include "common/p25/lc/tdulc/TDULCFactory.h"
#include "common/p25/P25Utils.h"
#include "common/p25/Sync.h"
#include "common/AESCrypto.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "p25/lc/tsbk/OSP_DVM_GIT_HASH.h"
#include "p25/packet/Voice.h"
#include "p25/packet/ControlSignaling.h"
#include "p25/lookups/P25AffiliationLookup.h"
#include "ActivityLog.h"
#include "HostMain.h"
#include "Host.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc::tsbk;
using namespace p25::data;
using namespace p25::packet;

#include <cassert>
#include <cstring>
#include <random>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

// Make sure control data is supported.
#define IS_SUPPORT_CONTROL_CHECK(_PCKT_STR, _PCKT, _SRCID)                              \
    if (!m_p25->m_enableControl) {                                                      \
        LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, unsupported service, srcId = %u", _PCKT_STR.c_str(), _SRCID); \
        writeRF_TSDU_Deny(_SRCID, WUID_FNE, ReasonCode::DENY_SYS_UNSUPPORTED_SVC, _PCKT); \
        m_p25->m_rfState = RS_RF_REJECTED;                                              \
        return false;                                                                   \
    }

// Validate the source RID.
#define VALID_SRCID(_PCKT_STR, _PCKT, _SRCID)                                           \
    if (!acl::AccessControl::validateSrcId(_SRCID)) {                                   \
        LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, RID rejection, srcId = %u", _PCKT_STR.c_str(), _SRCID); \
        writeRF_TSDU_Deny(_SRCID, WUID_FNE, ReasonCode::DENY_REQ_UNIT_NOT_VALID, _PCKT); \
        denialInhibit(_SRCID);                                                          \
        m_p25->m_rfState = RS_RF_REJECTED;                                              \
        return false;                                                                   \
    }

// Validate the target RID.
#define VALID_DSTID(_PCKT_STR, _PCKT, _SRCID, _DSTID)                                   \
    if (!acl::AccessControl::validateSrcId(_DSTID)) {                                   \
        LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, RID rejection, dstId = %u", _PCKT_STR.c_str(), _DSTID); \
        writeRF_TSDU_Deny(_SRCID, _DSTID, ReasonCode::DENY_TGT_UNIT_NOT_VALID, _PCKT);  \
        m_p25->m_rfState = RS_RF_REJECTED;                                              \
        return false;                                                                   \
    }

// Validate the talkgroup ID.
#define VALID_TGID(_PCKT_STR, _PCKT, _SRCID, _DSTID)                                    \
    if (!acl::AccessControl::validateTGId(_DSTID)) {                                    \
        LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, TGID rejection, dstId = %u", _PCKT_STR.c_str(), _DSTID); \
        writeRF_TSDU_Deny(_SRCID, _DSTID, ReasonCode::DENY_TGT_GROUP_NOT_VALID, _PCKT); \
        m_p25->m_rfState = RS_RF_REJECTED;                                              \
        return false;                                                                   \
    }

// Verify the source RID is registered.
#define VERIFY_SRCID_REG(_PCKT_STR, _PCKT, _SRCID)                                      \
    if (!m_p25->m_affiliations->isUnitReg(_SRCID) && m_verifyReg) {                     \
        LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, RID not registered, srcId = %u", _PCKT_STR.c_str(), _SRCID); \
        writeRF_TSDU_Deny(_SRCID, WUID_FNE, ReasonCode::DENY_REQ_UNIT_NOT_AUTH, _PCKT); \
        writeRF_TSDU_U_Reg_Cmd(_SRCID);                                                 \
        m_p25->m_rfState = RS_RF_REJECTED;                                              \
        return false;                                                                   \
    }

// Verify the source RID is affiliated.
#define VERIFY_SRCID_AFF(_PCKT_STR, _PCKT, _SRCID, _DSTID)                              \
    if (!m_p25->m_affiliations->isGroupAff(_SRCID, _DSTID) && m_verifyAff) {            \
        LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, RID not affiliated to TGID, srcId = %u, dstId = %u", _PCKT_STR.c_str(), _SRCID, _DSTID); \
        writeRF_TSDU_Deny(_SRCID, _DSTID, ReasonCode::DENY_REQ_UNIT_NOT_AUTH, _PCKT);   \
        writeRF_TSDU_U_Reg_Cmd(_SRCID);                                                 \
        m_p25->m_rfState = RS_RF_REJECTED;                                              \
        return false;                                                                   \
    }

// Validate the source RID (network).
#define VALID_SRCID_NET(_PCKT_STR, _SRCID)                                              \
    if (!acl::AccessControl::validateSrcId(_SRCID)) {                                   \
        LogWarning(LOG_NET, P25_TSDU_STR ", %s denial, RID rejection, srcId = %u", _PCKT_STR.c_str(), _SRCID); \
        return false;                                                                   \
    }

// Validate the target RID (network).
#define VALID_DSTID_NET(_PCKT_STR, _DSTID)                                              \
    if (!acl::AccessControl::validateSrcId(_DSTID)) {                                   \
        LogWarning(LOG_NET, P25_TSDU_STR ", %s denial RID rejection, dstId = %u", _PCKT_STR.c_str(), _DSTID); \
        return false;                                                                   \
    }

// Macro helper to verbose log a generic TSBK.
#define VERBOSE_LOG_TSBK(_PCKT_STR, _SRCID, _DSTID)                                     \
    if (m_verbose) {                                                                    \
        LogMessage(LOG_RF, P25_TSDU_STR ", %s, srcId = %u, dstId = %u", _PCKT_STR.c_str(), _SRCID, _DSTID); \
    }

// Macro helper to verbose log a generic TSBK.
#define VERBOSE_LOG_TSBK_DST(_PCKT_STR, _DSTID)                                         \
    if (m_verbose) {                                                                    \
        LogMessage(LOG_RF, P25_TSDU_STR ", %s, dstId = %u", _PCKT_STR.c_str(), _DSTID); \
    }

// Macro helper to verbose log a generic network TSBK.
#define VERBOSE_LOG_TSBK_NET(_PCKT_STR, _SRCID, _DSTID)                                 \
    if (m_verbose) {                                                                    \
        LogMessage(LOG_NET, P25_TSDU_STR ", %s, srcId = %u, dstId = %u", _PCKT_STR.c_str(), _SRCID, _DSTID); \
    }

// Macro helper to verbose log a generic network TSBK.
#define DEBUG_LOG_TSBK(_PCKT_STR)                                                       \
    if (m_debug) {                                                                      \
        LogMessage(LOG_RF, P25_TSDU_STR ", %s", _PCKT_STR.c_str());                     \
    }

#define RF_TO_WRITE_NET(OSP)                                                            \
    if (m_p25->m_network != nullptr) {                                                  \
        uint8_t _buf[P25_TSDU_FRAME_LENGTH_BYTES];                                      \
        writeNet_TSDU_From_RF(OSP, _buf);                                               \
        writeNetworkRF(OSP, _buf, true);                                                \
    }

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t ADJ_SITE_TIMER_TIMEOUT = 60U;
const uint32_t ADJ_SITE_UPDATE_CNT = 5U;
const uint32_t TSDU_CTRL_BURST_COUNT = 2U;
const uint32_t TSBK_MBF_CNT = 3U;
const uint32_t GRANT_TIMER_TIMEOUT = 15U;
const uint8_t CONV_FALLBACK_PACKET_DELAY = 8U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Process a data frame from the RF interface. */

bool ControlSignaling::process(uint8_t* data, uint32_t len, std::unique_ptr<lc::TSBK> preDecodedTSBK)
{
    assert(data != nullptr);

    if (!m_p25->m_enableControl)
        return false;

    DUID::E duid = DUID::HDU;
    if (preDecodedTSBK == nullptr) {
        // Decode the NID
        bool valid = m_p25->m_nid.decode(data + 2U);

        if (m_p25->m_rfState == RS_RF_LISTENING && !valid)
            return false;

        duid = m_p25->m_nid.getDUID();
    } else {
        duid = DUID::TSDU;
    }

    RPT_RF_STATE prevRfState = m_p25->m_rfState;
    std::unique_ptr<lc::TSBK> tsbk;

    // handle individual DUIDs
    if (duid == DUID::TSDU) {
        m_inbound = true;

        if (m_p25->m_rfState != RS_RF_DATA) {
            m_p25->m_rfState = RS_RF_DATA;
        }

        if (preDecodedTSBK == nullptr) {
            tsbk = TSBKFactory::createTSBK(data + 2U);
            if (tsbk == nullptr) {
                LogWarning(LOG_RF, P25_TSDU_STR ", undecodable LC");
                m_p25->m_rfState = prevRfState;
                return false;
            }
        } else {
            tsbk = std::move(preDecodedTSBK);
        }

        const uint32_t constValue = 0x17DC0U;
        if ((m_p25->m_siteData.netId() >> 8) == (constValue >> 5)) {
            ::fatal("error 16\n");
        }

        uint32_t srcId = tsbk->getSrcId();
        uint32_t dstId = tsbk->getDstId();

        m_p25->m_affiliations->touchUnitReg(srcId);
        m_lastMFID = tsbk->getMFId();

        // handle standard P25 reference opcodes
        switch (tsbk->getLCO()) {
            case TSBKO::IOSP_GRP_VCH:
            {
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK(tsbk->toString(true), TSBKO::IOSP_GRP_VCH, srcId);

                // validate the source RID
                VALID_SRCID(tsbk->toString(true), TSBKO::IOSP_GRP_VCH, srcId);

                // validate the talkgroup ID
                VALID_TGID(tsbk->toString(true), TSBKO::IOSP_GRP_VCH, srcId, dstId);

                // verify the source RID is affiliated
                VERIFY_SRCID_AFF(tsbk->toString(true), TSBKO::IOSP_GRP_VCH, srcId, dstId);

                VERBOSE_LOG_TSBK(tsbk->toString(true), srcId, dstId);
                if (m_p25->m_authoritative) {
                    uint8_t serviceOptions = (tsbk->getEmergency() ? 0x80U : 0x00U) +       // Emergency Flag
                        (tsbk->getEncrypted() ? 0x40U : 0x00U) +                            // Encrypted Flag
                        (tsbk->getPriority() & 0x07U);                                      // Priority

                    writeRF_TSDU_Grant(srcId, dstId, serviceOptions, true);
                } else {
                    if (m_p25->m_network != nullptr)
                        m_p25->m_network->writeGrantReq(modem::DVM_STATE::STATE_P25, srcId, dstId, 0U, false);
                }
            }
            break;
            case TSBKO::IOSP_UU_VCH:
            {
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK(tsbk->toString(true), TSBKO::IOSP_UU_VCH, srcId);

                // validate the source RID
                VALID_SRCID(tsbk->toString(true), TSBKO::IOSP_UU_VCH, srcId);

                // validate the target RID
                VALID_DSTID(tsbk->toString(true), TSBKO::IOSP_UU_VCH, srcId, dstId);

                // verify the source RID is registered
                VERIFY_SRCID_REG(tsbk->toString(true), TSBKO::IOSP_UU_VCH, srcId);

                VERBOSE_LOG_TSBK(tsbk->toString(true), srcId, dstId);
                if (m_unitToUnitAvailCheck) {
                    writeRF_TSDU_UU_Ans_Req(srcId, dstId);
                }
                else {
                    if (m_p25->m_authoritative) {
                        uint8_t serviceOptions = (tsbk->getEmergency() ? 0x80U : 0x00U) +   // Emergency Flag
                            (tsbk->getEncrypted() ? 0x40U : 0x00U) +                        // Encrypted Flag
                            (tsbk->getPriority() & 0x07U);                                  // Priority

                        writeRF_TSDU_Grant(srcId, dstId, serviceOptions, false);
                    } else {
                        if (m_p25->m_network != nullptr)
                            m_p25->m_network->writeGrantReq(modem::DVM_STATE::STATE_P25, srcId, dstId, 0U, true);
                    }
                }
            }
            break;
            case TSBKO::IOSP_UU_ANS:
            {
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK(tsbk->toString(true), TSBKO::IOSP_UU_ANS, srcId);

                // validate the source RID
                VALID_SRCID(tsbk->toString(true), TSBKO::IOSP_UU_ANS, srcId);

                // validate the target RID
                VALID_DSTID(tsbk->toString(true), TSBKO::IOSP_UU_ANS, srcId, dstId);

                IOSP_UU_ANS* iosp = static_cast<IOSP_UU_ANS*>(tsbk.get());
                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", %s, response = $%02X, srcId = %u, dstId = %u",
                        tsbk->toString(true).c_str(), iosp->getResponse(), srcId, dstId);
                }

                if (iosp->getResponse() == ResponseCode::ANS_PROCEED) {
                    if (m_p25->m_authoritative) {
                        uint8_t serviceOptions = (tsbk->getEmergency() ? 0x80U : 0x00U) +   // Emergency Flag
                            (tsbk->getEncrypted() ? 0x40U : 0x00U) +                        // Encrypted Flag
                            (tsbk->getPriority() & 0x07U);                                  // Priority

                        writeRF_TSDU_Grant(srcId, dstId, serviceOptions, false);
                    } else {
                        if (m_p25->m_network != nullptr)
                            m_p25->m_network->writeGrantReq(modem::DVM_STATE::STATE_P25, srcId, dstId, 0U, true);
                    }
                }
                else if (iosp->getResponse() == ResponseCode::ANS_DENY) {
                    writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_TGT_UNIT_REFUSED, TSBKO::IOSP_UU_ANS);
                }
                else if (iosp->getResponse() == ResponseCode::ANS_WAIT) {
                    writeRF_TSDU_Queue(srcId, dstId, ReasonCode::QUE_TGT_UNIT_QUEUED, TSBKO::IOSP_UU_ANS);
                }
            }
            break;
            case TSBKO::IOSP_TELE_INT_ANS:
            {
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK(tsbk->toString(true), TSBKO::IOSP_TELE_INT_ANS, srcId);

                // validate the source RID
                VALID_SRCID(tsbk->toString(true), TSBKO::IOSP_TELE_INT_ANS, srcId);

                writeRF_TSDU_Deny(srcId, WUID_FNE, ReasonCode::DENY_SYS_UNSUPPORTED_SVC, TSBKO::IOSP_TELE_INT_ANS);
            }
            break;
            case TSBKO::ISP_SNDCP_CH_REQ:
            {
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK(tsbk->toString(true), TSBKO::ISP_SNDCP_CH_REQ, srcId);

                // validate the source RID
                VALID_SRCID(tsbk->toString(true), TSBKO::ISP_SNDCP_CH_REQ, srcId);

                ISP_SNDCP_CH_REQ* isp = static_cast<ISP_SNDCP_CH_REQ*>(tsbk.get());
                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", %s, dataServiceOptions = $%02X, dataAccessControl = $%04X, srcId = %u",
                        tsbk->toString(true).c_str(), isp->getDataServiceOptions(), isp->getDataAccessControl(), srcId);
                }

                if (m_p25->m_sndcpSupport) {
                    writeRF_TSDU_SNDCP_Grant(srcId, false);
                }
                else {
                    writeRF_TSDU_Deny(srcId, WUID_FNE, ReasonCode::DENY_SYS_UNSUPPORTED_SVC, TSBKO::ISP_SNDCP_CH_REQ);
                }
            }
            break;
            case TSBKO::ISP_SNDCP_REC_REQ:
            {
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK(tsbk->toString(true), TSBKO::ISP_SNDCP_CH_REQ, srcId);

                // validate the source RID
                VALID_SRCID(tsbk->toString(true), TSBKO::ISP_SNDCP_CH_REQ, srcId);

                ISP_SNDCP_CH_REQ* isp = static_cast<ISP_SNDCP_CH_REQ*>(tsbk.get());
                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", %s, dataServiceOptions = $%02X, dataAccessControl = %u, srcId = %u",
                        tsbk->toString(true).c_str(), isp->getDataServiceOptions(), isp->getDataAccessControl(), srcId);
                }

                if (m_p25->m_sndcpSupport) {
                    writeRF_TSDU_SNDCP_Grant(srcId, false);
                }
                else {
                    writeRF_TSDU_Deny(srcId, WUID_FNE, ReasonCode::DENY_SYS_UNSUPPORTED_SVC, TSBKO::ISP_SNDCP_REC_REQ);
                }
            }
            break;
            case TSBKO::IOSP_STS_UPDT:
            {
                // validate the source RID
                VALID_SRCID(tsbk->toString(true), TSBKO::IOSP_STS_UPDT, srcId);

                IOSP_STS_UPDT* iosp = static_cast<IOSP_STS_UPDT*>(tsbk.get());
                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", %s, status = $%02X, srcId = %u", 
                        tsbk->toString(true).c_str(), iosp->getStatus(), srcId);
                }

                RF_TO_WRITE_NET(iosp);

                if (!m_noStatusAck) {
                    writeRF_TSDU_ACK_FNE(srcId, TSBKO::IOSP_STS_UPDT, false, false);
                }

                ::ActivityLog("P25", true, "status update from %u", srcId);
            }
            break;
            case TSBKO::IOSP_MSG_UPDT:
            {
                // validate the source RID
                VALID_SRCID(tsbk->toString(true), TSBKO::IOSP_MSG_UPDT, srcId);

                IOSP_MSG_UPDT* iosp = static_cast<IOSP_MSG_UPDT*>(tsbk.get());
                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", %s, message = $%02X, srcId = %u, dstId = %u",
                        tsbk->toString(true).c_str(), iosp->getMessage(), srcId, dstId);
                }

                RF_TO_WRITE_NET(iosp);

                if (!m_noMessageAck) {
                    writeRF_TSDU_ACK_FNE(srcId, TSBKO::IOSP_MSG_UPDT, false, false);
                }

                ::ActivityLog("P25", true, "message update from %u", srcId);
            }
            break;
            case TSBKO::IOSP_RAD_MON:
            {
                // validate the source RID
                VALID_SRCID(tsbk->toString(true), TSBKO::IOSP_RAD_MON, srcId);

                // validate the target RID
                VALID_DSTID(tsbk->toString(true), TSBKO::IOSP_RAD_MON, srcId, dstId);

                IOSP_RAD_MON* iosp = static_cast<IOSP_RAD_MON*>(tsbk.get());
                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", %s, srcId = %u, dstId = %u, txMult = %u", 
                        tsbk->toString(true).c_str(), srcId, dstId, iosp->getTxMult());
                }

                ::ActivityLog("P25" , true , "radio monitor request from %u to %u" , srcId , dstId);

                writeRF_TSDU_Radio_Mon(srcId, dstId, iosp->getTxMult());
            }
            break;
            case TSBKO::IOSP_CALL_ALRT:
            {
                // validate the source RID
                VALID_SRCID(tsbk->toString(true), TSBKO::IOSP_CALL_ALRT, srcId);

                // validate the target RID
                VALID_DSTID(tsbk->toString(true), TSBKO::IOSP_CALL_ALRT, srcId, dstId);

                VERBOSE_LOG_TSBK(tsbk->toString(true), srcId, dstId);
                ::ActivityLog("P25", true, "call alert request from %u to %u", srcId, dstId);

                writeRF_TSDU_Call_Alrt(srcId, dstId);
            }
            break;
            case TSBKO::IOSP_ACK_RSP:
            {
                // validate the source RID
                VALID_SRCID(tsbk->toString(true), TSBKO::IOSP_ACK_RSP, srcId);

                // validate the target RID
                VALID_DSTID(tsbk->toString(true), TSBKO::IOSP_ACK_RSP, srcId, dstId);

                IOSP_ACK_RSP* iosp = static_cast<IOSP_ACK_RSP*>(tsbk.get());
                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", %s, AIV = %u, serviceType = $%02X, srcId = %u, dstId = %u",
                        tsbk->toString(true).c_str(), iosp->getAIV(), iosp->getService(), srcId, dstId);
                }

                ::ActivityLog("P25", true, "ack response from %u to %u", srcId, dstId);

                // bryanb: HACK -- for some reason, if the AIV is false and we have a dstId
                // its very likely srcId and dstId are swapped so we'll swap them
                if (!iosp->getAIV() && dstId != 0U) {
                    iosp->setAIV(true);
                    iosp->setSrcId(dstId);
                    iosp->setDstId(srcId);
                }

                writeRF_TSDU_SBF(iosp, false);
            }
            break;
            case TSBKO::ISP_CAN_SRV_REQ:
            {
                ISP_CAN_SRV_REQ* isp = static_cast<ISP_CAN_SRV_REQ*>(tsbk.get());
                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", %s, AIV = %u, serviceType = $%02X, reason = $%02X, srcId = %u, dstId = %u",
                        tsbk->toString(true).c_str(), isp->getAIV(), isp->getService(), isp->getResponse(), srcId, dstId);
                }

                ::ActivityLog("P25", true, "cancel service request from %u", srcId);

                writeRF_TSDU_ACK_FNE(srcId, isp->getService(), false, true);
            }
            break;
            case TSBKO::IOSP_EXT_FNCT:
            {
                IOSP_EXT_FNCT* iosp = static_cast<IOSP_EXT_FNCT*>(tsbk.get());
                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", %s, op = $%02X, arg = %u, tgt = %u",
                        tsbk->toString(true).c_str(), iosp->getExtendedFunction(), srcId, dstId);
                }

                // generate activity log entry
                switch (iosp->getExtendedFunction()) {
                // Standard
                case ExtendedFunctions::CHECK_ACK:
                    ::ActivityLog("P25", true, "radio check response from %u to %u", srcId, dstId);
                    break;
                case ExtendedFunctions::INHIBIT_ACK:
                    ::ActivityLog("P25", true, "radio inhibit response from %u to %u", srcId, dstId);
                    break;
                case ExtendedFunctions::UNINHIBIT_ACK:
                    ::ActivityLog("P25", true, "radio uninhibit response from %u to %u", srcId, dstId);
                    break;
                // Dynamic Regroup
                case ExtendedFunctions::DYN_REGRP_REQ_ACK:
                    ::ActivityLog("P25", true, "radio dynamic regroup response from %u to TG%u", srcId, dstId);
                    break;
                case ExtendedFunctions::DYN_REGRP_CANCEL_ACK:
                    ::ActivityLog("P25", true, "radio dynamic regroup cancel response from %u to TG%u", srcId, dstId);
                    break;
                case ExtendedFunctions::DYN_REGRP_LOCK_ACK:
                    ::ActivityLog("P25", true, "radio dynamic regroup selector lock response from %u", srcId);
                    break;
                case ExtendedFunctions::DYN_REGRP_UNLOCK_ACK:
                    ::ActivityLog("P25", true, "radio dynamic regroup selector unlock response from %u", srcId);
                    break;
                }

                writeRF_TSDU_SBF(iosp, true);
            }
            break;
            case TSBKO::ISP_EMERG_ALRM_REQ:
            {
                ISP_EMERG_ALRM_REQ* isp = static_cast<ISP_EMERG_ALRM_REQ*>(tsbk.get());
                if (isp->getEmergency()) {
                    VERBOSE_LOG_TSBK(tsbk->toString(true), srcId, dstId);

                    ::ActivityLog("P25", true, "emergency alarm request request from %u", srcId);

                    // emergency functions are expressly not supported by DVM -- DVM will *ACKNOWLEDGE* the request but will not do any
                    // further processing with it
                    writeRF_TSDU_ACK_FNE(srcId, TSBKO::ISP_EMERG_ALRM_REQ, false, true);
                }
            }
            break;
            case TSBKO::IOSP_GRP_AFF:
            {
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK(tsbk->toString(true), TSBKO::IOSP_GRP_AFF, srcId);

                VERBOSE_LOG_TSBK(tsbk->toString(true), srcId, dstId);
                if (m_p25->m_ackTSBKRequests) {
                    writeRF_TSDU_ACK_FNE(srcId, TSBKO::IOSP_GRP_AFF, true, true);
                }

                if (writeRF_TSDU_Grp_Aff_Rsp(srcId, dstId) == ResponseCode::REFUSED) {
                    if (m_p25->m_demandUnitRegForRefusedAff) {
                        writeRF_TSDU_U_Reg_Cmd(srcId);
                    }
                }
            }
            break;
            case TSBKO::ISP_GRP_AFF_Q_RSP:
            {
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK(tsbk->toString(true), TSBKO::ISP_GRP_AFF_Q_RSP, srcId);

                // validate the source RID
                VALID_SRCID(tsbk->toString(true), TSBKO::IOSP_ACK_RSP, srcId);

                // validate the target RID
                VALID_DSTID(tsbk->toString(true), TSBKO::IOSP_ACK_RSP, srcId, dstId);

                if (m_p25->m_ackTSBKRequests) {
                    writeRF_TSDU_ACK_FNE(srcId, TSBKO::ISP_GRP_AFF_Q_RSP, true, true);
                }

                ISP_GRP_AFF_Q_RSP* isp = static_cast<ISP_GRP_AFF_Q_RSP*>(tsbk.get());
                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", %s, srcId = %u, dstId = %u, anncId = %u", 
                        tsbk->toString(true).c_str(), srcId, dstId, isp->getAnnounceGroup());
                }

                ::ActivityLog("P25", true, "group affiliation query response from %u to %s %u", srcId, "TG ", dstId);

                if (!m_p25->m_affiliations->isGroupAff(srcId, dstId)) {
                    // update dynamic affiliation table
                    m_p25->m_affiliations->groupAff(srcId, dstId);

                    if (m_p25->m_network != nullptr)
                        m_p25->m_network->announceGroupAffiliation(srcId, dstId);
                }
            }
            break;
            case TSBKO::ISP_U_DEREG_REQ:
            {
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK(tsbk->toString(true), TSBKO::ISP_U_DEREG_REQ, srcId);

                // validate the source RID
                VALID_SRCID(tsbk->toString(true), TSBKO::ISP_U_DEREG_REQ, srcId);

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", %s, srcId = %u, sysId = $%03X, netId = $%05X",
                        tsbk->toString(true).c_str(), srcId, tsbk->getSysId(), tsbk->getNetId());
                }

                if (m_p25->m_ackTSBKRequests) {
                    writeRF_TSDU_ACK_FNE(srcId, TSBKO::ISP_U_DEREG_REQ, true, true);
                }

                writeRF_TSDU_U_Dereg_Ack(srcId);
            }
            break;
            case TSBKO::IOSP_U_REG:
            {
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK(tsbk->toString(true), TSBKO::IOSP_U_REG, srcId);

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", %s, srcId = %u, sysId = $%03X, netId = $%05X",
                        tsbk->toString(true).c_str(), srcId, tsbk->getSysId(), tsbk->getNetId());
                }

                if (m_p25->m_ackTSBKRequests) {
                    writeRF_TSDU_ACK_FNE(srcId, TSBKO::IOSP_U_REG, true, true);
                }

                if (m_requireLLAForReg) {
                    writeRF_TSDU_Auth_Dmd(srcId);
                }
                else {
                    writeRF_TSDU_U_Reg_Rsp(srcId, tsbk->getSysId());
                }
            }
            break;
            case TSBKO::ISP_LOC_REG_REQ:
            {
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK(tsbk->toString(true), TSBKO::ISP_LOC_REG_REQ, srcId);

                VERBOSE_LOG_TSBK(tsbk->toString(true), srcId, dstId);
                writeRF_TSDU_Loc_Reg_Rsp(srcId, dstId, tsbk->getGroup());
            }
            break;
            case TSBKO::ISP_AUTH_RESP:
            {
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK(tsbk->toString(true), TSBKO::ISP_AUTH_RESP, srcId);

                ISP_AUTH_RESP* isp = static_cast<ISP_AUTH_RESP*>(tsbk.get());
                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", %s, srcId = %u", 
                        tsbk->toString(true).c_str(), srcId);
                }

                ::ActivityLog("P25", true, "authentication response from %u", srcId);

                crypto::AES* aes = new crypto::AES(crypto::AESKeyLength::AES_128);

                // get RES1 from response
                uint8_t RES1[AUTH_RES_LENGTH_BYTES];
                isp->getAuthRes(RES1);

                // get challenge for our SU
                ulong64_t challenge = 0U;
                try {
                    challenge = m_llaDemandTable.at(srcId);
                }
                catch (...) {
                    challenge = 0U;
                }

                uint8_t RC[AUTH_RAND_CHLNG_LENGTH_BYTES];
                SET_UINT32(challenge >> 8, RC, 0);
                RC[4U] = (uint8_t)(challenge & 0xFFU);

                // expand RAND1 to 16 bytes
                uint8_t expandedRAND1[16];
                ::memset(expandedRAND1, 0x00U, AUTH_KEY_LENGTH_BYTES);
                for (uint32_t i = 0; i < AUTH_RAND_CHLNG_LENGTH_BYTES; i++)
                    expandedRAND1[i] = RC[i];

                // generate XRES1
                uint8_t* XRES1 = aes->encryptECB(expandedRAND1, AUTH_KEY_LENGTH_BYTES * sizeof(uint8_t), m_p25->m_llaKS);

                // compare RES1 and XRES1
                bool authFailed = false;
                for (uint32_t i = 0; i < AUTH_RES_LENGTH_BYTES; i++) {
                    if (XRES1[i] != RES1[i]) {
                        authFailed = true;
                    }
                }

                // cleanup buffers
                delete[] XRES1;
                delete aes;

                if (!authFailed) {
                    writeRF_TSDU_U_Reg_Rsp(srcId, m_p25->m_siteData.sysId());
                }
                else {
                    LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, AUTH failed, src = %u", isp->toString().c_str(), srcId);
                    ::ActivityLog("P25", true, "unit registration request from %u denied, authentication failure", srcId);
                    writeRF_TSDU_Deny(srcId, WUID_FNE, ReasonCode::DENY_SU_FAILED_AUTH, TSBKO::IOSP_U_REG);
                }
            }
            break;
            default:
                LogError(LOG_RF, P25_TSDU_STR ", unhandled LCO, mfId = $%02X, lco = $%02X", tsbk->getMFId(), tsbk->getLCO());
                break;
        } // switch (tsbk->getLCO())

        // add trailing null pad; only if control data isn't being transmitted
        if (!m_p25->m_ccRunning) {
            m_p25->writeRF_Nulls();
        }

        m_inbound = false;
        m_p25->m_rfState = prevRfState;
        return true;
    }
    else {
        LogError(LOG_RF, "P25 unhandled data DUID, duid = $%02X", duid);
    }

    return false;
}

/* Process a data frame from the network. */

bool ControlSignaling::processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, DUID::E& duid)
{
    if (!m_p25->m_enableControl)
        return false;
    if (m_p25->m_rfState != RS_RF_LISTENING && m_p25->m_netState == RS_NET_IDLE)
        return false;

    switch (duid) {
        case DUID::TSDU:
            if (m_p25->m_netState == RS_NET_IDLE) {
                std::unique_ptr<lc::TSBK> tsbk = TSBKFactory::createTSBK(data);
                if (tsbk == nullptr) {
                    return false;
                }

                // handle updating internal adjacent site information
                if (tsbk->getLCO() == TSBKO::OSP_ADJ_STS_BCAST) {
                    if (!m_p25->m_enableControl) {
                        return false;
                    }

                    if (m_p25->m_disableAdjSiteBroadcast) {
                        return false;
                    }

                    OSP_ADJ_STS_BCAST* osp = static_cast<OSP_ADJ_STS_BCAST*>(tsbk.get());
                    if (osp->getAdjSiteId() != m_p25->m_siteData.siteId()) {
                        // update site table data
                        SiteData site;
                        try {
                            site = m_adjSiteTable.at(osp->getAdjSiteId());
                        } catch (...) {
                            site = SiteData();
                        }

                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", %s, sysId = $%03X, rfss = $%02X, site = $%02X, chNo = %u-%u, svcClass = $%02X", tsbk->toString().c_str(),
                                osp->getAdjSiteSysId(), osp->getAdjSiteRFSSId(), osp->getAdjSiteId(), osp->getAdjSiteChnId(), osp->getAdjSiteChnNo(), osp->getAdjSiteSvcClass());
                        }

                        site.setAdjSite(osp->getAdjSiteSysId(), osp->getAdjSiteRFSSId(), osp->getAdjSiteId(), osp->getAdjSiteChnId(), osp->getAdjSiteChnNo(), osp->getAdjSiteSvcClass());

                        m_adjSiteTable[site.siteId()] = site;
                        m_adjSiteUpdateCnt[site.siteId()] = ADJ_SITE_UPDATE_CNT;
                    } else {
                        /*
                        ** treat same site adjacent site broadcast as a SCCB for this site
                        */
                        // update site table data
                        SiteData site;
                        try {
                            site = m_sccbTable.at(osp->getAdjSiteRFSSId());
                        }
                        catch (...) {
                            site = SiteData();
                        }

                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", %s, sysId = $%03X, rfss = $%02X, site = $%02X, chNo = %u-%u, svcClass = $%02X", tsbk->toString().c_str(),
                                osp->getAdjSiteSysId(), osp->getAdjSiteRFSSId(), osp->getAdjSiteId(), osp->getAdjSiteChnId(), osp->getAdjSiteChnNo(), osp->getAdjSiteSvcClass());
                        }

                        site.setAdjSite(osp->getAdjSiteSysId(), osp->getAdjSiteRFSSId(), osp->getAdjSiteId(), osp->getAdjSiteChnId(), osp->getAdjSiteChnNo(), osp->getAdjSiteSvcClass());

                        m_sccbTable[site.rfssId()] = site;
                        m_sccbUpdateCnt[site.rfssId()] = ADJ_SITE_UPDATE_CNT;
                    }

                    return true;
                }

                uint32_t srcId = tsbk->getSrcId();
                uint32_t dstId = tsbk->getDstId();

                // handle internal / Omaha Communication Systems DVM TSDUs
                if (tsbk->getMFId() == MFG_DVM_OCS) {
                    switch (tsbk->getLCO()) {
                        case LCO::CALL_TERM:
                        {
                            if (m_p25->m_dedicatedControl) {
                                // is the specified channel granted?
                                if (/*m_p25->m_affiliations->isChBusy(chNo) &&*/ m_p25->m_affiliations->isGranted(dstId)) {
                                    uint32_t chNo = tsbk->getGrpVchNo();

                                    if (m_verbose) {
                                        LogMessage(LOG_NET, P25_TSDU_STR ", %s, chNo = %u, srcId = %u, dstId = %u", 
                                            tsbk->toString().c_str(), chNo, srcId, dstId);
                                    }

                                    m_p25->m_affiliations->releaseGrant(dstId, false);
                                }
                            }
                            
                            return true; // don't allow this to write to the air
                        }
                        case TSBKO::OSP_DVM_GIT_HASH:
                            // ignore
                            return true; // don't allow this to write to the air
                        default:
                            LogError(LOG_NET, P25_TSDU_STR ", unhandled LCO, mfId = $%02X, lco = $%02X", tsbk->getMFId(), tsbk->getLCO());
                            return false;
                    }

                    writeNet_TSDU(tsbk.get());
                    return true;
                }

                // handle standard P25 reference opcodes
                switch (tsbk->getLCO()) {
                    case TSBKO::IOSP_GRP_VCH:
                    case TSBKO::IOSP_UU_VCH:
                    {
                        if (m_p25->m_enableControl && m_p25->m_dedicatedControl) {
                            if (!m_p25->m_affiliations->isGranted(dstId)) {
                                if (m_verbose) {
                                    LogMessage(LOG_NET, P25_TSDU_STR ", %s, emerg = %u, encrypt = %u, prio = %u, chNo = %u-%u, srcId = %u, dstId = %u",
                                        tsbk->toString(true).c_str(), tsbk->getEmergency(), tsbk->getEncrypted(), tsbk->getPriority(), tsbk->getGrpVchId(), tsbk->getGrpVchNo(), srcId, dstId);
                                }

                                uint8_t serviceOptions = (tsbk->getEmergency() ? 0x80U : 0x00U) +   // Emergency Flag
                                    (tsbk->getEncrypted() ? 0x40U : 0x00U) +                        // Encrypted Flag
                                    (tsbk->getPriority() & 0x07U);                                  // Priority

                                writeRF_TSDU_Grant(srcId, dstId, serviceOptions, (tsbk->getLCO() == TSBKO::IOSP_GRP_VCH), true);
                            }
                        }
                    }
                    return true; // don't allow this to write to the air
                    case TSBKO::OSP_GRP_VCH_GRANT_UPD:
                    case TSBKO::OSP_UU_VCH_GRANT_UPD:
                        return true; // don't allow this to write to the air
                    case TSBKO::IOSP_UU_ANS:
                    {
                        IOSP_UU_ANS* iosp = static_cast<IOSP_UU_ANS*>(tsbk.get());
                        if (iosp->getResponse() > 0U) {
                            if (m_verbose) {
                                LogMessage(LOG_NET, P25_TSDU_STR ", %s, response = $%02X, srcId = %u, dstId = %u",
                                    tsbk->toString(true).c_str(), iosp->getResponse(), srcId, dstId);
                            }
                        }
                        else {
                            VERBOSE_LOG_TSBK_NET(tsbk->toString(), srcId, dstId);
                        }
                    }
                    break;
                    case TSBKO::IOSP_STS_UPDT:
                    {
                        // validate the source RID
                        VALID_SRCID_NET(tsbk->toString(), srcId);

                        IOSP_STS_UPDT* iosp = static_cast<IOSP_STS_UPDT*>(tsbk.get());
                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", %s, status = $%02X, srcId = %u",
                                tsbk->toString(true).c_str(), iosp->getStatus(), srcId);
                        }

                        ::ActivityLog("P25", false, "status update from %u", srcId);
                    }
                    break;
                    case TSBKO::IOSP_MSG_UPDT:
                    {
                        // validate the source RID
                        VALID_SRCID_NET(tsbk->toString(), srcId);

                        IOSP_MSG_UPDT* iosp = static_cast<IOSP_MSG_UPDT*>(tsbk.get());
                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", %s, message = $%02X, srcId = %u, dstId = %u",
                                tsbk->toString(true).c_str(), iosp->getMessage(), srcId, dstId);
                        }

                        ::ActivityLog("P25", false, "message update from %u", srcId);
                    }
                    break;
                    case TSBKO::IOSP_RAD_MON:
                    {
                        // validate the source RID
                        VALID_SRCID(tsbk->toString(true), TSBKO::IOSP_RAD_MON, srcId);

                        // validate the target RID
                        VALID_DSTID(tsbk->toString(true), TSBKO::IOSP_RAD_MON, srcId, dstId);

                        IOSP_RAD_MON* iosp = static_cast<IOSP_RAD_MON*>(tsbk.get());
                        VERBOSE_LOG_TSBK_NET(tsbk->toString(true), srcId, dstId);

                        ::ActivityLog("P25" , true , "radio monitor request from %u to %u" , srcId , dstId);

                        writeRF_TSDU_Radio_Mon(srcId , dstId , iosp->getTxMult());
                    }
                    break;
                    case TSBKO::IOSP_CALL_ALRT:
                    {
                        // validate the source RID
                        VALID_SRCID_NET(tsbk->toString(true), srcId);

                        // validate the target RID
                        VALID_DSTID_NET(tsbk->toString(true), dstId);

                        // validate source RID
                        if (!acl::AccessControl::validateSrcId(srcId)) {
                            LogWarning(LOG_NET, "DUID::TSDU (Trunking System Data Unit) denial, RID rejection, srcId = %u", srcId);
                            return false;
                        }

                        VERBOSE_LOG_TSBK_NET(tsbk->toString(true), srcId, dstId);
                        ::ActivityLog("P25", false, "call alert request from %u to %u", srcId, dstId);
                    }
                    break;
                    case TSBKO::IOSP_ACK_RSP:
                    {
                        // validate the source RID
                        VALID_SRCID_NET(tsbk->toString(true), srcId);

                        // validate the target RID
                        VALID_DSTID_NET(tsbk->toString(true), dstId);

                        IOSP_ACK_RSP* iosp = static_cast<IOSP_ACK_RSP*>(tsbk.get());
                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", %s, AIV = %u, serviceType = $%02X, srcId = %u, dstId = %u",
                                tsbk->toString(true).c_str(), iosp->getAIV(), iosp->getService(), dstId, srcId);
                        }

                        ::ActivityLog("P25", false, "ack response from %u to %u", srcId, dstId);
                    }
                    break;
                    case TSBKO::IOSP_EXT_FNCT:
                    {
                        // validate the target RID
                        VALID_DSTID_NET(tsbk->toString(true), dstId);

                        IOSP_EXT_FNCT* iosp = static_cast<IOSP_EXT_FNCT*>(tsbk.get());
                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", %s, serviceType = $%02X, arg = %u, tgt = %u",
                                tsbk->toString(true).c_str(), iosp->getService(), srcId, dstId);
                        }

                        // generate activity log entry
                        switch (iosp->getExtendedFunction()) {
                        // Standard
                        case ExtendedFunctions::CHECK_ACK:
                            ::ActivityLog("P25", false, "radio check response from %u to %u", srcId, dstId);
                            break;
                        case ExtendedFunctions::INHIBIT_ACK:
                            ::ActivityLog("P25", false, "radio inhibit response from %u to %u", srcId, dstId);
                            break;
                        case ExtendedFunctions::UNINHIBIT_ACK:
                            ::ActivityLog("P25", false, "radio uninhibit response from %u to %u", srcId, dstId);
                            break;
                        // Dynamic Regroup
                        case ExtendedFunctions::DYN_REGRP_REQ_ACK:
                            ::ActivityLog("P25", false, "radio dynamic regroup response from %u to TG%u", srcId, dstId);
                            break;
                        case ExtendedFunctions::DYN_REGRP_CANCEL_ACK:
                            ::ActivityLog("P25", false, "radio dynamic regroup cancel response from %u to TG%u", srcId, dstId);
                            break;
                        case ExtendedFunctions::DYN_REGRP_LOCK_ACK:
                            ::ActivityLog("P25", false, "radio dynamic regroup selector lock response from %u", srcId);
                            break;
                        case ExtendedFunctions::DYN_REGRP_UNLOCK_ACK:
                            ::ActivityLog("P25", false, "radio dynamic regroup selector unlock response from %u", srcId);
                            break;
                        }
                    }
                    break;
                    case TSBKO::ISP_EMERG_ALRM_REQ:
                    {
                        // non-emergency mode is a TSBKO::OSP_DENY_RSP
                        if (!tsbk->getEmergency()) {
                            break; // the FNE may explicitly send these
                        } else {
                            VERBOSE_LOG_TSBK_NET(tsbk->toString(true), srcId, dstId);
                            return true; // don't allow this to write to the air
                        }
                    }
                    break;
                    case TSBKO::IOSP_GRP_AFF:
                        // ignore a network group affiliation command
                        return true; // don't allow this to write to the air
                    case TSBKO::OSP_U_DEREG_ACK:
                        // ignore a network user deregistration command
                        return true; // don't allow this to write to the air
                    case TSBKO::OSP_LOC_REG_RSP:
                        // ignore a network location registration command
                        return true; // don't allow this to write to the air
                    case TSBKO::OSP_U_REG_CMD:
                        break; // the FNE may explicitly send these
                    case TSBKO::OSP_QUE_RSP:
                        break; // the FNE may explicitly send these
                    case TSBKO::OSP_SNDCP_CH_GNT:
                        return true; // don't allow this to write to the air
                    default:
                        LogError(LOG_NET, P25_TSDU_STR ", unhandled LCO, mfId = $%02X, lco = $%02X", tsbk->getMFId(), tsbk->getLCO());
                        return false;
                } // switch (tsbk->getLCO())

                writeNet_TSDU(tsbk.get());
            }
            break;
        default:
            return false;
    }

    return true;
}

/* Helper used to process AMBTs from PDU data. */

bool ControlSignaling::processMBT(DataHeader& dataHeader, DataBlock* blocks)
{
    if (!m_p25->m_enableControl) {
        return false;
    }

    uint8_t data[1U];
    ::memset(data, 0x00U, 1U);

    bool ret = false;

    std::unique_ptr<lc::AMBT> ambt = TSBKFactory::createAMBT(dataHeader, blocks);
    if (ambt != nullptr) {
        ret = process(data, 1U, std::move(ambt));
    }

    return ret;
}

/* Helper to write P25 adjacent site information to the network. */
void ControlSignaling::writeAdjSSNetwork()
{
    if (!m_p25->m_enableControl) {
        return;
    }

    if (m_p25->m_disableAdjSiteBroadcast) {
        return;
    }

    if (m_p25->m_network != nullptr) {
        uint8_t cfva = CFVA::VALID;
        if (m_p25->m_enableControl && !m_p25->m_dedicatedControl) {
            cfva |= CFVA::CONV;
        }

        // transmit adjacent site broadcast
        std::unique_ptr<OSP_ADJ_STS_BCAST> osp = std::make_unique<OSP_ADJ_STS_BCAST>();
        osp->setSrcId(WUID_FNE);
        osp->setAdjSiteCFVA(cfva);
        osp->setAdjSiteSysId(m_p25->m_siteData.sysId());
        osp->setAdjSiteRFSSId(m_p25->m_siteData.rfssId());
        osp->setAdjSiteId(m_p25->m_siteData.siteId());
        osp->setAdjSiteChnId(m_p25->m_siteData.channelId());
        osp->setAdjSiteChnNo(m_p25->m_siteData.channelNo());
        osp->setAdjSiteSvcClass(m_p25->m_siteData.serviceClass());

        if (m_verbose) {
            LogMessage(LOG_NET, P25_TSDU_STR ", %s, network announce, sysId = $%03X, rfss = $%02X, site = $%02X, chNo = %u-%u, svcClass = $%02X", osp->toString().c_str(),
                m_p25->m_siteData.sysId(), m_p25->m_siteData.rfssId(), m_p25->m_siteData.siteId(), m_p25->m_siteData.channelId(), m_p25->m_siteData.channelNo(), m_p25->m_siteData.serviceClass());
        }

        RF_TO_WRITE_NET(osp.get());
    }
}

/* Helper to write a call alert packet. */

void ControlSignaling::writeRF_TSDU_Call_Alrt(uint32_t srcId, uint32_t dstId)
{
    std::unique_ptr<IOSP_CALL_ALRT> iosp = std::make_unique<IOSP_CALL_ALRT>();
    iosp->setSrcId(srcId);
    iosp->setDstId(dstId);

    if (m_lastMFID != MFG_STANDARD) {
        iosp->setMFId(m_lastMFID);
        m_lastMFID = MFG_STANDARD;
    }

    VERBOSE_LOG_TSBK(iosp->toString(), srcId, dstId);
    ::ActivityLog("P25", true, "call alert request from %u to %u", srcId, dstId);

    writeRF_TSDU_SBF_Imm(iosp.get(), false);
}

/* Helper to write a radio monitor packet. */

void ControlSignaling::writeRF_TSDU_Radio_Mon(uint32_t srcId, uint32_t dstId, uint8_t txMult)
{
    std::unique_ptr<IOSP_RAD_MON> iosp = std::make_unique<IOSP_RAD_MON>();
    iosp->setSrcId(srcId);
    iosp->setDstId(dstId);
    iosp->setTxMult(txMult);

    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", %s, srcId = %u, dstId = %u, txMult = %u", iosp->toString().c_str(), srcId, dstId, txMult);
    }

    ::ActivityLog("P25", true, "Radio Unit Monitor request from %u to %u", srcId, dstId);

    writeRF_TSDU_SBF_Imm(iosp.get(), false);
}

/* Helper to write a extended function packet. */

void ControlSignaling::writeRF_TSDU_Ext_Func(uint32_t func, uint32_t arg, uint32_t dstId)
{
    std::unique_ptr<IOSP_EXT_FNCT> iosp = std::make_unique<IOSP_EXT_FNCT>();
    iosp->setExtendedFunction(func);
    iosp->setSrcId(arg);
    iosp->setDstId(dstId);

    if (m_lastMFID != MFG_STANDARD) {
        iosp->setMFId(m_lastMFID);
        m_lastMFID = MFG_STANDARD;
    }

    // class $02 is Motorola -- set the MFID properly
    if ((func >> 8) == 0x02U) {
        iosp->setMFId(MFG_MOT);
    }

    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", %s, mfId = $%02X, op = $%02X, arg = %u, tgt = %u",
            iosp->toString().c_str(), iosp->getMFId(), iosp->getExtendedFunction(), iosp->getSrcId(), iosp->getDstId());
    }

    // generate activity log entry
    switch (func) {
    // Standard
    case ExtendedFunctions::CHECK:
        ::ActivityLog("P25", true, "radio check request from %u to %u", arg, dstId);
        break;
    case ExtendedFunctions::INHIBIT:
        ::ActivityLog("P25", true, "radio inhibit request from %u to %u", arg, dstId);
        break;
    case ExtendedFunctions::UNINHIBIT:
        ::ActivityLog("P25", true, "radio uninhibit request from %u to %u", arg, dstId);
        break;
    // Dynamic Regroup
    case ExtendedFunctions::DYN_REGRP_REQ:
        ::ActivityLog("P25", true, "radio dynamic regroup request TG%u for %u", arg, dstId);
        break;
    case ExtendedFunctions::DYN_REGRP_CANCEL:
        ::ActivityLog("P25", true, "radio dynamic regroup cancel for %u", dstId);
        break;
    case ExtendedFunctions::DYN_REGRP_LOCK:
        ::ActivityLog("P25", true, "radio dynamic regroup selector lock for %u", dstId);
        break;
    case ExtendedFunctions::DYN_REGRP_UNLOCK:
        ::ActivityLog("P25", true, "radio dynamic regroup selector unlock for %u", dstId);
        break;
    }

    writeRF_TSDU_SBF_Imm(iosp.get(), true);
}

/* Helper to write a group affiliation query packet. */

void ControlSignaling::writeRF_TSDU_Grp_Aff_Q(uint32_t dstId)
{
    std::unique_ptr<OSP_GRP_AFF_Q> osp = std::make_unique<OSP_GRP_AFF_Q>();
    osp->setSrcId(WUID_FNE);
    osp->setDstId(dstId);

    if (m_lastMFID != MFG_STANDARD) {
        osp->setMFId(m_lastMFID);
        m_lastMFID = MFG_STANDARD;
    }

    VERBOSE_LOG_TSBK_DST(osp->toString(), dstId);
    ::ActivityLog("P25", true, "group affiliation query command from %u to %u", WUID_FNE, dstId);

    writeRF_TSDU_SBF_Imm(osp.get(), true);
}

/* Helper to write a unit registration command packet. */

void ControlSignaling::writeRF_TSDU_U_Reg_Cmd(uint32_t dstId)
{
    std::unique_ptr<OSP_U_REG_CMD> osp = std::make_unique<OSP_U_REG_CMD>();
    osp->setSrcId(WUID_FNE);
    osp->setDstId(dstId);

    if (m_lastMFID != MFG_STANDARD) {
        osp->setMFId(m_lastMFID);
        m_lastMFID = MFG_STANDARD;
    }

    VERBOSE_LOG_TSBK_DST(osp->toString(), dstId);
    ::ActivityLog("P25", true, "unit registration command from %u to %u", WUID_FNE, dstId);

    writeRF_TSDU_SBF_Imm(osp.get(), true);
}

/* Helper to write a emergency alarm packet. */

void ControlSignaling::writeRF_TSDU_Emerg_Alrm(uint32_t srcId, uint32_t dstId)
{
    std::unique_ptr<ISP_EMERG_ALRM_REQ> isp = std::make_unique<ISP_EMERG_ALRM_REQ>();
    isp->setSrcId(srcId);
    isp->setDstId(dstId);

    VERBOSE_LOG_TSBK(isp->toString(), srcId, dstId);
    writeRF_TSDU_SBF(isp.get(), true);
}

/* Helper to write a raw TSBK. */

void ControlSignaling::writeRF_TSDU_Raw(const uint8_t* tsbk)
{
    if (tsbk == nullptr) {
        return;
    }

    std::unique_ptr<OSP_TSBK_RAW> osp = std::make_unique<OSP_TSBK_RAW>();
    osp->setTSBK(tsbk);

    writeRF_TSDU_SBF(osp.get(), true);
}

/* Helper to change the conventional fallback state. */

void ControlSignaling::setConvFallback(bool fallback)
{
    m_convFallback = fallback;
    if (m_convFallback && m_p25->m_enableControl) {
        m_convFallbackPacketDelay = 0U;

        std::unique_ptr<OSP_MOT_PSH_CCH> osp = std::make_unique<OSP_MOT_PSH_CCH>();
        for (uint8_t i = 0U; i < 3U; i++) {
            writeRF_TSDU_SBF(osp.get(), true);
        }
    }
}

/* Helper to change the TSBK verbose state. */

void ControlSignaling::setTSBKVerbose(bool verbose)
{
    m_dumpTSBK = verbose;
    lc::TSBK::setVerbose(verbose);
    lc::TDULC::setVerbose(verbose);
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ControlSignaling class. */

ControlSignaling::ControlSignaling(Control* p25, bool dumpTSBKData, bool debug, bool verbose) :
    m_p25(p25),
    m_patchSuperGroup(0xFFFEU),
    m_announcementGroup(0xFFFEU),
    m_verifyAff(false),
    m_verifyReg(false),
    m_requireLLAForReg(false),
    m_rfMBF(nullptr),
    m_mbfCnt(0U),
    m_mbfIdenCnt(0U),
    m_mbfAdjSSCnt(0U),
    m_mbfSCCBCnt(0U),
    m_mbfGrpGrntCnt(0U),
    m_adjSiteTable(),
    m_adjSiteUpdateCnt(),
    m_sccbTable(),
    m_sccbUpdateCnt(),
    m_llaDemandTable(),
    m_lastMFID(MFG_STANDARD),
    m_noStatusAck(false),
    m_noMessageAck(true),
    m_unitToUnitAvailCheck(true),
    m_convFallbackPacketDelay(0U),
    m_convFallback(false),
    m_adjSiteUpdateTimer(1000U),
    m_adjSiteUpdateInterval(ADJ_SITE_TIMER_TIMEOUT),
    m_microslotCount(0U),
    m_ctrlTimeDateAnn(false),
    m_ctrlTSDUMBF(true),
    m_disableGrantSrcIdCheck(false),
    m_redundantImmediate(true),
    m_redundantGrant(false),
    m_inbound(false),
    m_dumpTSBK(dumpTSBKData),
    m_verbose(verbose),
    m_debug(debug)
{
    m_rfMBF = new uint8_t[P25_PDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(m_rfMBF, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

    m_adjSiteTable.clear();
    m_adjSiteUpdateCnt.clear();

    m_sccbTable.clear();
    m_sccbUpdateCnt.clear();

    m_llaDemandTable.clear();

    m_adjSiteUpdateInterval = ADJ_SITE_TIMER_TIMEOUT;
    m_adjSiteUpdateTimer.setTimeout(m_adjSiteUpdateInterval);
    m_adjSiteUpdateTimer.start();

    lc::TSBK::setVerbose(dumpTSBKData);
    lc::TDULC::setVerbose(dumpTSBKData);
}

/* Finalizes a instance of the ControlSignaling class. */

ControlSignaling::~ControlSignaling()
{
    delete[] m_rfMBF;
}

/* Write data processed from RF to the network. */

void ControlSignaling::writeNetworkRF(lc::TSBK* tsbk, const uint8_t* data, bool autoReset)
{
    assert(tsbk != nullptr);
    assert(data != nullptr);

    if (m_p25->m_network == nullptr)
        return;

    if (m_p25->m_rfTimeout.isRunning() && m_p25->m_rfTimeout.hasExpired())
        return;

    lc::LC lc = lc::LC();
    lc.setLCO(tsbk->getLCO());
    lc.setMFId(tsbk->getMFId());
    lc.setSrcId(tsbk->getSrcId());
    lc.setDstId(tsbk->getDstId());

    m_p25->m_network->writeP25TSDU(lc, data);
    if (autoReset)
        m_p25->m_network->resetP25();
}

/* Write data processed from RF to the network. */

void ControlSignaling::writeNetworkRF(lc::TDULC* tduLc, const uint8_t* data, bool autoReset)
{
    assert(data != nullptr);

    if (m_p25->m_network == nullptr)
        return;

    if (m_p25->m_rfTimeout.isRunning() && m_p25->m_rfTimeout.hasExpired())
        return;

    lc::LC lc = lc::LC();
    lc.setLCO(tduLc->getLCO());
    lc.setMFId(tduLc->getMFId());
    lc.setSrcId(tduLc->getSrcId());
    lc.setDstId(tduLc->getDstId());

    m_p25->m_network->writeP25TDULC(lc, data);
    if (autoReset)
        m_p25->m_network->resetP25();
}

/*
** Modem Frame Queuing
*/

/* Helper to write a P25 TDU w/ link control packet. */

void ControlSignaling::writeRF_TDULC(lc::TDULC* lc, bool noNetwork)
{
    uint8_t data[P25_TDULC_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, P25_TDULC_FRAME_LENGTH_BYTES);

    // generate Sync
    Sync::addP25Sync(data + 2U);

    // generate NID
    m_p25->m_nid.encode(data + 2U, DUID::TDULC);

    // generate TDULC Data
    lc->encode(data + 2U);

    // add status bits
    P25Utils::addStatusBits(data + 2U, P25_TDULC_FRAME_LENGTH_BITS, false, false);

    m_p25->m_rfTimeout.stop();

    if (!noNetwork)
        writeNetworkRF(lc, data + 2U, false);

    if (m_p25->m_duplex) {
        data[0U] = modem::TAG_EOT;
        data[1U] = 0x00U;

        m_p25->addFrame(data, P25_TDULC_FRAME_LENGTH_BYTES + 2U);
    }

    //if (m_verbose) {
    //    LogMessage(LOG_RF, P25_TDULC_STR ", lc = $%02X, srcId = %u", m_rfTDULC.getLCO(), m_rfTDULC.getSrcId());
    //}
}

/* Helper to write a network P25 TDU w/ link control packet. */

void ControlSignaling::writeNet_TDULC(lc::TDULC* lc)
{
    uint8_t buffer[P25_TDULC_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_TDULC_FRAME_LENGTH_BYTES + 2U);

    buffer[0U] = modem::TAG_EOT;
    buffer[1U] = 0x00U;

    // generate Sync
    Sync::addP25Sync(buffer + 2U);

    // generate NID
    m_p25->m_nid.encode(buffer + 2U, DUID::TDULC);

    // regenerate TDULC Data
    lc->encode(buffer + 2U);

    // add status bits
    P25Utils::addStatusBits(buffer + 2U, P25_TDULC_FRAME_LENGTH_BITS, false, false);

    m_p25->addFrame(buffer, P25_TDULC_FRAME_LENGTH_BYTES + 2U, true);

    if (m_verbose) {
        LogMessage(LOG_NET, P25_TDULC_STR ", lc = $%02X, srcId = %u", lc->getLCO(), lc->getSrcId());
    }

    if (m_p25->m_voice->m_netFrames > 0) {
        ::ActivityLog("P25", false, "network end of transmission, %.1f seconds, %u%% packet loss",
            float(m_p25->m_voice->m_netFrames) / 50.0F, (m_p25->m_voice->m_netLost * 100U) / m_p25->m_voice->m_netFrames);
    }
    else {
        ::ActivityLog("P25", false, "network end of transmission, %u frames", m_p25->m_voice->m_netFrames);
    }

    if (m_p25->m_network != nullptr)
        m_p25->m_network->resetP25();

    m_p25->m_netTimeout.stop();
    m_p25->m_networkWatchdog.stop();
    m_p25->m_netState = RS_NET_IDLE;
    m_p25->m_tailOnIdle = true;
}

/* Helper to write a single-block P25 TSDU packet. */

void ControlSignaling::writeRF_TSDU_SBF(lc::TSBK* tsbk, bool noNetwork, bool forceSingle, bool imm)
{
    if (!m_p25->m_enableControl)
        return;

    assert(tsbk != nullptr);

    uint8_t data[P25_TSDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, P25_TSDU_FRAME_LENGTH_BYTES);

    // generate Sync
    Sync::addP25Sync(data + 2U);

    // generate NID
    m_p25->m_nid.encode(data + 2U, DUID::TSDU);

    // generate TSBK block
    tsbk->setLastBlock(true); // always set last block -- this a Single Block TSDU
    tsbk->encode(data + 2U);

    if (m_debug) {
        LogDebug(LOG_RF, P25_TSDU_STR ", lco = $%02X, mfId = $%02X, lastBlock = %u, AIV = %u, EX = %u, srcId = %u, dstId = %u, sysId = $%03X, netId = $%05X",
            tsbk->getLCO(), tsbk->getMFId(), tsbk->getLastBlock(), tsbk->getAIV(), tsbk->getEX(), tsbk->getSrcId(), tsbk->getDstId(),
            tsbk->getSysId(), tsbk->getNetId());

        Utils::dump(1U, "!!! *TSDU (SBF) TSBK Block Data", data + P25_PREAMBLE_LENGTH_BYTES + 2U, P25_TSBK_FEC_LENGTH_BYTES);
    }

    // add status bits
    P25Utils::addStatusBits(data + 2U, P25_TSDU_FRAME_LENGTH_BITS, m_inbound, true);
    P25Utils::addIdleStatusBits(data + 2U, P25_TSDU_FRAME_LENGTH_BITS);
    P25Utils::setStatusBitsStartIdle(data + 2U);

    if (!noNetwork)
        writeNetworkRF(tsbk, data + 2U, true);

    // we always force any immediate TSDUs as single-block
    if (imm) {
        forceSingle = true;
    }

    if (!forceSingle) {
        if (m_p25->m_dedicatedControl && m_ctrlTSDUMBF) {
            writeRF_TSDU_MBF(tsbk);
            return;
        }

        if (m_p25->m_ccRunning && m_ctrlTSDUMBF) {
            writeRF_TSDU_MBF(tsbk);
            return;
        }
    }

    if (m_p25->m_duplex) {
        data[0U] = modem::TAG_DATA;
        data[1U] = 0x00U;

        m_p25->addFrame(data, P25_TSDU_FRAME_LENGTH_BYTES + 2U, false, imm);
        
        if (imm && m_redundantImmediate) {
            // queue an immediate frame at least twice
            m_p25->addFrame(data, P25_TSDU_FRAME_LENGTH_BYTES + 2U, false, imm);
        }
    }
}

/* Helper to write a network single-block P25 TSDU packet. */

void ControlSignaling::writeNet_TSDU(lc::TSBK* tsbk)
{
    assert(tsbk != nullptr);

    uint8_t buffer[P25_TSDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_TSDU_FRAME_LENGTH_BYTES + 2U);

    buffer[0U] = modem::TAG_DATA;
    buffer[1U] = 0x00U;

    // generate Sync
    Sync::addP25Sync(buffer + 2U);

    // generate NID
    m_p25->m_nid.encode(buffer + 2U, DUID::TSDU);

    // regenerate TSDU Data
    tsbk->setLastBlock(true); // always set last block -- this a Single Block TSDU
    tsbk->encode(buffer + 2U);

    // add status bits
    P25Utils::addStatusBits(buffer + 2U, P25_TSDU_FRAME_LENGTH_BYTES, false, true);
    P25Utils::addIdleStatusBits(buffer + 2U, P25_TSDU_FRAME_LENGTH_BYTES);
    P25Utils::setStatusBitsStartIdle(buffer + 2U);

    m_p25->addFrame(buffer, P25_TSDU_FRAME_LENGTH_BYTES + 2U, true);

    if (m_p25->m_network != nullptr)
        m_p25->m_network->resetP25();
}

/* Helper to write a multi-block (3-block) P25 TSDU packet. */

void ControlSignaling::writeRF_TSDU_MBF(lc::TSBK* tsbk)
{
    if (!m_p25->m_enableControl) {
        ::memset(m_rfMBF, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);
        m_mbfCnt = 0U;
        return;
    }

    assert(tsbk != nullptr);

    uint8_t frame[P25_TSBK_FEC_LENGTH_BYTES];
    ::memset(frame, 0x00U, P25_TSBK_FEC_LENGTH_BYTES);

    // LogDebug(LOG_P25, "writeRF_TSDU_MBF, mbfCnt = %u", m_mbfCnt);

    // trunking data is unsupported in simplex operation
    if (!m_p25->m_duplex) {
        ::memset(m_rfMBF, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);
        m_mbfCnt = 0U;
        return;
    }

    if (m_mbfCnt == 0U) {
        ::memset(m_rfMBF, 0x00U, P25_TSBK_FEC_LENGTH_BYTES * TSBK_MBF_CNT);
    }

    // trigger encoding of last block and write to queue
    if (m_mbfCnt + 1U == TSBK_MBF_CNT) {
        // generate TSBK block
        tsbk->setLastBlock(true); // set last block
        tsbk->encode(frame, true);

        if (m_debug) {
            LogDebug(LOG_RF, P25_TSDU_STR " (MBF), lco = $%02X, mfId = $%02X, lastBlock = %u, AIV = %u, EX = %u, srcId = %u, dstId = %u, sysId = $%03X, netId = $%05X",
                tsbk->getLCO(), tsbk->getMFId(), tsbk->getLastBlock(), tsbk->getAIV(), tsbk->getEX(), tsbk->getSrcId(), tsbk->getDstId(),
                tsbk->getSysId(), tsbk->getNetId());

            Utils::dump(1U, "!!! *TSDU MBF Last TSBK Block", frame, P25_TSBK_FEC_LENGTH_BYTES);
        }

        Utils::setBitRange(frame, m_rfMBF, (m_mbfCnt * P25_TSBK_FEC_LENGTH_BITS), P25_TSBK_FEC_LENGTH_BITS);

        // generate TSDU frame
        uint8_t tsdu[P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES];
        ::memset(tsdu, 0x00U, P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES);

        uint32_t offset = 0U;
        for (uint8_t i = 0U; i < m_mbfCnt + 1U; i++) {
            ::memset(frame, 0x00U, P25_TSBK_FEC_LENGTH_BYTES);
            Utils::getBitRange(m_rfMBF, frame, offset, P25_TSBK_FEC_LENGTH_BITS);

            if (m_debug) {
                LogDebug(LOG_RF, P25_TSDU_STR " (MBF), lco = $%02X, mfId = $%02X, lastBlock = %u, AIV = %u, EX = %u, srcId = %u, dstId = %u, sysId = $%03X, netId = $%05X",
                    tsbk->getLCO(), tsbk->getMFId(), tsbk->getLastBlock(), tsbk->getAIV(), tsbk->getEX(), tsbk->getSrcId(), tsbk->getDstId(),
                    tsbk->getSysId(), tsbk->getNetId());

                Utils::dump(1U, "!!! *TSDU (MBF) TSBK Block", frame, P25_TSBK_FEC_LENGTH_BYTES);
            }

            // add TSBK data
            Utils::setBitRange(frame, tsdu, offset, P25_TSBK_FEC_LENGTH_BITS);

            offset += P25_TSBK_FEC_LENGTH_BITS;
        }

        // Utils::dump(2U, "!!! *TSDU DEBUG - tsdu", tsdu, P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES);

        uint8_t data[P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES + 2U];
        ::memset(data + 2U, 0x00U, P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES);

        // generate Sync
        Sync::addP25Sync(data + 2U);

        // generate NID
        m_p25->m_nid.encode(data + 2U, DUID::TSDU);

        // interleave
        P25Utils::encode(tsdu, data + 2U, 114U, 720U);

        // add busy bits
        P25Utils::addStatusBits(data + 2U, P25_TSDU_TRIPLE_FRAME_LENGTH_BITS, m_inbound, true);
        P25Utils::addIdleStatusBits(data + 2U, P25_TSDU_TRIPLE_FRAME_LENGTH_BITS);

        data[0U] = modem::TAG_DATA;
        data[1U] = 0x00U;

        m_p25->addFrame(data, P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES + 2U);

        ::memset(m_rfMBF, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);
        m_mbfCnt = 0U;
        return;
    }

    // generate TSBK block
    tsbk->setLastBlock(false); // clear last block
    tsbk->encode(frame, true);

    if (m_debug) {
        LogDebug(LOG_RF, P25_TSDU_STR " (MBF), lco = $%02X, mfId = $%02X, lastBlock = %u, AIV = %u, EX = %u, srcId = %u, dstId = %u, sysId = $%03X, netId = $%05X",
            tsbk->getLCO(), tsbk->getMFId(), tsbk->getLastBlock(), tsbk->getAIV(), tsbk->getEX(), tsbk->getSrcId(), tsbk->getDstId(),
            tsbk->getSysId(), tsbk->getNetId());

        Utils::dump(1U, "!!! *TSDU MBF Block Data", frame, P25_TSBK_FEC_LENGTH_BYTES);
    }

    Utils::setBitRange(frame, m_rfMBF, (m_mbfCnt * P25_TSBK_FEC_LENGTH_BITS), P25_TSBK_FEC_LENGTH_BITS);
    m_mbfCnt++;
}

/* Helper to write a alternate multi-block trunking PDU packet. */

void ControlSignaling::writeRF_TSDU_AMBT(lc::AMBT* ambt, bool imm)
{
    if (!m_p25->m_enableControl)
        return;

    assert(ambt != nullptr);

    DataHeader header = DataHeader();
    uint8_t pduUserData[P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES];
    ::memset(pduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES);

    // Generate TSBK block
    ambt->setLastBlock(true); // always set last block -- this a Single Block TSDU
    ambt->encodeMBT(header, pduUserData);

    if (m_debug) {
        LogDebug(LOG_RF, P25_PDU_STR ", ack = %u, outbound = %u, fmt = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, n = %u, seqNo = %u, hdrOffset = %u",
            header.getAckNeeded(), header.getOutbound(), header.getFormat(), header.getSAP(), header.getFullMessage(),
            header.getBlocksToFollow(), header.getPadLength(), header.getNs(), header.getFSN(),
            header.getHeaderOffset());
        LogDebug(LOG_RF, P25_PDU_STR " AMBT, lco = $%02X, mfId = $%02X, lastBlock = %u, AIV = %u, EX = %u, srcId = %u, dstId = %u, sysId = $%03X, netId = $%05X",
            ambt->getLCO(), ambt->getMFId(), ambt->getLastBlock(), ambt->getAIV(), ambt->getEX(), ambt->getSrcId(), ambt->getDstId(),
            ambt->getSysId(), ambt->getNetId());

        Utils::dump(1U, "!!! *PDU (AMBT) TSBK Block Data", pduUserData, P25_PDU_UNCONFIRMED_LENGTH_BYTES * header.getBlocksToFollow());
    }

    m_p25->m_data->writeRF_PDU_User(header, false, pduUserData, imm);
}

/*
** Control Signalling Logic
*/

/* Helper to write a P25 TDU w/ link control channel release packet. */

void ControlSignaling::writeRF_TDULC_ChanRelease(bool grp, uint32_t srcId, uint32_t dstId)
{
    if (!m_p25->m_duplex) {
        return;
    }

    uint32_t count = m_p25->m_hangCount / 2;
    if (!m_p25->m_dedicatedControl || m_p25->m_voiceOnControl) {
        count = count / 2;
    }
    std::unique_ptr<lc::TDULC> lc = nullptr;

    if (m_p25->m_enableControl) {
        for (uint32_t i = 0; i < count; i++) {
            if ((srcId != 0U) && (dstId != 0U)) {
                if (grp) {
                    lc = std::make_unique<lc::tdulc::LC_GROUP>();
                } else {
                    lc = std::make_unique<lc::tdulc::LC_PRIVATE>();
                }

                lc->setSrcId(srcId);
                lc->setDstId(dstId);
                lc->setEmergency(false);

                writeRF_TDULC(lc.get(), true);
            }

            lc = std::make_unique<lc::tdulc::LC_NET_STS_BCAST>();
            writeRF_TDULC(lc.get(), true);
            lc = std::make_unique<lc::tdulc::LC_RFSS_STS_BCAST>();
            writeRF_TDULC(lc.get(), true);
        }
    }

    if (m_verbose) {
        LogMessage(LOG_RF, P25_TDULC_STR ", CALL_TERM (Call Termination), srcId = %u, dstId = %u", srcId, dstId);
    }

    lc = std::make_unique<lc::tdulc::LC_CALL_TERM>();
    lc->setDstId(dstId);
    writeRF_TDULC(lc.get(), true);

    if (m_p25->m_enableControl) {
        writeNet_TSDU_Call_Term(srcId, dstId);
    }
}

/* Helper to write control channel packet data. */

void ControlSignaling::writeRF_ControlData(uint8_t frameCnt, uint8_t n, bool adjSS)
{
    if (!m_p25->m_enableControl)
        return;

    // disable verbose tSBK dumping during control data writes (if necessary)
    bool tsbkVerbose = lc::TSBK::getVerbose();
    if (tsbkVerbose)
        lc::TSBK::setVerbose(false);

    // disable debug logging during control data writes (if necessary)
    bool controlDebug = m_p25->m_debug;
    if (!m_p25->m_ccDebug)
        m_p25->m_debug = m_debug = false;

    if (m_convFallback) {
        bool fallbackTx = (frameCnt % 253U) == 0U;
        if (fallbackTx && n == 8U) {
            if (m_convFallbackPacketDelay >= CONV_FALLBACK_PACKET_DELAY) {
                std::unique_ptr<lc::tdulc::LC_CONV_FALLBACK> lc = std::make_unique<lc::tdulc::LC_CONV_FALLBACK>();
                for (uint8_t i = 0U; i < 3U; i++) {
                    writeRF_TDULC(lc.get(), true);
                }

                m_convFallbackPacketDelay = 0U;
            } else {
                m_convFallbackPacketDelay++;
            }
        }
        else {
            if (n == 8U) {
                std::unique_ptr<lc::tdulc::LC_FAILSOFT> lc = std::make_unique<lc::tdulc::LC_FAILSOFT>();
                writeRF_TDULC(lc.get(), true);
            }
        }

        return;
    }

    if (m_debug) {
        LogDebugEx(LOG_P25, "ControlSignaling::writeRF_ControlData()", "mbfCnt = %u, frameCnt = %u, seq = %u, adjSS = %u", m_mbfCnt, frameCnt, n, adjSS);
    }

    // bryanb: this is just a simple counter because we treat the SYNC_BCST as unlocked
    m_microslotCount++;
    if (m_microslotCount > 7999U)
        m_microslotCount = 0;

    bool forcePad = false;
    bool alt = (frameCnt % 2U) > 0U;
    switch (n)
    {
    /** required data */
    case 0:
    default:
        queueRF_TSBK_Ctrl(TSBKO::OSP_IDEN_UP);
        break;
    case 1:
        if (alt)
            queueRF_TSBK_Ctrl(TSBKO::OSP_RFSS_STS_BCAST);
        else
            queueRF_TSBK_Ctrl(TSBKO::OSP_NET_STS_BCAST);
        break;
    case 2:
        if (alt)
            queueRF_TSBK_Ctrl(TSBKO::OSP_NET_STS_BCAST);
        else
            queueRF_TSBK_Ctrl(TSBKO::OSP_RFSS_STS_BCAST);
        break;
    case 3:
        if (alt)
            queueRF_TSBK_Ctrl(TSBKO::OSP_RFSS_STS_BCAST);
        else
            queueRF_TSBK_Ctrl(TSBKO::OSP_NET_STS_BCAST);
        break;
    case 4:
        queueRF_TSBK_Ctrl(TSBKO::OSP_SYNC_BCAST);
        break;
    /** update data */
    case 5:
        if (m_p25->m_affiliations->grantSize() > 0) {
            writeRF_TSDU_Grant_Update();
        }
        break;
    /** extra data */
    case 6:
        queueRF_TSBK_Ctrl(TSBKO::OSP_SNDCP_CH_ANN);
        break;
    case 7:
        // write ADJSS
        if (adjSS && m_adjSiteTable.size() > 0) {
            queueRF_TSBK_Ctrl(TSBKO::OSP_ADJ_STS_BCAST);
            break;
        } else {
            forcePad = true;
        }
        break;
    case 8:
        // write SCCB
        if (adjSS && m_sccbTable.size() > 0) {
            queueRF_TSBK_Ctrl(TSBKO::OSP_SCCB_EXP);
            break;
        }
        break;
    }

    // are we transmitting the time/date annoucement?
    bool timeDateAnn = (frameCnt % 64U) == 0U;
    if (m_ctrlTimeDateAnn && timeDateAnn && n > 4U) {
        queueRF_TSBK_Ctrl(TSBKO::OSP_TIME_DATE_ANN);
    }

    // should we insert the BSI bursts?
    bool bsi = (frameCnt % 127U) == 0U;
    if (bsi && n > 4U) {
        queueRF_TSBK_Ctrl(TSBKO::OSP_MOT_CC_BSI);
    }

    // should we insert the Git Hash burst?
    bool hash = (frameCnt % 125U) == 0U;
    if (hash && n > 4U) {
        queueRF_TSBK_Ctrl(TSBKO::OSP_DVM_GIT_HASH);
    }

    // add padding after the last sequence or if forced; and only
    // if we're doing multiblock frames (MBF)
    if ((n >= 4U || forcePad) && m_ctrlTSDUMBF)
    {
        // pad MBF if we have 1 queued TSDUs
        if (m_mbfCnt == 1U) {
            queueRF_TSBK_Ctrl(TSBKO::OSP_RFSS_STS_BCAST);
            queueRF_TSBK_Ctrl(TSBKO::OSP_NET_STS_BCAST);
            if (m_debug) {
                LogDebugEx(LOG_P25, "ControlSignaling::writeRF_ControlData()]", "have 1 pad 2, mbfCnt = %u", m_mbfCnt);
            }
        }

        // pad MBF if we have 2 queued TSDUs
        if (m_mbfCnt == 2U) {
            std::vector<::lookups::IdenTable> entries = m_p25->m_idenTable->list();
            if (entries.size() > 1U) {
                queueRF_TSBK_Ctrl(TSBKO::OSP_IDEN_UP);
            }
            else {
                queueRF_TSBK_Ctrl(TSBKO::OSP_RFSS_STS_BCAST);
            }

            if (m_debug) {
                LogDebugEx(LOG_P25, "ControlSignaling::writeRF_ControlData()", "have 2 pad 1, mbfCnt = %u", m_mbfCnt);
            }
        }

        // reset MBF count
        m_mbfCnt = 0U;
    }

    lc::TSBK::setVerbose(tsbkVerbose);
    m_p25->m_debug = m_debug = controlDebug;
}

/* Helper to generate the given control TSBK into the TSDU frame queue. */

void ControlSignaling::queueRF_TSBK_Ctrl(uint8_t lco)
{
    if (!m_p25->m_enableControl)
        return;

    std::unique_ptr<lc::TSBK> tsbk;

    switch (lco) {
        case TSBKO::OSP_IDEN_UP:
            {
                std::vector<::lookups::IdenTable> entries = m_p25->m_idenTable->list();
                if (m_mbfIdenCnt >= entries.size())
                    m_mbfIdenCnt = 0U;

                uint8_t i = 0U;
                for (auto entry : entries) {
                    // no good very bad way of skipping entries...
                    if (i != m_mbfIdenCnt) {
                        i++;
                        continue;
                    }
                    else {
                        // LogDebug(LOG_P25, "baseFrequency = %uHz, txOffsetMhz = %fMHz, chBandwidthKhz = %fKHz, chSpaceKhz = %fKHz",
                        //    entry.baseFrequency(), entry.txOffsetMhz(), entry.chBandwidthKhz(), entry.chSpaceKhz());

                        // handle 700/800/900 identities
                        if (entry.baseFrequency() >= 762000000U) {
                            std::unique_ptr<OSP_IDEN_UP> osp = std::make_unique<OSP_IDEN_UP>();
                            DEBUG_LOG_TSBK(osp->toString());
                            osp->siteIdenEntry(entry);

                            // transmit channel ident broadcast
                            tsbk = std::move(osp);
                        }
                        else {
                            std::unique_ptr<OSP_IDEN_UP_VU> osp = std::make_unique<OSP_IDEN_UP_VU>();
                            DEBUG_LOG_TSBK(osp->toString());
                            osp->siteIdenEntry(entry);

                            // transmit channel ident broadcast
                            tsbk = std::move(osp);
                        }

                        m_mbfIdenCnt++;
                        break;
                    }
                }
            }
            break;
        case TSBKO::OSP_NET_STS_BCAST:
            // transmit net status burst
            tsbk = std::make_unique<OSP_NET_STS_BCAST>();
            DEBUG_LOG_TSBK(tsbk->toString());
            break;
        case TSBKO::OSP_RFSS_STS_BCAST:
            // transmit rfss status burst
            tsbk = std::make_unique<OSP_RFSS_STS_BCAST>();
            DEBUG_LOG_TSBK(tsbk->toString());
            break;
        case TSBKO::OSP_ADJ_STS_BCAST:
            // write ADJSS
            if (m_adjSiteTable.size() > 0) {
                if (m_mbfAdjSSCnt >= m_adjSiteTable.size())
                    m_mbfAdjSSCnt = 0U;

                std::unique_ptr<OSP_ADJ_STS_BCAST> osp = std::make_unique<OSP_ADJ_STS_BCAST>();
                DEBUG_LOG_TSBK(osp->toString());

                uint8_t i = 0U;
                for (auto entry : m_adjSiteTable) {
                    // no good very bad way of skipping entries...
                    if (i != m_mbfAdjSSCnt) {
                        i++;
                        continue;
                    }
                    else {
                        SiteData site = entry.second;

                        // this should never happen -- but prevent announcing ourselves as a neighbor
                        if (site.channelId() == m_p25->m_siteData.channelId() && site.channelNo() == m_p25->m_siteData.channelNo() &&
                            site.siteId() == m_p25->m_siteData.siteId() && site.sysId() == m_p25->m_siteData.sysId())
                            continue;

                        uint8_t cfva = CFVA::NETWORK;
                        if (m_adjSiteUpdateCnt[site.siteId()] == 0U) {
                            cfva |= CFVA::FAILURE;
                        }
                        else {
                            cfva |= CFVA::VALID;
                        }

                        // transmit adjacent site broadcast
                        osp->setAdjSiteCFVA(cfva);
                        osp->setAdjSiteSysId(site.sysId());
                        osp->setAdjSiteRFSSId(site.rfssId());
                        osp->setAdjSiteId(site.siteId());
                        osp->setAdjSiteChnId(site.channelId());
                        osp->setAdjSiteChnNo(site.channelNo());
                        osp->setAdjSiteSvcClass(site.serviceClass());

                        tsbk = std::move(osp);
                        m_mbfAdjSSCnt++;
                        break;
                    }
                }
            }
            else {
                return; // don't create anything
            }
            break;
        case TSBKO::OSP_SCCB_EXP:
            // write SCCB
            if (m_sccbTable.size() > 0) {
                if (m_mbfSCCBCnt >= m_sccbTable.size())
                    m_mbfSCCBCnt = 0U;

                std::unique_ptr<OSP_SCCB_EXP> osp = std::make_unique<OSP_SCCB_EXP>();
                DEBUG_LOG_TSBK(osp->toString());

                uint8_t i = 0U;
                for (auto entry : m_sccbTable) {
                    // no good very bad way of skipping entries...
                    if (i != m_mbfSCCBCnt) {
                        i++;
                        continue;
                    }
                    else {
                        SiteData site = entry.second;

                        // transmit SCCB broadcast
                        osp->setLCO(TSBKO::OSP_SCCB_EXP);
                        osp->setSCCBChnId1(site.channelId());
                        osp->setSCCBChnNo(site.channelNo());

                        tsbk = std::move(osp);
                        m_mbfSCCBCnt++;
                        break;
                    }
                }
            }
            else {
                return; // don't create anything
            }
            break;
        case TSBKO::OSP_SNDCP_CH_ANN:
        {
            // transmit SNDCP announcement
            std::unique_ptr<OSP_SNDCP_CH_ANN> osp = std::make_unique<OSP_SNDCP_CH_ANN>();
            osp->siteIdenEntry(m_p25->m_idenEntry);
            if (!m_p25->m_sndcpSupport) {
                osp->setImplicitChannel(true);
            }
            tsbk = std::move(osp);
            DEBUG_LOG_TSBK(tsbk->toString());
        }
        break;
        case TSBKO::OSP_SYNC_BCAST:
        {
            // transmit sync broadcast
            std::unique_ptr<OSP_SYNC_BCAST> osp = std::make_unique<OSP_SYNC_BCAST>();
            DEBUG_LOG_TSBK(osp->toString());
            osp->setMicroslotCount(m_microslotCount);
            tsbk = std::move(osp);
        }
        break;
        case TSBKO::OSP_TIME_DATE_ANN:
        {
            if (m_ctrlTimeDateAnn) {
                // transmit time/date announcement
                tsbk = std::make_unique<OSP_TIME_DATE_ANN>();
                DEBUG_LOG_TSBK(tsbk->toString());
            }
        }
        break;

        /** Motorola CC data */
        case TSBKO::OSP_MOT_PSH_CCH:
            // transmit motorola PSH CCH burst
            tsbk = std::make_unique<OSP_MOT_PSH_CCH>();
            DEBUG_LOG_TSBK(tsbk->toString());
            break;

        case TSBKO::OSP_MOT_CC_BSI:
            // transmit motorola CC BSI burst
            tsbk = std::make_unique<OSP_MOT_CC_BSI>();
            DEBUG_LOG_TSBK(tsbk->toString());
            break;

        /** DVM CC data */
        case TSBKO::OSP_DVM_GIT_HASH:
            // transmit git hash burst
            tsbk = std::make_unique<OSP_DVM_GIT_HASH>();
            DEBUG_LOG_TSBK(tsbk->toString());
            break;
    }

    if (tsbk != nullptr) {
        tsbk->setLastBlock(true); // always set last block

        // are we transmitting CC as a multi-block?
        if (m_ctrlTSDUMBF) {
            writeRF_TSDU_MBF(tsbk.get());
        }
        else {
            writeRF_TSDU_SBF(tsbk.get(), true);
        }
    }
}

/* Helper to write a grant packet. */

bool ControlSignaling::writeRF_TSDU_Grant(uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp, bool net, bool skip, uint32_t chNo)
{
    bool emergency = ((serviceOptions & 0xFFU) & 0x80U) == 0x80U;           // Emergency Flag
    bool encryption = ((serviceOptions & 0xFFU) & 0x40U) == 0x40U;          // Encryption Flag
    uint8_t priority = ((serviceOptions & 0xFFU) & 0x07U);                  // Priority

    if (dstId == TGID_ALL) {
        return true; // do not generate grant packets for $FFFF (All Call) TGID
    }

    // are network channel grants disabled?
    if (m_p25->m_disableNetworkGrant) {
        // don't process RF grant if the network isn't in a idle state and the RF destination is the network destination
        if (m_p25->m_netState != RS_NET_IDLE && dstId == m_p25->m_netLastDstId) {
            LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing network traffic!");
            LogWarning(LOG_RF, P25_TSDU_STR ", TSBKO, IOSP_GRP_VCH (Group Voice Channel Request) denied, traffic collision, dstId = %u", dstId);
            writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_PTT_COLLIDE, (grp) ? TSBKO::IOSP_GRP_VCH : TSBKO::IOSP_UU_VCH, grp, true);

            ::ActivityLog("P25", true, "group grant request from %u to TG %u denied", srcId, dstId);
            m_p25->m_rfState = RS_RF_REJECTED;
            return false;
        }

        // ensure network watchdog is stopped
        if (m_p25->m_networkWatchdog.isRunning()) {
            m_p25->m_networkWatchdog.stop();
        }
    }

    // are we skipping checking?
    if (!skip) {
        if (m_p25->m_rfState != RS_RF_LISTENING && m_p25->m_rfState != RS_RF_DATA) {
            if (!net) {
                LogWarning(LOG_RF, P25_TSDU_STR ", TSBKO, IOSP_GRP_VCH (Group Voice Channel Request) denied, traffic in progress, dstId = %u", dstId);
                writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_PTT_COLLIDE, (grp) ? TSBKO::IOSP_GRP_VCH : TSBKO::IOSP_UU_VCH, grp, true);

                ::ActivityLog("P25", true, "group grant request from %u to TG %u denied", srcId, dstId);
                m_p25->m_rfState = RS_RF_REJECTED;
            }
            else {
                LogWarning(LOG_NET, P25_TSDU_STR ", TSBKO, IOSP_GRP_VCH (Group Voice Channel Request) denied, traffic in progress, dstId = %u", dstId);
            }

            return false;
        }

        // only do the last destination ID checking if we're operating in non-dedicated mode (e.g. DVRS)
        if (!m_p25->m_dedicatedControl) {
            if (m_p25->m_netState != RS_NET_IDLE && dstId == m_p25->m_netLastDstId) {
                if (!net) {
                    LogWarning(LOG_RF, P25_TSDU_STR ", TSBKO, IOSP_GRP_VCH (Group Voice Channel Request) denied, traffic in progress, dstId = %u", dstId);
                    writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_PTT_COLLIDE, (grp) ? TSBKO::IOSP_GRP_VCH : TSBKO::IOSP_UU_VCH, grp, true);

                    ::ActivityLog("P25", true, "group grant request from %u to TG %u denied", srcId, dstId);
                    m_p25->m_rfState = RS_RF_REJECTED;
                }
                else {
                    LogWarning(LOG_NET, P25_TSDU_STR ", TSBKO, IOSP_GRP_VCH (Group Voice Channel Request) denied, traffic in progress, dstId = %u", dstId);
                }

                return false;
            }

            // don't transmit grants if the destination ID's don't match and the network TG hang timer is running
            if (m_p25->m_rfLastDstId != 0U) {
                if (m_p25->m_rfLastDstId != dstId && (m_p25->m_rfTGHang.isRunning() && !m_p25->m_rfTGHang.hasExpired())) {
                    if (!net) {
                        writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_PTT_BONK, (grp) ? TSBKO::IOSP_GRP_VCH : TSBKO::IOSP_UU_VCH, grp, true);
                        m_p25->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
            }
        }

        if (!m_p25->m_affiliations->isGranted(dstId)) {
            if (grp && !m_p25->m_ignoreAffiliationCheck) {
                // is this an affiliation required group?
                ::lookups::TalkgroupRuleGroupVoice tid = m_p25->m_tidLookup->find(dstId);
                if (tid.config().affiliated()) {
                    if (!m_p25->m_affiliations->hasGroupAff(dstId)) {
                        LogWarning(LOG_NET, P25_TSDU_STR ", TSBKO, IOSP_GRP_VCH (Group Voice Channel Request) ignored, no group affiliations, dstId = %u", dstId);
                        return false;
                    }
                }
            }

            if (!grp && !m_p25->m_ignoreAffiliationCheck) {
                // is this the target registered?
                if (!m_p25->m_affiliations->isUnitReg(dstId)) {
                    LogWarning(LOG_NET, P25_TSDU_STR ", TSBKO, IOSP_UU_VCH (Unit-to-Unit Voice Channel Request) ignored, no unit registration, dstId = %u", dstId);
                    return false;
                }
            }

            if (!m_p25->m_affiliations->rfCh()->isRFChAvailable()) {
                if (grp) {
                    if (!net) {
                        LogWarning(LOG_RF, P25_TSDU_STR ", TSBKO, IOSP_GRP_VCH (Group Voice Channel Request) denied, no channels available, dstId = %u", dstId);
                        writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_NO_RF_RSRC_AVAIL, TSBKO::IOSP_GRP_VCH, grp, true);

                        ::ActivityLog("P25", true, "group grant request from %u to TG %u denied", srcId, dstId);
                        m_p25->m_rfState = RS_RF_REJECTED;
                    }
                    else {
                        LogWarning(LOG_NET, P25_TSDU_STR ", TSBKO, IOSP_GRP_VCH (Group Voice Channel Request) denied, no channels available, dstId = %u", dstId);
                    }

                    return false;
                }
                else {
                    if (!net) {
                        LogWarning(LOG_RF, P25_TSDU_STR ", TSBKO, IOSP_UU_VCH (Unit-to-Unit Voice Channel Request) denied, no channels available, dstId = %u", dstId);
                        writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_NO_RF_RSRC_AVAIL, TSBKO::IOSP_GRP_VCH, grp, true);

                        ::ActivityLog("P25", true, "unit-to-unit grant request from %u to %u denied", srcId, dstId);
                        m_p25->m_rfState = RS_RF_REJECTED;
                    }
                    else {
                        LogWarning(LOG_NET, P25_TSDU_STR ", TSBKO, IOSP_UU_VCH (Unit-to-Unit Voice Channel Request) denied, no channels available, dstId = %u", dstId);
                    }

                    return false;
                }
            }
            else {
                if (m_p25->m_affiliations->grantCh(dstId, srcId, GRANT_TIMER_TIMEOUT, grp, net)) {
                    chNo = m_p25->m_affiliations->getGrantedCh(dstId);
                    m_p25->m_siteData.setChCnt(m_p25->m_affiliations->rfCh()->rfChSize() + m_p25->m_affiliations->getGrantedRFChCnt());
                }
            }
        }
        else {
            if (!m_disableGrantSrcIdCheck && !net) {
                // do collision check between grants to see if a SU is attempting a "grant retry" or if this is a
                // different source from the original grant
                uint32_t grantedSrcId = m_p25->m_affiliations->getGrantedSrcId(dstId);
                if (srcId != grantedSrcId) {
                    if (!net) {
                        LogWarning(LOG_RF, P25_TSDU_STR ", TSBKO, IOSP_GRP_VCH (Group Voice Channel Request) denied, traffic collision, dstId = %u", dstId);
                        writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_PTT_COLLIDE, (grp) ? TSBKO::IOSP_GRP_VCH : TSBKO::IOSP_UU_VCH, grp, true);

                        ::ActivityLog("P25", true, "group grant request from %u to TG %u denied", srcId, dstId);
                        m_p25->m_rfState = RS_RF_REJECTED;
                    }
                    else {
                        LogWarning(LOG_NET, P25_TSDU_STR ", TSBKO, IOSP_GRP_VCH (Group Voice Channel Request) denied, traffic collision, dstId = %u", dstId);
                    }

                    return false;
                }
            }

            chNo = m_p25->m_affiliations->getGrantedCh(dstId);
            m_p25->m_affiliations->touchGrant(dstId);
        }
    }
    else {
        if (m_p25->m_affiliations->isGranted(dstId)) {
            chNo = m_p25->m_affiliations->getGrantedCh(dstId);
            m_p25->m_affiliations->touchGrant(dstId);
        }
        else {
            return false;
        }
    }

    if (chNo > 0U) {
        ::lookups::VoiceChData voiceChData = m_p25->m_affiliations->rfCh()->getRFChData(chNo);

        if (grp) {
            // callback RPC to permit the granted TG on the specified voice channel
            if (m_p25->m_authoritative && m_p25->m_supervisor) {
                if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0 &&
                    chNo != m_p25->m_siteData.channelNo()) {
                    json::object req = json::object();
                    req["dstId"].set<uint32_t>(dstId);

                    // send blocking RPC request
                    bool requestFailed = !g_RPC->req(RPC_PERMIT_P25_TG, req, [=, &requestFailed](json::object& req, json::object& reply) {
                        if (!req["status"].is<int>()) {
                            return;
                        }

                        int status = req["status"].get<int>();
                        if (status != network::NetRPC::OK) {
                            if (req["message"].is<std::string>()) {
                                std::string retMsg = req["message"].get<std::string>();
                                ::LogError((net) ? LOG_NET : LOG_RF, "P25, RPC failed, %s", retMsg.c_str());
                            }
                            requestFailed = true;
                        } else {
                            requestFailed = false;
                        }
                    }, voiceChData.address(), voiceChData.port(), true);

                    // if the request failed block grant
                    if (requestFailed) {
                        ::LogError((net) ? LOG_NET : LOG_RF, P25_TSDU_STR ", TSBKO, IOSP_GRP_VCH (Group Voice Channel Request), failed to permit TG for use, chNo = %u-%u", voiceChData.chId(), chNo);

                        m_p25->m_affiliations->releaseGrant(dstId, false);
                        if (!net) {
                            writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_PTT_BONK, (grp) ? TSBKO::IOSP_GRP_VCH : TSBKO::IOSP_UU_VCH, grp, true);
                            m_p25->m_rfState = RS_RF_REJECTED;
                        }

                        return false;
                    }
                }
                else {
                    ::LogError((net) ? LOG_NET : LOG_RF, P25_TSDU_STR ", TSBKO, IOSP_GRP_VCH (Group Voice Channel Request), failed to permit TG for use, chNo = %u-%u", voiceChData.chId(), chNo);
                }
            }

            if (!net) {
                ::ActivityLog("P25", true, "group grant request from %u to TG %u", srcId, dstId);
            }

            if (voiceChData.isExplicitCh()) {
                std::unique_ptr<MBT_OSP_GRP_VCH_GRANT> osp = std::make_unique<MBT_OSP_GRP_VCH_GRANT>();
                osp->setMFId(m_lastMFID);
                osp->setSrcId(srcId);
                osp->setDstId(dstId);
                osp->setGrpVchId(voiceChData.chId());
                osp->setGrpVchNo(chNo);
                osp->setRxGrpVchId(voiceChData.rxChId());
                osp->setRxGrpVchNo(voiceChData.rxChNo());
                osp->setEmergency(emergency);
                osp->setEncrypted(encryption);
                osp->setPriority(priority);

                osp->setForceChannelId(true);

                if (m_verbose) {
                    LogMessage((net) ? LOG_NET : LOG_RF, P25_TSDU_STR ", %s, emerg = %u, encrypt = %u, prio = %u, chNo = %u-%u, srcId = %u, dstId = %u",
                        osp->toString().c_str(), osp->getEmergency(), osp->getEncrypted(), osp->getPriority(), osp->getGrpVchId(), osp->getGrpVchNo(), osp->getSrcId(), osp->getDstId());
                }

                // transmit group grant
                writeRF_TSDU_AMBT(osp.get(), true);
                if (m_redundantGrant) {
                    for (int i = 0; i < 3; i++)
                        writeRF_TSDU_AMBT(osp.get(), true);
                }
            }

            std::unique_ptr<IOSP_GRP_VCH> iosp = std::make_unique<IOSP_GRP_VCH>();
            iosp->setMFId(m_lastMFID);
            iosp->setSrcId(srcId);
            iosp->setDstId(dstId);
            iosp->setGrpVchId(voiceChData.chId());
            iosp->setGrpVchNo(chNo);
            iosp->setEmergency(emergency);
            iosp->setEncrypted(encryption);
            iosp->setPriority(priority);

            if (!voiceChData.isExplicitCh()) {
                if (m_verbose) {
                    LogMessage((net) ? LOG_NET : LOG_RF, P25_TSDU_STR ", %s, emerg = %u, encrypt = %u, prio = %u, chNo = %u-%u, srcId = %u, dstId = %u",
                        iosp->toString().c_str(), iosp->getEmergency(), iosp->getEncrypted(), iosp->getPriority(), iosp->getGrpVchId(), iosp->getGrpVchNo(), iosp->getSrcId(), iosp->getDstId());
                }

                // transmit group grant
                writeRF_TSDU_SBF_Imm(iosp.get(), net);
                if (m_redundantGrant) {
                    for (int i = 0; i < 3; i++)
                        writeRF_TSDU_SBF(iosp.get(), net);
                }
            } else {
                if (!net) {
                    writeNet_TSDU(iosp.get());
                }
            }
        }
        else {
            // callback RPC to permit the granted TG on the specified voice channel
            if (m_p25->m_authoritative && m_p25->m_supervisor) {
                if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0 &&
                    chNo != m_p25->m_siteData.channelNo()) {
                    json::object req = json::object();
                    req["dstId"].set<uint32_t>(dstId);

                    // send blocking RPC request
                    bool requestFailed = !g_RPC->req(RPC_PERMIT_P25_TG, req, [=, &requestFailed](json::object& req, json::object& reply) {
                        if (!req["status"].is<int>()) {
                            return;
                        }

                        int status = req["status"].get<int>();
                        if (status != network::NetRPC::OK) {
                            if (req["message"].is<std::string>()) {
                                std::string retMsg = req["message"].get<std::string>();
                                ::LogError((net) ? LOG_NET : LOG_RF, "P25, RPC failed, %s", retMsg.c_str());
                            }
                            requestFailed = true;
                        } else {
                            requestFailed = false;
                        }
                    }, voiceChData.address(), voiceChData.port(), true);

                    // if the request failed block grant
                    if (requestFailed) {
                        ::LogError((net) ? LOG_NET : LOG_RF, P25_TSDU_STR ", TSBKO, IOSP_UU_VCH (Unit-to-Unit Voice Channel Request), failed to permit TG for use, chNo = %u-%u", voiceChData.chId(), chNo);

                        m_p25->m_affiliations->releaseGrant(dstId, false);
                        if (!net) {
                            writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_PTT_BONK, (grp) ? TSBKO::IOSP_GRP_VCH : TSBKO::IOSP_UU_VCH, grp, true);
                            m_p25->m_rfState = RS_RF_REJECTED;
                        }

                        return false;
                    }
                }
                else {
                    ::LogError((net) ? LOG_NET : LOG_RF, P25_TSDU_STR ", TSBKO, IOSP_UU_VCH (Unit-to-Unit Voice Channel Request), failed to permit TG for use, chNo = %u-%u", voiceChData.chId(), chNo);
                }
            }

            if (!net) {
                ::ActivityLog("P25", true, "unit-to-unit grant request from %u to %u", srcId, dstId);
            }

            if (voiceChData.isExplicitCh()) {
                std::unique_ptr<MBT_OSP_UU_VCH_GRANT> osp = std::make_unique<MBT_OSP_UU_VCH_GRANT>();
                osp->setMFId(m_lastMFID);
                osp->setSrcId(srcId);
                osp->setDstId(dstId);
                osp->setGrpVchId(voiceChData.chId());
                osp->setGrpVchNo(chNo);
                osp->setRxGrpVchId(voiceChData.rxChId());
                osp->setRxGrpVchNo(voiceChData.rxChNo());
                osp->setEmergency(emergency);
                osp->setEncrypted(encryption);
                osp->setPriority(priority);

                osp->setForceChannelId(true);

                if (m_verbose) {
                    LogMessage((net) ? LOG_NET : LOG_RF, P25_TSDU_STR ", %s, emerg = %u, encrypt = %u, prio = %u, chNo = %u-%u, srcId = %u, dstId = %u",
                        osp->toString().c_str(), osp->getEmergency(), osp->getEncrypted(), osp->getPriority(), osp->getGrpVchId(), osp->getGrpVchNo(), osp->getSrcId(), osp->getDstId());
                }

                // transmit private grant
                writeRF_TSDU_AMBT(osp.get(), true);
                if (m_redundantGrant) {
                    for (int i = 0; i < 3; i++)
                        writeRF_TSDU_AMBT(osp.get(), true);
                }
            }

            std::unique_ptr<IOSP_UU_VCH> iosp = std::make_unique<IOSP_UU_VCH>();
            iosp->setMFId(m_lastMFID);
            iosp->setSrcId(srcId);
            iosp->setDstId(dstId);
            iosp->setGrpVchId(voiceChData.chId());
            iosp->setGrpVchNo(chNo);
            iosp->setEmergency(emergency);
            iosp->setEncrypted(encryption);
            iosp->setPriority(priority);

            if (!voiceChData.isExplicitCh()) {
                if (m_verbose) {
                    LogMessage((net) ? LOG_NET : LOG_RF, P25_TSDU_STR ", %s, emerg = %u, encrypt = %u, prio = %u, chNo = %u-%u, srcId = %u, dstId = %u",
                        iosp->toString().c_str(), iosp->getEmergency(), iosp->getEncrypted(), iosp->getPriority(), iosp->getGrpVchId(), iosp->getGrpVchNo(), iosp->getSrcId(), iosp->getDstId());
                }

                // transmit private grant
                writeRF_TSDU_SBF_Imm(iosp.get(), net);
                if (m_redundantGrant) {
                    for (int i = 0; i < 3; i++)
                        writeRF_TSDU_SBF(iosp.get(), net);
                }
            } else {
                if (!net) {
                    writeNet_TSDU(iosp.get());
                }
            }
        }
    }

    return true;
}

/* Helper to write a grant update packet. */

void ControlSignaling::writeRF_TSDU_Grant_Update()
{
    if (m_p25->m_voiceOnControl)
        return;

    // write group voice grant update
    if (m_p25->m_affiliations->grantSize() > 0) {
        if (m_mbfGrpGrntCnt >= m_p25->m_affiliations->grantSize())
            m_mbfGrpGrntCnt = 0U;

        std::unique_ptr<lc::TSBK> osp;

        bool noData = false;
        uint8_t i = 0U;
        std::unordered_map<uint32_t, uint32_t> grantTable = m_p25->m_affiliations->grantTable();
        for (auto entry : grantTable) {
            // no good very bad way of skipping entries...
            if (i != m_mbfGrpGrntCnt) {
                i++;
                continue;
            }
            else {
                uint32_t dstId = entry.first;
                uint32_t chNo = entry.second;
                bool grp = m_p25->m_affiliations->isGroup(dstId);

                ::lookups::VoiceChData voiceChData = m_p25->m_affiliations->rfCh()->getRFChData(chNo);

                if (chNo == 0U) {
                    noData = true;
                    m_mbfGrpGrntCnt++;
                    break;
                }
                else {
                    if (grp) {
                        osp = std::make_unique<OSP_GRP_VCH_GRANT_UPD>();
                        DEBUG_LOG_TSBK(osp->toString());

                        // transmit group voice grant update
                        osp->setLCO(TSBKO::OSP_GRP_VCH_GRANT_UPD);
                        osp->setDstId(dstId);
                        osp->setGrpVchId(voiceChData.chId());
                        osp->setGrpVchNo(chNo);

                        m_mbfGrpGrntCnt++;
                        break;
                    } else {
                        uint32_t srcId = m_p25->m_affiliations->getGrantedSrcId(dstId);

                        osp = std::make_unique<OSP_UU_VCH_GRANT_UPD>();
                        DEBUG_LOG_TSBK(osp->toString());

                        // transmit group voice grant update
                        osp->setLCO(TSBKO::OSP_UU_VCH_GRANT_UPD);
                        osp->setSrcId(srcId);
                        osp->setDstId(dstId);
                        osp->setGrpVchId(voiceChData.chId());
                        osp->setGrpVchNo(chNo);

                        m_mbfGrpGrntCnt++;
                        break;
                    }
                }
            }
        }

        if (noData) {
            return; // don't create anything
        } else {
            writeRF_TSDU_SBF_Imm(osp.get(), true);
        }
    }
    else {
        return; // don't create anything
    }
}

/* Helper to write a SNDCP grant packet. */

bool ControlSignaling::writeRF_TSDU_SNDCP_Grant(uint32_t srcId, bool skip, uint32_t chNo)
{
    if (!m_p25->m_sndcpSupport)
        return false;

    std::unique_ptr<OSP_SNDCP_CH_GNT> osp = std::make_unique<OSP_SNDCP_CH_GNT>();
    osp->setMFId(m_lastMFID);
    osp->siteIdenEntry(m_p25->m_idenEntry);
    osp->setSrcId(srcId);
    osp->setDstId(srcId);

    // are we skipping checking?
    if (!skip) {
        if (m_p25->m_rfState != RS_RF_LISTENING && m_p25->m_rfState != RS_RF_DATA) {
            LogWarning(LOG_RF, P25_TSDU_STR ", TSBKO, ISP_SNDCP_CH_REQ (SNDCP Data Channel Request) denied, traffic in progress, srcId = %u", srcId);
            writeRF_TSDU_Deny(WUID_FNE, srcId, ReasonCode::DENY_PTT_COLLIDE, TSBKO::ISP_SNDCP_CH_REQ, false, true);

            ::ActivityLog("P25", true, "SNDCP grant request from %u denied", srcId);
            m_p25->m_rfState = RS_RF_REJECTED;

            return false;
        }

        if (!m_p25->m_affiliations->isGranted(srcId)) {
            if (!m_p25->m_affiliations->rfCh()->isRFChAvailable()) {
                LogWarning(LOG_RF, P25_TSDU_STR ", TSBKO, ISP_SNDCP_CH_REQ (SNDCP Data Channel Request) denied, no channels available, srcId = %u", srcId);
                writeRF_TSDU_Deny(WUID_FNE, srcId, ReasonCode::DENY_NO_RF_RSRC_AVAIL, TSBKO::ISP_SNDCP_CH_REQ, false, true);

                ::ActivityLog("P25", true, "SNDCP grant request from %u denied", srcId);
                m_p25->m_rfState = RS_RF_REJECTED;
                return false;
            }
            else {
                if (m_p25->m_affiliations->grantCh(srcId, srcId, GRANT_TIMER_TIMEOUT, false, false)) {
                    chNo = m_p25->m_affiliations->getGrantedCh(srcId);
                    ::lookups::VoiceChData voiceChData = m_p25->m_affiliations->rfCh()->getRFChData(chNo);

                    osp->setGrpVchId(voiceChData.chId());
                    osp->setGrpVchNo(chNo);
                    osp->setDataChnNo(chNo);
                    m_p25->m_siteData.setChCnt(m_p25->m_affiliations->rfCh()->rfChSize() + m_p25->m_affiliations->getGrantedRFChCnt());
                }
            }
        }
        else {
            chNo = m_p25->m_affiliations->getGrantedCh(srcId);
            ::lookups::VoiceChData voiceChData = m_p25->m_affiliations->rfCh()->getRFChData(chNo);

            osp->setGrpVchId(voiceChData.chId());
            osp->setGrpVchNo(chNo);
            osp->setDataChnNo(chNo);

            m_p25->m_affiliations->touchGrant(srcId);
        }
    }

    if (chNo > 0U) {
        ::lookups::VoiceChData voiceChData = m_p25->m_affiliations->rfCh()->getRFChData(chNo);

        // callback RPC to permit the granted TG on the specified voice channel
        if (m_p25->m_authoritative && m_p25->m_supervisor) {
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0 &&
                chNo != m_p25->m_siteData.channelNo()) {
                json::object req = json::object();
                int state = modem::DVM_STATE::STATE_P25;
                req["state"].set<int>(state);
                req["dstId"].set<uint32_t>(srcId);
                bool dataCh = true;
                req["dataPermit"].set<bool>(dataCh);

                // send blocking RPC request
                bool requestFailed = !g_RPC->req(RPC_PERMIT_P25_TG, req, [=, &requestFailed](json::object& req, json::object& reply) {
                    if (!req["status"].is<int>()) {
                        return;
                    }

                    int status = req["status"].get<int>();
                    if (status != network::NetRPC::OK) {
                        if (req["message"].is<std::string>()) {
                            std::string retMsg = req["message"].get<std::string>();
                            ::LogError(LOG_RF, "P25, RPC failed, %s", retMsg.c_str());
                        }
                        requestFailed = true;
                    } else {
                        requestFailed = false;
                    }
                }, voiceChData.address(), voiceChData.port(), true);

                // if the request failed block grant
                if (requestFailed) {
                    ::LogError(LOG_RF, P25_TSDU_STR ", TSBKO, ISP_SNDCP_CH_REQ (SNDCP Data Channel Request), failed to permit for use, chNo = %u-%u", voiceChData.chId(), chNo);

                    m_p25->m_affiliations->releaseGrant(srcId, false);
                    writeRF_TSDU_Deny(srcId, srcId, ReasonCode::DENY_PTT_BONK, TSBKO::ISP_SNDCP_CH_REQ, false, true);
                    m_p25->m_rfState = RS_RF_REJECTED;

                    return false;
                }
            }
            else {
                ::LogError(LOG_RF, P25_TSDU_STR ", TSBKO, ISP_SNDCP_CH_REQ (SNDCP Data Channel Request), failed to permit for use, chNo = %u-%u", voiceChData.chId(), chNo);
            }
        }

        ::ActivityLog("P25", true, "SNDCP grant request from %u", srcId);

        if (m_verbose) {
            LogMessage(LOG_RF, P25_TSDU_STR ", %s, chNo = %u-%u, srcId = %u",
                osp->toString().c_str(), voiceChData.chId(), osp->getDataChnNo(), osp->getSrcId());
        }

        // transmit group grant
        writeRF_TSDU_SBF_Imm(osp.get(), true);
        if (m_redundantGrant) {
            for (int i = 0; i < 3; i++)
                writeRF_TSDU_SBF(osp.get(), true);
        }
    }

    return true;
}

/* Helper to write a unit to unit answer request packet. */

void ControlSignaling::writeRF_TSDU_UU_Ans_Req(uint32_t srcId, uint32_t dstId)
{
    std::unique_ptr<IOSP_UU_ANS> iosp = std::make_unique<IOSP_UU_ANS>();
    iosp->setMFId(m_lastMFID);
    iosp->setSrcId(srcId);
    iosp->setDstId(dstId);

    VERBOSE_LOG_TSBK(iosp->toString(), srcId, dstId);
    writeRF_TSDU_SBF_Imm(iosp.get(), false);
}

/* Helper to write a acknowledge packet. */

void ControlSignaling::writeRF_TSDU_ACK_FNE(uint32_t srcId, uint32_t service, bool extended, bool noNetwork)
{
    std::unique_ptr<IOSP_ACK_RSP> iosp = std::make_unique<IOSP_ACK_RSP>();
    iosp->setSrcId(srcId);
    iosp->setService(service);

    if (extended) {
        iosp->setAIV(true);
        iosp->setEX(true);
    }

    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", %s, AIV = %u, EX = %u, serviceType = $%02X, srcId = %u",
            iosp->toString().c_str(), iosp->getAIV(), iosp->getEX(), iosp->getService(), srcId);
    }

    writeRF_TSDU_SBF_Imm(iosp.get(), noNetwork);
}

/* Helper to write a deny packet. */

void ControlSignaling::writeRF_TSDU_Deny(uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service, bool grp, bool aiv)
{
    std::unique_ptr<OSP_DENY_RSP> osp = std::make_unique<OSP_DENY_RSP>();
    osp->setAIV(aiv);
    osp->setSrcId(srcId);
    osp->setDstId(dstId);
    osp->setService(service);
    osp->setResponse(reason);
    osp->setGroup(grp);

    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", %s, AIV = %u, reason = $%02X, srcId = %u, dstId = %u",
            osp->toString().c_str(), osp->getAIV(), reason, osp->getSrcId(), osp->getDstId());
    }

    writeRF_TSDU_SBF_Imm(osp.get(), false);
}

/* Helper to write a group affiliation response packet. */

uint8_t ControlSignaling::writeRF_TSDU_Grp_Aff_Rsp(uint32_t srcId, uint32_t dstId)
{
    std::unique_ptr<IOSP_GRP_AFF> iosp = std::make_unique<IOSP_GRP_AFF>();
    iosp->setMFId(m_lastMFID);
    iosp->setAnnounceGroup(m_announcementGroup);
    iosp->setSrcId(srcId);
    iosp->setDstId(dstId);
    iosp->setResponse(ResponseCode::ACCEPT);

    bool noNet = false;

    // validate the source RID
    if (!acl::AccessControl::validateSrcId(srcId)) {
        LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, RID rejection, srcId = %u", iosp->toString().c_str(), srcId);
        ::ActivityLog("P25", true, "group affiliation request from %u to %s %u denied", srcId, "TG ", dstId);
        iosp->setResponse(ResponseCode::REFUSED);
        noNet = true;
    }

    // register the RID if the MFID is $90 (this is typically DVRS, and DVRS won't unit register so we'll do it for them)
    if (!m_p25->m_affiliations->isUnitReg(srcId) && m_lastMFID == MFG_MOT) {
        // validate the source RID
        if (!acl::AccessControl::validateSrcId(srcId)) {
            LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, RID rejection, srcId = %u", iosp->toString().c_str(), srcId);
            ::ActivityLog("P25", true, "unit registration request from %u denied", srcId);
            iosp->setResponse(ResponseCode::REFUSED);
            noNet = true;
        }
        else {
            // update dynamic unit registration table
            if (!m_p25->m_affiliations->isUnitReg(srcId)) {
                m_p25->m_affiliations->unitReg(srcId);
            }

            if (m_p25->m_network != nullptr)
                m_p25->m_network->announceUnitRegistration(srcId);
        }
    }

    // validate the source RID is registered
    if (!m_p25->m_affiliations->isUnitReg(srcId) && m_verifyReg) {
        LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, RID not registered, srcId = %u", iosp->toString().c_str(), srcId);
        ::ActivityLog("P25", true, "group affiliation request from %u to %s %u denied", srcId, "TG ", dstId);
        iosp->setResponse(ResponseCode::REFUSED);
        noNet = true;
    }

    // validate the talkgroup ID
    if (dstId == 0U) {
        LogWarning(LOG_RF, P25_TSDU_STR ", %s, TGID 0, dstId = %u", iosp->toString().c_str(), dstId);
    }
    else {
        if (!acl::AccessControl::validateTGId(dstId)) {
            LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, TGID rejection, dstId = %u", iosp->toString().c_str(), dstId);
            ::ActivityLog("P25", true, "group affiliation request from %u to %s %u denied", srcId, "TG ", dstId);
            iosp->setResponse(ResponseCode::DENY);
            noNet = true;
        }

        // deny affiliation if the TG is non-preferred on this site/CC
        if (acl::AccessControl::tgidNonPreferred(dstId)) {
            LogWarning(LOG_RF, P25_TSDU_STR ", %s non-preferred on this site, TGID rejection, dstId = %u", iosp->toString().c_str(), dstId);
            ::ActivityLog("P25", true, "group affiliation request from %u to %s %u denied", srcId, "TG ", dstId);
            iosp->setResponse(ResponseCode::DENY);
            noNet = true;
        }
    }

    if (iosp->getResponse() == ResponseCode::ACCEPT) {
        if (m_verbose) {
            LogMessage(LOG_RF, P25_TSDU_STR ", %s, anncId = %u, srcId = %u, dstId = %u",
                iosp->toString().c_str(), m_announcementGroup, srcId, dstId);
        }

        ::ActivityLog("P25", true, "group affiliation request from %u to %s %u", srcId, "TG ", dstId);

        // update dynamic affiliation table
        m_p25->m_affiliations->groupAff(srcId, dstId);

        if (m_p25->m_network != nullptr)
            m_p25->m_network->announceGroupAffiliation(srcId, dstId);
    }

    writeRF_TSDU_SBF_Imm(iosp.get(), noNet);
    return iosp->getResponse();
}

/* Helper to write a unit registration response packet. */

void ControlSignaling::writeRF_TSDU_U_Reg_Rsp(uint32_t srcId, uint32_t sysId)
{
    std::unique_ptr<IOSP_U_REG> iosp = std::make_unique<IOSP_U_REG>();
    iosp->setMFId(m_lastMFID);
    iosp->setResponse(ResponseCode::ACCEPT);
    iosp->setSrcId(srcId);
    iosp->setDstId(srcId);

    // validate the system ID
    if (sysId != m_p25->m_siteData.sysId()) {
        LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, SYSID rejection, sysId = $%03X", iosp->toString().c_str(), sysId);
        ::ActivityLog("P25", true, "unit registration request from %u denied", srcId);
        iosp->setResponse(ResponseCode::DENY);
    }

    // validate the source RID
    if (!acl::AccessControl::validateSrcId(srcId)) {
        LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, RID rejection, srcId = %u", iosp->toString().c_str(), srcId);
        ::ActivityLog("P25", true, "unit registration request from %u denied", srcId);
        iosp->setResponse(ResponseCode::REFUSED);
    }

    if (iosp->getResponse() == ResponseCode::ACCEPT) {
        if (m_verbose) {
            LogMessage(LOG_RF, P25_TSDU_STR ", %s, srcId = %u, sysId = $%03X", iosp->toString().c_str(), srcId, sysId);
        }

        ::ActivityLog("P25", true, "unit registration request from %u", srcId);

        // update dynamic unit registration table
        if (!m_p25->m_affiliations->isUnitReg(srcId)) {
            m_p25->m_affiliations->unitReg(srcId);
        }

        if (m_p25->m_network != nullptr)
            m_p25->m_network->announceUnitRegistration(srcId);
    }

    writeRF_TSDU_SBF_Imm(iosp.get(), true);

    // validate the source RID
    if (!acl::AccessControl::validateSrcId(srcId)) {
        denialInhibit(srcId); // inhibit source radio automatically
    }
}

/* Helper to write a unit de-registration acknowledge packet. */

void ControlSignaling::writeRF_TSDU_U_Dereg_Ack(uint32_t srcId)
{
    bool dereged = false;

    // remove dynamic unit registration table entry
    dereged = m_p25->m_affiliations->unitDereg(srcId);

    if (dereged) {
        std::unique_ptr<OSP_U_DEREG_ACK> osp = std::make_unique<OSP_U_DEREG_ACK>();
        osp->setMFId(m_lastMFID);
        osp->setSrcId(WUID_FNE);
        osp->setDstId(srcId);

        if (m_verbose) {
            LogMessage(LOG_RF, P25_TSDU_STR ", %s, srcId = %u", osp->toString().c_str(), srcId);
        }

        ::ActivityLog("P25", true, "unit deregistration request from %u", srcId);

        writeRF_TSDU_SBF_Imm(osp.get(), false);

//        if (m_p25->m_network != nullptr)
//            m_p25->m_network->announceUnitDeregistration(srcId);
    }
}

/* Helper to write a queue packet. */

void ControlSignaling::writeRF_TSDU_Queue(uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service, bool grp, bool aiv)
{
    std::unique_ptr<OSP_QUE_RSP> osp = std::make_unique<OSP_QUE_RSP>();
    osp->setAIV(aiv);
    osp->setSrcId(srcId);
    osp->setDstId(dstId);
    osp->setService(service);
    osp->setResponse(reason);
    osp->setGroup(grp);

    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", %s, AIV = %u, reason = $%02X, srcId = %u, dstId = %u",
            osp->toString().c_str(), osp->getAIV(), reason, osp->getSrcId(), osp->getDstId());
    }

    writeRF_TSDU_SBF_Imm(osp.get(), false);
}

/* Helper to write a location registration response packet. */

bool ControlSignaling::writeRF_TSDU_Loc_Reg_Rsp(uint32_t srcId, uint32_t dstId, bool grp)
{
    bool ret = false;

    std::unique_ptr<OSP_LOC_REG_RSP> osp = std::make_unique<OSP_LOC_REG_RSP>();
    osp->setMFId(m_lastMFID);
    osp->setResponse(ResponseCode::ACCEPT);
    osp->setDstId(dstId);
    osp->setSrcId(srcId);

    bool noNet = false;

    // validate the source RID
    if (!acl::AccessControl::validateSrcId(srcId)) {
        LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, RID rejection, srcId = %u", osp->toString().c_str(), srcId);
        ::ActivityLog("P25", true, "location registration request from %u denied", srcId);
        osp->setResponse(ResponseCode::REFUSED);
        noNet = true;
    }

    // validate the source RID is registered
    if (!m_p25->m_affiliations->isUnitReg(srcId)) {
        LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, RID not registered, srcId = %u", osp->toString().c_str(), srcId);
        ::ActivityLog("P25", true, "location registration request from %u denied", srcId);
        writeRF_TSDU_U_Reg_Cmd(srcId);
        return false;
    }

    // validate the talkgroup ID
    if (grp) {
        if (dstId == 0U) {
            LogWarning(LOG_RF, P25_TSDU_STR ", %s, TGID 0, dstId = %u", osp->toString().c_str(), dstId);
        }
        else {
            if (!acl::AccessControl::validateTGId(dstId)) {
                LogWarning(LOG_RF, P25_TSDU_STR ", %s denial, TGID rejection, dstId = %u", osp->toString().c_str(), dstId);
                ::ActivityLog("P25", true, "location registration request from %u to %s %u denied", srcId, "TG ", dstId);
                osp->setResponse(ResponseCode::DENY);
                noNet = true;
            }

            // deny affiliation if the TG is non-preferred on this site/CC
            if (acl::AccessControl::tgidNonPreferred(dstId)) {
                LogWarning(LOG_RF, P25_TSDU_STR ", %s non-preferred on this site, TGID rejection, dstId = %u", osp->toString().c_str(), dstId);
                ::ActivityLog("P25", true, "location registration request from %u to %s %u denied", srcId, "TG ", dstId);
                osp->setResponse(ResponseCode::DENY);
                noNet = true;
            }
        }
    }

    if (osp->getResponse() == ResponseCode::ACCEPT) {
        if (m_verbose) {
            LogMessage(LOG_RF, P25_TSDU_STR ", %s, srcId = %u, dstId = %u", osp->toString().c_str(), srcId, dstId);
        }

        ::ActivityLog("P25", true, "location registration request from %u", srcId);

        // update dynamic affiliation table
        m_p25->m_affiliations->groupAff(srcId, dstId);

        if (m_p25->m_network != nullptr)
            m_p25->m_network->announceGroupAffiliation(srcId, dstId);

        ret = true;
    }

    writeRF_TSDU_SBF_Imm(osp.get(), noNet);
    return ret;
}

/* Helper to write a LLA demand. */

void ControlSignaling::writeRF_TSDU_Auth_Dmd(uint32_t srcId)
{
    std::unique_ptr<MBT_OSP_AUTH_DMD> osp = std::make_unique<MBT_OSP_AUTH_DMD>();
    osp->setSrcId(WUID_FNE);
    osp->setDstId(srcId);
    osp->setAuthRS(m_p25->m_llaRS);

    // generate challenge
    uint8_t RC[AUTH_RAND_CHLNG_LENGTH_BYTES];
    std::uniform_int_distribution<uint32_t> dist(DVM_RAND_MIN, DVM_RAND_MAX);
    uint32_t rnd = dist(m_p25->m_random);
    SET_UINT32(rnd, RC, 0U);

    rnd = dist(m_p25->m_random);
    RC[4U] = (uint8_t)(rnd & 0xFFU);

    ulong64_t challenge = GET_UINT32(RC, 0U);
    challenge = (challenge << 8) + RC[4U];

    osp->setAuthRC(RC);

    m_llaDemandTable[srcId] = challenge;

    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", %s, srcId = %u, RC = %X", osp->toString().c_str(), srcId, challenge);
    }

    writeRF_TSDU_AMBT(osp.get(), true);
}

/* Helper to write a call termination packet. */

bool ControlSignaling::writeNet_TSDU_Call_Term(uint32_t srcId, uint32_t dstId)
{
    // is the specified channel granted?
    if (m_p25->m_affiliations->isGranted(dstId)) {
        m_p25->m_affiliations->releaseGrant(dstId, false);
    }

    std::unique_ptr<OSP_DVM_LC_CALL_TERM> osp = std::make_unique<OSP_DVM_LC_CALL_TERM>();
    osp->setGrpVchId(m_p25->m_siteData.channelId());
    osp->setGrpVchNo(m_p25->m_siteData.channelNo());
    osp->setDstId(dstId);
    osp->setSrcId(srcId);

    writeRF_TSDU_SBF(osp.get(), false);
    return true;
}

/* Helper to write a network TSDU from the RF data queue. */

void ControlSignaling::writeNet_TSDU_From_RF(lc::TSBK* tsbk, uint8_t* data)
{
    assert(tsbk != nullptr);
    assert(data != nullptr);

    ::memset(data, 0x00U, P25_TSDU_FRAME_LENGTH_BYTES);

    // generate Sync
    Sync::addP25Sync(data);

    // generate NID
    m_p25->m_nid.encode(data, DUID::TSDU);

    // regenerate TSDU Data
    tsbk->setLastBlock(true); // always set last block -- this a Single Block TSDU
    tsbk->encode(data);

    // add status bits
    P25Utils::addStatusBits(data, P25_TSDU_FRAME_LENGTH_BYTES, false, false);
    P25Utils::setStatusBitsStartIdle(data);
}

/* Helper to automatically inhibit a source ID on a denial. */

void ControlSignaling::denialInhibit(uint32_t srcId)
{
    if (!m_p25->m_inhibitUnauth) {
        return;
    }

    // this check should have already been done -- but do it again anyway
    if (!acl::AccessControl::validateSrcId(srcId)) {
        LogWarning(LOG_P25, P25_TSDU_STR ", denial, system auto-inhibit RID, srcId = %u", srcId);
        writeRF_TSDU_Ext_Func(ExtendedFunctions::INHIBIT, WUID_FNE, srcId);
    }
}
