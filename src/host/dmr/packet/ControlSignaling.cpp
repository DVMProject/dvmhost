// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017,2018 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/dmr/acl/AccessControl.h"
#include "common/dmr/lc/CSBK.h"
#include "common/dmr/lc/csbk/CSBKFactory.h"
#include "common/dmr/SlotType.h"
#include "common/dmr/Sync.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "dmr/lc/csbk/CSBK_DVM_GIT_HASH.h"
#include "dmr/packet/ControlSignaling.h"
#include "dmr/Slot.h"
#include "remote/RESTClient.h"
#include "ActivityLog.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc::csbk;
using namespace dmr::packet;

#include <cassert>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

// Don't process RF frames if the network isn't in a idle state.
#define CHECK_TRAFFIC_COLLISION(_DST_ID)                                                \
    if (m_slot->m_netState != RS_NET_IDLE && _DST_ID == m_slot->m_netLastDstId) {       \
        LogWarning(LOG_RF, "DMR Slot %u, Traffic collision detect, preempting new RF traffic to existing network traffic!", m_slot->m_slotNo); \
        return false;                                                                   \
    }

#define CHECK_TG_HANG(_DST_ID)                                                          \
    if (m_slot->m_rfLastDstId != 0U) {                                                  \
        if (m_slot->m_rfLastDstId != _DST_ID && (m_slot->m_rfTGHang.isRunning() && !m_slot->m_rfTGHang.hasExpired())) { \
            return;                                                                     \
        }                                                                               \
    }

// Make sure control data is supported.
#define IS_SUPPORT_CONTROL_CHECK(_PCKT_STR, _PCKT, _SRCID)                              \
    if (!m_slot->m_dmr->getTSCCSlot()->m_enableTSCC) {                                  \
        LogWarning(LOG_RF, "DMR Slot %u, %s denial, unsupported service, srcId = %u", m_slot->m_slotNo, _PCKT_STR.c_str(), _SRCID); \
        writeRF_CSBK_ACK_RSP(_SRCID, ReasonCode::TS_DENY_RSN_SYS_UNSUPPORTED_SVC, 0U);  \
        return false;                                                                   \
    }

// Validate the source RID.
#define VALID_SRCID(_PCKT_STR, _PCKT, _SRCID)                                           \
    if (!acl::AccessControl::validateSrcId(_SRCID)) {                                   \
        LogWarning(LOG_RF, "DMR Slot %u, %s denial, RID rejection, srcId = %u", m_slot->m_slotNo, _PCKT_STR.c_str(), _SRCID); \
        writeRF_CSBK_ACK_RSP(_SRCID, ReasonCode::TS_DENY_RSN_PERM_USER_REFUSED, 0U);    \
        return false;                                                                   \
    }

// Validate the target RID.
#define VALID_DSTID(_PCKT_STR, _PCKT, _SRCID, _DSTID)                                   \
    if (!acl::AccessControl::validateSrcId(_DSTID)) {                                   \
        LogWarning(LOG_RF, "DMR Slot %u, %s denial, RID rejection, dstId = %u", m_slot->m_slotNo, _PCKT_STR.c_str(), _DSTID); \
        writeRF_CSBK_ACK_RSP(_SRCID, ReasonCode::TS_DENY_RSN_TEMP_USER_REFUSED, 0U);    \
        return false;                                                                   \
    }

// Validate the talkgroup ID.
#define VALID_TGID(_PCKT_STR, _PCKT, _SRCID, _DSTID)                                    \
    if (!acl::AccessControl::validateTGId(0U, _DSTID)) {                                \
        LogWarning(LOG_RF, "DMR Slot %u, %s denial, TGID rejection, dstId = %u", m_slot->m_slotNo, _PCKT_STR.c_str(), _DSTID); \
        writeRF_CSBK_ACK_RSP(_SRCID, ReasonCode::TS_DENY_RSN_TGT_GROUP_NOT_VALID, 0U);  \
        return false;                                                                   \
    }

// Verify the source RID is registered.
#define VERIFY_SRCID_REG(_PCKT_STR, _PCKT, _SRCID)                                      \
    if (!m_slot->m_affiliations->isUnitReg(_SRCID) && m_slot->m_verifyReg) {            \
        LogWarning(LOG_RF, "DMR Slot %u, %s denial, RID not registered, srcId = %u", m_slot->m_slotNo, _PCKT_STR.c_str(), _SRCID); \
        writeRF_CSBK_ACK_RSP(_SRCID, ReasonCode::TS_DENY_RSN_PERM_USER_REFUSED, 0U);    \
        return false;                                                                   \
    }

// Macro helper to verbose log a generic CSBK.
#define VERBOSE_LOG_CSBK(_PCKT_STR, _SRCID, _DSTID)                                     \
    if (m_verbose) {                                                                    \
        LogMessage(LOG_RF, "DMR Slot %u, CSBK, %s, srcId = %u, dstId = %u", m_slot->m_slotNo, _PCKT_STR.c_str(), _SRCID, _DSTID); \
    }

// Macro helper to verbose log a generic CSBK.
#define VERBOSE_LOG_CSBK_DST(_PCKT_STR, _DSTID)                                         \
    if (m_verbose) {                                                                    \
        LogMessage(LOG_RF, "DMR Slot %u, CSBK, %s, dstId = %u", m_slot->m_slotNo, _PCKT_STR.c_str(), _DSTID);   \
    }

// Macro helper to verbose log a generic network CSBK.
#define VERBOSE_LOG_CSBK_NET(_PCKT_STR, _SRCID, _DSTID)                                 \
    if (m_verbose) {                                                                    \
        LogMessage(LOG_NET, "DMR Slot %u, CSBK, %s, srcId = %u, dstId = %u", m_slot->m_slotNo, _PCKT_STR.c_str(), _SRCID, _DSTID); \
    }

// Macro helper to verbose log a generic network CSBK.
#define DEBUG_LOG_CSBK(_PCKT_STR)                                                       \
    if (m_debug) {                                                                      \
        LogMessage(LOG_RF, "DMR Slot %u, CSBK, %s", m_slot->m_slotNo, _PCKT_STR.c_str());    \
    }

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t ADJ_SITE_UPDATE_CNT = 5U;
const uint32_t GRANT_TIMER_TIMEOUT = 15U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Process DMR data frame from the RF interface. */

bool ControlSignaling::process(uint8_t* data, uint32_t len)
{
    assert(data != nullptr);

    // get the type from the packet metadata
    DataType::E dataType = (DataType::E)(data[1U] & 0x0FU);

    SlotType slotType;
    slotType.setColorCode(m_slot->m_colorCode);
    slotType.setDataType(dataType);

    if (dataType == DataType::CSBK) {
        // generate a new CSBK and check validity
        std::unique_ptr<lc::CSBK> csbk = CSBKFactory::createCSBK(data + 2U, dataType);
        if (csbk == nullptr)
            return false;

        uint8_t csbko = csbk->getCSBKO();
        if (csbko == CSBKO::BSDWNACT)
            return false;

        bool gi = csbk->getGI();
        uint32_t srcId = csbk->getSrcId();
        uint32_t dstId = csbk->getDstId();

        m_slot->m_affiliations->touchUnitReg(srcId);

        if (srcId != 0U || dstId != 0U) {
            CHECK_TRAFFIC_COLLISION(dstId);

            // validate the source RID
            if (!acl::AccessControl::validateSrcId(srcId)) {
                LogWarning(LOG_RF, "DMR Slot %u, DataType::CSBK denial, RID rejection, srcId = %u", m_slot->m_slotNo, srcId);
                m_slot->m_rfState = RS_RF_REJECTED;
                return false;
            }

            // validate the target ID
            if (gi) {
                if (!acl::AccessControl::validateTGId(m_slot->m_slotNo, dstId)) {
                    LogWarning(LOG_RF, "DMR Slot %u, DataType::CSBK denial, TGID rejection, srcId = %u, dstId = %u", m_slot->m_slotNo, srcId, dstId);
                    m_slot->m_rfState = RS_RF_REJECTED;
                    return false;
                }
            }
        }

        // if data preamble, signal its existence
        if (m_slot->m_netState == RS_NET_IDLE && csbk->getDataContent()) {
            m_slot->setShortLC(m_slot->m_slotNo, dstId, gi ? FLCO::GROUP : FLCO::PRIVATE, Slot::SLCO_ACT_TYPE::DATA);
        } else {
            m_slot->setShortLC(m_slot->m_slotNo, dstId, gi ? FLCO::GROUP : FLCO::PRIVATE, Slot::SLCO_ACT_TYPE::CSBK);        
        }

        bool handled = false;
        switch (csbko) {
        case CSBKO::UU_V_REQ:
            VERBOSE_LOG_CSBK(csbk->toString(), srcId, dstId);
            break;
        case CSBKO::UU_ANS_RSP:
            VERBOSE_LOG_CSBK(csbk->toString(), srcId, dstId);
            break;
        case CSBKO::RAND:
        {
            if (csbk->getFID() == FID_DMRA) {
                if (m_verbose) {
                    LogMessage(LOG_RF, "DMR Slot %u, CSBK, %s, srcId = %u, dstId = %u",
                        m_slot->m_slotNo, csbk->toString().c_str(), srcId, dstId);
                }

                ::ActivityLog("DMR", true, "Slot %u call alert request from %u to %u", m_slot->m_slotNo, srcId, dstId);
            } else {
                handled = true;

                CSBK_RAND* isp = static_cast<CSBK_RAND*>(csbk.get());
                if (m_verbose) {
                    LogMessage(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access), serviceKind = $%02X, serviceOptions = $%02X, serviceExtra = $%02X, srcId = %u, dstId = %u",
                        m_slot->m_slotNo, isp->getServiceKind(), isp->getServiceOptions(), isp->getServiceExtra(), isp->getSrcId(), isp->getDstId());
                }

                switch (isp->getServiceKind()) {
                case ServiceKind::IND_VOICE_CALL:
                    // make sure control data is supported
                    IS_SUPPORT_CONTROL_CHECK(csbk->toString(), ServiceKind::IND_VOICE_CALL, srcId);

                    // validate the source RID
                    VALID_SRCID(csbk->toString(), ServiceKind::IND_VOICE_CALL, srcId);

                    // validate the target RID
                    VALID_DSTID(csbk->toString(), ServiceKind::IND_VOICE_CALL, srcId, dstId);

                    // verify the source RID is registered
                    VERIFY_SRCID_REG(csbk->toString(), ServiceKind::IND_VOICE_CALL, srcId);

                    writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_WAIT_RSN, 1U);

                    if (m_slot->m_authoritative) {
                        writeRF_CSBK_Grant(srcId, dstId, isp->getServiceOptions(), false);
                    } else {
                        if (m_slot->m_network != nullptr)
                            m_slot->m_network->writeGrantReq(modem::DVM_STATE::STATE_DMR, srcId, dstId, m_slot->m_slotNo, true);
                    }
                    break;
                case ServiceKind::GRP_VOICE_CALL:
                    // make sure control data is supported
                    IS_SUPPORT_CONTROL_CHECK(csbk->toString(), ServiceKind::GRP_VOICE_CALL, srcId);

                    // validate the source RID
                    VALID_SRCID(csbk->toString(), ServiceKind::GRP_VOICE_CALL, srcId);

                    // validate the talkgroup ID
                    VALID_TGID(csbk->toString(), ServiceKind::GRP_VOICE_CALL, srcId, dstId);

                    writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_WAIT_RSN, 1U);

                    if (m_slot->m_authoritative) {
                        writeRF_CSBK_Grant(srcId, dstId, isp->getServiceOptions(), true);
                    } else {
                        if (m_slot->m_network != nullptr)
                            m_slot->m_network->writeGrantReq(modem::DVM_STATE::STATE_DMR, srcId, dstId, m_slot->m_slotNo, false);
                    }
                    break;
                case ServiceKind::IND_DATA_CALL:
                case ServiceKind::IND_UDT_DATA_CALL:
                    // make sure control data is supported
                    IS_SUPPORT_CONTROL_CHECK(csbk->toString(), ServiceKind::IND_VOICE_CALL, srcId);

                    // validate the source RID
                    VALID_SRCID(csbk->toString(), ServiceKind::IND_VOICE_CALL, srcId);

                    // validate the target RID
                    VALID_DSTID(csbk->toString(), ServiceKind::IND_VOICE_CALL, srcId, dstId);

                    // verify the source RID is registered
                    VERIFY_SRCID_REG(csbk->toString(), ServiceKind::IND_VOICE_CALL, srcId);

                    writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_WAIT_RSN, 0U);

                    writeRF_CSBK_Data_Grant(srcId, dstId, isp->getServiceOptions(), false);
                    break;
                case ServiceKind::GRP_DATA_CALL:
                case ServiceKind::GRP_UDT_DATA_CALL:
                    // make sure control data is supported
                    IS_SUPPORT_CONTROL_CHECK(csbk->toString(), ServiceKind::GRP_VOICE_CALL, srcId);

                    // validate the source RID
                    VALID_SRCID(csbk->toString(), ServiceKind::GRP_VOICE_CALL, srcId);

                    // validate the talkgroup ID
                    VALID_TGID(csbk->toString(), ServiceKind::GRP_VOICE_CALL, srcId, dstId);

                    writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_WAIT_RSN, 0U);

                    writeRF_CSBK_Data_Grant(srcId, dstId, isp->getServiceOptions(), true);
                    break;
                case ServiceKind::REG_SVC:
                    // make sure control data is supported
                    IS_SUPPORT_CONTROL_CHECK(csbk->toString(), ServiceKind::REG_SVC, srcId);

                    writeRF_CSBK_U_Reg_Rsp(srcId, isp->getServiceOptions());
                    break;
                default:
                    LogWarning(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access), unhandled service, serviceKind = %02X", m_slot->m_slotNo, isp->getServiceKind());
                    // should we drop the CSBK and not repeat it?
                    break;
                }
            }
        }
        break;
        case CSBKO::ACK_RSP:
        {
            VERBOSE_LOG_CSBK(csbk->toString(), srcId, dstId);
            ::ActivityLog("DMR", true, "Slot %u ack response from %u to %u", m_slot->m_slotNo, srcId, dstId);
        }
        break;
        case CSBKO::EXT_FNCT:
        {
            CSBK_EXT_FNCT* isp = static_cast<CSBK_EXT_FNCT*>(csbk.get());
            if (m_verbose) {
                LogMessage(LOG_RF, "DMR Slot %u, CSBK, %s, op = $%02X, arg = %u, tgt = %u",
                    m_slot->m_slotNo, csbk->toString().c_str(), isp->getExtendedFunction(), dstId, srcId);
            }

            // generate activity log entry
            switch (isp->getExtendedFunction()) {
            case ExtendedFunctions::CHECK:
                ::ActivityLog("DMR", true, "Slot %u radio check request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case ExtendedFunctions::INHIBIT:
                ::ActivityLog("DMR", true, "Slot %u radio inhibit request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case ExtendedFunctions::UNINHIBIT:
                ::ActivityLog("DMR", true, "Slot %u radio uninhibit request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case ExtendedFunctions::CHECK_ACK:
                ::ActivityLog("DMR", true, "Slot %u radio check response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case ExtendedFunctions::INHIBIT_ACK:
                ::ActivityLog("DMR", true, "Slot %u radio inhibit response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case ExtendedFunctions::UNINHIBIT_ACK:
                ::ActivityLog("DMR", true, "Slot %u radio uninhibit response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            default:
                LogWarning(LOG_RF, "DMR Slot %u, CSBK, %s, unhandled op, op = $%02X", m_slot->m_slotNo, csbk->toString().c_str(), isp->getExtendedFunction());
                break;
            }
        }
        break;
        case CSBKO::NACK_RSP:
        {
            VERBOSE_LOG_CSBK(csbk->toString(), srcId, dstId);
        }
        break;
        case CSBKO::MAINT:
        {
            CSBK_MAINT* isp = static_cast<CSBK_MAINT*>(csbk.get());
            if (m_verbose) {
                LogMessage(LOG_RF, "DMR Slot %u, CSBK, %s, kind = $%02X, srcId = %u",
                    m_slot->m_slotNo, csbk->toString().c_str(), isp->getMaintKind(), srcId);
            }
        }
        break;
        case CSBKO::PRECCSBK:
        {
            if (m_verbose) {
                LogMessage(LOG_RF, "DMR Slot %u, CSBK, PRECCSBK (%s Preamble CSBK), toFollow = %u, srcId = %u, dstId = %u",
                    m_slot->m_slotNo, csbk->getDataContent() ? "Data" : "CSBK", csbk->getCBF(), srcId, dstId);
            }
        }
        break;
        default:
            LogWarning(LOG_RF, "DMR Slot %u, CSBK, unhandled CSBK, csbko = $%02X, fid = $%02X", m_slot->m_slotNo, csbko, csbk->getFID());
            // should we drop the CSBK and not repeat it?
            break;
        }

        if (!handled) {
            // regenerate the CSBK data
            lc::CSBK::regenerate(data + 2U, dataType);

            // regenerate the Slot Type
            slotType.encode(data + 2U);

            // convert the Data Sync to be from the BS or MS as needed
            Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

            m_slot->m_rfSeqNo = 0U;

            data[0U] = modem::TAG_DATA;
            data[1U] = 0x00U;

            if (m_slot->m_duplex)
                m_slot->addFrame(data);

            m_slot->writeNetwork(data, DataType::CSBK, gi ? FLCO::GROUP : FLCO::PRIVATE, srcId, dstId, 0U, true);
        }

        return true;
    }

    return false;
}

/* Process a data frame from the network. */

void ControlSignaling::processNetwork(const data::NetData& dmrData)
{
    DataType::E dataType = dmrData.getDataType();

    uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
    dmrData.getData(data + 2U);

    if (dataType == DataType::CSBK) {
        std::unique_ptr<lc::CSBK> csbk = CSBKFactory::createCSBK(data + 2U, dataType);
        if (csbk == nullptr) {
            LogError(LOG_NET, "DMR Slot %u, CSBK, unable to decode the network CSBK", m_slot->m_slotNo);
            return;
        }

        uint8_t csbko = csbk->getCSBKO();
        if (csbko == CSBKO::BSDWNACT)
            return;

        // handle updating internal adjacent site information
        if (csbko == CSBKO::BROADCAST) {
            CSBK_BROADCAST* osp = static_cast<CSBK_BROADCAST*>(csbk.get());
            if (osp->getAnncType() == BroadcastAnncType::ANN_WD_TSCC) {
                if (!m_slot->m_enableTSCC) {
                    return;
                }

                if (osp->getSystemId() != m_slot->m_siteData.systemIdentity()) {
                    // update site table data
                    AdjSiteData site;
                    try {
                        site = m_slot->m_adjSiteTable.at(osp->getSystemId());
                    } catch (...) {
                        site = AdjSiteData();
                    }

                    if (m_verbose) {
                        LogMessage(LOG_NET, "DMR Slot %u, CSBK, %s, sysId = $%03X, chNo = %u", m_slot->m_slotNo, csbk->toString().c_str(),
                            osp->getSystemId(), osp->getLogicalCh1());
                    }

                    site.channelNo = osp->getLogicalCh1();
                    site.systemIdentity = osp->getSystemId();
                    site.requireReg = osp->getRequireReg();

                    m_slot->m_adjSiteTable[site.systemIdentity] = site;
                    m_slot->m_adjSiteUpdateCnt[site.systemIdentity] = ADJ_SITE_UPDATE_CNT;
                }

                return;
            }
        }

        bool gi = csbk->getGI();
        uint32_t srcId = csbk->getSrcId();
        uint32_t dstId = csbk->getDstId();

        CHECK_TG_HANG(dstId);

        // if data preamble, signal its existence
        if (csbk->getDataContent()) {
            m_slot->setShortLC(m_slot->m_slotNo, dstId, gi ? FLCO::GROUP : FLCO::PRIVATE, Slot::SLCO_ACT_TYPE::DATA);
        } else {
            m_slot->setShortLC(m_slot->m_slotNo, dstId, gi ? FLCO::GROUP : FLCO::PRIVATE, Slot::SLCO_ACT_TYPE::CSBK);        
        }

        bool handled = false;
        switch (csbko) {
        case CSBKO::UU_V_REQ:
        {
            VERBOSE_LOG_CSBK_NET(csbk->toString(), srcId, dstId);
        }
        break;
        case CSBKO::UU_ANS_RSP:
        {
            VERBOSE_LOG_CSBK_NET(csbk->toString(), srcId, dstId);
        }
        break;
        case CSBKO::RAND:
        {
            if (csbk->getFID() == FID_DMRA) {
                if (m_verbose) {
                    LogMessage(LOG_NET, "DMR Slot %u, CSBK, %s, srcId = %u, dstId = %u",
                        m_slot->m_slotNo, csbk->toString().c_str(), srcId, dstId);
                }

                ::ActivityLog("DMR", false, "Slot %u call alert request from %u to %u", m_slot->m_slotNo, srcId, dstId);
            } else {
                CSBK_RAND* isp = static_cast<CSBK_RAND*>(csbk.get());
                if (m_verbose) {
                    LogMessage(LOG_NET, "DMR Slot %u, CSBK, RAND (Random Access), serviceKind = $%02X, serviceOptions = $%02X, serviceExtra = $%02X, srcId = %u, dstId = %u",
                        m_slot->m_slotNo, isp->getServiceKind(), isp->getServiceOptions(), isp->getServiceExtra(), isp->getSrcId(), isp->getDstId());
                }

                switch (isp->getServiceKind()) {
                case ServiceKind::IND_VOICE_CALL:
                    writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_WAIT_RSN, 1U);

                    if (!m_slot->m_affiliations->isGranted(dstId)) {
                        writeRF_CSBK_Grant(srcId, dstId, isp->getServiceOptions(), false, true);
                    }
                    break;
                case ServiceKind::GRP_VOICE_CALL:
                    writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_WAIT_RSN, 1U);

                    if (!m_slot->m_affiliations->isGranted(dstId)) {
                        writeRF_CSBK_Grant(srcId, dstId, isp->getServiceOptions(), true, true);
                    }
                    break;
                case ServiceKind::IND_DATA_CALL:
                case ServiceKind::IND_UDT_DATA_CALL:
                    writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_WAIT_RSN, 0U);

                    writeRF_CSBK_Data_Grant(srcId, dstId, isp->getServiceOptions(), false, true);
                    break;
                case ServiceKind::GRP_DATA_CALL:
                case ServiceKind::GRP_UDT_DATA_CALL:
                    writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_WAIT_RSN, 0U);

                    writeRF_CSBK_Data_Grant(srcId, dstId, isp->getServiceOptions(), true, true);
                    break;
                case ServiceKind::REG_SVC:
                    break;
                default:
                    LogWarning(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access), unhandled service, serviceKind = %02X", m_slot->m_slotNo, isp->getServiceKind());
                    // should we drop the CSBK and not repeat it?
                    break;
                }

                handled = true;
            }
        }
        break;
        case CSBKO::ACK_RSP:
        {
            VERBOSE_LOG_CSBK_NET(csbk->toString(), srcId, dstId);
            ::ActivityLog("DMR", false, "Slot %u ack response from %u to %u", m_slot->m_slotNo, srcId, dstId);
        }
        break;
        case CSBKO::EXT_FNCT:
        {
            CSBK_EXT_FNCT* isp = static_cast<CSBK_EXT_FNCT*>(csbk.get());
            if (m_verbose) {
                LogMessage(LOG_NET, "DMR Slot %u, CSBK, %s, op = $%02X, arg = %u, tgt = %u",
                    m_slot->m_slotNo, csbk->toString().c_str(), isp->getExtendedFunction(), dstId, srcId);
            }

            // generate activity log entry
            switch (isp->getExtendedFunction()) {
            case ExtendedFunctions::CHECK:
                ::ActivityLog("DMR", false, "Slot %u radio check request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case ExtendedFunctions::INHIBIT:
                ::ActivityLog("DMR", false, "Slot %u radio inhibit request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case ExtendedFunctions::UNINHIBIT:
                ::ActivityLog("DMR", false, "Slot %u radio uninhibit request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case ExtendedFunctions::CHECK_ACK:
                ::ActivityLog("DMR", false, "Slot %u radio check response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case ExtendedFunctions::INHIBIT_ACK:
                ::ActivityLog("DMR", false, "Slot %u radio inhibit response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case ExtendedFunctions::UNINHIBIT_ACK:
                ::ActivityLog("DMR", false, "Slot %u radio uninhibit response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            default:
                LogWarning(LOG_NET, "DMR Slot %u, CSBK, %s, unhandled op, op = $%02X", m_slot->m_slotNo, csbk->toString().c_str(), isp->getExtendedFunction());
                break;
            }
        }
        break;
        case CSBKO::NACK_RSP:
        {
            VERBOSE_LOG_CSBK_NET(csbk->toString(), srcId, dstId);
        }
        break;
        case CSBKO::PRECCSBK:
        {
            if (m_verbose) {
                LogMessage(LOG_NET, "DMR Slot %u, CSBK, PRECCSBK (%s Preamble CSBK), toFollow = %u, srcId = %u, dstId = %u",
                    m_slot->m_slotNo, csbk->getDataContent() ? "Data" : "CSBK", csbk->getCBF(), srcId, dstId);
            }
        }
        break;
        default:
            LogWarning(LOG_NET, "DMR Slot %u, CSBK, unhandled network CSBK, csbko = $%02X, fid = $%02X", m_slot->m_slotNo, csbko, csbk->getFID());
            // should we drop the CSBK and not repeat it?
            break;
        }

        if (!handled) {
            // regenerate the CSBK data
            lc::CSBK::regenerate(data + 2U, dataType);

            // regenerate the Slot Type
            SlotType slotType;
            slotType.decode(data + 2U);
            slotType.setColorCode(m_slot->m_colorCode);
            slotType.encode(data + 2U);

            // convert the Data Sync to be from the BS or MS as needed
            Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

            data[0U] = modem::TAG_DATA;
            data[1U] = 0x00U;

            m_slot->addFrame(data, true);
        }
    }
    else {
        // unhandled data type
        LogWarning(LOG_NET, "DMR Slot %u, unhandled network data, type = $%02X", m_slot->m_slotNo, dataType);
    }
}

/* Helper to write DMR adjacent site information to the network. */

void ControlSignaling::writeAdjSSNetwork()
{
    if (!m_slot->m_enableTSCC) {
        return;
    }

    if (m_slot->m_network != nullptr) {
        // transmit adjacent site broadcast
        std::unique_ptr<CSBK_BROADCAST> csbk = std::make_unique<CSBK_BROADCAST>();
        csbk->siteIdenEntry(m_slot->m_idenEntry);
        csbk->setCdef(false);
        csbk->setAnncType(BroadcastAnncType::ANN_WD_TSCC);
        csbk->setLogicalCh1(m_slot->m_channelNo);
        csbk->setAnnWdCh1(true);
        csbk->setSystemId(m_slot->m_siteData.systemIdentity());
        csbk->setRequireReg(m_slot->m_siteData.requireReg());

        if (m_verbose) {
            LogMessage(LOG_NET, "DMR Slot %u, CSBK, %s, network announce, sysId = $%03X, chNo = %u", m_slot->m_slotNo, csbk->toString().c_str(),
                m_slot->m_siteData.systemIdentity(), m_slot->m_channelNo);
        }

        writeNet_CSBK(csbk.get());
    }
}

/* Helper to write a extended function packet on the RF interface. */

void ControlSignaling::writeRF_Ext_Func(uint32_t func, uint32_t arg, uint32_t dstId)
{
    std::unique_ptr<CSBK_EXT_FNCT> csbk = std::make_unique<CSBK_EXT_FNCT>();
    csbk->setGI(false);
    csbk->setExtendedFunction(func);
    csbk->setSrcId(arg);
    csbk->setDstId(dstId);

    if (m_verbose) {
        LogMessage(LOG_RF, "DMR Slot %u, CSBK, %s, op = $%02X, arg = %u, tgt = %u",
            m_slot->m_slotNo, csbk->toString().c_str(), func, arg, dstId);
    }

    // generate activity log entry
    if (func == ExtendedFunctions::CHECK) {
        ::ActivityLog("DMR", true, "Slot %u radio check request from %u to %u", m_slot->m_slotNo, arg, dstId);
    }
    else if (func == ExtendedFunctions::INHIBIT) {
        ::ActivityLog("DMR", true, "Slot %u radio inhibit request from %u to %u", m_slot->m_slotNo, arg, dstId);
    }
    else if (func == ExtendedFunctions::UNINHIBIT) {
        ::ActivityLog("DMR", true, "Slot %u radio uninhibit request from %u to %u", m_slot->m_slotNo, arg, dstId);
    }

    writeRF_CSBK(csbk.get());
}

/* Helper to write a call alert packet on the RF interface. */

void ControlSignaling::writeRF_Call_Alrt(uint32_t srcId, uint32_t dstId)
{
    std::unique_ptr<CSBK_CALL_ALRT> csbk = std::make_unique<CSBK_CALL_ALRT>();
    csbk->setGI(false);
    csbk->setSrcId(srcId);
    csbk->setDstId(dstId);

    VERBOSE_LOG_CSBK(csbk->toString(), srcId, dstId);
    ::ActivityLog("DMR", true, "Slot %u call alert request from %u to %u", m_slot->m_slotNo, srcId, dstId);

    writeRF_CSBK(csbk.get());
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ControlSignaling class. */

ControlSignaling::ControlSignaling(Slot * slot, network::BaseNetwork * network, bool dumpCSBKData, bool debug, bool verbose) :
    m_slot(slot),
    m_dumpCSBKData(dumpCSBKData),
    m_verbose(verbose),
    m_debug(debug)
{
    /* stub */
}

/* Finalizes a instance of the ControlSignaling class. */

ControlSignaling::~ControlSignaling() = default;

/*
** Modem Frame Queuing
*/

/* Helper to write a CSBK packet. */

void ControlSignaling::writeRF_CSBK(lc::CSBK* csbk, bool imm)
{
    // don't add any frames if the queue is full
    uint8_t len = DMR_FRAME_LENGTH_BYTES + 2U;
    uint32_t space = m_slot->m_txQueue.freeSpace();
    if (space < (len + 1U)) {
        return;
    }

    uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, DMR_FRAME_LENGTH_BYTES);

    SlotType slotType;
    slotType.setColorCode(m_slot->m_colorCode);
    slotType.setDataType(DataType::CSBK);

    // Regenerate the CSBK data
    csbk->encode(data + 2U);

    // Regenerate the Slot Type
    slotType.encode(data + 2U);

    // Convert the Data Sync to be from the BS or MS as needed
    Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

    m_slot->m_rfSeqNo = 0U;

    data[0U] = modem::TAG_DATA;
    data[1U] = 0x00U;

    if (m_slot->m_duplex)
        m_slot->addFrame(data, false, imm);
}

/* Helper to write a network CSBK. */

void ControlSignaling::writeNet_CSBK(lc::CSBK* csbk)
{
    uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, DMR_FRAME_LENGTH_BYTES);

    SlotType slotType;
    slotType.setColorCode(m_slot->m_colorCode);
    slotType.setDataType(DataType::CSBK);

    // Regenerate the CSBK data
    csbk->encode(data + 2U);

    // Regenerate the Slot Type
    slotType.encode(data + 2U);

    // Convert the Data Sync to be from the BS or MS as needed
    Sync::addDMRDataSync(data + 2U, true);

    m_slot->m_rfSeqNo = 0U;

    data[0U] = modem::TAG_DATA;
    data[1U] = 0x00U;

    if (m_slot->m_duplex)
        m_slot->addFrame(data);

    m_slot->writeNetwork(data, DataType::CSBK, csbk->getGI() ? FLCO::GROUP : FLCO::PRIVATE, csbk->getSrcId(), csbk->getDstId(), 0U, true);
}

/*
** Control Signalling Logic
*/

/* Helper to write a ACK RSP packet. */

void ControlSignaling::writeRF_CSBK_ACK_RSP(uint32_t dstId, uint8_t reason, uint8_t responseInfo)
{
    std::unique_ptr<CSBK_ACK_RSP> csbk = std::make_unique<CSBK_ACK_RSP>();
    csbk->setResponse(responseInfo);
    csbk->setReason(reason);
    csbk->setSrcId(WUID_ALL); // hmmm...
    csbk->setDstId(dstId);

    writeRF_CSBK_Imm(csbk.get());
}

/* Helper to write a NACK RSP packet. */

void ControlSignaling::writeRF_CSBK_NACK_RSP(uint32_t dstId, uint8_t reason, uint8_t service)
{
    std::unique_ptr<CSBK_NACK_RSP> csbk = std::make_unique<CSBK_NACK_RSP>();
    csbk->setServiceKind(service);
    csbk->setReason(reason);
    csbk->setSrcId(WUID_ALL); // hmmm...
    csbk->setDstId(dstId);

    writeRF_CSBK_Imm(csbk.get());
}

/* Helper to write a grant packet. */

bool ControlSignaling::writeRF_CSBK_Grant(uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp, bool net, bool skip, uint32_t chNo)
{
    Slot *m_tscc = m_slot->m_dmr->getTSCCSlot();

    uint8_t slot = 0U;

    bool emergency = ((serviceOptions & 0xFFU) & 0x80U) == 0x80U;           // Emergency Flag
    bool privacy = ((serviceOptions & 0xFFU) & 0x40U) == 0x40U;             // Privacy Flag
    bool broadcast = ((serviceOptions & 0xFFU) & 0x10U) == 0x10U;           // Broadcast Flag
    uint8_t priority = ((serviceOptions & 0xFFU) & 0x03U);                  // Priority

    if (dstId == WUID_ALL || dstId == WUID_ALLZ || dstId == WUID_ALLL) {
        return true; // do not generate grant packets for $FFFF (All Call) TGID
    }

    // are we skipping checking?
    if (!skip) {
        if (m_slot->m_rfState != RS_RF_LISTENING && m_slot->m_rfState != RS_RF_DATA) {
            if (!net) {
                LogWarning(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access, VOICE_CALL (Voice Call) denied, traffic in progress, dstId = %u", m_tscc->m_slotNo, dstId);
                writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);

                ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u denied", m_tscc->m_slotNo, srcId, dstId);
                m_slot->m_rfState = RS_RF_REJECTED;
            }

            return false;
        }

        if (m_slot->m_netState != RS_NET_IDLE && dstId == m_slot->m_netLastDstId) {
            if (!net) {
                LogWarning(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access, VOICE_CALL (Voice Call) denied, traffic in progress, dstId = %u", m_tscc->m_slotNo, dstId);
                writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);

                ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u denied", m_tscc->m_slotNo, srcId, dstId);
                m_slot->m_rfState = RS_RF_REJECTED;
            }

            return false;
        }

        // don't transmit grants if the destination ID's don't match and the network TG hang timer is running
        if (m_slot->m_rfLastDstId != 0U) {
            if (m_slot->m_rfLastDstId != dstId && (m_slot->m_rfTGHang.isRunning() && !m_slot->m_rfTGHang.hasExpired())) {
                if (!net) {
                    writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);
                    m_slot->m_rfState = RS_RF_REJECTED;
                }

                return false;
            }
        }

        if (!m_tscc->m_affiliations->isGranted(dstId)) {
            ::lookups::TalkgroupRuleGroupVoice groupVoice = m_tscc->m_tidLookup->find(dstId);
            slot = groupVoice.source().tgSlot();

            // is this an affiliation required group?
            ::lookups::TalkgroupRuleGroupVoice tid = m_tscc->m_tidLookup->find(dstId, slot);
            if (tid.config().affiliated()) {
                if (!m_tscc->m_affiliations->hasGroupAff(dstId)) {
                    LogWarning(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access, GRP_VOICE_CALL (Group Voice Call) ignored, no group affiliations, dstId = %u", m_tscc->m_slotNo, dstId);
                    return false;
                }
            }

            uint32_t availChNo = m_tscc->m_affiliations->getAvailableChannelForSlot(slot);
            if (!m_tscc->m_affiliations->rfCh()->isRFChAvailable() || availChNo == 0U) {
                if (grp) {
                    if (!net) {
                        LogWarning(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access, GRP_VOICE_CALL (Group Voice Call) queued, no channels available, dstId = %u", m_tscc->m_slotNo, dstId);
                        writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_QUEUED_RSN_NO_RESOURCE, (grp) ? 1U : 0U);

                        ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u queued", m_tscc->m_slotNo, srcId, dstId);
                        m_slot->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
                else {
                    if (!net) {
                        LogWarning(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access, IND_VOICE_CALL (Individual Voice Call) queued, no channels available, dstId = %u", m_tscc->m_slotNo, dstId);
                        writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_QUEUED_RSN_NO_RESOURCE, (grp) ? 1U : 0U);

                        ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u queued", m_tscc->m_slotNo, srcId, dstId);
                        m_slot->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
            }
            else {
                if (m_tscc->m_affiliations->grantChSlot(dstId, srcId, slot, GRANT_TIMER_TIMEOUT, grp, net)) {
                    chNo = m_tscc->m_affiliations->getGrantedCh(dstId);
                    slot = m_tscc->m_affiliations->getGrantedSlot(dstId);
                    //m_tscc->m_siteData.setChCnt(m_tscc->m_affiliations->getRFChCnt() + m_tscc->m_affiliations->getGrantedRFChCnt());
                }
            }
        }
        else {
            if (!m_tscc->m_disableGrantSrcIdCheck && !net) {
                // do collision check between grants to see if a SU is attempting a "grant retry" or if this is a
                // different source from the original grant
                uint32_t grantedSrcId = m_tscc->m_affiliations->getGrantedSrcId(dstId);
                if (srcId != grantedSrcId) {
                    if (!net) {
                        LogWarning(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access, VOICE_CALL (Voice Call) denied, traffic in progress, dstId = %u", m_tscc->m_slotNo, dstId);
                        writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);

                        ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u denied", m_tscc->m_slotNo, srcId, dstId);
                        m_slot->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
            }

            chNo = m_tscc->m_affiliations->getGrantedCh(dstId);
            slot = m_tscc->m_affiliations->getGrantedSlot(dstId);

            m_tscc->m_affiliations->touchGrant(dstId);
        }
    }
    else {
        if (m_tscc->m_affiliations->isGranted(dstId)) {
            chNo = m_tscc->m_affiliations->getGrantedCh(dstId);
            slot = m_tscc->m_affiliations->getGrantedSlot(dstId);

            m_tscc->m_affiliations->touchGrant(dstId);
        }
        else {
            return false;
        }
    }

    if (grp) {
        if (!net) {
            ::ActivityLog("DMR", true, "Slot %u group grant request from %u to TG %u", m_tscc->m_slotNo, srcId, dstId);
        }

        // callback REST API to permit the granted TG on the specified voice channel
        if (m_tscc->m_authoritative && m_tscc->m_supervisor &&
            m_tscc->m_channelNo != chNo) {
            ::lookups::VoiceChData voiceChData = m_tscc->m_affiliations->rfCh()->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                json::object req = json::object();
                int state = modem::DVM_STATE::STATE_DMR;
                req["state"].set<int>(state);
                req["dstId"].set<uint32_t>(dstId);
                req["slot"].set<uint8_t>(slot);

                int ret = RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                    HTTP_PUT, PUT_PERMIT_TG, req, voiceChData.ssl(), REST_QUICK_WAIT, m_tscc->m_debug);
                if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
                    ::LogError(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access), failed to permit TG for use, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
                    m_tscc->m_affiliations->releaseGrant(dstId, false);
                    if (!net) {
                        writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);
                        m_slot->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
            }
            else {
                ::LogError(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access), failed to permit TG for use, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
            }
        }

        writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_ACK_RSN_MSG, (grp) ? 1U : 0U);

        std::unique_ptr<CSBK_TV_GRANT> csbk = std::make_unique<CSBK_TV_GRANT>();
        if (broadcast)
            csbk->setCSBKO(CSBKO::BTV_GRANT);
        csbk->setLogicalCh1(chNo);
        csbk->setSlotNo(slot);

        if (m_verbose) {
            LogMessage((net) ? LOG_NET : LOG_RF, "DMR Slot %u, CSBK, %s, emerg = %u, privacy = %u, broadcast = %u, prio = %u, chNo = %u, slot = %u, srcId = %u, dstId = %u",
                m_tscc->m_slotNo, csbk->toString().c_str(), emergency, privacy, broadcast, priority, csbk->getLogicalCh1(), csbk->getSlotNo(), srcId, dstId);
        }

        csbk->setEmergency(emergency);
        csbk->setSrcId(srcId);
        csbk->setDstId(dstId);

        // transmit group grant (x2)
        for (uint8_t i = 0; i < 2U; i++)
            writeRF_CSBK_Imm(csbk.get());

        // if the channel granted isn't the same as the TSCC; remote activate the payload channel
        if (chNo != m_tscc->m_channelNo) {
            ::lookups::VoiceChData voiceChData = m_tscc->m_affiliations->rfCh()->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                json::object req = json::object();
                req["dstId"].set<uint32_t>(dstId);
                req["srcId"].set<uint32_t>(srcId);
                req["slot"].set<uint8_t>(slot);
                req["group"].set<bool>(grp);
                bool voice = true;
                req["voice"].set<bool>(voice);

                RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                                 HTTP_PUT, PUT_DMR_TSCC_PAYLOAD_ACT, req, voiceChData.ssl(), REST_QUICK_WAIT, m_tscc->m_debug);
            }
            else {
                ::LogError(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access), failed to activate payload channel, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
            }
        }
        else {
            m_slot->m_dmr->tsccActivateSlot(slot, dstId, srcId, grp, true);
        }
    }
    else {
        if (!net) {
            ::ActivityLog("DMR", true, "Slot %u individual grant request from %u to TG %u", m_tscc->m_slotNo, srcId, dstId);
        }

        // callback REST API to permit the granted TG on the specified voice channel
        if (m_tscc->m_authoritative && m_tscc->m_supervisor &&
            m_tscc->m_channelNo != chNo) {
            ::lookups::VoiceChData voiceChData = m_tscc->m_affiliations->rfCh()->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                json::object req = json::object();
                int state = modem::DVM_STATE::STATE_DMR;
                req["state"].set<int>(state);
                req["dstId"].set<uint32_t>(dstId);
                req["slot"].set<uint8_t>(slot);

                int ret = RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                    HTTP_PUT, PUT_PERMIT_TG, req, voiceChData.ssl(), REST_QUICK_WAIT, m_tscc->m_debug);
                if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
                    ::LogError(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access), failed to permit TG for use, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
                    m_tscc->m_affiliations->releaseGrant(dstId, false);
                    if (!net) {
                        writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);
                        m_slot->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
            }
            else {
                ::LogError(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access), failed to permit TG for use, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
            }
        }

        writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_ACK_RSN_MSG, (grp) ? 1U : 0U);

        std::unique_ptr<CSBK_PV_GRANT> csbk = std::make_unique<CSBK_PV_GRANT>();
        csbk->setLogicalCh1(chNo);
        csbk->setSlotNo(slot);

        if (m_verbose) {
            LogMessage((net) ? LOG_NET : LOG_RF, "DMR Slot %u, CSBK, %s, emerg = %u, privacy = %u, broadcast = %u, prio = %u, chNo = %u, slot = %u, srcId = %u, dstId = %u",
                m_tscc->m_slotNo, csbk->toString().c_str(), emergency, privacy, broadcast, priority, csbk->getLogicalCh1(), csbk->getSlotNo(), srcId, dstId);
        }

        csbk->setEmergency(emergency);
        csbk->setSrcId(srcId);
        csbk->setDstId(dstId);

        // transmit private grant (x2)
        for (uint8_t i = 0; i < 2U; i++)
            writeRF_CSBK_Imm(csbk.get());

        // if the channel granted isn't the same as the TSCC; remote activate the payload channel
        if (chNo != m_tscc->m_channelNo) {
            ::lookups::VoiceChData voiceChData = m_tscc->m_affiliations->rfCh()->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                json::object req = json::object();
                req["dstId"].set<uint32_t>(dstId);
                req["srcId"].set<uint32_t>(srcId);
                req["slot"].set<uint8_t>(slot);
                req["group"].set<bool>(grp);
                bool voice = true;
                req["voice"].set<bool>(voice);

                RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                    HTTP_PUT, PUT_DMR_TSCC_PAYLOAD_ACT, req, voiceChData.ssl(), REST_QUICK_WAIT, m_tscc->m_debug);
            }
            else {
                ::LogError(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access), failed to activate payload channel, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
            }
        }
        else {
            m_slot->m_dmr->tsccActivateSlot(slot, dstId, srcId, grp, true);
        }
    }

    return true;
}

/* Helper to write a data grant packet. */

bool ControlSignaling::writeRF_CSBK_Data_Grant(uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp, bool net, bool skip, uint32_t chNo)
{
    Slot *m_tscc = m_slot->m_dmr->getTSCCSlot();

    uint8_t slot = 0U;

    bool emergency = ((serviceOptions & 0xFFU) & 0x80U) == 0x80U;           // Emergency Flag
    bool privacy = ((serviceOptions & 0xFFU) & 0x40U) == 0x40U;             // Privacy Flag
    bool broadcast = ((serviceOptions & 0xFFU) & 0x10U) == 0x10U;           // Broadcast Flag
    uint8_t priority = ((serviceOptions & 0xFFU) & 0x03U);                  // Priority

    if (dstId == WUID_ALL || dstId == WUID_ALLZ || dstId == WUID_ALLL) {
        return true; // do not generate grant packets for $FFFF (All Call) TGID
    }

    // are we skipping checking?
    if (!skip) {
        if (m_slot->m_rfState != RS_RF_LISTENING && m_slot->m_rfState != RS_RF_DATA) {
            if (!net) {
                LogWarning(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access, DATA_CALL (Data Call) denied, traffic in progress, dstId = %u", m_tscc->m_slotNo, dstId);
                writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);

                ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u denied", m_tscc->m_slotNo, srcId, dstId);
                m_slot->m_rfState = RS_RF_REJECTED;
            }

            return false;
        }

        if (m_slot->m_netState != RS_NET_IDLE && dstId == m_slot->m_netLastDstId) {
            if (!net) {
                LogWarning(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access, DATA_CALL (Data Call) denied, traffic in progress, dstId = %u", m_tscc->m_slotNo, dstId);
                writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);

                ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u denied", m_tscc->m_slotNo, srcId, dstId);
                m_slot->m_rfState = RS_RF_REJECTED;
            }

            return false;
        }

        // don't transmit grants if the destination ID's don't match and the network TG hang timer is running
        if (m_slot->m_rfLastDstId != 0U) {
            if (m_slot->m_rfLastDstId != dstId && (m_slot->m_rfTGHang.isRunning() && !m_slot->m_rfTGHang.hasExpired())) {
                if (!net) {
                    writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);
                    m_slot->m_rfState = RS_RF_REJECTED;
                }

                return false;
            }
        }

        if (!m_tscc->m_affiliations->isGranted(dstId)) {
            ::lookups::TalkgroupRuleGroupVoice groupVoice = m_tscc->m_tidLookup->find(dstId);
            slot = groupVoice.source().tgSlot();

            uint32_t availChNo = m_tscc->m_affiliations->getAvailableChannelForSlot(slot);
            if (!m_tscc->m_affiliations->rfCh()->isRFChAvailable() || availChNo == 0U) {
                if (grp) {
                    if (!net) {
                        LogWarning(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access, GRP_DATA_CALL (Group Data Call) queued, no channels available, dstId = %u", m_tscc->m_slotNo, dstId);
                        writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_QUEUED_RSN_NO_RESOURCE, (grp) ? 1U : 0U);

                        ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u queued", m_tscc->m_slotNo, srcId, dstId);
                        m_slot->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
                else {
                    if (!net) {
                        LogWarning(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access, IND_DATA_CALL (Individual Data Call) queued, no channels available, dstId = %u", m_tscc->m_slotNo, dstId);
                        writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_QUEUED_RSN_NO_RESOURCE, (grp) ? 1U : 0U);

                        ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u queued", m_tscc->m_slotNo, srcId, dstId);
                        m_slot->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
            }
            else {
                if (m_tscc->m_affiliations->grantChSlot(dstId, srcId, slot, GRANT_TIMER_TIMEOUT, grp, net)) {
                    chNo = m_tscc->m_affiliations->getGrantedCh(dstId);
                    slot = m_tscc->m_affiliations->getGrantedSlot(dstId);

                    //m_tscc->m_siteData.setChCnt(m_tscc->m_affiliations->getRFChCnt() + m_tscc->m_affiliations->getGrantedRFChCnt());
                }
            }
        }
        else {
            chNo = m_tscc->m_affiliations->getGrantedCh(dstId);
            slot = m_tscc->m_affiliations->getGrantedSlot(dstId);

            m_tscc->m_affiliations->touchGrant(dstId);
        }
    }


    if (grp) {
        if (!net) {
            ::ActivityLog("DMR", true, "Slot %u group grant request from %u to TG %u", m_tscc->m_slotNo, srcId, dstId);
        }

        writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_ACK_RSN_MSG, (grp) ? 1U : 0U);

        std::unique_ptr<CSBK_TD_GRANT> csbk = std::make_unique<CSBK_TD_GRANT>();
        csbk->setLogicalCh1(chNo);
        csbk->setSlotNo(slot);

        if (m_verbose) {
            LogMessage((net) ? LOG_NET : LOG_RF, "DMR Slot %u, CSBK, %s, emerg = %u, privacy = %u, broadcast = %u, prio = %u, chNo = %u, slot = %u, srcId = %u, dstId = %u",
                m_tscc->m_slotNo, csbk->toString().c_str(), emergency, privacy, broadcast, priority, csbk->getLogicalCh1(), csbk->getSlotNo(), srcId, dstId);
        }

        csbk->setEmergency(emergency);
        csbk->setSrcId(srcId);
        csbk->setDstId(dstId);

        // transmit group grant (x2)
        for (uint8_t i = 0; i < 2U; i++)
            writeRF_CSBK_Imm(csbk.get());

        // if the channel granted isn't the same as the TSCC; remote activate the payload channel
        if (chNo != m_tscc->m_channelNo) {
            ::lookups::VoiceChData voiceChData = m_tscc->m_affiliations->rfCh()->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                json::object req = json::object();
                req["dstId"].set<uint32_t>(dstId);
                req["srcId"].set<uint32_t>(srcId);
                req["slot"].set<uint8_t>(slot);
                req["group"].set<bool>(grp);
                bool voice = false;
                req["voice"].set<bool>(voice);

                RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                    HTTP_PUT, PUT_DMR_TSCC_PAYLOAD_ACT, req, voiceChData.ssl(), REST_QUICK_WAIT, m_tscc->m_debug);
            }
            else {
                ::LogError(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access), failed to activate payload channel, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
            }
        }
        else {
            m_slot->m_dmr->tsccActivateSlot(slot, dstId, srcId, grp, false);
        }
    }
    else {
        if (!net) {
            ::ActivityLog("DMR", true, "Slot %u individual grant request from %u to TG %u", m_tscc->m_slotNo, srcId, dstId);
        }

        writeRF_CSBK_ACK_RSP(srcId, ReasonCode::TS_ACK_RSN_MSG, (grp) ? 1U : 0U);

        std::unique_ptr<CSBK_PD_GRANT> csbk = std::make_unique<CSBK_PD_GRANT>();
        csbk->setLogicalCh1(chNo);
        csbk->setSlotNo(slot);

        if (m_verbose) {
            LogMessage((net) ? LOG_NET : LOG_RF, "DMR Slot %u, CSBK, %s, emerg = %u, privacy = %u, broadcast = %u, prio = %u, chNo = %u, slot = %u, srcId = %u, dstId = %u",
                m_tscc->m_slotNo, csbk->toString().c_str(), emergency, privacy, broadcast, priority, csbk->getLogicalCh1(), csbk->getSlotNo(), srcId, dstId);
        }

        csbk->setEmergency(emergency);
        csbk->setSrcId(srcId);
        csbk->setDstId(dstId);

        // transmit private grant (x2)
        for (uint8_t i = 0; i < 2U; i++)
            writeRF_CSBK_Imm(csbk.get());

        // if the channel granted isn't the same as the TSCC; remote activate the payload channel
        if (chNo != m_tscc->m_channelNo) {
            ::lookups::VoiceChData voiceChData = m_tscc->m_affiliations->rfCh()->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                json::object req = json::object();
                req["dstId"].set<uint32_t>(dstId);
                req["srcId"].set<uint32_t>(srcId);
                req["slot"].set<uint8_t>(slot);
                req["group"].set<bool>(grp);
                bool voice = false;
                req["voice"].set<bool>(voice);

                RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                    HTTP_PUT, PUT_DMR_TSCC_PAYLOAD_ACT, req, voiceChData.ssl(), REST_QUICK_WAIT, m_tscc->m_debug);
            }
            else {
                ::LogError(LOG_RF, "DMR Slot %u, CSBK, RAND (Random Access), failed to activate payload channel, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
            }
        }
        else {
            m_slot->m_dmr->tsccActivateSlot(slot, dstId, srcId, grp, false);
        }
    }

    return true;
}

/* Helper to write a unit registration response packet. */

void ControlSignaling::writeRF_CSBK_U_Reg_Rsp(uint32_t srcId, uint8_t serviceOptions)
{
    Slot *m_tscc = m_slot->m_dmr->getTSCCSlot();

    bool dereg = (serviceOptions & 0x01U) == 0x01U;
    uint8_t powerSave = (serviceOptions >> 1) & 0x07U;

    // is the SU asking for power saving? if so -- politely tell it off
    if (powerSave > 0U) {
        std::unique_ptr<CSBK_NACK_RSP> csbk = std::make_unique<CSBK_NACK_RSP>();
        csbk->setReason(ReasonCode::TS_DENY_RSN_REG_DENIED);

        if (m_verbose) {
            LogMessage(LOG_RF, "DMR Slot %u, CSBK, %s, SU power saving unsupported, srcId = %u, serviceOptions = $%02X", m_tscc->m_slotNo, csbk->toString().c_str(), srcId, serviceOptions);
        }

        csbk->setSrcId(WUID_REGI);
        csbk->setDstId(srcId);

        writeRF_CSBK_Imm(csbk.get());

        return;
    }

    std::unique_ptr<CSBK_ACK_RSP> csbk = std::make_unique<CSBK_ACK_RSP>();
    csbk->setResponse(0U); // disable TSCC power saving (ETSI TS-102.361-4 6.4.7.2)

    if (!dereg) {
        if (m_verbose) {
            LogMessage(LOG_RF, "DMR Slot %u, CSBK, %s, srcId = %u, serviceOptions = $%02X", m_tscc->m_slotNo, csbk->toString().c_str(), srcId, serviceOptions);
        }

        // remove dynamic unit registration table entry
        m_slot->m_affiliations->unitDereg(srcId);

//        if (m_slot->m_network != nullptr)
//            m_slot->m_network->announceUnitDeregistration(srcId);

        csbk->setReason(ReasonCode::TS_ACK_RSN_REG);
    }
    else
    {
        csbk->setReason(ReasonCode::TS_ACK_RSN_REG);

        // validate the source RID
        if (!acl::AccessControl::validateSrcId(srcId)) {
            LogWarning(LOG_RF, "DMR Slot %u, CSBK, %s, denial, RID rejection, srcId = %u", m_tscc->m_slotNo, csbk->toString().c_str(), srcId);
            ::ActivityLog("DMR", true, "unit registration request from %u denied", srcId);
            csbk->setReason(ReasonCode::TS_DENY_RSN_REG_DENIED);
        }

        if (csbk->getReason() == ReasonCode::TS_ACK_RSN_REG) {
            if (m_verbose) {
                LogMessage(LOG_RF, "DMR Slot %u, CSBK, %s, srcId = %u, serviceOptions = $%02X", m_tscc->m_slotNo, csbk->toString().c_str(), srcId, serviceOptions);
            }

            ::ActivityLog("DMR", true, "unit registration request from %u", srcId);

            // update dynamic unit registration table
            if (!m_slot->m_affiliations->isUnitReg(srcId)) {
                m_slot->m_affiliations->unitReg(srcId);
            }

            if (m_slot->m_network != nullptr)
                m_slot->m_network->announceUnitRegistration(srcId);
        }
    }

    csbk->setSrcId(WUID_REGI);
    csbk->setDstId(srcId);

    writeRF_CSBK_Imm(csbk.get());
}

/* Helper to write a TSCC late entry channel grant packet on the RF interface. */

void ControlSignaling::writeRF_CSBK_Grant_LateEntry(uint32_t dstId, uint32_t srcId, bool grp)
{
    Slot *m_tscc = m_slot->m_dmr->getTSCCSlot();

    uint32_t chNo = m_tscc->m_affiliations->getGrantedCh(dstId);
    uint8_t slot = m_tscc->m_affiliations->getGrantedSlot(dstId);

    if (grp) {
        std::unique_ptr<CSBK_TV_GRANT> csbk = std::make_unique<CSBK_TV_GRANT>();
        csbk->setLogicalCh1(chNo);
        csbk->setSlotNo(slot);

        csbk->setSrcId(srcId);
        csbk->setDstId(dstId);

        csbk->setLateEntry(true);

        writeRF_CSBK(csbk.get());
    }
    else {
/*        
        std::unique_ptr<CSBK_PV_GRANT> csbk = std::make_unique<CSBK_PV_GRANT>();
        csbk->setLogicalCh1(chNo);
        csbk->setSlotNo(slot);

        csbk->setSrcId(srcId);
        csbk->setDstId(dstId);

        writeRF_CSBK(csbk.get());
*/
    }
}

/* Helper to write a payload activation to a TSCC payload channel on the RF interface. */

void ControlSignaling::writeRF_CSBK_Payload_Activate(uint32_t dstId, uint32_t srcId, bool grp, bool voice, bool imm)
{
    std::unique_ptr<CSBK_P_GRANT> csbk = std::make_unique<CSBK_P_GRANT>();
    if (voice) {
        if (grp) {
            csbk->setCSBKO(CSBKO::TV_GRANT);
        }
        else {
            csbk->setCSBKO(CSBKO::PV_GRANT);
        }
    }
    else {
        if (grp) {
            csbk->setCSBKO(CSBKO::TD_GRANT);
        }
        else {
            csbk->setCSBKO(CSBKO::PD_GRANT);
        }
    }

    csbk->setLastBlock(true);

    csbk->setLogicalCh1(m_slot->m_channelNo);
    csbk->setSlotNo(m_slot->m_slotNo);

    csbk->setSrcId(srcId);
    csbk->setDstId(dstId);

    if (m_verbose) {
        LogMessage(LOG_RF, "DMR Slot %u, CSBK, %s, csbko = $%02X, chNo = %u, slot = %u, srcId = %u, dstId = %u",
            m_slot->m_slotNo, csbk->toString().c_str(), csbk->getCSBKO(), csbk->getLogicalCh1(), csbk->getSlotNo(), srcId, dstId);
    }

    m_slot->setShortLC_Payload(m_slot->m_siteData, 1U);
    for (uint8_t i = 0; i < 2U; i++)
        writeRF_CSBK(csbk.get(), imm);
}

/* Helper to write a payload clear to a TSCC payload channel on the RF interface. */

void ControlSignaling::writeRF_CSBK_Payload_Clear(uint32_t dstId, uint32_t srcId, bool grp, bool imm)
{
    std::unique_ptr<CSBK_P_CLEAR> csbk = std::make_unique<CSBK_P_CLEAR>();

    csbk->setGI(grp);

    csbk->setLastBlock(true);

    csbk->setLogicalCh1(m_slot->m_channelNo);
    csbk->setSlotNo(m_slot->m_slotNo);

    csbk->setSrcId(srcId);
    csbk->setDstId(dstId);

    if (m_verbose) {
        LogMessage(LOG_RF, "DMR Slot %u, CSBK, %s, group = %u, chNo = %u, slot = %u, srcId = %u, dstId = %u",
            m_slot->m_slotNo, csbk->toString().c_str(), csbk->getGI(), csbk->getLogicalCh1(), csbk->getSlotNo(), srcId, dstId);
    }

    for (uint8_t i = 0; i < 2U; i++)
        writeRF_CSBK(csbk.get(), imm);
}

/* Helper to write a TSCC Aloha broadcast packet on the RF interface. */

void ControlSignaling::writeRF_TSCC_Aloha()
{
    std::unique_ptr<CSBK_ALOHA> csbk = std::make_unique<CSBK_ALOHA>();
    DEBUG_LOG_CSBK(csbk->toString());
    csbk->setNRandWait(m_slot->m_alohaNRandWait);
    csbk->setBackoffNo(m_slot->m_alohaBackOff);

    writeRF_CSBK(csbk.get());
}

/* Helper to write a TSCC Ann-Wd broadcast packet on the RF interface. */

void ControlSignaling::writeRF_TSCC_Bcast_Ann_Wd(uint32_t channelNo, bool annWd, uint32_t systemIdentity, bool requireReg)
{
    m_slot->m_rfSeqNo = 0U;

    std::unique_ptr<CSBK_BROADCAST> csbk = std::make_unique<CSBK_BROADCAST>();
    csbk->siteIdenEntry(m_slot->m_idenEntry);
    csbk->setCdef(false);
    csbk->setAnncType(BroadcastAnncType::ANN_WD_TSCC);
    csbk->setLogicalCh1(channelNo);
    csbk->setAnnWdCh1(annWd);
    csbk->setSystemId(systemIdentity);
    csbk->setRequireReg(requireReg);

    if (m_debug) {
        LogMessage(LOG_RF, "DMR Slot %u, CSBK, %s, channelNo = %u, annWd = %u",
            m_slot->m_slotNo, csbk->toString().c_str(), channelNo, annWd);
    }

    writeRF_CSBK(csbk.get());
}

/* Helper to write a TSCC Sys_Parm broadcast packet on the RF interface. */

void ControlSignaling::writeRF_TSCC_Bcast_Sys_Parm()
{
    std::unique_ptr<CSBK_BROADCAST> csbk = std::make_unique<CSBK_BROADCAST>();
    DEBUG_LOG_CSBK(csbk->toString());
    csbk->setAnncType(BroadcastAnncType::SITE_PARMS);

    writeRF_CSBK(csbk.get());
}

/* Helper to write a TSCC Git Hash broadcast packet on the RF interface. */

void ControlSignaling::writeRF_TSCC_Git_Hash()
{
    std::unique_ptr<CSBK_DVM_GIT_HASH> csbk = std::make_unique<CSBK_DVM_GIT_HASH>();
    DEBUG_LOG_CSBK(csbk->toString());

    writeRF_CSBK(csbk.get());
}
