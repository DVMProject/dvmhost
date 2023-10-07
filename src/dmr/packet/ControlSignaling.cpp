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
*   Copyright (C) 2015,2016,2017,2018 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2023 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; version 2 of the License.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*/
#include "Defines.h"
#include "dmr/packet/ControlSignaling.h"
#include "dmr/acl/AccessControl.h"
#include "dmr/data/DataHeader.h"
#include "dmr/data/EMB.h"
#include "dmr/lc/ShortLC.h"
#include "dmr/lc/FullLC.h"
#include "dmr/lc/CSBK.h"
#include "dmr/lc/csbk/CSBKFactory.h"
#include "dmr/Slot.h"
#include "dmr/SlotType.h"
#include "dmr/Sync.h"
#include "edac/BPTC19696.h"
#include "edac/CRC.h"
#include "remote/RESTClient.h"
#include "Log.h"
#include "Utils.h"

using namespace dmr;
using namespace dmr::lc::csbk;
using namespace dmr::packet;

#include <cassert>
#include <ctime>
#include <algorithm>
#include <cmath>

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
        writeRF_CSBK_ACK_RSP(_SRCID, TS_DENY_RSN_SYS_UNSUPPORTED_SVC, 0U);              \
        return false;                                                                   \
    }

// Validate the source RID.
#define VALID_SRCID(_PCKT_STR, _PCKT, _SRCID)                                           \
    if (!acl::AccessControl::validateSrcId(_SRCID)) {                                   \
        LogWarning(LOG_RF, "DMR Slot %u, %s denial, RID rejection, srcId = %u", m_slot->m_slotNo, _PCKT_STR.c_str(), _SRCID); \
        writeRF_CSBK_ACK_RSP(_SRCID, TS_DENY_RSN_PERM_USER_REFUSED, 0U);                \
        return false;                                                                   \
    }

// Validate the target RID.
#define VALID_DSTID(_PCKT_STR, _PCKT, _SRCID, _DSTID)                                   \
    if (!acl::AccessControl::validateSrcId(_DSTID)) {                                   \
        LogWarning(LOG_RF, "DMR Slot %u, %s denial, RID rejection, dstId = %u", m_slot->m_slotNo, _PCKT_STR.c_str(), _DSTID); \
        writeRF_CSBK_ACK_RSP(_SRCID, TS_DENY_RSN_TEMP_USER_REFUSED, 0U);                \
        return false;                                                                   \
    }

// Validate the talkgroup ID.
#define VALID_TGID(_PCKT_STR, _PCKT, _SRCID, _DSTID)                                    \
    if (!acl::AccessControl::validateTGId(0U, _DSTID)) {                                \
        LogWarning(LOG_RF, "DMR Slot %u, %s denial, TGID rejection, dstId = %u", m_slot->m_slotNo, _PCKT_STR.c_str(), _DSTID); \
        writeRF_CSBK_ACK_RSP(_SRCID, TS_DENY_RSN_TGT_GROUP_NOT_VALID, 0U);              \
        return false;                                                                   \
    }

// Verify the source RID is registered.
#define VERIFY_SRCID_REG(_PCKT_STR, _PCKT, _SRCID)                                      \
    if (!m_slot->m_affiliations->isUnitReg(_SRCID) && m_slot->m_verifyReg) {            \
        LogWarning(LOG_RF, "DMR Slot %u, %s denial, RID not registered, srcId = %u", m_slot->m_slotNo, _PCKT_STR.c_str(), _SRCID); \
        writeRF_CSBK_ACK_RSP(_SRCID, TS_DENY_RSN_PERM_USER_REFUSED, 0U);                \
        return false;                                                                   \
    }

// Macro helper to verbose log a generic CSBK.
#define VERBOSE_LOG_CSBK(_PCKT_STR, _SRCID, _DSTID)                                     \
    if (m_verbose) {                                                                    \
        LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, %s, srcId = %u, dstId = %u", m_slot->m_slotNo, _PCKT_STR.c_str(), _SRCID, _DSTID); \
    }

// Macro helper to verbose log a generic CSBK.
#define VERBOSE_LOG_CSBK_DST(_PCKT_STR, _DSTID)                                         \
    if (m_verbose) {                                                                    \
        LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, %s, dstId = %u", m_slot->m_slotNo, _PCKT_STR.c_str(), _DSTID);   \
    }

// Macro helper to verbose log a generic network CSBK.
#define VERBOSE_LOG_CSBK_NET(_PCKT_STR, _SRCID, _DSTID)                                 \
    if (m_verbose) {                                                                    \
        LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, %s, srcId = %u, dstId = %u", m_slot->m_slotNo, _PCKT_STR.c_str(), _SRCID, _DSTID); \
    }

// Macro helper to verbose log a generic network CSBK.
#define DEBUG_LOG_CSBK(_PCKT_STR)                                                       \
    if (m_debug) {                                                                      \
        LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, %s", m_slot->m_slotNo, _PCKT_STR.c_str());    \
    }

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t GRANT_TIMER_TIMEOUT = 15U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Process DMR data frame from the RF interface.
/// </summary>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool ControlSignaling::process(uint8_t* data, uint32_t len)
{
    assert(data != nullptr);

    // Get the type from the packet metadata
    uint8_t dataType = data[1U] & 0x0FU;

    SlotType slotType;
    slotType.setColorCode(m_slot->m_colorCode);
    slotType.setDataType(dataType);

    if (dataType == DT_CSBK) {
        // generate a new CSBK and check validity
        std::unique_ptr<lc::CSBK> csbk = CSBKFactory::createCSBK(data + 2U);
        if (csbk == nullptr)
            return false;

        uint8_t csbko = csbk->getCSBKO();
        if (csbko == CSBKO_BSDWNACT)
            return false;

        bool gi = csbk->getGI();
        uint32_t srcId = csbk->getSrcId();
        uint32_t dstId = csbk->getDstId();

        if (srcId != 0U || dstId != 0U) {
            CHECK_TRAFFIC_COLLISION(dstId);

            // validate the source RID
            if (!acl::AccessControl::validateSrcId(srcId)) {
                LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK denial, RID rejection, srcId = %u", m_slot->m_slotNo, srcId);
                m_slot->m_rfState = RS_RF_REJECTED;
                return false;
            }

            // validate the target ID
            if (gi) {
                if (!acl::AccessControl::validateTGId(m_slot->m_slotNo, dstId)) {
                    LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK denial, TGID rejection, srcId = %u, dstId = %u", m_slot->m_slotNo, srcId, dstId);
                    m_slot->m_rfState = RS_RF_REJECTED;
                    return false;
                }
            }
        }

        bool handled = false;
        switch (csbko) {
        case CSBKO_UU_V_REQ:
            VERBOSE_LOG_CSBK(csbk->toString(), srcId, dstId);
            break;
        case CSBKO_UU_ANS_RSP:
            VERBOSE_LOG_CSBK(csbk->toString(), srcId, dstId);
            break;
        case CSBKO_RAND:
        {
            if (csbk->getFID() == FID_DMRA) {
                if (m_verbose) {
                    LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, %s, srcId = %u, dstId = %u",
                        m_slot->m_slotNo, csbk->toString().c_str(), srcId, dstId);
                }

                ::ActivityLog("DMR", true, "Slot %u call alert request from %u to %u", m_slot->m_slotNo, srcId, dstId);
            } else {
                handled = true;

                CSBK_RAND* isp = static_cast<CSBK_RAND*>(csbk.get());
                if (m_verbose) {
                    LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), serviceKind = $%02X, serviceOptions = $%02X, serviceExtra = $%02X, srcId = %u, dstId = %u",
                        m_slot->m_slotNo, isp->getServiceKind(), isp->getServiceOptions(), isp->getServiceExtra(), isp->getSrcId(), isp->getDstId());
                }

                switch (isp->getServiceKind()) {
                case SVC_KIND_IND_VOICE_CALL:
                    // make sure control data is supported
                    IS_SUPPORT_CONTROL_CHECK(csbk->toString(), SVC_KIND_IND_VOICE_CALL, srcId);

                    // validate the source RID
                    VALID_SRCID(csbk->toString(), SVC_KIND_IND_VOICE_CALL, srcId);

                    // validate the target RID
                    VALID_DSTID(csbk->toString(), SVC_KIND_IND_VOICE_CALL, srcId, dstId);

                    // verify the source RID is registered
                    VERIFY_SRCID_REG(csbk->toString(), SVC_KIND_IND_VOICE_CALL, srcId);

                    writeRF_CSBK_ACK_RSP(srcId, TS_WAIT_RSN, 1U);

                    if (m_slot->m_authoritative) {
                        writeRF_CSBK_Grant(srcId, dstId, isp->getServiceOptions(), false);
                    } else {
                        m_slot->m_network->writeGrantReq(modem::DVM_STATE::STATE_DMR, srcId, dstId, m_slot->m_slotNo, true);
                    }
                    break;
                case SVC_KIND_GRP_VOICE_CALL:
                    // make sure control data is supported
                    IS_SUPPORT_CONTROL_CHECK(csbk->toString(), SVC_KIND_GRP_VOICE_CALL, srcId);

                    // validate the source RID
                    VALID_SRCID(csbk->toString(), SVC_KIND_GRP_VOICE_CALL, srcId);

                    // validate the talkgroup ID
                    VALID_TGID(csbk->toString(), SVC_KIND_GRP_VOICE_CALL, srcId, dstId);

                    writeRF_CSBK_ACK_RSP(srcId, TS_WAIT_RSN, 1U);

                    if (m_slot->m_authoritative) {
                        writeRF_CSBK_Grant(srcId, dstId, isp->getServiceOptions(), true);
                    } else {
                        m_slot->m_network->writeGrantReq(modem::DVM_STATE::STATE_DMR, srcId, dstId, m_slot->m_slotNo, false);
                    }
                    break;
                case SVC_KIND_IND_DATA_CALL:
                case SVC_KIND_IND_UDT_DATA_CALL:
                    // make sure control data is supported
                    IS_SUPPORT_CONTROL_CHECK(csbk->toString(), SVC_KIND_IND_VOICE_CALL, srcId);

                    // validate the source RID
                    VALID_SRCID(csbk->toString(), SVC_KIND_IND_VOICE_CALL, srcId);

                    // validate the target RID
                    VALID_DSTID(csbk->toString(), SVC_KIND_IND_VOICE_CALL, srcId, dstId);

                    // verify the source RID is registered
                    VERIFY_SRCID_REG(csbk->toString(), SVC_KIND_IND_VOICE_CALL, srcId);

                    writeRF_CSBK_ACK_RSP(srcId, TS_WAIT_RSN, 0U);

                    writeRF_CSBK_Data_Grant(srcId, dstId, isp->getServiceOptions(), false);
                    break;
                case SVC_KIND_GRP_DATA_CALL:
                case SVC_KIND_GRP_UDT_DATA_CALL:
                    // make sure control data is supported
                    IS_SUPPORT_CONTROL_CHECK(csbk->toString(), SVC_KIND_GRP_VOICE_CALL, srcId);

                    // validate the source RID
                    VALID_SRCID(csbk->toString(), SVC_KIND_GRP_VOICE_CALL, srcId);

                    // validate the talkgroup ID
                    VALID_TGID(csbk->toString(), SVC_KIND_GRP_VOICE_CALL, srcId, dstId);

                    writeRF_CSBK_ACK_RSP(srcId, TS_WAIT_RSN, 0U);

                    writeRF_CSBK_Data_Grant(srcId, dstId, isp->getServiceOptions(), true);
                    break;
                case SVC_KIND_REG_SVC:
                    // make sure control data is supported
                    IS_SUPPORT_CONTROL_CHECK(csbk->toString(), SVC_KIND_REG_SVC, srcId);

                    writeRF_CSBK_U_Reg_Rsp(srcId, isp->getServiceOptions());
                    break;
                default:
                    LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), unhandled service, serviceKind = %02X", m_slot->m_slotNo, isp->getServiceKind());
                    // should we drop the CSBK and not repeat it?
                    break;
                }
            }
        }
        break;
        case CSBKO_ACK_RSP:
        {
            VERBOSE_LOG_CSBK(csbk->toString(), srcId, dstId);
            ::ActivityLog("DMR", true, "Slot %u ack response from %u to %u", m_slot->m_slotNo, srcId, dstId);
        }
        break;
        case CSBKO_EXT_FNCT:
        {
            CSBK_EXT_FNCT* isp = static_cast<CSBK_EXT_FNCT*>(csbk.get());
            if (m_verbose) {
                LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, %s, op = $%02X, arg = %u, tgt = %u",
                    m_slot->m_slotNo, csbk->toString().c_str(), isp->getExtendedFunction(), dstId, srcId);
            }

            // generate activity log entry
            switch (isp->getExtendedFunction()) {
            case DMR_EXT_FNCT_CHECK:
                ::ActivityLog("DMR", true, "Slot %u radio check request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case DMR_EXT_FNCT_INHIBIT:
                ::ActivityLog("DMR", true, "Slot %u radio inhibit request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case DMR_EXT_FNCT_UNINHIBIT:
                ::ActivityLog("DMR", true, "Slot %u radio uninhibit request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case DMR_EXT_FNCT_CHECK_ACK:
                ::ActivityLog("DMR", true, "Slot %u radio check response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case DMR_EXT_FNCT_INHIBIT_ACK:
                ::ActivityLog("DMR", true, "Slot %u radio inhibit response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case DMR_EXT_FNCT_UNINHIBIT_ACK:
                ::ActivityLog("DMR", true, "Slot %u radio uninhibit response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            default:
                LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, %s, unhandled op, op = $%02X", m_slot->m_slotNo, csbk->toString().c_str(), isp->getExtendedFunction());
                break;
            }
        }
        break;
        case CSBKO_NACK_RSP:
        {
            VERBOSE_LOG_CSBK(csbk->toString(), srcId, dstId);
        }
        break;
        case CSBKO_PRECCSBK:
        {
            if (m_verbose) {
                LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_PRECCSBK (%s Preamble CSBK), toFollow = %u, srcId = %u, dstId = %u",
                    m_slot->m_slotNo, csbk->getDataContent() ? "Data" : "CSBK", csbk->getCBF(), srcId, dstId);
            }
        }
        break;
        default:
            LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, unhandled CSBK, csbko = $%02X, fid = $%02X", m_slot->m_slotNo, csbko, csbk->getFID());
            // should we drop the CSBK and not repeat it?
            break;
        }

        if (!handled) {
            // regenerate the CSBK data
            lc::CSBK::regenerate(data + 2U);

            // regenerate the Slot Type
            slotType.encode(data + 2U);

            // convert the Data Sync to be from the BS or MS as needed
            Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

            m_slot->m_rfSeqNo = 0U;

            data[0U] = modem::TAG_DATA;
            data[1U] = 0x00U;

            if (m_slot->m_duplex)
                m_slot->addFrame(data);

            m_slot->writeNetwork(data, DT_CSBK, gi ? FLCO_GROUP : FLCO_PRIVATE, srcId, dstId);
        }

        return true;
    }

    return false;
}

/// <summary>
/// Process a data frame from the network.
/// </summary>
/// <param name="dmrData"></param>
void ControlSignaling::processNetwork(const data::Data & dmrData)
{
    uint8_t dataType = dmrData.getDataType();

    uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
    dmrData.getData(data + 2U);

    if (dataType == DT_CSBK) {
        std::unique_ptr<lc::CSBK> csbk = CSBKFactory::createCSBK(data + 2U);
        if (csbk == nullptr) {
            LogError(LOG_NET, "DMR Slot %u, DT_CSBK, unable to decode the network CSBK", m_slot->m_slotNo);
            return;
        }

        uint8_t csbko = csbk->getCSBKO();
        if (csbko == CSBKO_BSDWNACT)
            return;

        uint32_t srcId = csbk->getSrcId();
        uint32_t dstId = csbk->getDstId();

        CHECK_TG_HANG(dstId);

        bool handled = false;
        switch (csbko) {
        case CSBKO_UU_V_REQ:
        {
            VERBOSE_LOG_CSBK_NET(csbk->toString(), srcId, dstId);
        }
        break;
        case CSBKO_UU_ANS_RSP:
        {
            VERBOSE_LOG_CSBK_NET(csbk->toString(), srcId, dstId);
        }
        break;
        case CSBKO_RAND:
        {
            if (csbk->getFID() == FID_DMRA) {
                if (m_verbose) {
                    LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, %s, srcId = %u, dstId = %u",
                        m_slot->m_slotNo, csbk->toString().c_str(), srcId, dstId);
                }

                ::ActivityLog("DMR", false, "Slot %u call alert request from %u to %u", m_slot->m_slotNo, srcId, dstId);
            } else {
                CSBK_RAND* isp = static_cast<CSBK_RAND*>(csbk.get());
                if (m_verbose) {
                    LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), serviceKind = $%02X, serviceOptions = $%02X, serviceExtra = $%02X, srcId = %u, dstId = %u",
                        m_slot->m_slotNo, isp->getServiceKind(), isp->getServiceOptions(), isp->getServiceExtra(), isp->getSrcId(), isp->getDstId());
                }

                switch (isp->getServiceKind()) {
                case SVC_KIND_IND_VOICE_CALL:
                    writeRF_CSBK_ACK_RSP(srcId, TS_WAIT_RSN, 1U);

                    if (m_slot->m_authoritative) {
                        if (!m_slot->m_affiliations->isGranted(dstId)) {
                            writeRF_CSBK_Grant(srcId, dstId, isp->getServiceOptions(), false, true);
                        }
                    }
                    break;
                case SVC_KIND_GRP_VOICE_CALL:
                    writeRF_CSBK_ACK_RSP(srcId, TS_WAIT_RSN, 1U);

                    if (m_slot->m_authoritative) {
                        if (!m_slot->m_affiliations->isGranted(dstId)) {
                            writeRF_CSBK_Grant(srcId, dstId, isp->getServiceOptions(), true, true);
                        }
                    }
                    break;
                case SVC_KIND_IND_DATA_CALL:
                case SVC_KIND_IND_UDT_DATA_CALL:
                    writeRF_CSBK_ACK_RSP(srcId, TS_WAIT_RSN, 0U);

                    if (!m_slot->m_affiliations->isGranted(dstId)) {
                        writeRF_CSBK_Data_Grant(srcId, dstId, isp->getServiceOptions(), false, true);
                    }
                    break;
                case SVC_KIND_GRP_DATA_CALL:
                case SVC_KIND_GRP_UDT_DATA_CALL:
                    writeRF_CSBK_ACK_RSP(srcId, TS_WAIT_RSN, 0U);

                    if (!m_slot->m_affiliations->isGranted(dstId)) {
                        writeRF_CSBK_Data_Grant(srcId, dstId, isp->getServiceOptions(), true, true);
                    }
                    break;
                case SVC_KIND_REG_SVC:
                    break;
                default:
                    LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), unhandled service, serviceKind = %02X", m_slot->m_slotNo, isp->getServiceKind());
                    // should we drop the CSBK and not repeat it?
                    break;
                }

                handled = true;
            }
        }
        break;
        case CSBKO_ACK_RSP:
        {
            VERBOSE_LOG_CSBK_NET(csbk->toString(), srcId, dstId);
            ::ActivityLog("DMR", false, "Slot %u ack response from %u to %u", m_slot->m_slotNo, srcId, dstId);
        }
        break;
        case CSBKO_EXT_FNCT:
        {
            CSBK_EXT_FNCT* isp = static_cast<CSBK_EXT_FNCT*>(csbk.get());
            if (m_verbose) {
                LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, %s, op = $%02X, arg = %u, tgt = %u",
                    m_slot->m_slotNo, csbk->toString().c_str(), isp->getExtendedFunction(), dstId, srcId);
            }

            // generate activity log entry
            switch (isp->getExtendedFunction()) {
            case DMR_EXT_FNCT_CHECK:
                ::ActivityLog("DMR", false, "Slot %u radio check request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case DMR_EXT_FNCT_INHIBIT:
                ::ActivityLog("DMR", false, "Slot %u radio inhibit request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case DMR_EXT_FNCT_UNINHIBIT:
                ::ActivityLog("DMR", false, "Slot %u radio uninhibit request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case DMR_EXT_FNCT_CHECK_ACK:
                ::ActivityLog("DMR", false, "Slot %u radio check response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case DMR_EXT_FNCT_INHIBIT_ACK:
                ::ActivityLog("DMR", false, "Slot %u radio inhibit response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            case DMR_EXT_FNCT_UNINHIBIT_ACK:
                ::ActivityLog("DMR", false, "Slot %u radio uninhibit response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                break;
            default:
                LogWarning(LOG_NET, "DMR Slot %u, DT_CSBK, %s, unhandled op, op = $%02X", m_slot->m_slotNo, csbk->toString().c_str(), isp->getExtendedFunction());
                break;
            }
        }
        break;
        case CSBKO_NACK_RSP:
        {
            VERBOSE_LOG_CSBK_NET(csbk->toString(), srcId, dstId);
        }
        break;
        case CSBKO_PRECCSBK:
        {
            if (m_verbose) {
                LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_PRECCSBK (%s Preamble CSBK), toFollow = %u, srcId = %u, dstId = %u",
                    m_slot->m_slotNo, csbk->getDataContent() ? "Data" : "CSBK", csbk->getCBF(), srcId, dstId);
            }
        }
        break;
        default:
            LogWarning(LOG_NET, "DMR Slot %u, DT_CSBK, unhandled network CSBK, csbko = $%02X, fid = $%02X", m_slot->m_slotNo, csbko, csbk->getFID());
            // should we drop the CSBK and not repeat it?
            break;
        }

        if (!handled) {
            // regenerate the CSBK data
            lc::CSBK::regenerate(data + 2U);

            // regenerate the Slot Type
            SlotType slotType;
            slotType.decode(data + 2U);
            slotType.setColorCode(m_slot->m_colorCode);
            slotType.encode(data + 2U);

            // convert the Data Sync to be from the BS or MS as needed
            Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

            data[0U] = modem::TAG_DATA;
            data[1U] = 0x00U;

            if (csbko == CSBKO_PRECCSBK && csbk->getDataContent()) {
                uint32_t cbf = NO_PREAMBLE_CSBK + csbk->getCBF() - 1U;
                for (uint32_t i = 0U; i < NO_PREAMBLE_CSBK; i++, cbf--) {
                    // change blocks to follow
                    csbk->setCBF(cbf);

                    // regenerate the CSBK data
                    csbk->encode(data + 2U);

                    // regenerate the Slot Type
                    SlotType slotType;
                    slotType.decode(data + 2U);
                    slotType.setColorCode(m_slot->m_colorCode);
                    slotType.encode(data + 2U);

                    // convert the Data Sync to be from the BS or MS as needed
                    Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

                    m_slot->addFrame(data, true);
                }
            }
            else
                m_slot->addFrame(data, true);
        }
    }
    else {
        // unhandled data type
        LogWarning(LOG_NET, "DMR Slot %u, unhandled network data, type = $%02X", m_slot->m_slotNo, dataType);
    }
}

/// <summary>
/// Helper to write a extended function packet on the RF interface.
/// </summary>
/// <param name="func">Extended function opcode.</param>
/// <param name="arg">Extended function argument.</param>
/// <param name="dstId">Destination radio ID.</param>
void ControlSignaling::writeRF_Ext_Func(uint32_t func, uint32_t arg, uint32_t dstId)
{
    std::unique_ptr<CSBK_EXT_FNCT> csbk = new_unique(CSBK_EXT_FNCT);
    csbk->setGI(false);
    csbk->setExtendedFunction(func);
    csbk->setSrcId(arg);
    csbk->setDstId(dstId);

    if (m_verbose) {
        LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, %s, op = $%02X, arg = %u, tgt = %u",
            m_slot->m_slotNo, csbk->toString().c_str(), func, arg, dstId);
    }

    // generate activity log entry
    if (func == DMR_EXT_FNCT_CHECK) {
        ::ActivityLog("DMR", true, "Slot %u radio check request from %u to %u", m_slot->m_slotNo, arg, dstId);
    }
    else if (func == DMR_EXT_FNCT_INHIBIT) {
        ::ActivityLog("DMR", true, "Slot %u radio inhibit request from %u to %u", m_slot->m_slotNo, arg, dstId);
    }
    else if (func == DMR_EXT_FNCT_UNINHIBIT) {
        ::ActivityLog("DMR", true, "Slot %u radio uninhibit request from %u to %u", m_slot->m_slotNo, arg, dstId);
    }

    writeRF_CSBK(csbk.get());
}

/// <summary>
/// Helper to write a call alert packet on the RF interface.
/// </summary>
/// <param name="srcId">Source radio ID.</param>
/// <param name="dstId">Destination radio ID.</param>
void ControlSignaling::writeRF_Call_Alrt(uint32_t srcId, uint32_t dstId)
{
    std::unique_ptr<CSBK_CALL_ALRT> csbk = new_unique(CSBK_CALL_ALRT);
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

/// <summary>
/// Initializes a new instance of the ControlSignaling class.
/// </summary>
/// <param name="slot">DMR slot.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="dumpCSBKData"></param>
/// <param name="debug">Flag indicating whether DMR debug is enabled.</param>
/// <param name="verbose">Flag indicating whether DMR verbose logging is enabled.</param>
ControlSignaling::ControlSignaling(Slot * slot, network::BaseNetwork * network, bool dumpCSBKData, bool debug, bool verbose) :
    m_slot(slot),
    m_dumpCSBKData(dumpCSBKData),
    m_verbose(verbose),
    m_debug(debug)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the ControlSignaling class.
/// </summary>
ControlSignaling::~ControlSignaling()
{
    /* stub */
}

/*
** Modem Frame Queuing
*/

/// <summary>
/// Helper to write a CSBK packet.
/// </summary>
/// <param name="csbk"></param>
/// <param name="clearBeforeWrite"></param>
/// <param name="imm"></param>
void ControlSignaling::writeRF_CSBK(lc::CSBK* csbk, bool clearBeforeWrite, bool imm)
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
    slotType.setDataType(DT_CSBK);

    // Regenerate the CSBK data
    csbk->encode(data + 2U);

    // Regenerate the Slot Type
    slotType.encode(data + 2U);

    // Convert the Data Sync to be from the BS or MS as needed
    Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

    m_slot->m_rfSeqNo = 0U;

    data[0U] = modem::TAG_DATA;
    data[1U] = 0x00U;

    if (clearBeforeWrite) {
        if (m_slot->m_slotNo == 1U)
            m_slot->m_modem->clearDMRFrame1();
        if (m_slot->m_slotNo == 2U)
            m_slot->m_modem->clearDMRFrame2();
        m_slot->m_txQueue.clear();
    }

    if (m_slot->m_duplex)
        m_slot->addFrame(data, false, imm);
}

/*
** Control Signalling Logic
*/

/// <summary>
/// Helper to write a deny packet.
/// </summary>
/// <param name="dstId"></param>
/// <param name="reason"></param>
/// <param name="responseInfo"></param>
void ControlSignaling::writeRF_CSBK_ACK_RSP(uint32_t dstId, uint8_t reason, uint8_t responseInfo)
{
    std::unique_ptr<CSBK_ACK_RSP> csbk = new_unique(CSBK_ACK_RSP);
    csbk->setResponse(responseInfo);
    csbk->setReason(reason);
    csbk->setSrcId(DMR_WUID_ALL); // hmmm...
    csbk->setDstId(dstId);

    writeRF_CSBK_Imm(csbk.get());
}

/// <summary>
/// Helper to write a deny packet.
/// </summary>
/// <param name="dstId"></param>
/// <param name="reason"></param>
/// <param name="service"></param>
void ControlSignaling::writeRF_CSBK_NACK_RSP(uint32_t dstId, uint8_t reason, uint8_t service)
{
    std::unique_ptr<CSBK_NACK_RSP> csbk = new_unique(CSBK_NACK_RSP);
    csbk->setServiceKind(service);
    csbk->setReason(reason);
    csbk->setSrcId(DMR_WUID_ALL); // hmmm...
    csbk->setDstId(dstId);

    writeRF_CSBK_Imm(csbk.get());
}

/// <summary>
/// Helper to write a grant packet.
/// </summary>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
/// <param name="serviceOptions"></param>
/// <param name="grp"></param>
/// <param name="net"></param>
/// <param name="skip"></param>
/// <param name="chNo"></param>
/// <returns></returns>
bool ControlSignaling::writeRF_CSBK_Grant(uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp, bool net, bool skip, uint32_t chNo)
{
    Slot *m_tscc = m_slot->m_dmr->getTSCCSlot();

    uint8_t slot = 0U;

    bool emergency = ((serviceOptions & 0xFFU) & 0x80U) == 0x80U;           // Emergency Flag
    bool privacy = ((serviceOptions & 0xFFU) & 0x40U) == 0x40U;             // Privacy Flag
    bool broadcast = ((serviceOptions & 0xFFU) & 0x10U) == 0x10U;           // Broadcast Flag
    uint8_t priority = ((serviceOptions & 0xFFU) & 0x03U);                  // Priority

    if (dstId == DMR_WUID_ALL) {
        return true; // do not generate grant packets for $FFFF (All Call) TGID
    }

    // are we skipping checking?
    if (!skip) {
        if (m_slot->m_rfState != RS_RF_LISTENING && m_slot->m_rfState != RS_RF_DATA) {
            if (!net) {
                LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), SVC_KIND_VOICE_CALL (Voice Call) denied, traffic in progress, dstId = %u", m_tscc->m_slotNo, dstId);
                writeRF_CSBK_ACK_RSP(srcId, TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);

                ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u denied", m_tscc->m_slotNo, srcId, dstId);
                m_slot->m_rfState = RS_RF_REJECTED;
            }

            return false;
        }

        if (m_slot->m_netState != RS_NET_IDLE && dstId == m_slot->m_netLastDstId) {
            if (!net) {
                LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), SVC_KIND_VOICE_CALL (Voice Call) denied, traffic in progress, dstId = %u", m_tscc->m_slotNo, dstId);
                writeRF_CSBK_ACK_RSP(srcId, TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);

                ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u denied", m_tscc->m_slotNo, srcId, dstId);
                m_slot->m_rfState = RS_RF_REJECTED;
            }

            return false;
        }

        // don't transmit grants if the destination ID's don't match and the network TG hang timer is running
        if (m_slot->m_rfLastDstId != 0U) {
            if (m_slot->m_rfLastDstId != dstId && (m_slot->m_rfTGHang.isRunning() && !m_slot->m_rfTGHang.hasExpired())) {
                if (!net) {
                    writeRF_CSBK_ACK_RSP(srcId, TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);
                    m_slot->m_rfState = RS_RF_REJECTED;
                }

                return false;
            }
        }

        if (!m_tscc->m_affiliations->isGranted(dstId)) {
            if (!m_tscc->m_affiliations->isRFChAvailable()) {
                if (grp) {
                    if (!net) {
                        LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), SVC_KIND_GRP_VOICE_CALL (Group Voice Call) queued, no channels available, dstId = %u", m_tscc->m_slotNo, dstId);
                        writeRF_CSBK_ACK_RSP(srcId, TS_QUEUED_RSN_NO_RESOURCE, (grp) ? 1U : 0U);

                        ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u queued", m_tscc->m_slotNo, srcId, dstId);
                        m_slot->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
                else {
                    if (!net) {
                        LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), SVC_KIND_IND_VOICE_CALL (Individual Voice Call) queued, no channels available, dstId = %u", m_tscc->m_slotNo, dstId);
                        writeRF_CSBK_ACK_RSP(srcId, TS_QUEUED_RSN_NO_RESOURCE, (grp) ? 1U : 0U);

                        ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u queued", m_tscc->m_slotNo, srcId, dstId);
                        m_slot->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
            }
            else {
                if (m_tscc->m_affiliations->grantCh(dstId, srcId, GRANT_TIMER_TIMEOUT, net)) {
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
                        LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), SVC_KIND_VOICE_CALL (Voice Call) denied, traffic in progress, dstId = %u", m_tscc->m_slotNo, dstId);
                        writeRF_CSBK_ACK_RSP(srcId, TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);

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
        if (m_tscc->m_authoritative && m_tscc->m_supervisor) {
            ::lookups::VoiceChData voiceChData = m_tscc->m_affiliations->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                json::object req = json::object();
                int state = modem::DVM_STATE::STATE_DMR;
                req["state"].set<int>(state);
                req["dstId"].set<uint32_t>(dstId);
                req["slot"].set<uint8_t>(slot);

                int ret = RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                    HTTP_PUT, PUT_PERMIT_TG, req, m_tscc->m_debug);
                if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
                    ::LogError(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), failed to permit TG for use, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
                    m_tscc->m_affiliations->releaseGrant(dstId, false);
                    if (!net) {
                        writeRF_CSBK_ACK_RSP(srcId, TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);
                        m_slot->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
            }
            else {
                ::LogError(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), failed to permit TG for use, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
            }
        }

        writeRF_CSBK_ACK_RSP(srcId, TS_ACK_RSN_MSG, (grp) ? 1U : 0U);

        std::unique_ptr<CSBK_TV_GRANT> csbk = new_unique(CSBK_TV_GRANT);
        if (broadcast)
            csbk->setCSBKO(CSBKO_BTV_GRANT);
        csbk->setLogicalCh1(chNo);
        csbk->setSlotNo(slot);

        if (m_verbose) {
            LogMessage((net) ? LOG_NET : LOG_RF, "DMR Slot %u, DT_CSBK, %s, emerg = %u, privacy = %u, broadcast = %u, prio = %u, chNo = %u, slot = %u, srcId = %u, dstId = %u",
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
            ::lookups::VoiceChData voiceChData = m_tscc->m_affiliations->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                json::object req = json::object();
                req["dstId"].set<uint32_t>(dstId);
                req["srcId"].set<uint32_t>(srcId);
                req["slot"].set<uint8_t>(slot);
                req["group"].set<bool>(grp);
                bool voice = true;
                req["voice"].set<bool>(voice);

                RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                                 HTTP_PUT, PUT_DMR_TSCC_PAYLOAD_ACT, req, m_tscc->m_debug);
            }
            else {
                ::LogError(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), failed to activate payload channel, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
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
        if (m_tscc->m_authoritative && m_tscc->m_supervisor) {
            ::lookups::VoiceChData voiceChData = m_tscc->m_affiliations->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                json::object req = json::object();
                int state = modem::DVM_STATE::STATE_DMR;
                req["state"].set<int>(state);
                req["dstId"].set<uint32_t>(dstId);
                req["slot"].set<uint8_t>(slot);

                int ret = RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                    HTTP_PUT, PUT_PERMIT_TG, req, m_tscc->m_debug);
                if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
                    ::LogError(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), failed to permit TG for use, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
                    m_tscc->m_affiliations->releaseGrant(dstId, false);
                    if (!net) {
                        writeRF_CSBK_ACK_RSP(srcId, TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);
                        m_slot->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
            }
            else {
                ::LogError(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), failed to permit TG for use, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
            }
        }

        writeRF_CSBK_ACK_RSP(srcId, TS_ACK_RSN_MSG, (grp) ? 1U : 0U);

        std::unique_ptr<CSBK_PV_GRANT> csbk = new_unique(CSBK_PV_GRANT);
        csbk->setLogicalCh1(chNo);
        csbk->setSlotNo(slot);

        if (m_verbose) {
            LogMessage((net) ? LOG_NET : LOG_RF, "DMR Slot %u, DT_CSBK, %s, emerg = %u, privacy = %u, broadcast = %u, prio = %u, chNo = %u, slot = %u, srcId = %u, dstId = %u",
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
            ::lookups::VoiceChData voiceChData = m_tscc->m_affiliations->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                json::object req = json::object();
                req["dstId"].set<uint32_t>(dstId);
                req["srcId"].set<uint32_t>(srcId);
                req["slot"].set<uint8_t>(slot);
                req["group"].set<bool>(grp);
                bool voice = true;
                req["voice"].set<bool>(voice);

                RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                    HTTP_PUT, PUT_DMR_TSCC_PAYLOAD_ACT, req, m_tscc->m_debug);
            }
            else {
                ::LogError(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), failed to activate payload channel, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
            }
        }
        else {
            m_slot->m_dmr->tsccActivateSlot(slot, dstId, srcId, grp, true);
        }
    }

    return true;
}

/// <summary>
/// Helper to write a data grant packet.
/// </summary>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
/// <param name="serviceOptions"></param>
/// <param name="grp"></param>
/// <param name="net"></param>
/// <param name="skip"></param>
/// <param name="chNo"></param>
/// <returns></returns>
bool ControlSignaling::writeRF_CSBK_Data_Grant(uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp, bool net, bool skip, uint32_t chNo)
{
    Slot *m_tscc = m_slot->m_dmr->getTSCCSlot();

    uint8_t slot = 0U;

    bool emergency = ((serviceOptions & 0xFFU) & 0x80U) == 0x80U;           // Emergency Flag
    bool privacy = ((serviceOptions & 0xFFU) & 0x40U) == 0x40U;             // Privacy Flag
    bool broadcast = ((serviceOptions & 0xFFU) & 0x10U) == 0x10U;           // Broadcast Flag
    uint8_t priority = ((serviceOptions & 0xFFU) & 0x03U);                  // Priority

    if (dstId == DMR_WUID_ALL) {
        return true; // do not generate grant packets for $FFFF (All Call) TGID
    }

    // are we skipping checking?
    if (!skip) {
        if (m_slot->m_rfState != RS_RF_LISTENING && m_slot->m_rfState != RS_RF_DATA) {
            if (!net) {
                LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), SVC_KIND_DATA_CALL (Data Call) denied, traffic in progress, dstId = %u", m_tscc->m_slotNo, dstId);
                writeRF_CSBK_ACK_RSP(srcId, TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);

                ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u denied", m_tscc->m_slotNo, srcId, dstId);
                m_slot->m_rfState = RS_RF_REJECTED;
            }

            return false;
        }

        if (m_slot->m_netState != RS_NET_IDLE && dstId == m_slot->m_netLastDstId) {
            if (!net) {
                LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), SVC_KIND_DATA_CALL (Data Call) denied, traffic in progress, dstId = %u", m_tscc->m_slotNo, dstId);
                writeRF_CSBK_ACK_RSP(srcId, TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);

                ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u denied", m_tscc->m_slotNo, srcId, dstId);
                m_slot->m_rfState = RS_RF_REJECTED;
            }

            return false;
        }

        // don't transmit grants if the destination ID's don't match and the network TG hang timer is running
        if (m_slot->m_rfLastDstId != 0U) {
            if (m_slot->m_rfLastDstId != dstId && (m_slot->m_rfTGHang.isRunning() && !m_slot->m_rfTGHang.hasExpired())) {
                if (!net) {
                    writeRF_CSBK_ACK_RSP(srcId, TS_DENY_RSN_TGT_BUSY, (grp) ? 1U : 0U);
                    m_slot->m_rfState = RS_RF_REJECTED;
                }

                return false;
            }
        }

        if (!m_tscc->m_affiliations->isGranted(dstId)) {
            if (!m_tscc->m_affiliations->isRFChAvailable()) {
                if (grp) {
                    if (!net) {
                        LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), SVC_KIND_GRP_DATA_CALL (Group Data Call) queued, no channels available, dstId = %u", m_tscc->m_slotNo, dstId);
                        writeRF_CSBK_ACK_RSP(srcId, TS_QUEUED_RSN_NO_RESOURCE, (grp) ? 1U : 0U);

                        ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u queued", m_tscc->m_slotNo, srcId, dstId);
                        m_slot->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
                else {
                    if (!net) {
                        LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), SVC_KIND_IND_DATA_CALL (Individual Data Call) queued, no channels available, dstId = %u", m_tscc->m_slotNo, dstId);
                        writeRF_CSBK_ACK_RSP(srcId, TS_QUEUED_RSN_NO_RESOURCE, (grp) ? 1U : 0U);

                        ::ActivityLog("DMR", true, "Slot %u group grant request %u to TG %u queued", m_tscc->m_slotNo, srcId, dstId);
                        m_slot->m_rfState = RS_RF_REJECTED;
                    }

                    return false;
                }
            }
            else {
                if (m_tscc->m_affiliations->grantCh(dstId, srcId, GRANT_TIMER_TIMEOUT, net)) {
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

        writeRF_CSBK_ACK_RSP(srcId, TS_ACK_RSN_MSG, (grp) ? 1U : 0U);

        std::unique_ptr<CSBK_TD_GRANT> csbk = new_unique(CSBK_TD_GRANT);
        csbk->setLogicalCh1(chNo);
        csbk->setSlotNo(slot);

        if (m_verbose) {
            LogMessage((net) ? LOG_NET : LOG_RF, "DMR Slot %u, DT_CSBK, %s, emerg = %u, privacy = %u, broadcast = %u, prio = %u, chNo = %u, slot = %u, srcId = %u, dstId = %u",
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
            ::lookups::VoiceChData voiceChData = m_tscc->m_affiliations->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                json::object req = json::object();
                req["dstId"].set<uint32_t>(dstId);
                req["srcId"].set<uint32_t>(srcId);
                req["slot"].set<uint8_t>(slot);
                req["group"].set<bool>(grp);
                bool voice = false;
                req["voice"].set<bool>(voice);

                RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                    HTTP_PUT, PUT_DMR_TSCC_PAYLOAD_ACT, req, m_tscc->m_debug);
            }
            else {
                ::LogError(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), failed to activate payload channel, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
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

        writeRF_CSBK_ACK_RSP(srcId, TS_ACK_RSN_MSG, (grp) ? 1U : 0U);

        std::unique_ptr<CSBK_PD_GRANT> csbk = new_unique(CSBK_PD_GRANT);
        csbk->setLogicalCh1(chNo);
        csbk->setSlotNo(slot);

        if (m_verbose) {
            LogMessage((net) ? LOG_NET : LOG_RF, "DMR Slot %u, DT_CSBK, %s, emerg = %u, privacy = %u, broadcast = %u, prio = %u, chNo = %u, slot = %u, srcId = %u, dstId = %u",
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
            ::lookups::VoiceChData voiceChData = m_tscc->m_affiliations->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                json::object req = json::object();
                req["dstId"].set<uint32_t>(dstId);
                req["srcId"].set<uint32_t>(srcId);
                req["slot"].set<uint8_t>(slot);
                req["group"].set<bool>(grp);
                bool voice = false;
                req["voice"].set<bool>(voice);

                RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                    HTTP_PUT, PUT_DMR_TSCC_PAYLOAD_ACT, req, m_tscc->m_debug);
            }
            else {
                ::LogError(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), failed to activate payload channel, chNo = %u, slot = %u", m_tscc->m_slotNo, chNo, slot);
            }
        }
        else {
            m_slot->m_dmr->tsccActivateSlot(slot, dstId, srcId, grp, false);
        }
    }

    return true;
}

/// <summary>
/// Helper to write a unit registration response packet.
/// </summary>
/// <param name="srcId"></param>
/// <param name="serviceOptions"></param>
void ControlSignaling::writeRF_CSBK_U_Reg_Rsp(uint32_t srcId, uint8_t serviceOptions)
{
    Slot *m_tscc = m_slot->m_dmr->getTSCCSlot();

    bool dereg = (serviceOptions & 0x01U) == 0x01U;

    std::unique_ptr<CSBK_ACK_RSP> csbk = new_unique(CSBK_ACK_RSP);

    if (!dereg) {
        if (m_verbose) {
            LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, %s, srcId = %u, serviceOptions = $%02X", m_tscc->m_slotNo, csbk->toString().c_str(), srcId, serviceOptions);
        }

        // remove dynamic unit registration table entry
        m_slot->m_affiliations->unitDereg(srcId);

        csbk->setReason(TS_ACK_RSN_REG);
    }
    else
    {
        csbk->setReason(TS_ACK_RSN_REG);

        // validate the source RID
        if (!acl::AccessControl::validateSrcId(srcId)) {
            LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, %s, denial, RID rejection, srcId = %u", m_tscc->m_slotNo, csbk->toString().c_str(), srcId);
            ::ActivityLog("DMR", true, "unit registration request from %u denied", srcId);
            csbk->setReason(TS_DENY_RSN_REG_DENIED);
        }

        if (csbk->getReason() == TS_ACK_RSN_REG) {
            if (m_verbose) {
                LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, %s, srcId = %u, serviceOptions = $%02X", m_tscc->m_slotNo, csbk->toString().c_str(), srcId, serviceOptions);
            }

            ::ActivityLog("DMR", true, "unit registration request from %u", srcId);

            // update dynamic unit registration table
            if (!m_slot->m_affiliations->isUnitReg(srcId)) {
                m_slot->m_affiliations->unitReg(srcId);
            }
        }
    }

    csbk->setSrcId(DMR_WUID_REGI);
    csbk->setDstId(srcId);

    writeRF_CSBK_Imm(csbk.get());
}

/// <summary>
/// Helper to write a TSCC late entry channel grant packet on the RF interface.
/// </summary>
/// <param name="dstId"></param>
/// <param name="srcId"></param>
void ControlSignaling::writeRF_CSBK_Grant_LateEntry(uint32_t dstId, uint32_t srcId)
{
    Slot *m_tscc = m_slot->m_dmr->getTSCCSlot();

    uint32_t chNo = m_tscc->m_affiliations->getGrantedCh(dstId);
    uint8_t slot = m_tscc->m_affiliations->getGrantedSlot(dstId);

    std::unique_ptr<CSBK_TV_GRANT> csbk = new_unique(CSBK_TV_GRANT);
    csbk->setLogicalCh1(chNo);
    csbk->setSlotNo(slot);

    csbk->setSrcId(srcId);
    csbk->setDstId(dstId);

    writeRF_CSBK(csbk.get());
}

/// <summary>
/// Helper to write a payload grant to a TSCC payload channel on the RF interface.
/// </summary>
/// <param name="dstId"></param>
/// <param name="srcId"></param>
void ControlSignaling::writeRF_CSBK_Payload_Grant(uint32_t dstId, uint32_t srcId, bool grp, bool voice)
{
    std::unique_ptr<CSBK_P_GRANT> csbk = new_unique(CSBK_P_GRANT);
    if (voice) {
        if (grp) {
            csbk->setCSBKO(CSBKO_TV_GRANT);
        }
        else {
            csbk->setCSBKO(CSBKO_PV_GRANT);
        }
    }
    else {
        if (grp) {
            csbk->setCSBKO(CSBKO_TD_GRANT);
        }
        else {
            csbk->setCSBKO(CSBKO_PD_GRANT);
        }
    }

    csbk->setLogicalCh1(m_slot->m_channelNo);
    csbk->setSlotNo(m_slot->m_slotNo);

    csbk->setSrcId(srcId);
    csbk->setDstId(dstId);

    if (m_verbose) {
        LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, %s, chNo = %u, slot = %u, srcId = %u, dstId = %u",
            m_slot->m_slotNo, csbk->toString().c_str(), csbk->getLogicalCh1(), csbk->getSlotNo(), srcId, dstId);
    }

    writeRF_CSBK(csbk.get());
}

/// <summary>
/// Helper to write a TSCC Aloha broadcast packet on the RF interface.
/// </summary>
void ControlSignaling::writeRF_TSCC_Aloha()
{
    std::unique_ptr<CSBK_ALOHA> csbk = new_unique(CSBK_ALOHA);
    DEBUG_LOG_CSBK(csbk->toString());
    csbk->setNRandWait(m_slot->m_alohaNRandWait);
    csbk->setBackoffNo(m_slot->m_alohaBackOff);

    writeRF_CSBK(csbk.get());
}

/// <summary>
/// Helper to write a TSCC Ann-Wd broadcast packet on the RF interface.
/// </summary>
/// <param name="channelNo"></param>
/// <param name="annWd"></param>
void ControlSignaling::writeRF_TSCC_Bcast_Ann_Wd(uint32_t channelNo, bool annWd)
{
    m_slot->m_rfSeqNo = 0U;

    std::unique_ptr<CSBK_BROADCAST> csbk = new_unique(CSBK_BROADCAST);
    csbk->siteIdenEntry(m_slot->m_idenEntry);
    csbk->setCdef(false);
    csbk->setAnncType(BCAST_ANNC_ANN_WD_TSCC);
    csbk->setLogicalCh1(channelNo);
    csbk->setAnnWdCh1(annWd);

    if (m_debug) {
        LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, %s, channelNo = %u, annWd = %u",
            m_slot->m_slotNo, csbk->toString().c_str(), channelNo, annWd);
    }

    writeRF_CSBK(csbk.get());
}

/// <summary>
/// Helper to write a TSCC Sys_Parm broadcast packet on the RF interface.
/// </summary>
void ControlSignaling::writeRF_TSCC_Bcast_Sys_Parm()
{
    std::unique_ptr<CSBK_BROADCAST> csbk = new_unique(CSBK_BROADCAST);
    DEBUG_LOG_CSBK(csbk->toString());
    csbk->setAnncType(BCAST_ANNC_SITE_PARMS);

    writeRF_CSBK(csbk.get());
}

/// <summary>
/// Helper to write a TSCC Git Hash broadcast packet on the RF interface.
/// </summary>
void ControlSignaling::writeRF_TSCC_Git_Hash()
{
    std::unique_ptr<CSBK_DVM_GIT_HASH> csbk = new_unique(CSBK_DVM_GIT_HASH);
    DEBUG_LOG_CSBK(csbk->toString());

    writeRF_CSBK(csbk.get());
}
