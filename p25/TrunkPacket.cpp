/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2017-2022 by Bryan Biedenkapp N2PLL
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
#include "p25/TrunkPacket.h"
#include "p25/acl/AccessControl.h"
#include "p25/P25Utils.h"
#include "p25/Sync.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::data;

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------
// Make sure control data is supported.
#define IS_SUPPORT_CONTROL_CHECK(_PCKT_STR, _PCKT, _SRCID)                              \
    if (!m_p25->m_control) {                                                            \
        LogWarning(LOG_RF, P25_TSDU_STR ", " _PCKT_STR " denial, unsupported service, srcId = %u", _SRCID); \
        writeRF_TSDU_Deny(P25_DENY_RSN_SYS_UNSUPPORTED_SVC, _PCKT);                     \
        m_p25->m_rfState = RS_RF_REJECTED;                                              \
        return false;                                                                   \
    }

// Validate the source RID.
#define VALID_SRCID(_PCKT_STR, _PCKT, _SRCID)                                           \
    if (!acl::AccessControl::validateSrcId(_SRCID)) {                                   \
        LogWarning(LOG_RF, P25_TSDU_STR ", " _PCKT_STR " denial, RID rejection, srcId = %u", _SRCID); \
        writeRF_TSDU_Deny(P25_DENY_RSN_REQ_UNIT_NOT_VALID, _PCKT);                      \
        denialInhibit(_SRCID);                                                          \
        m_p25->m_rfState = RS_RF_REJECTED;                                              \
        return false;                                                                   \
    }

// Validate the target RID.
#define VALID_DSTID(_PCKT_STR, _PCKT, _DSTID)                                           \
    if (!acl::AccessControl::validateSrcId(_DSTID)) {                                   \
        LogWarning(LOG_RF, P25_TSDU_STR ", " _PCKT_STR " denial, RID rejection, dstId = %u", _DSTID); \
        writeRF_TSDU_Deny(P25_DENY_RSN_TGT_UNIT_NOT_VALID, _PCKT);                      \
        m_p25->m_rfState = RS_RF_REJECTED;                                              \
        return false;                                                                   \
    }

// Validate the talkgroup ID.
#define VALID_TGID(_PCKT_STR, _PCKT, _DSTID)                                            \
    if (!acl::AccessControl::validateTGId(_DSTID)) {                                    \
        LogWarning(LOG_RF, P25_TSDU_STR ", " _PCKT_STR " denial, TGID rejection, dstId = %u", _DSTID); \
        writeRF_TSDU_Deny(P25_DENY_RSN_TGT_GROUP_NOT_VALID, _PCKT);                     \
        m_p25->m_rfState = RS_RF_REJECTED;                                              \
        return false;                                                                   \
    }

// Verify the source RID is registered.
#define VERIFY_SRCID_REG(_PCKT_STR, _PCKT, _SRCID)                                      \
    if (!hasSrcIdUnitReg(_SRCID) && m_verifyReg) {                                      \
        LogWarning(LOG_RF, P25_TSDU_STR ", " _PCKT_STR " denial, RID not registered, srcId = %u", _SRCID); \
        writeRF_TSDU_Deny(P25_DENY_RSN_REQ_UNIT_NOT_AUTH, _PCKT);                       \
        writeRF_TSDU_U_Reg_Cmd(_SRCID);                                                 \
        m_p25->m_rfState = RS_RF_REJECTED;                                              \
        return false;                                                                   \
    }

// Verify the source RID is affiliated.
#define VERIFY_SRCID_AFF(_PCKT_STR, _PCKT, _SRCID, _DSTID)                              \
    if (!hasSrcIdGrpAff(_SRCID, _DSTID) && m_verifyAff) {                               \
        LogWarning(LOG_RF, P25_TSDU_STR ", " _PCKT_STR " denial, RID not affiliated to TGID, srcId = %u, dstId = %u", _SRCID, _DSTID); \
        writeRF_TSDU_Deny(P25_DENY_RSN_REQ_UNIT_NOT_AUTH, _PCKT);                       \
        writeRF_TSDU_U_Reg_Cmd(_SRCID);                                                 \
        m_p25->m_rfState = RS_RF_REJECTED;                                              \
        return false;                                                                   \
    }

// Validate the source RID (network).
#define VALID_SRCID_NET(_PCKT_STR, _SRCID)                                              \
    if (!acl::AccessControl::validateSrcId(_SRCID)) {                                   \
        LogWarning(LOG_NET, P25_TSDU_STR ", " _PCKT_STR " denial, RID rejection, srcId = %u", _SRCID); \
        return false;                                                                   \
    }

// Validate the target RID (network).
#define VALID_DSTID_NET(_PCKT_STR, _DSTID)                                              \
    if (!acl::AccessControl::validateSrcId(_DSTID)) {                                   \
        LogWarning(LOG_NET, P25_TSDU_STR ", " _PCKT_STR " denial RID rejection, dstId = %u", _DSTID); \
        return false;                                                                   \
    }

#define RF_TO_WRITE_NET()                                                               \
    if (m_network != NULL) {                                                            \
        uint8_t _buf[P25_TSDU_FRAME_LENGTH_BYTES];                                      \
        writeNet_TSDU_From_RF(_buf);                                                    \
        writeNetworkRF(_buf, true);                                                     \
    }

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t ADJ_SITE_TIMER_TIMEOUT = 30U;
const uint32_t ADJ_SITE_UPDATE_CNT = 5U;
const uint32_t TSDU_CTRL_BURST_COUNT = 2U;
const uint32_t TSBK_MBF_CNT = 3U;
const uint32_t GRANT_TIMER_TIMEOUT = 15U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Resets the data states for the RF interface.
/// </summary>
void TrunkPacket::resetRF()
{
    lc::TSBK tsbk = lc::TSBK(m_p25->m_siteData, m_p25->m_idenEntry, m_dumpTSBK);
    m_rfTSBK = tsbk;
}

/// <summary>
/// Resets the data states for the network.
/// </summary>
void TrunkPacket::resetNet()
{
    lc::TSBK tsbk = lc::TSBK(m_p25->m_siteData, m_p25->m_idenEntry, m_dumpTSBK);
    m_netTSBK = tsbk;
}

/// <summary>
/// Process a data frame from the RF interface.
/// </summary>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <param name="preDecoded">Flag indicating the TSBK data is pre-decoded TSBK data.</param>
/// <returns></returns>
bool TrunkPacket::process(uint8_t* data, uint32_t len, bool preDecoded)
{
    assert(data != NULL);

    if (!m_p25->m_control)
        return false;

    uint8_t duid = 0U;
    if (!preDecoded) {
        // Decode the NID
        bool valid = m_p25->m_nid.decode(data + 2U);

        if (m_p25->m_rfState == RS_RF_LISTENING && !valid)
            return false;

        duid = m_p25->m_nid.getDUID();
    } else {
        duid = P25_DUID_TSDU;
    }

    RPT_RF_STATE prevRfState = m_p25->m_rfState;
    
    // handle individual DUIDs
    if (duid == P25_DUID_TSDU) {
        if (m_p25->m_rfState != RS_RF_DATA) {
            m_p25->m_rfState = RS_RF_DATA;
        }

        m_p25->m_queue.clear();

        if (!preDecoded) {
            resetRF();
            resetNet();
            
            bool ret = m_rfTSBK.decode(data + 2U);
            if (!ret) {
                LogWarning(LOG_RF, P25_TSDU_STR ", undecodable LC");
                m_p25->m_rfState = prevRfState;
                return false;
            }
        }
        else {
            resetNet();
        }

        uint32_t srcId = m_rfTSBK.getSrcId();
        uint32_t dstId = m_rfTSBK.getDstId();

        // handle standard P25 reference opcodes
        switch (m_rfTSBK.getLCO()) {
            case TSBK_IOSP_GRP_VCH:
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK("TSBK_IOSP_GRP_VCH (Group Voice Channel Request)", TSBK_IOSP_GRP_VCH, srcId);

                // validate the source RID
                VALID_SRCID("TSBK_IOSP_GRP_VCH (Group Voice Channel Request)", TSBK_IOSP_GRP_VCH, srcId);

                // validate the talkgroup ID
                VALID_TGID("TSBK_IOSP_GRP_VCH (Group Voice Channel Request)", TSBK_IOSP_GRP_VCH, dstId);

                // verify the source RID is affiliated
                VERIFY_SRCID_AFF("TSBK_IOSP_GRP_VCH (Group Voice Channel Request)", TSBK_IOSP_GRP_VCH, srcId, dstId);

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_GRP_VCH (Group Voice Channel Request), srcId = %u, dstId = %u", srcId, dstId);
                }

                writeRF_TSDU_Grant(true);
                break;
            case TSBK_IOSP_UU_VCH:
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK("TSBK_IOSP_UU_VCH (Unit-to-Unit Voice Channel Request)", TSBK_IOSP_UU_VCH, srcId);

                // validate the source RID
                VALID_SRCID("TSBK_IOSP_UU_VCH (Unit-to-Unit Voice Channel Request)", TSBK_IOSP_UU_VCH, srcId);

                // validate the target RID
                VALID_DSTID("TSBK_IOSP_UU_VCH (Unit-to-Unit Voice Channel Request)", TSBK_IOSP_UU_VCH, dstId);

                // verify the source RID is registered
                VERIFY_SRCID_REG("TSBK_IOSP_UU_VCH (Unit-to-Unit Voice Channel Request)", TSBK_IOSP_UU_VCH, srcId);

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_UU_VCH (Unit-to-Unit Voice Channel Request), srcId = %u, dstId = %u", srcId, dstId);
                }

                if (m_unitToUnitAvailCheck) {
                    writeRF_TSDU_UU_Ans_Req(srcId, dstId);
                }
                else {
                    writeRF_TSDU_Grant(false);
                }
                break;
            case TSBK_IOSP_UU_ANS:
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK("TSBK_IOSP_UU_ANS (Unit-to-Unit Answer Response)", TSBK_IOSP_UU_ANS, srcId);

                // validate the source RID
                VALID_SRCID("TSBK_IOSP_UU_ANS (Unit-to-Unit Answer Response)", TSBK_IOSP_UU_ANS, srcId);

                // validate the target RID
                VALID_DSTID("TSBK_IOSP_UU_ANS (Unit-to-Unit Answer Response)", TSBK_IOSP_UU_ANS, dstId);

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_UU_ANS (Unit-to-Unit Answer Response), response = $%02X, srcId = %u, dstId = %u", 
                        m_rfTSBK.getResponse(), srcId, dstId);
                }

                if (m_rfTSBK.getResponse() == P25_ANS_RSP_PROCEED) {
                    if (m_p25->m_ackTSBKRequests) {
                        writeRF_TSDU_ACK_FNE(dstId, TSBK_IOSP_UU_ANS, false, true);
                    }

                    writeRF_TSDU_Grant(false);
                }
                else if (m_rfTSBK.getResponse() == P25_ANS_RSP_DENY) {
                    writeRF_TSDU_Deny(P25_DENY_RSN_TGT_UNIT_REFUSED, TSBK_IOSP_UU_ANS);
                }
                else if (m_rfTSBK.getResponse() == P25_ANS_RSP_WAIT) {
                    writeRF_TSDU_Queue(P25_QUE_RSN_TGT_UNIT_QUEUED, TSBK_IOSP_UU_ANS);
                }
                break;
            case TSBK_IOSP_TELE_INT_ANS:
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK("TSBK_IOSP_TELE_INT_ANS (Telephone Interconnect Answer Response)", TSBK_IOSP_TELE_INT_ANS, srcId);

                // validate the source RID
                VALID_SRCID("TSBK_IOSP_TELE_INT_ANS (Telephone Interconnect Answer Response)", TSBK_IOSP_TELE_INT_ANS, srcId);

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_TELE_INT_ANS (Telephone Interconnect Answer Response), response = $%02X, srcId = %u",
                        m_rfTSBK.getResponse(), srcId);
                }

                writeRF_TSDU_Deny(P25_DENY_RSN_SYS_UNSUPPORTED_SVC, TSBK_IOSP_TELE_INT_ANS);
                break;
            case TSBK_ISP_SNDCP_CH_REQ:
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK("TSBK_ISP_SNDCP_CH_REQ (SNDCP Channel Request)", TSBK_ISP_SNDCP_CH_REQ, srcId);

                // validate the source RID
                VALID_SRCID("TSBK_ISP_SNDCP_CH_REQ (SNDCP Channel Request)", TSBK_ISP_SNDCP_CH_REQ, srcId);

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_ISP_SNDCP_CH_REQ (SNDCP Channel Request), dataServiceOptions = $%02X, dataAccessControl = %u, srcId = %u",
                        m_rfTSBK.getDataServiceOptions(), m_rfTSBK.getDataAccessControl(), srcId);
                }

                // SNDCP data channel requests are currently unsupported -- maybe in the future?

                writeRF_TSDU_Deny(P25_DENY_RSN_SYS_UNSUPPORTED_SVC, TSBK_ISP_SNDCP_CH_REQ);
                break;
            case TSBK_IOSP_STS_UPDT:
                // validate the source RID
                VALID_SRCID("TSBK_IOSP_STS_UPDT (Status Update)", TSBK_IOSP_STS_UPDT, srcId);

                RF_TO_WRITE_NET();

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_STS_UPDT (Status Update), status = $%02X, srcId = %u", m_rfTSBK.getStatus(), srcId);
                }

                if (!m_noStatusAck) {
                    writeRF_TSDU_ACK_FNE(srcId, TSBK_IOSP_STS_UPDT, false, false);
                }

                ::ActivityLog("P25", true, "status update from %u", srcId);
                break;
            case TSBK_IOSP_MSG_UPDT:
                // validate the source RID
                VALID_SRCID("TSBK_IOSP_MSG_UPDT (Message Update)", TSBK_IOSP_MSG_UPDT, srcId);

                RF_TO_WRITE_NET();

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_MSG_UPDT (Message Update), message = $%02X, srcId = %u, dstId = %u", 
                        m_rfTSBK.getMessage(), srcId, dstId);
                }

                if (!m_noMessageAck) {
                    writeRF_TSDU_ACK_FNE(srcId, TSBK_IOSP_MSG_UPDT, false, false);
                }

                ::ActivityLog("P25", true, "message update from %u", srcId);
                break;
            case TSBK_IOSP_CALL_ALRT:
                // validate the source RID
                VALID_SRCID("TSBK_IOSP_CALL_ALRT (Call Alert)", TSBK_IOSP_CALL_ALRT, srcId);

                // validate the target RID
                VALID_DSTID("TSBK_IOSP_CALL_ALRT (Call Alert)", TSBK_IOSP_CALL_ALRT, dstId);

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_CALL_ALRT (Call Alert), srcId = %u, dstId = %u", srcId, dstId);
                }

                ::ActivityLog("P25", true, "call alert request from %u to %u", srcId, dstId);

                writeRF_TSDU_Call_Alrt(srcId, dstId);
                break;
            case TSBK_IOSP_ACK_RSP:
                // validate the source RID
                VALID_SRCID("TSBK_IOSP_ACK_RSP (Acknowledge Response)", TSBK_IOSP_ACK_RSP, srcId);

                // validate the target RID
                VALID_DSTID("TSBK_IOSP_ACK_RSP (Acknowledge Response)", TSBK_IOSP_ACK_RSP, dstId);

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_ACK_RSP (Acknowledge Response), AIV = %u, serviceType = $%02X, srcId = %u, dstId = %u", 
                        m_rfTSBK.getAIV(), m_rfTSBK.getService(), srcId, dstId);
                }

                ::ActivityLog("P25", true, "ack response from %u to %u", srcId, dstId);

                // bryanb: HACK -- for some reason, if the AIV is false and we have a dstId
                // its very likely srcId and dstId are swapped so we'll swap them
                if (!m_rfTSBK.getAIV() && dstId != 0U) {
                    m_rfTSBK.setAIV(true);
                    m_rfTSBK.setSrcId(dstId);
                    m_rfTSBK.setDstId(srcId);
                }

                writeRF_TSDU_SBF(false);
                break;
            case TSBK_ISP_CAN_SRV_REQ:
                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_ISP_CAN_SRV_REQ (Cancel Service Request), AIV = %u, serviceType = $%02X, reason = $%02X, srcId = %u, dstId = %u",
                        m_rfTSBK.getAIV(), m_rfTSBK.getService(), m_rfTSBK.getResponse(), srcId, dstId);
                }

                ::ActivityLog("P25", true, "cancel service request from %u", srcId);

                writeRF_TSDU_ACK_FNE(srcId, TSBK_ISP_CAN_SRV_REQ, false, true);
                break;
            case TSBK_IOSP_EXT_FNCT:
                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_EXT_FNCT (Extended Function), op = $%02X, arg = %u, tgt = %u", 
                        m_rfTSBK.getExtendedFunction(), dstId, srcId);
                }

                // generate activity log entry
                if (m_rfTSBK.getExtendedFunction() == P25_EXT_FNCT_CHECK_ACK) {
                    ::ActivityLog("P25", true, "radio check response from %u to %u", dstId, srcId);
                }
                else if (m_rfTSBK.getExtendedFunction() == P25_EXT_FNCT_INHIBIT_ACK) {
                    ::ActivityLog("P25", true, "radio inhibit response from %u to %u", dstId, srcId);
                }
                else if (m_rfTSBK.getExtendedFunction() == P25_EXT_FNCT_UNINHIBIT_ACK) {
                    ::ActivityLog("P25", true, "radio uninhibit response from %u to %u", dstId, srcId);
                }

                writeRF_TSDU_SBF(true);
                break;
            case TSBK_ISP_EMERG_ALRM_REQ:
                if (m_rfTSBK.getEmergency()) {
                    if (m_verbose) {
                        LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_ISP_EMERG_ALRM_REQ (Emergency Alarm Request), srcId = %u, dstId = %u",
                            srcId, dstId);
                    }

                    ::ActivityLog("P25", true, "emergency alarm request request from %u", srcId);

                    writeRF_TSDU_ACK_FNE(srcId, TSBK_ISP_EMERG_ALRM_REQ, false, true);
                }
                break;
            case TSBK_IOSP_GRP_AFF:
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK("TSBK_IOSP_GRP_AFF (Group Affiliation Request)", TSBK_IOSP_GRP_AFF, srcId);

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_GRP_AFF (Group Affiliation Request), srcId = %u, dstId = %u", srcId, dstId);
                }

                if (m_p25->m_ackTSBKRequests) {
                    writeRF_TSDU_ACK_FNE(srcId, TSBK_IOSP_GRP_AFF, true, true);
                }

                writeRF_TSDU_Grp_Aff_Rsp(srcId, dstId);
                break;
            case TSBK_ISP_GRP_AFF_Q_RSP:
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK("TSBK_IOSP_GRP_AFF (Group Affiliation Query Response)", TSBK_ISP_GRP_AFF_Q_RSP, srcId);

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_GRP_AFF (Group Affiliation Query Response), srcId = %u, dstId = %u, anncId = %u", srcId, dstId, 
                        m_rfTSBK.getPatchSuperGroupId());
                }

                ::ActivityLog("P25", true, "group affiliation query response from %u to %s %u", srcId, "TG ", dstId);
                break;
            case TSBK_ISP_U_DEREG_REQ:
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK("TSBK_ISP_U_DEREG_REQ (Unit Deregistration Request)", TSBK_ISP_U_DEREG_REQ, srcId);

                // validate the source RID
                VALID_SRCID("TSBK_ISP_U_DEREG_REQ (Unit Deregistration Request)", TSBK_ISP_U_DEREG_REQ, srcId);

                // HACK: ensure the DEREG_REQ transmits something ...
                if (dstId == 0U) {
                    dstId = P25_WUID_FNE;
                }

                if (m_p25->m_ackTSBKRequests) {
                    writeRF_TSDU_ACK_FNE(srcId, TSBK_ISP_U_DEREG_REQ, true, true);
                }

                writeRF_TSDU_U_Dereg_Ack(srcId);
                break;
            case TSBK_IOSP_U_REG:
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK("TSBK_ISP_U_REG_REQ (Unit Registration Request)", TSBK_IOSP_U_REG, srcId);

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_ISP_U_REG_REQ (Unit Registration Request), srcId = %u", srcId);
                }

                if (m_p25->m_ackTSBKRequests) {
                    writeRF_TSDU_ACK_FNE(srcId, TSBK_IOSP_U_REG, true, true);
                }

                writeRF_TSDU_U_Reg_Rsp(srcId);
                break;
            case TSBK_ISP_LOC_REG_REQ:
                // make sure control data is supported
                IS_SUPPORT_CONTROL_CHECK("TSBK_ISP_LOC_REG_REQ (Location Registration Request)", TSBK_ISP_LOC_REG_REQ, srcId);

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_ISP_LOC_REG_REQ (Location Registration Request), srcId = %u, dstId = %u", srcId, dstId);
                }

                writeRF_TSDU_Loc_Reg_Rsp(srcId, dstId);
                break;
            default:
                LogError(LOG_RF, P25_TSDU_STR ", unhandled LCO, mfId = $%02X, lco = $%02X", m_rfTSBK.getMFId(), m_rfTSBK.getLCO());
                break;
        } // switch (m_rfTSBK.getLCO())

        // add trailing null pad; only if control data isn't being transmitted
        if (!m_p25->m_ccRunning) {
            m_p25->writeRF_Nulls();
        }

        m_p25->m_rfState = prevRfState;
        return true;
    }
    else {
        LogError(LOG_RF, "P25 unhandled data DUID, duid = $%02X", duid);
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
bool TrunkPacket::processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, uint8_t& duid)
{
    if (!m_p25->m_control)
        return false;
    if (m_p25->m_rfState != RS_RF_LISTENING && m_p25->m_netState == RS_NET_IDLE)
        return false;

    switch (duid) {
        case P25_DUID_TSDU:
            if (m_p25->m_netState == RS_NET_IDLE) {
                resetRF();
                resetNet();

                bool ret = m_netTSBK.decode(data);
                if (!ret) {
                    return false;
                }

                // handle updating internal adjacent site information
                if (m_netTSBK.getLCO() == TSBK_OSP_ADJ_STS_BCAST) {
                    if (!m_p25->m_control) {
                        return false;
                    }

                    if (m_netTSBK.getAdjSiteId() != m_p25->m_siteData.siteId()) {
                        // update site table data
                        SiteData site;
                        try {
                            site = m_adjSiteTable.at(m_netTSBK.getAdjSiteId());
                        } catch (...) {
                            site = SiteData();
                        }

                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", TSBK_OSP_ADJ_STS_BCAST (Adjacent Site Status Broadcast), sysId = $%03X, rfss = $%02X, site = $%02X, chId = %u, chNo = %u, svcClass = $%02X",
                                m_netTSBK.getAdjSiteSysId(), m_netTSBK.getAdjSiteRFSSId(), m_netTSBK.getAdjSiteId(), m_netTSBK.getAdjSiteChnId(), m_netTSBK.getAdjSiteChnNo(), m_netTSBK.getAdjSiteSvcClass());
                        }

                        site.setAdjSite(m_netTSBK.getAdjSiteSysId(), m_netTSBK.getAdjSiteRFSSId(),
                            m_netTSBK.getAdjSiteId(), m_netTSBK.getAdjSiteChnId(), m_netTSBK.getAdjSiteChnNo(), m_netTSBK.getAdjSiteSvcClass());

                        m_adjSiteTable[site.siteId()] = site;
                        m_adjSiteUpdateCnt[site.siteId()] = ADJ_SITE_UPDATE_CNT;
                    } else {
                        /*
                        ** treat same site adjacent site broadcast as a SCCB for this site
                        */
                        // update site table data
                        SiteData site;
                        try {
                            site = m_sccbTable.at(m_netTSBK.getAdjSiteRFSSId());
                        }
                        catch (...) {
                            site = SiteData();
                        }

                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", TSBK_OSP_SCCB_EXP (Secondary Control Channel Broadcast), sysId = $%03X, rfss = $%02X, site = $%02X, chId = %u, chNo = %u, svcClass = $%02X",
                                m_netTSBK.getAdjSiteSysId(), m_netTSBK.getAdjSiteRFSSId(), m_netTSBK.getAdjSiteId(), m_netTSBK.getAdjSiteChnId(), m_netTSBK.getAdjSiteChnNo(), m_netTSBK.getAdjSiteSvcClass());
                        }

                        site.setAdjSite(m_netTSBK.getAdjSiteSysId(), m_netTSBK.getAdjSiteRFSSId(),
                            m_netTSBK.getAdjSiteId(), m_netTSBK.getAdjSiteChnId(), m_netTSBK.getAdjSiteChnNo(), m_netTSBK.getAdjSiteSvcClass());

                        m_sccbTable[site.rfssId()] = site;
                        m_sccbUpdateCnt[site.rfssId()] = ADJ_SITE_UPDATE_CNT;
                    }

                    return true;
                }

                uint32_t srcId = m_netTSBK.getSrcId();
                uint32_t dstId = m_netTSBK.getDstId();

                // handle internal DVM TSDUs
                if (m_netTSBK.getMFId() == P25_MFG_DVM) {
                    switch (m_netTSBK.getLCO()) {
                        case LC_CALL_TERM:
                            if (m_p25->m_dedicatedControl) {
                                uint32_t chNo = m_netTSBK.getGrpVchNo();

                                if (m_verbose) {
                                    LogMessage(LOG_NET, P25_TSDU_STR ", LC_CALL_TERM (Call Termination), chNo = %u, srcId = %u, dstId = %u", chNo, srcId, dstId);
                                }

                                // is the specified channel granted?
                                if (isChBusy(chNo) && hasDstIdGranted(dstId)) {
                                    releaseDstIdGrant(dstId, false);
                                }
                            }
                            break;
                        default:
                            LogError(LOG_NET, P25_TSDU_STR ", unhandled LCO, mfId = $%02X, lco = $%02X", m_netTSBK.getMFId(), m_netTSBK.getLCO());
                            return false;
                    }

                    writeNet_TSDU();

                    return true;
                }

                // handle standard P25 reference opcodes
                switch (m_netTSBK.getLCO()) {
                    case TSBK_IOSP_GRP_VCH:
                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", TSBK_IOSP_GRP_VCH (Group Voice Channel Grant), emerg = %u, encrypt = %u, prio = %u, chNo = %u, srcId = %u, dstId = %u",
                                m_netTSBK.getEmergency(), m_netTSBK.getEncrypted(), m_netTSBK.getPriority(), m_netTSBK.getGrpVchNo(), srcId, dstId);
                        }

                        // workaround for single channel dedicated sites to pass network traffic on a lone VC
                        if (m_p25->m_dedicatedControl && !m_p25->m_voiceOnControl && m_voiceChTable.size() == 1U) {
                            m_rfTSBK.setSrcId(srcId);
                            m_rfTSBK.setDstId(dstId);

                            m_rfTSBK.setEmergency(m_netTSBK.getEmergency());
                            m_rfTSBK.setEncrypted(m_netTSBK.getEncrypted());
                            m_rfTSBK.setPriority(m_netTSBK.getPriority());

                            writeRF_TSDU_Grant(true);
                        }

                        return true; // don't allow this to write to the air
                    case TSBK_IOSP_UU_VCH:
                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", TSBK_IOSP_UU_VCH (Unit-to-Unit Voice Channel Grant), emerg = %u, encrypt = %u, prio = %u, chNo = %u, srcId = %u, dstId = %u",
                                m_netTSBK.getEmergency(), m_netTSBK.getEncrypted(), m_netTSBK.getPriority(), m_netTSBK.getGrpVchNo(), srcId, dstId);
                        }

                        // workaround for single channel dedicated sites to pass network traffic on a lone VC
                        if (m_p25->m_dedicatedControl && !m_p25->m_voiceOnControl && m_voiceChTable.size() == 1U) {
                            m_rfTSBK.setSrcId(srcId);
                            m_rfTSBK.setDstId(dstId);

                            m_rfTSBK.setEmergency(m_netTSBK.getEmergency());
                            m_rfTSBK.setEncrypted(m_netTSBK.getEncrypted());
                            m_rfTSBK.setPriority(m_netTSBK.getPriority());

                            writeRF_TSDU_Grant(false);
                        }

                        return true; // don't allow this to write to the air
                    case TSBK_IOSP_UU_ANS:
                        if (m_netTSBK.getResponse() > 0U) {
                            if (m_verbose) {
                                LogMessage(LOG_NET, P25_TSDU_STR ", TSBK_IOSP_UU_ANS (Unit-to-Unit Answer Response), response = $%02X, srcId = %u, dstId = %u",
                                    m_netTSBK.getResponse(), srcId, dstId);
                            }
                        }
                        else {
                            if (m_verbose) {
                                LogMessage(LOG_NET, P25_TSDU_STR ", TSBK_IOSP_UU_ANS (Unit-to-Unit Answer Request), srcId = %u, dstId = %u", srcId, dstId);
                            }
                        }
                        break;
                    case TSBK_IOSP_STS_UPDT:
                        // validate the source RID
                        VALID_SRCID_NET("TSBK_IOSP_STS_UPDT (Status Update)", srcId);

                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", TSBK_IOSP_STS_UPDT (Status Update), status = $%02X, srcId = %u", 
                                m_netTSBK.getStatus(), srcId);
                        }

                        ::ActivityLog("P25", false, "status update from %u", srcId);
                        break;
                    case TSBK_IOSP_MSG_UPDT:
                        // validate the source RID
                        VALID_SRCID_NET("TSBK_IOSP_MSG_UPDT (Message Update)", srcId);

                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", TSBK_IOSP_MSG_UPDT (Message Update), message = $%02X, srcId = %u, dstId = %u", 
                                m_netTSBK.getMessage(), srcId, dstId);
                        }

                        ::ActivityLog("P25", false, "message update from %u", srcId);
                        break;
                    case TSBK_IOSP_CALL_ALRT:
                        // validate the source RID
                        VALID_SRCID_NET("TSBK_IOSP_CALL_ALRT (Call Alert)", srcId);

                        // validate the target RID
                        VALID_DSTID_NET("TSBK_IOSP_CALL_ALRT (Call Alert)", dstId);

                        // validate source RID
                        if (!acl::AccessControl::validateSrcId(srcId)) {
                            LogWarning(LOG_NET, "P25_DUID_TSDU (Trunking System Data Unit) denial, RID rejection, srcId = %u", srcId);
                            return false;
                        }

                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", TSBK_IOSP_CALL_ALRT (Call Alert), srcId = %u, dstId = %u", srcId, dstId);
                        }

                        ::ActivityLog("P25", false, "call alert request from %u to %u", srcId, dstId);
                        break;
                    case TSBK_IOSP_ACK_RSP:
                        // validate the source RID
                        VALID_SRCID_NET("TSBK_IOSP_ACK_RSP (Acknowledge Response)", srcId);

                        // validate the target RID
                        VALID_DSTID_NET("TSBK_IOSP_ACK_RSP (Acknowledge Response)", dstId);

                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", TSBK_IOSP_ACK_RSP (Acknowledge Response), AIV = %u, serviceType = $%02X, srcId = %u, dstId = %u", 
                                m_netTSBK.getAIV(), m_netTSBK.getService(), dstId, srcId);
                        }

                        ::ActivityLog("P25", false, "ack response from %u to %u", srcId, dstId);
                        break;
                    case TSBK_IOSP_EXT_FNCT:
                        // validate the target RID
                        VALID_DSTID_NET("TSBK_IOSP_EXT_FNCT (Extended Function)", dstId);

                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", TSBK_IOSP_EXT_FNCT (Extended Function), serviceType = $%02X, arg = %u, tgt = %u", 
                                m_netTSBK.getService(), srcId, dstId);
                        }
                        break;
                    case TSBK_ISP_EMERG_ALRM_REQ:
                        // non-emergency mode is a TSBK_OSP_DENY_RSP
                        if (!m_netTSBK.getEmergency()) {
                            if (m_verbose) {
                                LogMessage(LOG_NET, P25_TSDU_STR ", TSBK_OSP_DENY_RSP (Deny Response), AIV = %u, reason = $%02X, srcId = %u, dstId = %u",
                                    m_netTSBK.getAIV(), m_netTSBK.getResponse(), m_netTSBK.getSrcId(), m_netTSBK.getDstId());
                            }
                        } else {
                            if (m_verbose) {
                                LogMessage(LOG_NET, P25_TSDU_STR ", TSBK_ISP_EMERG_ALRM_REQ (Emergency Alarm Request), srcId = %u, dstId = %u",
                                    srcId, dstId);
                            }
                        }
                        break;
                    case TSBK_IOSP_GRP_AFF:
                        // ignore a network group affiliation command
                        return true; // don't allow this to write to the air
                    case TSBK_OSP_U_DEREG_ACK:
                        // ignore a network user deregistration command
                        return true; // don't allow this to write to the air
                    case TSBK_OSP_LOC_REG_RSP:
                        // ignore a network location registration command
                        return true; // don't allow this to write to the air
                    case TSBK_OSP_QUE_RSP:
                        if (m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", TSBK_OSP_QUE_RSP (Queue Response), AIV = %u, reason = $%02X, srcId = %u, dstId = %u",
                                m_netTSBK.getAIV(), m_netTSBK.getResponse(), m_netTSBK.getSrcId(), m_netTSBK.getDstId());
                        }
                        break;
                    default:
                        LogError(LOG_NET, P25_TSDU_STR ", unhandled LCO, mfId = $%02X, lco = $%02X", m_netTSBK.getMFId(), m_netTSBK.getLCO());
                        return false;
                } // switch (m_netTSBK.getLCO())

                writeNet_TSDU();
            }
            break;
        default:
            return false;
    }

    return true;
}

/// <summary>
/// Helper used to process AMBTs from PDU data.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="dataBlock"></param>
bool TrunkPacket::processMBT(DataHeader dataHeader, DataBlock* blocks)
{
    uint8_t data[1U];
    ::memset(data, 0x00U, 1U);

    bool ret = true;
    for (uint32_t i = 0; i < dataHeader.getBlocksToFollow(); i++) {
        bool blockRead = true;

        // get the raw block data
        uint8_t raw[P25_PDU_UNCONFIRMED_LENGTH_BYTES];
        uint32_t len = blocks[i].getData(raw);
        if (len != P25_PDU_UNCONFIRMED_LENGTH_BYTES) {
            LogError(LOG_P25, "TrunkPacket::processMBT(), failed to read PDU data block");
            blockRead = false;
        }

        if (blockRead) {
            bool mbtDecode = m_rfTSBK.decodeMBT(dataHeader, raw);
            if (mbtDecode) {
                process(data, 1U, true);
            } else {
                ret = false;
            }
        }
    }

    return ret;
}

/// <summary>
/// Helper to write P25 adjacent site information to the network.
/// </summary>
void TrunkPacket::writeAdjSSNetwork()
{
    if (!m_p25->m_control) {
        return;
    }

    resetRF();
    resetNet();

    if (m_network != NULL) {
        if (m_verbose) {
            LogMessage(LOG_NET, P25_TSDU_STR ", TSBK_OSP_ADJ_STS_BCAST (Adjacent Site Status Broadcast), network announce, sysId = $%03X, rfss = $%02X, site = $%02X, chId = %u, chNo = %u, svcClass = $%02X", 
                m_p25->m_siteData.sysId(), m_p25->m_siteData.rfssId(), m_p25->m_siteData.siteId(), m_p25->m_siteData.channelId(), m_p25->m_siteData.channelNo(), m_p25->m_siteData.serviceClass());
        }

        uint8_t cfva = P25_CFVA_VALID;
        if (m_p25->m_control && m_p25->m_voiceOnControl) {
            cfva |= P25_CFVA_CONV;
        }

        // transmit adjacent site broadcast
        m_rfTSBK.setLCO(TSBK_OSP_ADJ_STS_BCAST);
        m_rfTSBK.setAdjSiteCFVA(cfva);
        m_rfTSBK.setAdjSiteSysId(m_p25->m_siteData.sysId());
        m_rfTSBK.setAdjSiteRFSSId(m_p25->m_siteData.rfssId());
        m_rfTSBK.setAdjSiteId(m_p25->m_siteData.siteId());
        m_rfTSBK.setAdjSiteChnId(m_p25->m_siteData.channelId());
        m_rfTSBK.setAdjSiteChnNo(m_p25->m_siteData.channelNo());
        m_rfTSBK.setAdjSiteSvcClass(m_p25->m_siteData.serviceClass());
        
        RF_TO_WRITE_NET();
    }
}

/// <summary>
/// Helper to determine if the source ID has affiliated to the group destination ID.
/// </summary>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
/// <returns></returns>
bool TrunkPacket::hasSrcIdGrpAff(uint32_t srcId, uint32_t dstId) const
{
    // lookup dynamic affiliation table entry
    try {
        uint32_t tblDstId = m_grpAffTable.at(srcId);
        if (tblDstId == dstId) {
            return true;
        }
        else {
            return false;
        }
    } catch (...) {
        return false;
    }
}

/// <summary>
/// Helper to determine if the source ID has unit registered.
/// </summary>
/// <param name="srcId"></param>
/// <returns></returns>
bool TrunkPacket::hasSrcIdUnitReg(uint32_t srcId) const
{
    // lookup dynamic unit registration table entry
    if (std::find(m_unitRegTable.begin(), m_unitRegTable.end(), srcId) != m_unitRegTable.end()) {
        return true;
    }
    else {
        return false;
    }
}

/// <summary>
/// Helper to determine if the channel number is busy.
/// </summary>
/// <param name="chNo"></param>
/// <returns></returns>
bool TrunkPacket::isChBusy(uint32_t chNo) const
{
    if (chNo == 0U) {
        return false;
    }

    // lookup dynamic channel grant table entry
    for (auto it = m_grantChTable.begin(); it != m_grantChTable.end(); ++it) {
        if (it->second == chNo) {
            return true;
        }
    }

    return false;
}

/// <summary>
/// Helper to determine if the destination ID is already granted.
/// </summary>
/// <param name="dstId"></param>
/// <returns></returns>
bool TrunkPacket::hasDstIdGranted(uint32_t dstId) const
{
    if (dstId == 0U) {
        return false;
    }

    // lookup dynamic channel grant table entry
    try {
        uint32_t chNo = m_grantChTable.at(dstId);
        if (chNo != 0U) {
            return true;
        }
        else {
            return false;
        }
    } catch (...) {
        return false;
    }
}

/// <summary>
/// Helper to start the destination ID grant timer.
/// </summary>
/// <param name="dstId"></param>
/// <returns></returns>
void TrunkPacket::touchDstIdGrant(uint32_t dstId)
{
    if (dstId == 0U) {
        return;
    }

    if (hasDstIdGranted(dstId)) {
        m_grantTimers[dstId].start();
    }
}

/// <summary>
/// Helper to release the channel grant for the destination ID.
/// </summary>
/// <param name="dstId"></param>
/// <param name="releaseAll"></param>
void TrunkPacket::releaseDstIdGrant(uint32_t dstId, bool releaseAll)
{
    if (dstId == 0U && !releaseAll) {
        return;
    }

    if (dstId == 0U && releaseAll) {
        LogWarning(LOG_RF, "P25, force releasing all channel grants");

        std::vector<uint32_t> gntsToRel = std::vector<uint32_t>();
        for (auto it = m_grantChTable.begin(); it != m_grantChTable.end(); ++it) {
            uint32_t dstId = it->first;
            gntsToRel.push_back(dstId);
        }

        // release grants
        for (auto it = gntsToRel.begin(); it != gntsToRel.end(); ++it) {
            releaseDstIdGrant(*it, false);
        }

        return;
    }

    if (hasDstIdGranted(dstId)) {
        uint32_t chNo = m_grantChTable.at(dstId);

        if (m_verbose) {
            LogMessage(LOG_RF, "P25, releasing channel grant, chNo = %u, dstId = %u",
                chNo, dstId);
        }

        m_grantChTable[dstId] = 0U;
        m_voiceChTable.push_back(chNo);

        if (m_voiceGrantChCnt > 0U) {
            m_voiceGrantChCnt--;
            m_p25->m_siteData.setChCnt(m_voiceChCnt + m_voiceGrantChCnt);
        }
        else {
            m_voiceGrantChCnt = 0U;
            m_p25->m_siteData.setChCnt(m_voiceChCnt);
        }

        m_grantTimers[dstId].stop();
    }
}

/// <summary>
/// Helper to release group affiliations.
/// </summary>
/// <param name="dstId"></param>
/// <param name="releaseAll"></param>
void TrunkPacket::clearGrpAff(uint32_t dstId, bool releaseAll)
{
    if (dstId == 0U && !releaseAll) {
        return;
    }

    std::vector<uint32_t> srcToRel = std::vector<uint32_t>();
    if (dstId == 0U && releaseAll) {
        LogWarning(LOG_RF, "P25, releasing all group affiliations");
        for (auto it = m_grpAffTable.begin(); it != m_grpAffTable.end(); ++it) {
            uint32_t srcId = it->first;
            srcToRel.push_back(srcId);
        }
    }
    else {
        LogWarning(LOG_RF, "P25, releasing group affiliations, dstId = %u", dstId);
        for (auto it = m_grpAffTable.begin(); it != m_grpAffTable.end(); ++it) {
            uint32_t srcId = it->first;
            uint32_t grpId = it->second;
            if (grpId == dstId) {
                srcToRel.push_back(srcId);
            }
        }
    }

    // release affiliations
    for (auto it = srcToRel.begin(); it != srcToRel.end(); ++it) {
        writeRF_TSDU_U_Dereg_Ack(*it);
    }
}

/// <summary>
/// Updates the processor by the passed number of milliseconds.
/// </summary>
/// <param name="ms"></param>
void TrunkPacket::clock(uint32_t ms)
{
    if (m_p25->m_control) {
        // clock all the grant timers
        std::vector<uint32_t> gntsToRel = std::vector<uint32_t>();
        for (auto it = m_grantChTable.begin(); it != m_grantChTable.end(); ++it) {
            uint32_t dstId = it->first;

            m_grantTimers[dstId].clock(ms);
            if (m_grantTimers[dstId].isRunning() && m_grantTimers[dstId].hasExpired()) {
                gntsToRel.push_back(dstId);
            }
        }

        // release grants that have timed out
        for (auto it = gntsToRel.begin(); it != gntsToRel.end(); ++it) {
            releaseDstIdGrant(*it, false);
        }

        // clock adjacent site and SCCB update timers
        m_adjSiteUpdateTimer.clock(ms);
        if (m_adjSiteUpdateTimer.isRunning() && m_adjSiteUpdateTimer.hasExpired()) {
            // update adjacent site data
            for (auto it = m_adjSiteUpdateCnt.begin(); it != m_adjSiteUpdateCnt.end(); ++it) {
                uint8_t siteId = it->first;
                
                uint8_t updateCnt = it->second;
                if (updateCnt > 0U) {
                    updateCnt--;
                }
                
                if (updateCnt == 0U) {
                    SiteData siteData = m_adjSiteTable[siteId];
                    LogWarning(LOG_NET, P25_TSDU_STR ", TSBK_OSP_ADJ_STS_BCAST (Adjacent Site Status Broadcast), no data [FAILED], sysId = $%03X, rfss = $%02X, site = $%02X, chId = %u, chNo = %u, svcClass = $%02X",
                        siteData.sysId(), siteData.rfssId(), siteData.siteId(), siteData.channelId(), siteData.channelNo(), siteData.serviceClass());
                }

                m_adjSiteUpdateCnt[siteId] = updateCnt;
            }

            // update SCCB data
            for (auto it = m_sccbUpdateCnt.begin(); it != m_sccbUpdateCnt.end(); ++it) {
                uint8_t rfssId = it->first;

                uint8_t updateCnt = it->second;
                if (updateCnt > 0U) {
                    updateCnt--;
                }

                if (updateCnt == 0U) {
                    SiteData siteData = m_sccbTable[rfssId];
                    LogWarning(LOG_NET, P25_TSDU_STR ", TSBK_OSP_SCCB (Secondary Control Channel Broadcast), no data [FAILED], sysId = $%03X, rfss = $%02X, site = $%02X, chId = %u, chNo = %u, svcClass = $%02X",
                        siteData.sysId(), siteData.rfssId(), siteData.siteId(), siteData.channelId(), siteData.channelNo(), siteData.serviceClass());
                }

                m_sccbUpdateCnt[rfssId] = updateCnt;
            }

            m_adjSiteUpdateTimer.setTimeout(m_adjSiteUpdateInterval);
            m_adjSiteUpdateTimer.start();
        }
    }
}

/// <summary>
/// Helper to write a call alert packet.
/// </summary>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
void TrunkPacket::writeRF_TSDU_Call_Alrt(uint32_t srcId, uint32_t dstId)
{
    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_CALL_ALRT (Call Alert), srcId = %u, dstId = %u", srcId, dstId);
    }

    ::ActivityLog("P25", true, "call alert request from %u to %u", srcId, dstId);

    m_rfTSBK.setLCO(TSBK_IOSP_CALL_ALRT);
    m_rfTSBK.setSrcId(srcId);
    m_rfTSBK.setDstId(dstId);
    writeRF_TSDU_SBF(false);
}

/// <summary>
/// Helper to write a extended function packet.
/// </summary>
/// <param name="func"></param>
/// <param name="arg"></param>
/// <param name="dstId"></param>
void TrunkPacket::writeRF_TSDU_Ext_Func(uint32_t func, uint32_t arg, uint32_t dstId)
{
    uint8_t lco = m_rfTSBK.getLCO();
    uint8_t mfId = m_rfTSBK.getMFId();

    m_rfTSBK.setMFId(P25_MFG_STANDARD);

    m_rfTSBK.setLCO(TSBK_IOSP_EXT_FNCT);
    m_rfTSBK.setExtendedFunction(func);
    m_rfTSBK.setSrcId(arg);
    m_rfTSBK.setDstId(dstId);

    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_EXT_FNCT (Extended Function), op = $%02X, arg = %u, tgt = %u",
            m_rfTSBK.getExtendedFunction(), m_rfTSBK.getSrcId(), m_rfTSBK.getDstId());
    }

    // generate activity log entry
    if (func == P25_EXT_FNCT_CHECK) {
        ::ActivityLog("P25", true, "radio check request from %u to %u", arg, dstId);
    }
    else if (func == P25_EXT_FNCT_INHIBIT) {
        ::ActivityLog("P25", true, "radio inhibit request from %u to %u", arg, dstId);
    }
    else if (func == P25_EXT_FNCT_UNINHIBIT) {
        ::ActivityLog("P25", true, "radio uninhibit request from %u to %u", arg, dstId);
    }

    writeRF_TSDU_SBF(false);

    m_rfTSBK.setLCO(lco);
    m_rfTSBK.setMFId(mfId);
}

/// <summary>
/// Helper to write a group affiliation query packet.
/// </summary>
/// <param name="dstId"></param>
void TrunkPacket::writeRF_TSDU_Grp_Aff_Q(uint32_t dstId)
{
    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_OSP_GRP_AFF_Q (Group Affiliation Query), dstId = %u", dstId);
    }

    ::ActivityLog("P25", true, "group affiliation query command from %u to %u", P25_WUID_FNE, dstId);

    m_rfTSBK.setLCO(TSBK_OSP_GRP_AFF_Q);
    m_rfTSBK.setSrcId(P25_WUID_FNE);
    m_rfTSBK.setDstId(dstId);
    writeRF_TSDU_SBF(true);
}

/// <summary>
/// Helper to write a unit registration command packet.
/// </summary>
/// <param name="dstId"></param>
void TrunkPacket::writeRF_TSDU_U_Reg_Cmd(uint32_t dstId)
{
    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_OSP_U_REG_CMD (Unit Registration Command), dstId = %u", dstId);
    }

    ::ActivityLog("P25", true, "unit registration command from %u to %u", P25_WUID_FNE, dstId);

    m_rfTSBK.setLCO(TSBK_OSP_U_REG_CMD);
    m_rfTSBK.setSrcId(P25_WUID_FNE);
    m_rfTSBK.setDstId(dstId);
    writeRF_TSDU_SBF(true);
}

/// <summary>
/// Helper to write a Motorola patch packet.
/// </summary>
/// <param name="group1"></param>
/// <param name="group2"></param>
/// <param name="group3"></param>
void TrunkPacket::writeRF_TSDU_Mot_Patch(uint32_t group1, uint32_t group2, uint32_t group3)
{
    uint8_t lco = m_rfTSBK.getLCO();

    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_OSP_MOT_GRG_ADD (Group Regroup Add - Patch Supergroup), superGrp = %u, group1 = %u, group2 = %u, group3 = %u",
            m_patchSuperGroup, group1, group2, group3);
    }

    m_rfTSBK.setLCO(TSBK_OSP_MOT_GRG_ADD);
    m_rfTSBK.setMFId(P25_MFG_MOT);
    m_rfTSBK.setPatchSuperGroupId(m_patchSuperGroup);
    m_rfTSBK.setPatchGroup1Id(group1);
    m_rfTSBK.setPatchGroup2Id(group2);
    m_rfTSBK.setPatchGroup3Id(group3);
    writeRF_TSDU_SBF(true);

    m_rfTSBK.setLCO(lco);
    m_rfTSBK.setMFId(P25_MFG_STANDARD);
}

/// <summary>
/// Helper to change the TSBK verbose state.
/// </summary>
/// <param name="verbose">Flag indicating whether TSBK dumping is enabled.</param>
void TrunkPacket::setTSBKVerbose(bool verbose)
{
    m_dumpTSBK = verbose;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the TrunkPacket class.
/// </summary>
/// <param name="p25">Instance of the Control class.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="dumpTSBKData">Flag indicating whether TSBK data is dumped to the log.</param>
/// <param name="debug">Flag indicating whether P25 debug is enabled.</param>
/// <param name="verbose">Flag indicating whether P25 verbose logging is enabled.</param>
TrunkPacket::TrunkPacket(Control* p25, network::BaseNetwork* network, bool dumpTSBKData, bool debug, bool verbose) :
    m_p25(p25),
    m_network(network),
    m_patchSuperGroup(0xFFFFU),
    m_verifyAff(false),
    m_verifyReg(false),
    m_rfTSBK(SiteData(), lookups::IdenTable()),
    m_netTSBK(SiteData(), lookups::IdenTable()),
    m_rfMBF(NULL),
    m_mbfCnt(0U),
    m_mbfIdenCnt(0U),
    m_mbfAdjSSCnt(0U),
    m_mbfSCCBCnt(0U),
    m_mbfGrpGrntCnt(0U),
    m_voiceChTable(),
    m_adjSiteTable(),
    m_adjSiteUpdateCnt(),
    m_sccbTable(),
    m_sccbUpdateCnt(),
    m_unitRegTable(),
    m_grpAffTable(),
    m_grantChTable(),
    m_grantTimers(),
    m_voiceChCnt(1U),
    m_voiceGrantChCnt(0U),
    m_noStatusAck(false),
    m_noMessageAck(true),
    m_unitToUnitAvailCheck(true),
    m_adjSiteUpdateTimer(1000U),
    m_adjSiteUpdateInterval(ADJ_SITE_TIMER_TIMEOUT),
    m_ctrlTSDUMBF(true),
    m_dumpTSBK(dumpTSBKData),
    m_verbose(verbose),
    m_debug(debug)
{
    m_rfMBF = new uint8_t[P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(m_rfMBF, 0x00U, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);

    m_voiceChTable.clear();
 
    m_adjSiteTable.clear();
    m_adjSiteUpdateCnt.clear();

    m_sccbTable.clear();
    m_sccbUpdateCnt.clear();
 
    m_unitRegTable.clear();
    m_grpAffTable.clear();

    m_grantChTable.clear();
    m_grantTimers.clear();

    m_adjSiteUpdateInterval = ADJ_SITE_TIMER_TIMEOUT + m_p25->m_ccBcstInterval;
    m_adjSiteUpdateTimer.setTimeout(m_adjSiteUpdateInterval);
    m_adjSiteUpdateTimer.start();
}

/// <summary>
/// Finalizes a instance of the TrunkPacket class.
/// </summary>
TrunkPacket::~TrunkPacket()
{
    delete[] m_rfMBF;
}

/// <summary>
/// Write data processed from RF to the network.
/// </summary>
/// <param name="data"></param>
/// <param name="autoReset"></param>
void TrunkPacket::writeNetworkRF(const uint8_t* data, bool autoReset)
{
    assert(data != NULL);

    if (m_network == NULL)
        return;

    if (m_p25->m_rfTimeout.isRunning() && m_p25->m_rfTimeout.hasExpired())
        return;

    m_network->writeP25TSDU(m_rfTSBK, data);
    if (autoReset)
        m_network->resetP25();
}

/// <summary>
/// Helper to write control channel packet data.
/// </summary>
/// <param name="frameCnt"></param>
/// <param name="n"></param>
/// <param name="adjSS"></param>
void TrunkPacket::writeRF_ControlData(uint8_t frameCnt, uint8_t n, bool adjSS)
{
    if (!m_p25->m_control)
        return;

    resetRF();

    if (m_debug) {
        LogDebug(LOG_P25, "writeRF_ControlData, mbfCnt = %u, frameCnt = %u, seq = %u, adjSS = %u", m_mbfCnt, frameCnt, n, adjSS);
    }

    bool forcePad = false;
    bool alt = (frameCnt % 2U) > 0U;
    switch (n)
    {
    /** required data */
    case 0:
    default:
        queueRF_TSBK_Ctrl(TSBK_OSP_IDEN_UP);
        break;
    case 1:
        if (alt)
            queueRF_TSBK_Ctrl(TSBK_OSP_RFSS_STS_BCAST);
        else
            queueRF_TSBK_Ctrl(TSBK_OSP_NET_STS_BCAST);
        break;
    case 2:
        if (alt)
            queueRF_TSBK_Ctrl(TSBK_OSP_NET_STS_BCAST);
        else
            queueRF_TSBK_Ctrl(TSBK_OSP_RFSS_STS_BCAST);
        break;
    case 3:
        if (alt)
            queueRF_TSBK_Ctrl(TSBK_OSP_RFSS_STS_BCAST);
        else
            queueRF_TSBK_Ctrl(TSBK_OSP_NET_STS_BCAST);
        break;
    /** update data */
    case 4:
        if (m_grantChTable.size() > 0) {
            queueRF_TSBK_Ctrl(TSBK_OSP_GRP_VCH_GRANT_UPD);
        }
        break;
    /** extra data */
    case 5:
        queueRF_TSBK_Ctrl(TSBK_OSP_SNDCP_CH_ANN);
        break;
    case 6:
        // write ADJSS
        if (adjSS && m_adjSiteTable.size() > 0) {
            queueRF_TSBK_Ctrl(TSBK_OSP_ADJ_STS_BCAST);
            break;
        } else {
            forcePad = true;
        }
        break;
    case 7:
        // write SCCB
        if (adjSS && m_sccbTable.size() > 0) {
            queueRF_TSBK_Ctrl(TSBK_OSP_SCCB_EXP);
            break;
        }
        break;
    }

    // should we insert the BSI bursts?
    bool bsi = (frameCnt % 127U) == 0U;
    if (bsi && n > 3U) {
        queueRF_TSBK_Ctrl(TSBK_OSP_MOT_CC_BSI);
    }

    // add padding after the last sequence or if forced; and only
    // if we're doing multiblock frames (MBF)
    if ((n >= 4U || forcePad) && m_ctrlTSDUMBF)
    {
        // pad MBF if we have 1 queued TSDUs
        if (m_mbfCnt == 1U) {
            queueRF_TSBK_Ctrl(TSBK_OSP_RFSS_STS_BCAST);
            queueRF_TSBK_Ctrl(TSBK_OSP_NET_STS_BCAST);
            if (m_debug) {
                LogDebug(LOG_P25, "writeRF_ControlData, have 1 pad 2, mbfCnt = %u", m_mbfCnt);
            }
        }

        // pad MBF if we have 2 queued TSDUs
        if (m_mbfCnt == 2U) {
            std::vector<lookups::IdenTable> entries = m_p25->m_idenTable->list();
            if (entries.size() > 1U) {
                queueRF_TSBK_Ctrl(TSBK_OSP_IDEN_UP);
            }
            else {
                queueRF_TSBK_Ctrl(TSBK_OSP_RFSS_STS_BCAST);
            }

            if (m_debug) {
                LogDebug(LOG_P25, "writeRF_ControlData, have 2 pad 1, mbfCnt = %u", m_mbfCnt);
            }
        }

        // reset MBF count
        m_mbfCnt = 0U;
    }
}

/// <summary>
/// Helper to write a P25 TDU w/ link control packet.
/// </summary>
/// <param name="lc"></param>
/// <param name="noNetwork"></param>
void TrunkPacket::writeRF_TDULC(lc::TDULC lc, bool noNetwork)
{
    uint8_t data[P25_TDULC_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, P25_TDULC_FRAME_LENGTH_BYTES);

    // Generate Sync
    Sync::addP25Sync(data + 2U);

    // Generate NID
    m_p25->m_nid.encode(data + 2U, P25_DUID_TDULC);

    // Generate TDULC Data
    lc.encode(data + 2U);

    // Add busy bits
    m_p25->addBusyBits(data + 2U, P25_TDULC_FRAME_LENGTH_BITS, true, true);

    m_p25->m_rfTimeout.stop();

    if (!noNetwork)
        writeNetworkRF(data + 2U, P25_DUID_TDULC);

    if (m_p25->m_duplex) {
        data[0U] = modem::TAG_EOT;
        data[1U] = 0x00U;

        m_p25->writeQueueRF(data, P25_TDULC_FRAME_LENGTH_BYTES + 2U);
    }

    //if (m_verbose) {
    //    LogMessage(LOG_RF, P25_TDULC_STR ", lc = $%02X, srcId = %u", m_rfTDULC.getLCO(), m_rfTDULC.getSrcId());
    //}
}

/// <summary>
/// Helper to write a P25 TDU w/ link control channel release packet.
/// </summary>
/// <param name="grp"></param>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
void TrunkPacket::writeRF_TDULC_ChanRelease(bool grp, uint32_t srcId, uint32_t dstId)
{
    if (!m_p25->m_duplex) {
        return;
    }

    uint32_t count = m_p25->m_hangCount / 2;
    lc::TDULC lc = lc::TDULC(m_p25->m_siteData, m_p25->m_idenEntry, m_dumpTSBK);

    if (m_p25->m_control) {
        for (uint32_t i = 0; i < count; i++) {
            if ((srcId != 0U) && (dstId != 0U)) {
                lc.setSrcId(srcId);
                lc.setDstId(dstId);
                lc.setEmergency(false);

                if (grp) {
                    lc.setLCO(LC_GROUP);
                    writeRF_TDULC(lc, true);
                }
                else {
                    lc.setLCO(LC_PRIVATE);
                    writeRF_TDULC(lc, true);
                }
            }

            lc.setLCO(LC_NET_STS_BCAST);
            writeRF_TDULC(lc, true);
            lc.setLCO(LC_RFSS_STS_BCAST);
            writeRF_TDULC(lc, true);
        }
    }

    if (m_verbose) {
        LogMessage(LOG_RF, P25_TDULC_STR ", LC_CALL_TERM (Call Termination), srcId = %u, dstId = %u", lc.getSrcId(), lc.getDstId());
    }

    lc.setLCO(LC_CALL_TERM);
    writeRF_TDULC(lc, true);

    if (m_p25->m_control) {
        writeNet_TSDU_Call_Term(srcId, dstId);
    }
}

/// <summary>
/// Helper to write a single-block P25 TSDU packet.
/// </summary>
/// <param name="noNetwork"></param>
/// <param name="clearBeforeWrite"></param>
/// <param name="force"></param>
void TrunkPacket::writeRF_TSDU_SBF(bool noNetwork, bool clearBeforeWrite, bool force)
{
    if (!m_p25->m_control)
        return;

    uint8_t data[P25_TSDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, P25_TSDU_FRAME_LENGTH_BYTES);

    // Generate Sync
    Sync::addP25Sync(data + 2U);

    // Generate NID
    m_p25->m_nid.encode(data + 2U, P25_DUID_TSDU);

    // Generate TSBK block
    m_rfTSBK.setLastBlock(true); // always set last block -- this a Single Block TSDU
    m_rfTSBK.encode(data + 2U);

    if (m_debug) {
        LogDebug(LOG_RF, P25_TSDU_STR ", lco = $%02X, mfId = $%02X, lastBlock = %u, AIV = %u, EX = %u, srcId = %u, dstId = %u, sysId = $%03X, netId = $%05X",
            m_rfTSBK.getLCO(), m_rfTSBK.getMFId(), m_rfTSBK.getLastBlock(), m_rfTSBK.getAIV(), m_rfTSBK.getEX(), m_rfTSBK.getSrcId(), m_rfTSBK.getDstId(),
            m_rfTSBK.getSysId(), m_rfTSBK.getNetId());

        Utils::dump(1U, "!!! *TSDU (SBF) TSBK Block Data", data + P25_PREAMBLE_LENGTH_BYTES + 2U, P25_TSBK_FEC_LENGTH_BYTES);
    }

    // Add busy bits
    m_p25->addBusyBits(data + 2U, P25_TSDU_FRAME_LENGTH_BITS, true, false);

    // Set first busy bits to 1,1
    m_p25->setBusyBits(data + 2U, P25_SS0_START, true, true);

    if (!noNetwork)
        writeNetworkRF(data + 2U, true);

    if (!force) {
        if (m_p25->m_dedicatedControl && m_ctrlTSDUMBF) {
            writeRF_TSDU_MBF(clearBeforeWrite);
            return;
        }

        if (m_p25->m_ccRunning && m_ctrlTSDUMBF) {
            writeRF_TSDU_MBF(clearBeforeWrite);
            return;
        }

        if (clearBeforeWrite) {
            m_p25->m_modem->clearP25Data();
            m_p25->m_queue.clear();
        }
    }

    if (m_p25->m_duplex) {
        data[0U] = modem::TAG_DATA;
        data[1U] = 0x00U;

        m_p25->writeQueueRF(data, P25_TSDU_FRAME_LENGTH_BYTES + 2U);
    }
}

/// <summary>
/// Helper to write a multi-block (3-block) P25 TSDU packet.
/// </summary>
/// <param name="clearBeforeWrite"></param>
void TrunkPacket::writeRF_TSDU_MBF(bool clearBeforeWrite)
{
    if (!m_p25->m_control) {
        ::memset(m_rfMBF, 0x00U, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);
        m_mbfCnt = 0U;
        return;
    }

    uint8_t tsbk[P25_TSBK_FEC_LENGTH_BYTES];
    ::memset(tsbk, 0x00U, P25_TSBK_FEC_LENGTH_BYTES);

    // LogDebug(LOG_P25, "writeRF_TSDU_MBF, mbfCnt = %u", m_mbfCnt);

    // trunking data is unsupported in simplex operation
    if (!m_p25->m_duplex) {
        ::memset(m_rfMBF, 0x00U, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);
        m_mbfCnt = 0U;
        return;
    }

    if (m_mbfCnt == 0U) {
        ::memset(m_rfMBF, 0x00U, P25_TSBK_FEC_LENGTH_BYTES * TSBK_MBF_CNT);
    }

    // trigger encoding of last block and write to queue
    if (m_mbfCnt + 1U == TSBK_MBF_CNT) {
        // Generate TSBK block
        m_rfTSBK.setLastBlock(true); // set last block
        m_rfTSBK.encode(tsbk, true);

        if (m_debug) {
            LogDebug(LOG_RF, P25_TSDU_STR " (MBF), lco = $%02X, mfId = $%02X, lastBlock = %u, AIV = %u, EX = %u, srcId = %u, dstId = %u, sysId = $%03X, netId = $%05X",
                m_rfTSBK.getLCO(), m_rfTSBK.getMFId(), m_rfTSBK.getLastBlock(), m_rfTSBK.getAIV(), m_rfTSBK.getEX(), m_rfTSBK.getSrcId(), m_rfTSBK.getDstId(),
                m_rfTSBK.getSysId(), m_rfTSBK.getNetId());

            Utils::dump(1U, "!!! *TSDU MBF Last TSBK Block", tsbk, P25_TSBK_FEC_LENGTH_BYTES);
        }

        Utils::setBitRange(tsbk, m_rfMBF, (m_mbfCnt * P25_TSBK_FEC_LENGTH_BITS), P25_TSBK_FEC_LENGTH_BITS);

        // Generate TSDU frame
        uint8_t tsdu[P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES];
        ::memset(tsdu, 0x00U, P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES);

        uint32_t offset = 0U;
        for (uint8_t i = 0U; i < m_mbfCnt + 1U; i++) {
            ::memset(tsbk, 0x00U, P25_TSBK_FEC_LENGTH_BYTES);
            Utils::getBitRange(m_rfMBF, tsbk, offset, P25_TSBK_FEC_LENGTH_BITS);

            if (m_debug) {
                LogDebug(LOG_RF, P25_TSDU_STR " (MBF), lco = $%02X, mfId = $%02X, lastBlock = %u, AIV = %u, EX = %u, srcId = %u, dstId = %u, sysId = $%03X, netId = $%05X",
                    m_rfTSBK.getLCO(), m_rfTSBK.getMFId(), m_rfTSBK.getLastBlock(), m_rfTSBK.getAIV(), m_rfTSBK.getEX(), m_rfTSBK.getSrcId(), m_rfTSBK.getDstId(),
                    m_rfTSBK.getSysId(), m_rfTSBK.getNetId());

                Utils::dump(1U, "!!! *TSDU (MBF) TSBK Block", tsbk, P25_TSBK_FEC_LENGTH_BYTES);
            }
                
            // Add TSBK data
            Utils::setBitRange(tsbk, tsdu, offset, P25_TSBK_FEC_LENGTH_BITS);

            offset += P25_TSBK_FEC_LENGTH_BITS;
        }

        // Utils::dump(2U, "!!! *TSDU DEBUG - tsdu", tsdu, P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES);

        uint8_t data[P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES + 2U];
        ::memset(data + 2U, 0x00U, P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES);

        // Generate Sync
        Sync::addP25Sync(data + 2U);

        // Generate NID
        m_p25->m_nid.encode(data + 2U, P25_DUID_TSDU);

        // interleave
        P25Utils::encode(tsdu, data + 2U, 114U, 720U);

        // Add busy bits
        m_p25->addBusyBits(data + 2U, P25_TSDU_TRIPLE_FRAME_LENGTH_BITS, true, false);

        // Add idle bits
        addIdleBits(data + 2U, P25_TSDU_TRIPLE_FRAME_LENGTH_BITS, true, true);

        data[0U] = modem::TAG_DATA;
        data[1U] = 0x00U;
        
        if (clearBeforeWrite) {
            m_p25->m_modem->clearP25Data();
            m_p25->m_queue.clear();
        }

        m_p25->writeQueueRF(data, P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES + 2U);

        ::memset(m_rfMBF, 0x00U, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);
        m_mbfCnt = 0U;
        return;
    }

    // Generate TSBK block
    m_rfTSBK.setLastBlock(false); // clear last block
    m_rfTSBK.encode(tsbk, true);

    if (m_debug) {
        LogDebug(LOG_RF, P25_TSDU_STR " (MBF), lco = $%02X, mfId = $%02X, lastBlock = %u, AIV = %u, EX = %u, srcId = %u, dstId = %u, sysId = $%03X, netId = $%05X",
            m_rfTSBK.getLCO(), m_rfTSBK.getMFId(), m_rfTSBK.getLastBlock(), m_rfTSBK.getAIV(), m_rfTSBK.getEX(), m_rfTSBK.getSrcId(), m_rfTSBK.getDstId(),
            m_rfTSBK.getSysId(), m_rfTSBK.getNetId());

        Utils::dump(1U, "!!! *TSDU MBF Block Data", tsbk, P25_TSBK_FEC_LENGTH_BYTES);
    }

    Utils::setBitRange(tsbk, m_rfMBF, (m_mbfCnt * P25_TSBK_FEC_LENGTH_BITS), P25_TSBK_FEC_LENGTH_BITS);
    m_mbfCnt++;
}

/// <summary>
/// Helper to generate the given control TSBK into the TSDU frame queue.
/// </summary>
/// <param name="lco"></param>
void TrunkPacket::queueRF_TSBK_Ctrl(uint8_t lco)
{
    if (!m_p25->m_control)
        return;

    resetRF();

    switch (lco) {
        case TSBK_OSP_GRP_VCH_GRANT_UPD:
            // write group voice grant update
            if (m_grantChTable.size() > 0) {
                if (m_mbfGrpGrntCnt >= m_grantChTable.size())
                    m_mbfGrpGrntCnt = 0U;

                if (m_debug) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_OSP_GRP_VCH_GRANT_UPD (Group Voice Channel Grant Update)");
                }

                bool noData = false;
                uint8_t i = 0U;
                for (auto it = m_grantChTable.begin(); it != m_grantChTable.end(); ++it) {
                    // no good very bad way of skipping entries...
                    if (i != m_mbfGrpGrntCnt) {
                        i++;
                        continue;
                    }
                    else {
                        uint32_t dstId = it->first;
                        uint32_t chNo = it->second;

                        if (chNo == 0U) {
                            noData = true;
                            m_mbfGrpGrntCnt++;
                            break;
                        }
                        else {
                            // transmit group voice grant update
                            m_rfTSBK.setLCO(TSBK_OSP_GRP_VCH_GRANT_UPD);
                            m_rfTSBK.setDstId(dstId);
                            m_rfTSBK.setGrpVchNo(chNo);

                            m_mbfGrpGrntCnt++;
                            break;
                        }
                    }
                }

                if (noData) {
                    return; // don't create anything
                }
            }
            else {
                return; // don't create anything
            }
            break;
        case TSBK_OSP_IDEN_UP:
            {
                if (m_debug) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_OSP_IDEN_UP (Identity Update)");
                }

                std::vector<lookups::IdenTable> entries = m_p25->m_idenTable->list();
                if (m_mbfIdenCnt >= entries.size())
                    m_mbfIdenCnt = 0U;

                uint8_t i = 0U;
                for (auto it = entries.begin(); it != entries.end(); ++it) {
                    // no good very bad way of skipping entries...
                    if (i != m_mbfIdenCnt) {
                        i++;
                        continue;
                    }
                    else {
                        lookups::IdenTable entry = *it;

                        // LogDebug(LOG_P25, "baseFrequency = %uHz, txOffsetMhz = %fMHz, chBandwidthKhz = %fKHz, chSpaceKhz = %fKHz",
                        //    entry.baseFrequency(), entry.txOffsetMhz(), entry.chBandwidthKhz(), entry.chSpaceKhz());

                        // handle 700/800/900 identities
                        if (entry.baseFrequency() >= 762000000U) {
                            m_rfTSBK.siteIdenEntry(entry);

                            // transmit channel ident broadcast
                            m_rfTSBK.setLCO(TSBK_OSP_IDEN_UP);
                        }
                        else {
                            // handle as a VHF/UHF identity
                            m_rfTSBK.siteIdenEntry(entry);

                            // transmit channel ident broadcast
                            m_rfTSBK.setLCO(TSBK_OSP_IDEN_UP_VU);
                        }

                        m_mbfIdenCnt++;
                        break;
                    }
                }
            }
            break;
        case TSBK_OSP_NET_STS_BCAST:
            if (m_debug) {
                LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_OSP_NET_STS_BCAST (Network Status Broadcast)");
            }

            // transmit net status burst
            m_rfTSBK.setLCO(TSBK_OSP_NET_STS_BCAST);
            break;
        case TSBK_OSP_RFSS_STS_BCAST:
            if (m_debug) {
                LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_OSP_RFSS_STS_BCAST (RFSS Status Broadcast)");
            }

            // transmit rfss status burst
            m_rfTSBK.setLCO(TSBK_OSP_RFSS_STS_BCAST);
            break;
        case TSBK_OSP_ADJ_STS_BCAST:
            // write ADJSS
            if (m_adjSiteTable.size() > 0) {
                if (m_mbfAdjSSCnt >= m_adjSiteTable.size())
                    m_mbfAdjSSCnt = 0U;

                if (m_debug) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_OSP_ADJ_STS_BCAST (Adjacent Site Broadcast)");
                }

                uint8_t i = 0U;
                for (auto it = m_adjSiteTable.begin(); it != m_adjSiteTable.end(); ++it) {
                    // no good very bad way of skipping entries...
                    if (i != m_mbfAdjSSCnt) {
                        i++;
                        continue;
                    }
                    else {
                        SiteData site = it->second;

                        uint8_t cfva = P25_CFVA_NETWORK;
                        if (m_adjSiteUpdateCnt[site.siteId()] == 0U) {
                            cfva |= P25_CFVA_FAILURE;
                        }
                        else {
                            cfva |= P25_CFVA_VALID;
                        }

                        // transmit adjacent site broadcast
                        m_rfTSBK.setLCO(TSBK_OSP_ADJ_STS_BCAST);
                        m_rfTSBK.setAdjSiteCFVA(cfva);
                        m_rfTSBK.setAdjSiteSysId(site.sysId());
                        m_rfTSBK.setAdjSiteRFSSId(site.rfssId());
                        m_rfTSBK.setAdjSiteId(site.siteId());
                        m_rfTSBK.setAdjSiteChnId(site.channelId());
                        m_rfTSBK.setAdjSiteChnNo(site.channelNo());
                        m_rfTSBK.setAdjSiteSvcClass(site.serviceClass());

                        m_mbfAdjSSCnt++;
                        break;
                    }
                }
            }
            else {
                return; // don't create anything
            }
            break;
        case TSBK_OSP_SCCB_EXP:
            // write SCCB
            if (m_sccbTable.size() > 0) {
                if (m_mbfSCCBCnt >= m_sccbTable.size())
                    m_mbfSCCBCnt = 0U;

                if (m_debug) {
                    LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_OSP_SCCB_EXP (Secondary Control Channel Broadcast)");
                }

                uint8_t i = 0U;
                for (auto it = m_sccbTable.begin(); it != m_sccbTable.end(); ++it) {
                    // no good very bad way of skipping entries...
                    if (i != m_mbfSCCBCnt) {
                        i++;
                        continue;
                    }
                    else {
                        SiteData site = it->second;

                        // transmit SCCB broadcast
                        m_rfTSBK.setLCO(TSBK_OSP_SCCB_EXP);
                        m_rfTSBK.setSCCBChnId1(site.channelId());
                        m_rfTSBK.setSCCBChnNo(site.channelNo());

                        m_mbfSCCBCnt++;
                        break;
                    }
                }
            }
            else {
                return; // don't create anything
            }
            break;
        case TSBK_OSP_SNDCP_CH_ANN:
            if (m_debug) {
                LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_OSP_SNDCP_CH_ANN (SNDCP Channel Announcement)");
            }

            // transmit SNDCP announcement
            m_rfTSBK.setLCO(TSBK_OSP_SNDCP_CH_ANN);
            break;

        /** Motorola CC data */
        case TSBK_OSP_MOT_PSH_CCH:
            if (m_debug) {
                LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_OSP_MOT_PSH_CCH (Motorola Planned Shutdown)");
            }

            // transmit motorola PSH CCH burst
            m_rfTSBK.setLCO(TSBK_OSP_MOT_PSH_CCH);
            m_rfTSBK.setMFId(P25_MFG_MOT);
            break;

        case TSBK_OSP_MOT_CC_BSI:
            if (m_debug) {
                LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_OSP_MOT_CC_BSI (Motorola Control Channel BSI)");
            }

            // transmit motorola CC BSI burst
            m_rfTSBK.setLCO(TSBK_OSP_MOT_CC_BSI);
            m_rfTSBK.setMFId(P25_MFG_MOT);
            break;
    }

    m_rfTSBK.setLastBlock(true); // always set last block

    // are we transmitting CC as a multi-block?
    if (m_ctrlTSDUMBF) {
        writeRF_TSDU_MBF();
    }
    else {
        writeRF_TSDU_SBF(false);
    }
}

/// <summary>
/// Helper to write a grant packet.
/// </summary>
/// <param name="grp"></param>
/// <param name="skip"></param>
/// <param name="net"></param>
/// <returns></returns>
bool TrunkPacket::writeRF_TSDU_Grant(bool grp, bool skip, bool net)
{
    uint8_t lco = m_rfTSBK.getLCO();

    if (m_rfTSBK.getDstId() == P25_TGID_ALL) {
        return true; // do not generate grant packets for $FFFF (All Call) TGID
    }

    // are we skipping checking?
    if (!skip) {
        if (m_p25->m_rfState != RS_RF_LISTENING && m_p25->m_rfState != RS_RF_DATA) {
            if (!net) {
                LogWarning(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_GRP_VCH (Group Voice Channel Request) denied, traffic in progress, dstId = %u", m_rfTSBK.getDstId());
                writeRF_TSDU_Deny(P25_DENY_RSN_PTT_COLLIDE, (grp) ? TSBK_IOSP_GRP_VCH : TSBK_IOSP_UU_VCH);

                ::ActivityLog("P25", true, "group grant request from %u to TG %u denied", m_rfTSBK.getSrcId(), m_rfTSBK.getDstId());
                m_p25->m_rfState = RS_RF_REJECTED;
            }

            m_rfTSBK.setLCO(lco);
            return false;
        }

        if (m_p25->m_netState != RS_NET_IDLE && m_rfTSBK.getDstId() == m_p25->m_netLastDstId) {
            if (!net) {
                LogWarning(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_GRP_VCH (Group Voice Channel Request) denied, traffic in progress, dstId = %u", m_rfTSBK.getDstId());
                writeRF_TSDU_Deny(P25_DENY_RSN_PTT_COLLIDE, (grp) ? TSBK_IOSP_GRP_VCH : TSBK_IOSP_UU_VCH);

                ::ActivityLog("P25", true, "group grant request from %u to TG %u denied", m_rfTSBK.getSrcId(), m_rfTSBK.getDstId());
                m_p25->m_rfState = RS_RF_REJECTED;
            }

            m_rfTSBK.setLCO(lco);
            return false;
        }

        // don't transmit grants if the destination ID's don't match and the network TG hang timer is running
        if (m_p25->m_rfLastDstId != 0U) {
            if (m_p25->m_rfLastDstId != m_rfTSBK.getDstId() && (m_p25->m_rfTGHang.isRunning() && !m_p25->m_rfTGHang.hasExpired())) {
                if (!net) {
                    writeRF_TSDU_Deny(P25_DENY_RSN_PTT_BONK, (grp) ? TSBK_IOSP_GRP_VCH : TSBK_IOSP_UU_VCH);
                    m_p25->m_rfState = RS_RF_REJECTED;
                }

                m_rfTSBK.setLCO(lco);
                return false;
            }
        }

        if (!hasDstIdGranted(m_rfTSBK.getDstId())) {
            if (m_voiceChTable.empty()) {
                if (grp) {
                    if (!net) {
                        LogWarning(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_GRP_VCH (Group Voice Channel Request) queued, no channels available, dstId = %u", m_rfTSBK.getDstId());
                        writeRF_TSDU_Queue(P25_QUE_RSN_CHN_RESOURCE_NOT_AVAIL, TSBK_IOSP_GRP_VCH);

                        ::ActivityLog("P25", true, "group grant request from %u to TG %u queued", m_rfTSBK.getSrcId(), m_rfTSBK.getDstId());
                        m_p25->m_rfState = RS_RF_REJECTED;
                    }

                    m_rfTSBK.setLCO(lco);
                    return false;
                }
                else {
                    if (!net) {
                        LogWarning(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_UU_VCH (Unit-to-Unit Voice Channel Request) queued, no channels available, dstId = %u", m_rfTSBK.getDstId());
                        writeRF_TSDU_Queue(P25_QUE_RSN_CHN_RESOURCE_NOT_AVAIL, TSBK_IOSP_UU_VCH);

                        ::ActivityLog("P25", true, "unit-to-unit grant request from %u to %u queued", m_rfTSBK.getSrcId(), m_rfTSBK.getDstId());
                        m_p25->m_rfState = RS_RF_REJECTED;
                    }

                    m_rfTSBK.setLCO(lco);
                    return false;
                }
            }
            else {
                uint32_t chNo = m_voiceChTable.at(0);
                auto it = std::find(m_voiceChTable.begin(), m_voiceChTable.end(), chNo);
                m_voiceChTable.erase(it);

                m_grantChTable[m_rfTSBK.getDstId()] = chNo;
                m_rfTSBK.setGrpVchNo(chNo);

                m_grantTimers[m_rfTSBK.getDstId()] = Timer(1000U, GRANT_TIMER_TIMEOUT);
                m_grantTimers[m_rfTSBK.getDstId()].start();

                m_voiceGrantChCnt++;
                m_p25->m_siteData.setChCnt(m_voiceChCnt + m_voiceGrantChCnt);
            }
        }
        else {
            uint32_t chNo = m_grantChTable[m_rfTSBK.getDstId()];
            m_rfTSBK.setGrpVchNo(chNo);

            m_grantTimers[m_rfTSBK.getDstId()].start();
        }
    }

    if (grp) {
        if (!net) {
            ::ActivityLog("P25", true, "group grant request from %u to TG %u", m_rfTSBK.getSrcId(), m_rfTSBK.getDstId());
        }

        if (m_verbose) {
            LogMessage((net) ? LOG_NET : LOG_RF, P25_TSDU_STR ", TSBK_IOSP_GRP_VCH (Group Voice Channel Grant), emerg = %u, encrypt = %u, prio = %u, chNo = %u, srcId = %u, dstId = %u",
                m_rfTSBK.getEmergency(), m_rfTSBK.getEncrypted(), m_rfTSBK.getPriority(), m_rfTSBK.getGrpVchNo(), m_rfTSBK.getSrcId(), m_rfTSBK.getDstId());
        }

        // transmit group grant
        m_rfTSBK.setLCO(TSBK_IOSP_GRP_VCH);
        writeRF_TSDU_SBF(false, true, net);
    }
    else {
        if (!net) {
            ::ActivityLog("P25", true, "unit-to-unit grant request from %u to %u", m_rfTSBK.getSrcId(), m_rfTSBK.getDstId());
        }

        if (m_verbose) {
            LogMessage((net) ? LOG_NET : LOG_RF, P25_TSDU_STR ", TSBK_IOSP_UU_VCH (Unit-to-Unit Voice Channel Grant), emerg = %u, encrypt = %u, prio = %u, chNo = %u, srcId = %u, dstId = %u",
                m_rfTSBK.getEmergency(), m_rfTSBK.getEncrypted(), m_rfTSBK.getPriority(), m_rfTSBK.getGrpVchNo(), m_rfTSBK.getSrcId(), m_rfTSBK.getDstId());
        }

        // transmit private grant
        m_rfTSBK.setLCO(TSBK_IOSP_UU_VCH);
        writeRF_TSDU_SBF(false, true, net);
    }

    m_rfTSBK.setLCO(lco);
    return true;
}

/// <summary>
/// Helper to write a unit to unit answer request packet.
/// </summary>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
void TrunkPacket::writeRF_TSDU_UU_Ans_Req(uint32_t srcId, uint32_t dstId)
{
    uint8_t lco = m_rfTSBK.getLCO();

    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_UU_ANS (Unit-to-Unit Answer Request), srcId = %u, dstId = %u", srcId, dstId);
    }

    m_rfTSBK.setLCO(TSBK_IOSP_UU_ANS);
    m_rfTSBK.setSrcId(srcId);
    m_rfTSBK.setDstId(dstId);
    m_rfTSBK.setVendorSkip(true);
    writeRF_TSDU_SBF(false);

    m_rfTSBK.setLCO(lco);
    m_rfTSBK.setVendorSkip(false);
}

/// <summary>
/// Helper to write a acknowledge packet.
/// </summary>
/// <param name="srcId"></param>
/// <param name="service"></param>
/// <param name="noNetwork"></param>
void TrunkPacket::writeRF_TSDU_ACK_FNE(uint32_t srcId, uint32_t service, bool extended, bool noNetwork)
{
    uint8_t lco = m_rfTSBK.getLCO();
    uint8_t mfId = m_rfTSBK.getMFId();
    uint32_t _srcId = m_rfTSBK.getSrcId();

    m_rfTSBK.setLCO(TSBK_IOSP_ACK_RSP);
    m_rfTSBK.setMFId(P25_MFG_STANDARD);
    m_rfTSBK.setSrcId(srcId);
    m_rfTSBK.setService(service);

    if (extended) {
        m_rfTSBK.setAIV(true);
        m_rfTSBK.setEX(true);
    }

    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_ACK_RSP (Acknowledge Response), AIV = %u, EX = %u, serviceType = $%02X, srcId = %u",
            m_rfTSBK.getAIV(), m_rfTSBK.getEX(), m_rfTSBK.getService(), srcId);
    }

    writeRF_TSDU_SBF(noNetwork);

    m_rfTSBK.setLCO(lco);
    m_rfTSBK.setMFId(mfId);
    m_rfTSBK.setSrcId(_srcId);
}

/// <summary>
/// Helper to write a deny packet.
/// </summary>
/// <param name="reason"></param>
/// <param name="service"></param>
void TrunkPacket::writeRF_TSDU_Deny(uint8_t reason, uint8_t service)
{
    uint8_t lco = m_rfTSBK.getLCO();
    uint32_t srcId = m_rfTSBK.getSrcId();

    m_rfTSBK.setLCO(TSBK_OSP_DENY_RSP);
    m_rfTSBK.setSrcId(P25_WUID_FNE);
    m_rfTSBK.setService(service);
    m_rfTSBK.setResponse(reason);

    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_OSP_DENY_RSP (Deny Response), AIV = %u, reason = $%02X, service = $%02X, srcId = %u, dstId = %u", 
            m_rfTSBK.getAIV(), reason, service, m_rfTSBK.getSrcId(), m_rfTSBK.getDstId());
    }

    writeRF_TSDU_SBF(false);

    m_rfTSBK.setLCO(lco);
    m_rfTSBK.setSrcId(srcId);
}

/// <summary>
/// Helper to write a group affiliation response packet.
/// </summary>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
bool TrunkPacket::writeRF_TSDU_Grp_Aff_Rsp(uint32_t srcId, uint32_t dstId)
{
    bool ret = false;

    m_rfTSBK.setLCO(TSBK_IOSP_GRP_AFF);
    m_rfTSBK.setResponse(P25_RSP_ACCEPT);
    m_rfTSBK.setPatchSuperGroupId(m_patchSuperGroup);

    // validate the source RID
    if (!acl::AccessControl::validateSrcId(srcId)) {
        LogWarning(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_GRP_AFF (Group Affiliation Response) denial, RID rejection, srcId = %u", srcId);
        ::ActivityLog("P25", true, "group affiliation request from %u to %s %u denied", srcId, "TG ", dstId);
        m_rfTSBK.setResponse(P25_RSP_REFUSED);
    }

    // validate the source RID is registered
    if (!hasSrcIdUnitReg(srcId) && m_verifyReg) {
        LogWarning(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_GRP_AFF (Group Affiliation Response) denial, RID not registered, srcId = %u", srcId);
        ::ActivityLog("P25", true, "group affiliation request from %u to %s %u denied", srcId, "TG ", dstId);
        m_rfTSBK.setResponse(P25_RSP_REFUSED);
    }

    // validate the talkgroup ID
    if (m_rfTSBK.getGroup()) {
        if (dstId == 0U) {
            LogWarning(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_GRP_AFF (Group Affiliation Response), TGID 0, dstId = %u", dstId);
        }
        else {
            if (!acl::AccessControl::validateTGId(dstId)) {
                LogWarning(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_GRP_AFF (Group Affiliation Response) denial, TGID rejection, dstId = %u", dstId);
                ::ActivityLog("P25", true, "group affiliation request from %u to %s %u denied", srcId, "TG ", dstId);
                m_rfTSBK.setResponse(P25_RSP_DENY);
            }
        }
    }

    if (m_rfTSBK.getResponse() == P25_RSP_ACCEPT) {
        if (m_verbose) {
            LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_GRP_AFF (Group Affiliation Response), anncId = %u, srcId = %u, dstId = %u",
                m_patchSuperGroup, srcId, dstId);
        }

        ::ActivityLog("P25", true, "group affiliation request from %u to %s %u", srcId, "TG ", dstId);
        ret = true;

        // update dynamic affiliation table
        m_grpAffTable[srcId] = dstId;
    }

    writeRF_TSDU_SBF(false);
    return ret;
}

/// <summary>
/// Helper to write a unit registration response packet.
/// </summary>
/// <param name="srcId"></param>
void TrunkPacket::writeRF_TSDU_U_Reg_Rsp(uint32_t srcId)
{
    m_rfTSBK.setLCO(TSBK_IOSP_U_REG);
    m_rfTSBK.setResponse(P25_RSP_ACCEPT);

    // validate the system ID
    if (m_rfTSBK.getSysId() != m_p25->m_siteData.sysId()) {
        LogWarning(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_U_REG (Unit Registration Response) denial, SYSID rejection, sysId = $%03X", m_rfTSBK.getSysId());
        ::ActivityLog("P25", true, "unit registration request from %u denied", srcId);
        m_rfTSBK.setResponse(P25_RSP_DENY);
    }

    // validate the source RID
    if (!acl::AccessControl::validateSrcId(srcId)) {
        LogWarning(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_U_REG (Unit Registration Response) denial, RID rejection, srcId = %u", srcId);
        ::ActivityLog("P25", true, "unit registration request from %u denied", srcId);
        m_rfTSBK.setResponse(P25_RSP_REFUSED);
    }

    if (m_rfTSBK.getResponse() == P25_RSP_ACCEPT) {
        if (m_verbose) {
            LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_IOSP_U_REG (Unit Registration Response), srcId = %u, sysId = $%03X, netId = $%05X", srcId,
                m_rfTSBK.getSysId(), m_rfTSBK.getNetId());
        }

        ::ActivityLog("P25", true, "unit registration request from %u", srcId);

        // update dynamic unit registration table
        if (!hasSrcIdUnitReg(srcId)) {
            m_unitRegTable.push_back(srcId);
        }
    }

    m_rfTSBK.setSrcId(srcId);
    m_rfTSBK.setDstId(srcId);

    writeRF_TSDU_SBF(true);

    // validate the source RID
    if (!acl::AccessControl::validateSrcId(srcId)) {
        denialInhibit(srcId); // inhibit source radio automatically
    }
}

/// <summary>
/// Helper to write a unit de-registration acknowledge packet.
/// </summary>
/// <param name="srcId"></param>
void TrunkPacket::writeRF_TSDU_U_Dereg_Ack(uint32_t srcId)
{
    bool dereged = false;

    m_rfTSBK.setLCO(TSBK_OSP_U_DEREG_ACK);

    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_ISP_U_DEREG_REQ (Unit Deregistration Request) srcId = %u, sysId = $%03X, netId = $%05X",
            srcId, m_rfTSBK.getSysId(), m_rfTSBK.getNetId());
    }

    // remove dynamic unit registration table entry
    if (std::find(m_unitRegTable.begin(), m_unitRegTable.end(), srcId) != m_unitRegTable.end()) {
        auto it = std::find(m_unitRegTable.begin(), m_unitRegTable.end(), srcId);
        m_unitRegTable.erase(it);
        dereged = true;
    }

    // remove dynamic affiliation table entry
    try {
        m_grpAffTable.at(srcId);
        m_grpAffTable.erase(srcId);
        dereged = true;
    }
    catch (...) {
        // stub
    }

    if (dereged) {
        ::ActivityLog("P25", true, "unit deregistration request from %u", srcId);

        m_rfTSBK.setSrcId(P25_WUID_FNE);
        m_rfTSBK.setDstId(srcId);

        writeRF_TSDU_SBF(false);
    }
    else {
        ::ActivityLog("P25", true, "unit deregistration request from %u denied", srcId);
    }
}

/// <summary>
/// Helper to write a queue packet.
/// </summary>
/// <param name="reason"></param>
/// <param name="service"></param>
void TrunkPacket::writeRF_TSDU_Queue(uint8_t reason, uint8_t service)
{
    uint8_t lco = m_rfTSBK.getLCO();
    uint32_t srcId = m_rfTSBK.getSrcId();

    m_rfTSBK.setLCO(TSBK_OSP_QUE_RSP);
    m_rfTSBK.setSrcId(P25_WUID_FNE);
    m_rfTSBK.setService(service);
    m_rfTSBK.setResponse(reason);

    if (m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_OSP_QUE_RSP (Queue Response), AIV = %u, reason = $%02X, srcId = %u, dstId = %u", 
            m_rfTSBK.getAIV(), reason, m_rfTSBK.getSrcId(), m_rfTSBK.getDstId());
    }

    writeRF_TSDU_SBF(false);

    m_rfTSBK.setLCO(lco);
    m_rfTSBK.setSrcId(srcId);
}

/// <summary>
/// Helper to write a location registration response packet.
/// </summary>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
bool TrunkPacket::writeRF_TSDU_Loc_Reg_Rsp(uint32_t srcId, uint32_t dstId)
{
    bool ret = false;

    m_rfTSBK.setLCO(TSBK_OSP_LOC_REG_RSP);
    m_rfTSBK.setResponse(P25_RSP_ACCEPT);
    m_rfTSBK.setDstId(dstId);
    m_rfTSBK.setSrcId(srcId);

    // validate the source RID
    if (!acl::AccessControl::validateSrcId(srcId)) {
        LogWarning(LOG_RF, P25_TSDU_STR ", TSBK_OSP_LOC_REG_RSP (Location Registration Response) denial, RID rejection, srcId = %u", srcId);
        ::ActivityLog("P25", true, "location registration request from %u denied", srcId);
        m_rfTSBK.setResponse(P25_RSP_REFUSED);
    }

    // validate the source RID is registered
    if (!hasSrcIdUnitReg(srcId)) {
        LogWarning(LOG_RF, P25_TSDU_STR ", TSBK_OSP_LOC_REG_RSP (Location Registration Response) denial, RID not registered, srcId = %u", srcId);
        ::ActivityLog("P25", true, "location registration request from %u denied", srcId);
        writeRF_TSDU_U_Reg_Cmd(srcId);
        return false;
    }

    // validate the talkgroup ID
    if (m_rfTSBK.getGroup()) {
                if (dstId == 0U) {
            LogWarning(LOG_RF, P25_TSDU_STR ", TSBK_OSP_LOC_REG_RSP (Location Registration Response), TGID 0, dstId = %u", dstId);
        }
        else {
            if (!acl::AccessControl::validateTGId(dstId)) {
                LogWarning(LOG_RF, P25_TSDU_STR ", TSBK_OSP_LOC_REG_RSP (Location Registration Response) denial, TGID rejection, dstId = %u", dstId);
                ::ActivityLog("P25", true, "location registration request from %u to %s %u denied", srcId, "TG ", dstId);
                m_rfTSBK.setResponse(P25_RSP_DENY);
            }
        }
    }

    if (m_rfTSBK.getResponse() == P25_RSP_ACCEPT) {
        if (m_verbose) {
            LogMessage(LOG_RF, P25_TSDU_STR ", TSBK_OSP_LOC_REG_RSP (Location Registration Response), lra = %u, srcId = %u, dstId = %u",
                m_rfTSBK.getLRA(), srcId, dstId);
        }

        ::ActivityLog("P25", true, "location registration request from %u", srcId);
        ret = true;
    }

    writeRF_TSDU_SBF(false);
    return ret;
}

/// <summary>
/// Helper to write a call termination packet.
/// </summary>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
bool TrunkPacket::writeNet_TSDU_Call_Term(uint32_t srcId, uint32_t dstId)
{
    bool ret = false;

    m_rfTSBK.setLCO(LC_CALL_TERM);
    m_rfTSBK.setMFId(P25_MFG_DVM);
    m_rfTSBK.setGrpVchId(m_p25->m_siteData.channelId());
    m_rfTSBK.setGrpVchNo(m_p25->m_siteData.channelNo());
    m_rfTSBK.setDstId(dstId);
    m_rfTSBK.setSrcId(srcId);

    writeRF_TSDU_SBF(false); // the problem with this is the vendor code going over the air!
    return ret;
}

/// <summary>
/// Helper to write a network TSDU from the RF data queue.
/// </summary>
/// <param name="data"></param>
void TrunkPacket::writeNet_TSDU_From_RF(uint8_t* data)
{
    ::memset(data, 0x00U, P25_TSDU_FRAME_LENGTH_BYTES);

    // Generate Sync
    Sync::addP25Sync(data);

    // Generate NID
    m_p25->m_nid.encode(data, P25_DUID_TSDU);

    // Regenerate TSDU Data
    m_rfTSBK.setLastBlock(true); // always set last block -- this a Single Block TSDU
    m_rfTSBK.encode(data);

    // Add busy bits
    m_p25->addBusyBits(data, P25_TSDU_FRAME_LENGTH_BYTES, true, false);

    // Set first busy bits to 1,1
    m_p25->setBusyBits(data, P25_SS0_START, true, true);
}

/// <summary>
/// Helper to write a network P25 TDU w/ link control packet.
/// </summary>
/// <param name="lc"></param>
void TrunkPacket::writeNet_TDULC(lc::TDULC lc)
{
    uint8_t buffer[P25_TDULC_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_TDULC_FRAME_LENGTH_BYTES + 2U);

    buffer[0U] = modem::TAG_EOT;
    buffer[1U] = 0x00U;

    // Generate Sync
    Sync::addP25Sync(buffer + 2U);

    // Generate NID
    m_p25->m_nid.encode(buffer + 2U, P25_DUID_TDULC);

    // Regenerate TDULC Data
    lc.encode(buffer + 2U);

    // Add busy bits
    m_p25->addBusyBits(buffer + 2U, P25_TDULC_FRAME_LENGTH_BITS, true, true);

    m_p25->writeQueueNet(buffer, P25_TDULC_FRAME_LENGTH_BYTES + 2U);

    if (m_verbose) {
        LogMessage(LOG_NET, P25_TDULC_STR ", lc = $%02X, srcId = %u", lc.getLCO(), lc.getSrcId());
    }

    if (m_p25->m_voice->m_netFrames > 0) {
        ::ActivityLog("P25", false, "network end of transmission, %.1f seconds, %u%% packet loss", 
            float(m_p25->m_voice->m_netFrames) / 50.0F, (m_p25->m_voice->m_netLost * 100U) / m_p25->m_voice->m_netFrames);
    }
    else {
        ::ActivityLog("P25", false, "network end of transmission, %u frames", m_p25->m_voice->m_netFrames);
    }

    if (m_network != NULL)
        m_network->resetP25();

    m_p25->m_netTimeout.stop();
    m_p25->m_networkWatchdog.stop();
    m_p25->m_netState = RS_NET_IDLE;
    m_p25->m_tailOnIdle = true;
}

/// <summary>
/// Helper to write a network single-block P25 TSDU packet.
/// </summary>
void TrunkPacket::writeNet_TSDU()
{
    uint8_t buffer[P25_TSDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_TSDU_FRAME_LENGTH_BYTES + 2U);

    buffer[0U] = modem::TAG_DATA;
    buffer[1U] = 0x00U;

    // Generate Sync
    Sync::addP25Sync(buffer + 2U);

    // Generate NID
    m_p25->m_nid.encode(buffer + 2U, P25_DUID_TSDU);

    // Regenerate TSDU Data
    m_netTSBK.setLastBlock(true); // always set last block -- this a Single Block TSDU
    m_netTSBK.encode(buffer + 2U);

    // Add busy bits
    m_p25->addBusyBits(buffer + 2U, P25_TSDU_FRAME_LENGTH_BYTES, true, false);

    // Set first busy bits to 1,1
    m_p25->setBusyBits(buffer + 2U, P25_SS0_START, true, true);

    m_p25->writeQueueNet(buffer, P25_TSDU_FRAME_LENGTH_BYTES + 2U);

    if (m_network != NULL)
        m_network->resetP25();
}

/// <summary>
/// Helper to automatically inhibit a source ID on a denial.
/// </summary>
/// <param name="srcId"></param>
void TrunkPacket::denialInhibit(uint32_t srcId)
{
    if (!m_p25->m_inhibitIllegal) {
        return;
    }

    // this check should have already been done -- but do it again anyway
    if (!acl::AccessControl::validateSrcId(srcId)) {
        LogWarning(LOG_P25, P25_TSDU_STR ", denial, system auto-inhibit RID, srcId = %u", srcId);
        writeRF_TSDU_Ext_Func(P25_EXT_FNCT_INHIBIT, P25_WUID_FNE, srcId);
    }
}

/// <summary>
/// Helper to add the idle status bits on P25 frame data.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="b1"></param>
/// <param name="b2"></param>
void TrunkPacket::addIdleBits(uint8_t* data, uint32_t length, bool b1, bool b2)
{
    assert(data != NULL);

    for (uint32_t ss0Pos = P25_SS0_START; ss0Pos < length; ss0Pos += (P25_SS_INCREMENT * 5U)) {
        uint32_t ss1Pos = ss0Pos + 1U;
        WRITE_BIT(data, ss0Pos, b1);
        WRITE_BIT(data, ss1Pos, b2);
    }
}
