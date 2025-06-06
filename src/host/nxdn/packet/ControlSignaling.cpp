// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022-2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/channel/CAC.h"
#include "common/nxdn/acl/AccessControl.h"
#include "common/nxdn/lc/rcch/RCCHFactory.h"
#include "common/nxdn/Sync.h"
#include "common/nxdn/NXDNUtils.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "nxdn/packet/ControlSignaling.h"
#include "ActivityLog.h"
#include "Host.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::lc;
using namespace nxdn::lc::rcch;
using namespace nxdn::packet;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

// Make sure control data is supported.
#define IS_SUPPORT_CONTROL_CHECK(_PCKT_STR, _PCKT, _SRCID)                              \
    if (!m_nxdn->m_enableControl) {                                                     \
        LogWarning(LOG_RF, "NXDN, %s denial, unsupported service, srcId = %u", _PCKT_STR.c_str(), _SRCID); \
        writeRF_Message_Deny(0U, _SRCID, CauseResponse::SVC_UNAVAILABLE, _PCKT);        \
        m_nxdn->m_rfState = RS_RF_REJECTED;                                             \
        return false;                                                                   \
    }

// Validate the source RID.
#define VALID_SRCID(_PCKT_STR, _PCKT, _SRCID, _RSN)                                     \
    if (!acl::AccessControl::validateSrcId(_SRCID)) {                                   \
        LogWarning(LOG_RF, "NXDN, %s denial, RID rejection, srcId = %u", _PCKT_STR.c_str(), _SRCID); \
        writeRF_Message_Deny(0U, _SRCID, _RSN, _PCKT);                                  \
        m_nxdn->m_rfState = RS_RF_REJECTED;                                             \
        return false;                                                                   \
    }

// Validate the target RID.
#define VALID_DSTID(_PCKT_STR, _PCKT, _DSTID, _RSN)                                     \
    if (!acl::AccessControl::validateSrcId(_DSTID)) {                                   \
        LogWarning(LOG_RF, "NXDN, %s denial, RID rejection, dstId = %u", _PCKT_STR.c_str(), _DSTID); \
        writeRF_Message_Deny(0U, _SRCID, _RSN, _PCKT);                                  \
        m_nxdn->m_rfState = RS_RF_REJECTED;                                             \
        return false;                                                                   \
    }

// Validate the talkgroup ID.
#define VALID_TGID(_PCKT_STR, _PCKT, _DSTID, _SRCID, _RSN)                              \
    if (!acl::AccessControl::validateTGId(_DSTID)) {                                    \
        LogWarning(LOG_RF, "NXDN, %s denial, TGID rejection, dstId = %u", _PCKT_STR.c_str(), _DSTID); \
        writeRF_Message_Deny(0U, _SRCID, _RSN, _PCKT);                                  \
        m_nxdn->m_rfState = RS_RF_REJECTED;                                             \
        return false;                                                                   \
    }

// Verify the source RID is registered.
#define VERIFY_SRCID_REG(_PCKT_STR, _PCKT, _SRCID, _RSN)                                \
    if (!m_nxdn->m_affiliations->isUnitReg(_SRCID) && m_verifyReg) {                    \
        LogWarning(LOG_RF, "NXDN, %s denial, RID not registered, srcId = %u", _PCKT_STR.c_str(), _SRCID); \
        writeRF_Message_Deny(0U, _SRCID, _RSN, _PCKT);                                  \
        m_nxdn->m_rfState = RS_RF_REJECTED;                                             \
        return false;                                                                   \
    }

// Verify the source RID is affiliated.
#define VERIFY_SRCID_AFF(_PCKT_STR, _PCKT, _SRCID, _DSTID, _RSN)                        \
    if (!m_nxdn->m_affiliations->isGroupAff(_SRCID, _DSTID) && m_verifyAff) {           \
        LogWarning(LOG_RF, "NXDN, %s denial, RID not affiliated to TGID, srcId = %u, dstId = %u", _PCKT_STR.c_str(), _SRCID, _DSTID); \
        writeRF_Message_Deny(0U, _SRCID, _RSN, _PCKT);                                  \
        m_nxdn->m_rfState = RS_RF_REJECTED;                                             \
        return false;                                                                   \
    }

// Macro helper to verbose log a generic message.
#define VERBOSE_LOG_MSG(_PCKT_STR, _SRCID, _DSTID)                                      \
    if (m_verbose) {                                                                    \
        LogMessage(LOG_RF, "NXDN, %s, srcId = %u, dstId = %u", _PCKT_STR.c_str(), _SRCID, _DSTID); \
    }

// Macro helper to verbose log a generic message.
#define VERBOSE_LOG_MSG_DST(_PCKT_STR, _DSTID)                                          \
    if (m_verbose) {                                                                    \
        LogMessage(LOG_RF, "NXDN, %s, dstId = %u", _PCKT_STR.c_str(), _DSTID);          \
    }

// Macro helper to verbose log a generic network message.
#define VERBOSE_LOG_MSG_NET(_PCKT_STR, _SRCID, _DSTID)                                  \
    if (m_verbose) {                                                                    \
        LogMessage(LOG_NET, "NXDN, %s, srcId = %u, dstId = %u", _PCKT_STR.c_str(), _SRCID, _DSTID); \
    }

// Macro helper to verbose log a generic network message.
#define DEBUG_LOG_MSG(_PCKT_STR)                                                        \
    if (m_debug) {                                                                      \
        LogMessage(LOG_RF, "NXDN, %s", _PCKT_STR.c_str());                              \
    }

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t GRANT_TIMER_TIMEOUT = 15U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Process a data frame from the RF interface. */

bool ControlSignaling::process(FuncChannelType::E fct, ChOption::E option, uint8_t* data, uint32_t len)
{
    assert(data != nullptr);

    channel::CAC cac;
    bool validCAC = cac.decode(data + 2U, (fct == FuncChannelType::CAC_INBOUND_LONG));
    if (m_nxdn->m_rfState == RS_RF_LISTENING && !validCAC) {
        return false;
    }

    if (validCAC) {
        uint8_t ran = cac.getRAN();
        if (ran != m_nxdn->m_ran && ran != 0U)
            return false;
    }

    RPT_RF_STATE prevRfState = m_nxdn->m_rfState;
    if (m_nxdn->m_rfState != RS_RF_DATA) {
        m_nxdn->m_rfState = RS_RF_DATA;
    }

    m_nxdn->m_txQueue.clear();

    // the layer3 data will only be correct if valid is true
    uint8_t buffer[NXDN_FRAME_LENGTH_BYTES];
    cac.getData(buffer);

    std::unique_ptr<RCCH> rcch = rcch::RCCHFactory::createRCCH(buffer, NXDN_RCCH_CAC_LC_SHORT_LENGTH_BITS);
    if (rcch == nullptr)
        return false;

    uint16_t srcId = rcch->getSrcId();
    uint16_t dstId = rcch->getDstId();
    m_nxdn->m_affiliations->touchUnitReg(srcId);

    switch (rcch->getMessageType()) {
        case MessageType::RTCH_VCALL:
        {
            // make sure control data is supported
            IS_SUPPORT_CONTROL_CHECK(rcch->toString(true), MessageType::RTCH_VCALL, srcId);

            // validate the source RID
            VALID_SRCID(rcch->toString(true), MessageType::RTCH_VCALL, srcId, CauseResponse::VD_REQ_UNIT_NOT_PERM);

            // validate the talkgroup ID
            VALID_TGID(rcch->toString(true), MessageType::RTCH_VCALL, dstId, srcId, CauseResponse::VD_TGT_UNIT_NOT_PERM);

            // verify the source RID is affiliated
            VERIFY_SRCID_AFF(rcch->toString(true), MessageType::RTCH_VCALL, srcId, dstId, CauseResponse::VD_REQ_UNIT_NOT_REG);

            VERBOSE_LOG_MSG(rcch->toString(true), srcId, dstId);
            uint8_t serviceOptions = (rcch->getEmergency() ? 0x80U : 0x00U) +       // Emergency Flag
                (rcch->getEncrypted() ? 0x40U : 0x00U) +                            // Encrypted Flag
                (rcch->getPriority() & 0x07U);                                      // Priority

            if (m_nxdn->m_authoritative) {
                writeRF_Message_Grant(srcId, dstId, serviceOptions, true);
            } else {
                if (m_nxdn->m_network != nullptr)
                    m_nxdn->m_network->writeGrantReq(modem::DVM_STATE::STATE_NXDN, srcId, dstId, 0U, false);
            }
        }
        break;
        case MessageType::RCCH_REG:
        {
            // make sure control data is supported
            IS_SUPPORT_CONTROL_CHECK(rcch->toString(true), MessageType::RCCH_REG, srcId);

            if (m_verbose) {
                LogMessage(LOG_RF, "NXDN, %s, srcId = %u, locId = $%06X, regOption = $%02X", 
                    rcch->toString(true).c_str(), srcId, rcch->getLocId(), rcch->getRegOption());
            }

            writeRF_Message_U_Reg_Rsp(srcId, dstId, rcch->getLocId());
        }
        break;
        case MessageType::RCCH_GRP_REG:
        {
            // make sure control data is supported
            IS_SUPPORT_CONTROL_CHECK(rcch->toString(true), MessageType::RCCH_GRP_REG, srcId);

            if (m_verbose) {
                LogMessage(LOG_RF, "NXDN, %s, srcId = %u, dstId = %u, locId = $%06X", 
                    rcch->toString(true).c_str(), srcId, dstId, rcch->getLocId());
            }

            writeRF_Message_Grp_Reg_Rsp(srcId, dstId, rcch->getLocId());
        }
        break;
        default:
            LogError(LOG_RF, "NXDN, unhandled message type, messageType = $%02X", rcch->getMessageType());
            break;
    }

    m_nxdn->m_rfState = prevRfState;
    return true;
}

/* Process a data frame from the RF interface. */

bool ControlSignaling::processNetwork(FuncChannelType::E fct, ChOption::E option, lc::RTCH& netLC, uint8_t* data, uint32_t len)
{
    assert(data != nullptr);

    if (!m_nxdn->m_enableControl)
        return false;
    if (m_nxdn->m_rfState != RS_RF_LISTENING && m_nxdn->m_netState == RS_NET_IDLE)
        return false;

    if (m_nxdn->m_netState == RS_NET_IDLE) {
        m_nxdn->m_txQueue.clear();
    }

    if (m_nxdn->m_netState == RS_NET_IDLE) {
        std::unique_ptr<lc::RCCH> rcch = RCCHFactory::createRCCH(data, len);
        if (rcch == nullptr) {
            return false;
        }

        uint16_t srcId = rcch->getSrcId();
        uint16_t dstId = rcch->getDstId();

        // handle standard NXDN message opcodes
        switch (rcch->getMessageType()) {
            case MessageType::RTCH_VCALL:
            {
                if (m_nxdn->m_dedicatedControl) {
                    if (!m_nxdn->m_affiliations->isGranted(dstId)) {
                        if (m_verbose) {
                            LogMessage(LOG_NET, "NXDN, %s, emerg = %u, encrypt = %u, prio = %u, chNo = %u, srcId = %u, dstId = %u",
                                rcch->toString().c_str(), rcch->getEmergency(), rcch->getEncrypted(), rcch->getPriority(), rcch->getGrpVchNo(), srcId, dstId);
                        }

                        uint8_t serviceOptions = (rcch->getEmergency() ? 0x80U : 0x00U) +   // Emergency Flag
                            (rcch->getEncrypted() ? 0x40U : 0x00U) +                        // Encrypted Flag
                            (rcch->getPriority() & 0x07U);                                  // Priority

                        writeRF_Message_Grant(srcId, dstId, serviceOptions, true, true);
                    }
                }
            }
            return true; // don't allow this to write to the air
            case MessageType::RCCH_VCALL_CONN:
                break; // the FNE may explicitly send these
            default:
                LogError(LOG_NET, "NXDN, unhandled message type, messageType = $%02X", rcch->getMessageType());
                return false;
        } // switch (rcch->getMessageType())

        writeRF_Message(rcch.get(), true);
    }

    return true;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ControlSignaling class. */

ControlSignaling::ControlSignaling(Control* nxdn, bool debug, bool verbose) :
    m_nxdn(nxdn),
    m_bcchCnt(1U),
    m_rcchGroupingCnt(1U),
    m_ccchPagingCnt(2U),
    m_ccchMultiCnt(2U),
    m_rcchIterateCnt(2U),
    m_verifyAff(false),
    m_verifyReg(false),
    m_disableGrantSrcIdCheck(false),
    m_lastRejectId(0U),
    m_verbose(verbose),
    m_debug(debug)
{
    /* stub */
}

/* Finalizes a instance of the ControlSignaling class. */

ControlSignaling::~ControlSignaling() = default;

/* Write data processed from RF to the network. */

void ControlSignaling::writeNetwork(const uint8_t *data, uint32_t len)
{
    assert(data != nullptr);

    if (m_nxdn->m_network == nullptr)
        return;

    if (m_nxdn->m_rfTimeout.isRunning() && m_nxdn->m_rfTimeout.hasExpired())
        return;

    m_nxdn->m_network->writeNXDN(m_nxdn->m_rfLC, data, len, true);
}

/*
** Modem Frame Queuing
*/

/* Helper to write a single-block RCCH packet. */

void ControlSignaling::writeRF_Message(RCCH* rcch, bool noNetwork, bool imm)
{
    if (!m_nxdn->m_enableControl)
        return;

    uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, NXDN_FRAME_LENGTH_BYTES);

    Sync::addNXDNSync(data + 2U);

    // generate the LICH
    channel::LICH lich;
    lich.setRFCT(RFChannelType::RCCH);
    lich.setFCT(FuncChannelType::CAC_OUTBOUND);
    lich.setOption(ChOption::DATA_NORMAL);
    lich.setOutbound(true);
    lich.encode(data + 2U);

    uint8_t buffer[NXDN_RCCH_LC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES);

    rcch->encode(buffer, NXDN_RCCH_LC_LENGTH_BITS);

    // generate the CAC
    channel::CAC cac;
    cac.setRAN(m_nxdn->m_ran);
    cac.setStructure(ChStructure::SR_RCCH_SINGLE);
    cac.setData(buffer);
    cac.encode(data + 2U);

    data[0U] = modem::TAG_DATA;
    data[1U] = 0x00U;

    NXDNUtils::scrambler(data + 2U);
    NXDNUtils::addPostBits(data + 2U);

    if (!noNetwork)
        writeNetwork(data, NXDN_FRAME_LENGTH_BYTES + 2U);

    if (m_nxdn->m_duplex) {
        m_nxdn->addFrame(data, false, imm);
    }
}

/*
** Control Signalling Logic
*/

/* Helper to write control channel packet data. */

void ControlSignaling::writeRF_ControlData(uint8_t frameCnt, uint8_t n, bool adjSS)
{
    uint8_t i = 0U, seqCnt = 0U;

    if (!m_nxdn->m_enableControl)
        return;

    // disable verbose RCCH dumping during control data writes (if necessary)
    bool rcchVerbose = lc::RCCH::getVerbose();
    if (rcchVerbose)
        lc::RCCH::setVerbose(false);

    // disable debug logging during control data writes (if necessary)
    bool controlDebug = m_nxdn->m_debug;
    if (!m_nxdn->m_ccDebug)
        m_nxdn->m_debug = m_debug = false;

    // don't add any frames if the queue is full
    uint8_t len = NXDN_FRAME_LENGTH_BYTES + 2U;
    uint32_t space = m_nxdn->m_txQueue.freeSpace();
    if (space < (len + 1U)) {
        return;
    }

    do
    {
        if (m_debug) {
            LogDebugEx(LOG_NXDN, "ControlSignaling::writeRF_ControlData()", "frameCnt = %u, seq = %u", frameCnt, n);
        }

        switch (n)
        {
        case 0:
            writeRF_CC_Message_Site_Info();
            break;
        default:
            writeRF_CC_Message_Service_Info();
            break;
        }

        if (seqCnt > 0U)
            n++;
        i++;
    } while (i <= seqCnt);

    lc::RCCH::setVerbose(rcchVerbose);
    m_nxdn->m_debug = m_debug = controlDebug;
}

/* Helper to write a grant packet. */

bool ControlSignaling::writeRF_Message_Grant(uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp, bool net, bool skip, uint32_t chNo)
{
    bool emergency = ((serviceOptions & 0xFFU) & 0x80U) == 0x80U;           // Emergency Flag
    bool encryption = ((serviceOptions & 0xFFU) & 0x40U) == 0x40U;          // Encryption Flag
    uint8_t priority = ((serviceOptions & 0xFFU) & 0x07U);                  // Priority

    std::unique_ptr<rcch::MESSAGE_TYPE_VCALL_CONN> rcch = std::make_unique<rcch::MESSAGE_TYPE_VCALL_CONN>();

    // are we skipping checking?
    if (!skip) {
        if (m_nxdn->m_rfState != RS_RF_LISTENING && m_nxdn->m_rfState != RS_RF_DATA) {
            if (!net) {
                LogWarning(LOG_RF, "NXDN, %s denied, traffic in progress, dstId = %u", rcch->toString().c_str(), dstId);
                writeRF_Message_Deny(0U, srcId, CauseResponse::VD_QUE_GRP_BUSY, MessageType::RTCH_VCALL);

                ::ActivityLog("NXDN", true, "group grant request from %u to TG %u denied", srcId, dstId);
                m_nxdn->m_rfState = RS_RF_REJECTED;
            }

            return false;
        }

        if (m_nxdn->m_netState != RS_NET_IDLE && dstId == m_nxdn->m_netLastDstId) {
            if (!net) {
                LogWarning(LOG_RF, "NXDN, %s denied, traffic in progress, dstId = %u", rcch->toString().c_str(), dstId);
                writeRF_Message_Deny(0U, srcId, CauseResponse::VD_QUE_GRP_BUSY, MessageType::RTCH_VCALL);

                ::ActivityLog("NXDN", true, "group grant request from %u to TG %u denied", srcId, dstId);
                m_nxdn->m_rfState = RS_RF_REJECTED;
            }

            return false;
        }

        // don't transmit grants if the destination ID's don't match and the network TG hang timer is running
        if (m_nxdn->m_rfLastDstId != 0U) {
            if (m_nxdn->m_rfLastDstId != dstId && (m_nxdn->m_rfTGHang.isRunning() && !m_nxdn->m_rfTGHang.hasExpired())) {
                if (!net) {
                    writeRF_Message_Deny(0U, srcId, CauseResponse::VD_QUE_GRP_BUSY, MessageType::RTCH_VCALL);
                    m_nxdn->m_rfState = RS_RF_REJECTED;
                }

                return false;
            }
        }

        if (!m_nxdn->m_affiliations->isGranted(dstId)) {
            if (grp && !m_nxdn->m_ignoreAffiliationCheck) {
                // is this an affiliation required group?
                ::lookups::TalkgroupRuleGroupVoice tid = m_nxdn->m_tidLookup->find(dstId);
                if (tid.config().affiliated()) {
                    if (!m_nxdn->m_affiliations->hasGroupAff(dstId)) {
                        LogWarning(LOG_RF, "NXDN, %s ignored, no group affiliations, dstId = %u", rcch->toString().c_str(), dstId);
                        return false;
                    }
                }
            }

            if (!grp && !m_nxdn->m_ignoreAffiliationCheck) {
                // is this the target registered?
                if (!m_nxdn->m_affiliations->isUnitReg(dstId)) {
                    LogWarning(LOG_RF, "NXDN, %s ignored, no unit registration, dstId = %u", rcch->toString().c_str(), dstId);
                    return false;
                }
            }

            if (!m_nxdn->m_affiliations->rfCh()->isRFChAvailable()) {
                if (grp) {
                    if (!net) {
                        LogWarning(LOG_RF, "NXDN, %s queued, no channels available, dstId = %u", rcch->toString().c_str(), dstId);
                        writeRF_Message_Deny(0U, srcId, CauseResponse::VD_QUE_CHN_RESOURCE_NOT_AVAIL, MessageType::RTCH_VCALL);

                        ::ActivityLog("NXDN", true, "group grant request from %u to TG %u queued", srcId, dstId);
                        m_nxdn->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
                else {
                    if (!net) {
                        LogWarning(LOG_RF, "NXDN, %s queued, no channels available, dstId = %u", rcch->toString().c_str(), dstId);
                        writeRF_Message_Deny(0U, srcId, CauseResponse::VD_QUE_CHN_RESOURCE_NOT_AVAIL, MessageType::RTCH_VCALL);

                        ::ActivityLog("P25", true, "unit-to-unit grant request from %u to %u queued", srcId, dstId);
                        m_nxdn->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
            }
            else {
                if (m_nxdn->m_affiliations->grantCh(dstId, srcId, GRANT_TIMER_TIMEOUT, grp, net)) {
                    chNo = m_nxdn->m_affiliations->getGrantedCh(dstId);
                }
            }
        }
        else {
            if (!m_disableGrantSrcIdCheck && !net) {
                // do collision check between grants to see if a SU is attempting a "grant retry" or if this is a
                // different source from the original grant
                uint32_t grantedSrcId = m_nxdn->m_affiliations->getGrantedSrcId(dstId);
                if (srcId != grantedSrcId) {
                    if (!net) {
                        LogWarning(LOG_RF, "NXDN, %s denied, traffic in progress, dstId = %u", rcch->toString().c_str(), dstId);
                        writeRF_Message_Deny(0U, srcId, CauseResponse::VD_QUE_GRP_BUSY, MessageType::RTCH_VCALL);

                        ::ActivityLog("NXDN", true, "group grant request from %u to TG %u denied", srcId, dstId);
                        m_nxdn->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
            }

            chNo = m_nxdn->m_affiliations->getGrantedCh(dstId);
            m_nxdn->m_affiliations->touchGrant(dstId);
        }
    }
    else {
        if (m_nxdn->m_affiliations->isGranted(dstId)) {
            chNo = m_nxdn->m_affiliations->getGrantedCh(dstId);
            m_nxdn->m_affiliations->touchGrant(dstId);
        }
        else {
            return false;
        }
    }

    if (grp) {
        if (!net) {
            ::ActivityLog("NXDN", true, "group grant request from %u to TG %u", srcId, dstId);
        }
    }
    else {
        if (!net) {
            ::ActivityLog("NXDN", true, "unit-to-unit grant request from %u to %u", srcId, dstId);
        }
    }

    // callback RPC to permit the granted TG on the specified voice channel
    if (m_nxdn->m_authoritative && m_nxdn->m_supervisor) {
        ::lookups::VoiceChData voiceChData = m_nxdn->m_affiliations->rfCh()->getRFChData(chNo);
        if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0 &&
            chNo != m_nxdn->m_siteData.channelNo()) {
            json::object req = json::object();
            req["dstId"].set<uint32_t>(dstId);

            // send blocking RPC request
            bool requestFailed = !g_RPC->req(RPC_PERMIT_NXDN_TG, req, [=, &requestFailed](json::object& req, json::object& reply) {
                if (!req["status"].is<int>()) {
                    return;
                }

                int status = req["status"].get<int>();
                if (status != network::NetRPC::OK) {
                    if (req["message"].is<std::string>()) {
                        std::string retMsg = req["message"].get<std::string>();
                        ::LogError((net) ? LOG_NET : LOG_RF, "NXDN, RPC failed, %s", retMsg.c_str());
                    }
                    requestFailed = true;
                } else {
                    requestFailed = false;
                }
            }, voiceChData.address(), voiceChData.port(), true);

            // if the request failed block grant
            if (requestFailed) {
                ::LogError((net) ? LOG_NET : LOG_RF, "NXDN, %s, failed to permit TG for use, chNo = %u", rcch->toString().c_str(), chNo);

                m_nxdn->m_affiliations->releaseGrant(dstId, false);
                if (!net) {
                    writeRF_Message_Deny(0U, srcId, CauseResponse::VD_QUE_GRP_BUSY, MessageType::RTCH_VCALL);
                    m_nxdn->m_rfState = RS_RF_REJECTED;
                }

                return false;
            }
        }
        else {
            ::LogError((net) ? LOG_NET : LOG_RF, "NXDN, %s, failed to permit TG for use, chNo = %u", rcch->toString().c_str(), chNo);
        }
    }

    rcch->setMessageType(MessageType::RTCH_VCALL);
    rcch->setGrpVchNo(chNo);
    rcch->setGroup(grp);
    rcch->setSrcId(srcId);
    rcch->setDstId(dstId);

    rcch->setEmergency(emergency);
    rcch->setEncrypted(encryption);
    rcch->setPriority(priority);

    if (m_verbose) {
        LogMessage((net) ? LOG_NET : LOG_RF, "NXDN, %s, emerg = %u, encrypt = %u, prio = %u, chNo = %u, srcId = %u, dstId = %u",
            rcch->toString().c_str(), rcch->getEmergency(), rcch->getEncrypted(), rcch->getPriority(), rcch->getGrpVchNo(), rcch->getSrcId(), rcch->getDstId());
    }

    // transmit group grant
    writeRF_Message_Imm(rcch.get(), net);
    return true;
}

/* Helper to write a deny packet. */

void ControlSignaling::writeRF_Message_Deny(uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service)
{
    std::unique_ptr<RCCH> rcch = nullptr;

    switch (service) {
    case MessageType::RTCH_VCALL:
        rcch = std::make_unique<rcch::MESSAGE_TYPE_VCALL_CONN>();
        rcch->setMessageType(MessageType::RTCH_VCALL);
    default:
        return;
    }

    rcch->setCauseResponse(reason);
    rcch->setSrcId(srcId);
    rcch->setDstId(dstId);

    if (m_verbose) {
        LogMessage(LOG_RF, "NXDN, MSG_DENIAL (Message Denial), reason = $%02X, service = $%02X, srcId = %u, dstId = %u",
            service, srcId, dstId);
    }

    writeRF_Message_Imm(rcch.get(), false);
}

/* Helper to write a group registration response packet. */

bool ControlSignaling::writeRF_Message_Grp_Reg_Rsp(uint32_t srcId, uint32_t dstId, uint32_t locId)
{
    bool ret = false;

    std::unique_ptr<rcch::MESSAGE_TYPE_GRP_REG> rcch = std::make_unique<rcch::MESSAGE_TYPE_GRP_REG>();
    rcch->setCauseResponse(CauseResponse::MM_REG_ACCEPTED);

    // validate the location ID
    if (locId != m_nxdn->m_siteData.locId()) {
        LogWarning(LOG_RF, "NXDN, %s denial, LOCID rejection, locId = $%06X", rcch->toString().c_str(), locId);
        ::ActivityLog("NXDN", true, "group affiliation request from %u denied", srcId);
        rcch->setCauseResponse(CauseResponse::MM_REG_FAILED);
    }

    // validate the source RID
    if (!acl::AccessControl::validateSrcId(srcId)) {
        LogWarning(LOG_RF, "NXDN, %s denial, RID rejection, srcId = %u", rcch->toString().c_str(), srcId);
        ::ActivityLog("NXDN", true, "group affiliation request from %u to %s %u denied", srcId, "TG ", dstId);
        rcch->setCauseResponse(CauseResponse::MM_REG_FAILED);
    }

    // validate the source RID is registered
    if (!m_nxdn->m_affiliations->isUnitReg(srcId) && m_verifyReg) {
        LogWarning(LOG_RF, "NXDN, %s denial, RID not registered, srcId = %u", rcch->toString().c_str(), srcId);
        ::ActivityLog("NXDN", true, "group affiliation request from %u to %s %u denied", srcId, "TG ", dstId);
        rcch->setCauseResponse(CauseResponse::MM_REG_REFUSED);
    }

    // validate the talkgroup ID
    if (dstId == 0U) {
        LogWarning(LOG_RF, "NXDN, %s, TGID 0, dstId = %u", rcch->toString().c_str(), dstId);
    }
    else {
        if (!acl::AccessControl::validateTGId(dstId)) {
            LogWarning(LOG_RF, "NXDN, %s denial, TGID rejection, dstId = %u", rcch->toString().c_str(), dstId);
            ::ActivityLog("NXDN", true, "group affiliation request from %u to %s %u denied", srcId, "TG ", dstId);
            rcch->setCauseResponse(CauseResponse::MM_LOC_ACPT_GRP_REFUSE);
        }
    }

    if (rcch->getCauseResponse() == CauseResponse::MM_REG_ACCEPTED) {
        VERBOSE_LOG_MSG(rcch->toString(), srcId, dstId);

        ::ActivityLog("NXDN", true, "group affiliation request from %u to %s %u", srcId, "TG ", dstId);
        ret = true;

        // update dynamic affiliation table
        m_nxdn->m_affiliations->groupAff(srcId, dstId);

        if (m_nxdn->m_network != nullptr)
            m_nxdn->m_network->announceGroupAffiliation(srcId, dstId);
    }

    writeRF_Message_Imm(rcch.get(), false);
    return ret;
}

/* Helper to write a unit registration response packet. */

void ControlSignaling::writeRF_Message_U_Reg_Rsp(uint32_t srcId, uint32_t dstId, uint32_t locId)
{
    std::unique_ptr<rcch::MESSAGE_TYPE_REG> rcch = std::make_unique<rcch::MESSAGE_TYPE_REG>();
    rcch->setCauseResponse(CauseResponse::MM_REG_ACCEPTED);

    // validate the location ID
    if (locId != ((m_nxdn->m_siteData.locId() >> 12U) << 7U)) {
        LogWarning(LOG_RF, "NXDN, %s denial, LOCID rejection, locId = $%06X", rcch->toString().c_str(), locId);
        ::ActivityLog("NXDN", true, "unit registration request from %u denied", srcId);
        rcch->setCauseResponse(CauseResponse::MM_REG_FAILED);
    }

    // validate the source RID
    if (!acl::AccessControl::validateSrcId(srcId)) {
        LogWarning(LOG_RF, "NXDN, %s denial, RID rejection, srcId = %u", rcch->toString().c_str(), srcId);
        ::ActivityLog("NXDN", true, "unit registration request from %u denied", srcId);
        rcch->setCauseResponse(CauseResponse::MM_REG_FAILED);
    }

    // validate the talkgroup ID
    if (dstId == 0U) {
        LogWarning(LOG_RF, "NXDN, %s, TGID 0, dstId = %u", rcch->toString().c_str(), dstId);
    }
    else {
        if (!acl::AccessControl::validateTGId(dstId)) {
            LogWarning(LOG_RF, "NXDN, %s denial, TGID rejection, dstId = %u", rcch->toString().c_str(), dstId);
            ::ActivityLog("NXDN", true, "unit registration request from %u to %s %u denied", srcId, "TG ", dstId);
            rcch->setCauseResponse(CauseResponse::MM_REG_FAILED);
        }
    }

    if (rcch->getCauseResponse() == CauseResponse::MM_REG_ACCEPTED) {
        if (m_verbose) {
            LogMessage(LOG_RF, "NXDN, %s, srcId = %u, locId = $%06X", 
                rcch->toString().c_str(), srcId, locId);
        }

        ::ActivityLog("NXDN", true, "unit registration request from %u", srcId);

        // update dynamic unit registration table
        if (!m_nxdn->m_affiliations->isUnitReg(srcId)) {
            m_nxdn->m_affiliations->unitReg(srcId);
        }

        if (m_nxdn->m_network != nullptr)
            m_nxdn->m_network->announceUnitRegistration(srcId);
    }

    rcch->setSrcId(srcId);
    rcch->setDstId(dstId);

    writeRF_Message_Imm(rcch.get(), true);
}

/* Helper to write a CC SITE_INFO broadcast packet on the RF interface. */

void ControlSignaling::writeRF_CC_Message_Site_Info()
{
    uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, NXDN_FRAME_LENGTH_BYTES);

    Sync::addNXDNSync(data + 2U);

    // generate the LICH
    channel::LICH lich;
    lich.setRFCT(RFChannelType::RCCH);
    lich.setFCT(FuncChannelType::CAC_OUTBOUND);
    lich.setOption(ChOption::DATA_NORMAL);
    lich.setOutbound(true);
    lich.encode(data + 2U);

    uint8_t buffer[NXDN_RCCH_LC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES);

    std::unique_ptr<rcch::MESSAGE_TYPE_SITE_INFO> rcch = std::make_unique<rcch::MESSAGE_TYPE_SITE_INFO>();
    DEBUG_LOG_MSG(rcch->toString());
    rcch->setBcchCnt(m_bcchCnt);
    rcch->setRcchGroupingCnt(m_rcchGroupingCnt);
    rcch->setCcchPagingCnt(m_ccchPagingCnt);
    rcch->setCcchMultiCnt(m_ccchMultiCnt);
    rcch->setRcchIterateCount(m_rcchIterateCnt);

    rcch->encode(buffer, NXDN_RCCH_LC_LENGTH_BITS);

    // generate the CAC
    channel::CAC cac;
    cac.setRAN(m_nxdn->m_ran);
    cac.setStructure(ChStructure::SR_RCCH_HEAD_SINGLE);
    cac.setData(buffer);
    cac.encode(data + 2U);

    data[0U] = modem::TAG_DATA;
    data[1U] = 0x00U;

    NXDNUtils::scrambler(data + 2U);
    NXDNUtils::addPostBits(data + 2U);

    if (m_nxdn->m_duplex) {
        m_nxdn->addFrame(data);
    }
}

/* Helper to write a CC SRV_INFO broadcast packet on the RF interface. */

void ControlSignaling::writeRF_CC_Message_Service_Info()
{
    uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, NXDN_FRAME_LENGTH_BYTES);

    Sync::addNXDNSync(data + 2U);

    // generate the LICH
    channel::LICH lich;
    lich.setRFCT(RFChannelType::RCCH);
    lich.setFCT(FuncChannelType::CAC_OUTBOUND);
    lich.setOption(ChOption::DATA_NORMAL);
    lich.setOutbound(true);
    lich.encode(data + 2U);

    uint8_t buffer[NXDN_RCCH_LC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES);

    std::unique_ptr<rcch::MESSAGE_TYPE_SRV_INFO> rcch = std::make_unique<rcch::MESSAGE_TYPE_SRV_INFO>();
    DEBUG_LOG_MSG(rcch->toString());
    rcch->encode(buffer, NXDN_RCCH_LC_LENGTH_BITS / 2U);
    //rcch->encode(buffer, NXDN_RCCH_LC_LENGTH_BITS / 2U, NXDN_RCCH_LC_LENGTH_BITS / 2U);

    // generate the CAC
    channel::CAC cac;
    cac.setRAN(m_nxdn->m_ran);
    cac.setStructure(ChStructure::SR_RCCH_SINGLE);
    cac.setData(buffer);
    cac.encode(data + 2U);

    data[0U] = modem::TAG_DATA;
    data[1U] = 0x00U;

    NXDNUtils::scrambler(data + 2U);
    NXDNUtils::addPostBits(data + 2U);

    if (m_nxdn->m_duplex) {
        m_nxdn->addFrame(data);
    }
}
