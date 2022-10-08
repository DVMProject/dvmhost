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
*   Copyright (C) 2017-2022 by Bryan Biedenkapp N2PLL
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
#include "dmr/edac/Trellis.h"
#include "dmr/lc/ShortLC.h"
#include "dmr/lc/FullLC.h"
#include "dmr/lc/CSBK.h"
#include "dmr/Slot.h"
#include "dmr/SlotType.h"
#include "dmr/Sync.h"
#include "edac/BPTC19696.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace dmr;
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
        LogWarning(LOG_RF, "DMR Slot %u, " _PCKT_STR " denial, unsupported service, srcId = %u", m_slot->m_slotNo, _SRCID); \
        writeRF_CSBK_ACK_RSP(TS_DENY_RSN_SYS_UNSUPPORTED_SVC, _PCKT);                   \
        return false;                                                                   \
    }

// Validate the source RID.
#define VALID_SRCID(_PCKT_STR, _PCKT, _SRCID, _RSN)                                     \
    if (!acl::AccessControl::validateSrcId(_SRCID)) {                                   \
        LogWarning(LOG_RF, "DMR Slot %u, " _PCKT_STR " denial, RID rejection, srcId = %u", m_slot->m_slotNo, _SRCID); \
        writeRF_CSBK_ACK_RSP(_RSN, _PCKT);                                              \
        return false;                                                                   \
    }

// Validate the target RID.
#define VALID_DSTID(_PCKT_STR, _PCKT, _DSTID, _RSN)                                     \
    if (!acl::AccessControl::validateSrcId(_DSTID)) {                                   \
        LogWarning(LOG_RF, "DMR Slot %u, " _PCKT_STR " denial, RID rejection, dstId = %u", m_slot->m_slotNo, _DSTID); \
        writeRF_CSBK_ACK_RSP(_RSN, _PCKT);                                              \
        return false;                                                                   \
    }

// Validate the talkgroup ID.
#define VALID_TGID(_PCKT_STR, _PCKT, _DSTID, _RSN)                                      \
    if (!acl::AccessControl::validateTGId(_DSTID)) {                                    \
        LogWarning(LOG_RF, "DMR Slot %u, " _PCKT_STR " denial, TGID rejection, dstId = %u", m_slot->m_slotNo, _DSTID); \
        writeRF_CSBK_ACK_RSP(_RSN, _PCKT);                                              \
        return false;                                                                   \
    }

// Verify the source RID is registered.
#define VERIFY_SRCID_REG(_PCKT_STR, _PCKT, _SRCID, _RSN)                                \
    if (!m_slot->m_affiliations->isUnitReg(_SRCID) && m_slot->m_verifyReg) {            \
        LogWarning(LOG_RF, "DMR Slot %u, " _PCKT_STR " denial, RID not registered, srcId = %u", m_slot->m_slotNo, _SRCID); \
        writeRF_CSBK_ACK_RSP(_RSN, _PCKT);                                              \
        return false;                                                                   \
    }

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
    assert(data != NULL);

    // Get the type from the packet metadata
    uint8_t dataType = data[1U] & 0x0FU;

    SlotType slotType;
    slotType.setColorCode(m_slot->m_colorCode);
    slotType.setDataType(dataType);

    if (dataType == DT_CSBK) {
        // generate a new CSBK and check validity
        lc::CSBK csbk = lc::CSBK(m_slot->m_siteData, m_slot->m_idenEntry, m_slot->m_dumpCSBKData);

        bool valid = csbk.decode(data + 2U);
        if (!valid)
            return false;

        uint8_t csbko = csbk.getCSBKO();
        if (csbko == CSBKO_BSDWNACT)
            return false;

        bool gi = csbk.getGI();
        uint32_t srcId = csbk.getSrcId();
        uint32_t dstId = csbk.getDstId();

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
            if (m_verbose) {
                LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_UU_V_REQ (Unit to Unit Voice Service Request), src = %u, dst = %s%u",
                    m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
            }
            break;
        case CSBKO_UU_ANS_RSP:
            if (m_verbose) {
                LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_UU_ANS_RSP (Unit to Unit Voice Service Answer Response), src = %u, dst = %s%u",
                    m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
            }
            break;
        case CSBKO_RAND:
            if (csbk.getFID() == FID_DMRA) {
                if (m_verbose) {
                    LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_CALL_ALRT (Call Alert), src = %u, dst = %s%u",
                        m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
                }

                ::ActivityLog("DMR", true, "Slot %u call alert request from %u to %u", m_slot->m_slotNo, srcId, dstId);
            } else {
                handled = true;

                if (m_verbose) {
                    LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), emerg = %u, prio = %u, serviceKind = $%02X, serviceOptions = $%02X, serviceExtra = $%02X, srcId = %u, dstId = %u",
                        m_slot->m_slotNo, csbk.getEmergency(), csbk.getPriority(), csbk.getServiceKind(), csbk.getServiceOptions(), csbk.getServiceExtra(), csbk.getSrcId(), csbk.getDstId());
                }

                switch (csbk.getServiceKind()) {
                case SVC_KIND_IND_VOICE_CALL:
                    // TODO TODO TODO
                    break;
                case SVC_KIND_GRP_VOICE_CALL:
                    // TODO TODO TODO
                    break;
                case SVC_KIND_IND_DATA_CALL:
                case SVC_KIND_IND_UDT_DATA_CALL:
                    // TODO TODO TODO
                    break;
                case SVC_KIND_GRP_DATA_CALL:
                case SVC_KIND_GRP_UDT_DATA_CALL:
                    // TODO TODO TODO
                    break;
                case SVC_KIND_REG_SVC:
                    // make sure control data is supported
                    IS_SUPPORT_CONTROL_CHECK("DT_CSBK, CSBKO_RAND (Random Access), SVC_KIND_REG_SVC (Registration Service)", SVC_KIND_REG_SVC, srcId);

                    writeRF_CSBK_U_Reg_Rsp(srcId, csbk.getServiceOptions());
                    break;
                default:
                    LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), unhandled service, serviceKind = %02X", m_slot->m_slotNo, csbk.getServiceKind());
                    // should we drop the CSBK and not repeat it?
                    break;
                }
            }
            break;
        case CSBKO_ACK_RSP:
            if (m_verbose) {
                LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_ACK_RSP (Acknowledge Response), src = %u, dst = %s%u",
                    m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
            }

            ::ActivityLog("DMR", true, "Slot %u ack response from %u to %u", m_slot->m_slotNo, srcId, dstId);
            break;
        case CSBKO_EXT_FNCT:
            if (m_verbose) {
                LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_EXT_FNCT (Extended Function), op = $%02X, arg = %u, tgt = %u",
                    m_slot->m_slotNo, csbk.getServiceKind(), dstId, srcId);
            }

            // generate activity log entry
            switch (csbk.getServiceKind()) {
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
                LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_EXT_FNCT (Extended Function), unhandled op, op = $%02X", m_slot->m_slotNo, csbk.getServiceKind());
                break;
            }
            break;
        case CSBKO_NACK_RSP:
            if (m_verbose) {
                LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_NACK_RSP (Negative Acknowledgment Response), src = %u, dst = %s%u",
                    m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
            }
            break;
        case CSBKO_PRECCSBK:
            if (m_verbose) {
                LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_PRECCSBK (%s Preamble CSBK), toFollow = %u, src = %u, dst = %s%u",
                    m_slot->m_slotNo, csbk.getDataContent() ? "Data" : "CSBK", csbk.getCBF(), srcId, gi ? "TG " : "", dstId);
            }
            break;
        default:
            LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, unhandled CSBK, csbko = $%02X, fid = $%02X", m_slot->m_slotNo, csbko, csbk.getFID());
            // should we drop the CSBK and not repeat it?
            break;
        }

        if (!handled) {
            // regenerate the CSBK data
            csbk.encode(data + 2U);

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
        lc::CSBK csbk = lc::CSBK(m_slot->m_siteData, m_slot->m_idenEntry, m_slot->m_dumpCSBKData);
        csbk.setVerbose(m_dumpCSBKData);

        bool valid = csbk.decode(data + 2U);
        if (!valid) {
            LogError(LOG_NET, "DMR Slot %u, DT_CSBK, unable to decode the network CSBK", m_slot->m_slotNo);
            return;
        }

        uint8_t csbko = csbk.getCSBKO();
        if (csbko == CSBKO_BSDWNACT)
            return;

        bool gi = csbk.getGI();
        uint32_t srcId = csbk.getSrcId();
        uint32_t dstId = csbk.getDstId();

        CHECK_TG_HANG(dstId);

        bool handled = false;
        switch (csbko) {
        case CSBKO_UU_V_REQ:
            if (m_verbose) {
                LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_UU_V_REQ (Unit to Unit Voice Service Request), src = %u, dst = %s%u",
                    m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
            }
            break;
        case CSBKO_UU_ANS_RSP:
            if (m_verbose) {
                LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_UU_ANS_RSP (Unit to Unit Voice Service Answer Response), src = %u, dst = %s%u",
                    m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
            }
            break;
        case CSBKO_RAND:
            if (csbk.getFID() == FID_DMRA) {
                if (m_verbose) {
                    LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_CALL_ALRT (Call Alert), src = %u, dst = %s%u",
                        m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
                }

                ::ActivityLog("DMR", false, "Slot %u call alert request from %u to %u", m_slot->m_slotNo, srcId, dstId);
            } else {
                if (m_verbose) {
                    LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), emerg = %u, prio = %u, serviceKind = $%02X, serviceOptions = $%02X, serviceExtra = $%02X, srcId = %u, dstId = %u",
                        m_slot->m_slotNo, csbk.getEmergency(), csbk.getPriority(), csbk.getServiceKind(), csbk.getServiceOptions(), csbk.getServiceExtra(), csbk.getSrcId(), csbk.getDstId());
                }
            }
            break;
        case CSBKO_ACK_RSP:
            if (m_verbose) {
                LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_ACK_RSP (Acknowledge Response), src = %u, dst = %s%u",
                    m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
            }

            ::ActivityLog("DMR", false, "Slot %u ack response from %u to %u", m_slot->m_slotNo, srcId, dstId);
            break;
        case CSBKO_EXT_FNCT:
            if (m_verbose) {
                LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_EXT_FNCT (Extended Function), op = $%02X, arg = %u, tgt = %u",
                    m_slot->m_slotNo, csbk.getServiceKind(), dstId, srcId);
            }

            // generate activity log entry
            switch (csbk.getServiceKind()) {
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
                LogWarning(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_EXT_FNCT (Extended Function), unhandled op, op = $%02X", m_slot->m_slotNo, csbk.getServiceKind());
                break;
            }
            break;
        case CSBKO_NACK_RSP:
            if (m_verbose) {
                LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_NACK_RSP (Negative Acknowledgment Response), src = %u, dst = %s%u",
                    m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
            }
            break;
        case CSBKO_PRECCSBK:
            if (m_verbose) {
                LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_PRECCSBK (%s Preamble CSBK), toFollow = %u, src = %u, dst = %s%u",
                    m_slot->m_slotNo, csbk.getDataContent() ? "Data" : "CSBK", csbk.getCBF(), srcId, gi ? "TG " : "", dstId);
            }
            break;
        default:
            LogWarning(LOG_NET, "DMR Slot %u, DT_CSBK, unhandled network CSBK, csbko = $%02X, fid = $%02X", m_slot->m_slotNo, csbko, csbk.getFID());
            // should we drop the CSBK and not repeat it?
            break;
        }

        if (!handled) {
            // regenerate the CSBK data
            csbk.encode(data + 2U);

            // regenerate the Slot Type
            SlotType slotType;
            slotType.decode(data + 2U);
            slotType.setColorCode(m_slot->m_colorCode);
            slotType.encode(data + 2U);

            // convert the Data Sync to be from the BS or MS as needed
            Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

            data[0U] = modem::TAG_DATA;
            data[1U] = 0x00U;

            if (csbko == CSBKO_PRECCSBK && csbk.getDataContent()) {
                uint32_t cbf = NO_PREAMBLE_CSBK + csbk.getCBF() - 1U;
                for (uint32_t i = 0U; i < NO_PREAMBLE_CSBK; i++, cbf--) {
                    // change blocks to follow
                    csbk.setCBF(cbf);

                    // regenerate the CSBK data
                    csbk.encode(data + 2U);

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
    if (m_verbose) {
        LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_EXT_FNCT (Extended Function), op = $%02X, arg = %u, tgt = %u",
            m_slot->m_slotNo, func, arg, dstId);
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

    lc::CSBK csbk = lc::CSBK(m_slot->m_siteData, m_slot->m_idenEntry, m_slot->m_dumpCSBKData);
    csbk.setVerbose(m_dumpCSBKData);
    csbk.setCSBKO(CSBKO_EXT_FNCT);
    csbk.setFID(FID_DMRA);

    csbk.setGI(false);
    csbk.setCBF(func);
    csbk.setSrcId(arg);
    csbk.setDstId(dstId);

    writeRF_CSBK(csbk);
}

/// <summary>
/// Helper to write a call alert packet on the RF interface.
/// </summary>
/// <param name="srcId">Source radio ID.</param>
/// <param name="dstId">Destination radio ID.</param>
void ControlSignaling::writeRF_Call_Alrt(uint32_t srcId, uint32_t dstId)
{
    if (m_verbose) {
        LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_CALL_ALRT (Call Alert), src = %u, dst = %u",
            m_slot->m_slotNo, srcId, dstId);
    }

    ::ActivityLog("DMR", true, "Slot %u call alert request from %u to %u", m_slot->m_slotNo, srcId, dstId);

    lc::CSBK csbk = lc::CSBK(m_slot->m_siteData, m_slot->m_idenEntry, m_slot->m_dumpCSBKData);
    csbk.setVerbose(m_dumpCSBKData);
    csbk.setCSBKO(CSBKO_RAND);
    csbk.setFID(FID_DMRA);

    csbk.setGI(false);
    csbk.setSrcId(srcId);
    csbk.setDstId(dstId);

    writeRF_CSBK(csbk);
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

/// <summary>
/// Helper to write a CSBK packet.
/// </summary>
/// <param name="csbk"></param>
/// <param name="clearBeforeWrite"></param>
void ControlSignaling::writeRF_CSBK(lc::CSBK csbk, bool clearBeforeWrite)
{
    Slot *m_tscc = m_slot->m_dmr->getTSCCSlot();
    if (m_tscc != NULL) {
        if (!m_tscc->m_enableTSCC)
            return;

        // don't add any frames if the queue is full
        uint8_t len = DMR_FRAME_LENGTH_BYTES + 2U;
        uint32_t space = m_tscc->m_queue.freeSpace();
        if (space < (len + 1U)) {
            return;
        }

        uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
        ::memset(data + 2U, 0x00U, DMR_FRAME_LENGTH_BYTES);

        SlotType slotType;
        slotType.setColorCode(m_tscc->m_colorCode);
        slotType.setDataType(DT_CSBK);

        // Regenerate the CSBK data
        csbk.encode(data + 2U);

        // Regenerate the Slot Type
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_tscc->m_duplex);

        m_tscc->m_rfSeqNo = 0U;

        data[0U] = modem::TAG_DATA;
        data[1U] = 0x00U;

        if (clearBeforeWrite) {
            if (m_tscc->m_slotNo == 1U)
                m_tscc->m_modem->clearDMRData1();
            if (m_tscc->m_slotNo == 2U)
                m_tscc->m_modem->clearDMRData2();
            m_tscc->m_queue.clear();
        }

        if (m_tscc->m_duplex)
            m_tscc->addFrame(data);
    }
}

/// <summary>
/// Helper to write a deny packet.
/// </summary>
/// <param name="reason"></param>
/// <param name="service"></param>
void ControlSignaling::writeRF_CSBK_ACK_RSP(uint8_t reason, uint8_t service)
{
    lc::CSBK csbk = lc::CSBK(m_slot->m_siteData, m_slot->m_idenEntry, m_slot->m_dumpCSBKData);
    csbk.setVerbose(m_dumpCSBKData);
    csbk.setCSBKO(CSBKO_ACK_RSP);
    csbk.setFID(FID_ETSI);

    csbk.setReason(reason);

    writeRF_CSBK(csbk);
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

    lc::CSBK csbk = lc::CSBK(m_slot->m_siteData, m_slot->m_idenEntry, m_slot->m_dumpCSBKData);
    csbk.setVerbose(m_dumpCSBKData);
    csbk.setCSBKO(CSBKO_ACK_RSP);
    csbk.setFID(FID_ETSI);

    if (!dereg) {
        if (m_verbose) {
            LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), SVC_KIND_REG_SVC (Registration Service), srcId = %u, serviceOptions = $%02X", m_tscc->m_slotNo, srcId, serviceOptions);
        }

        // remove dynamic unit registration table entry
        m_slot->m_affiliations->unitDereg(srcId);

        csbk.setReason(TS_ACK_RSN_REG);
    }
    else
    {
        csbk.setReason(TS_ACK_RSN_REG);

        // validate the source RID
        if (!acl::AccessControl::validateSrcId(srcId)) {
            LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), SVC_KIND_REG_SVC (Registration Service), denial, RID rejection, srcId = %u", m_tscc->m_slotNo, srcId);
            ::ActivityLog("DMR", true, "unit registration request from %u denied", srcId);
            csbk.setReason(TS_DENY_RSN_REG_DENIED);
        }

        if (csbk.getReason() == TS_ACK_RSN_REG) {
            if (m_verbose) {
                LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), SVC_KIND_REG_SVC (Registration Service), srcId = %u, serviceOptions = $%02X", m_tscc->m_slotNo, srcId, serviceOptions);
            }

            ::ActivityLog("DMR", true, "unit registration request from %u", srcId);

            // update dynamic unit registration table
            if (!m_slot->m_affiliations->isUnitReg(srcId)) {
                m_slot->m_affiliations->unitReg(srcId);
            }
        }
    }

    csbk.setSrcId(DMR_WUID_REGI);
    csbk.setDstId(srcId);

    writeRF_CSBK(csbk);
}

/// <summary>
/// Helper to write a TSCC Aloha broadcast packet on the RF interface.
/// </summary>
void ControlSignaling::writeRF_TSCC_Aloha()
{
    if (m_debug) {
        LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_ALOHA (Aloha)", m_slot->m_slotNo);
    }

    lc::CSBK csbk = lc::CSBK(m_slot->m_siteData, m_slot->m_idenEntry, false/*m_slot->m_dumpCSBKData*/);
    csbk.setCSBKO(CSBKO_ALOHA);
    csbk.setFID(FID_ETSI);

    csbk.setNRandWait(m_slot->m_alohaNRandWait);
    csbk.setBackoffNo(m_slot->m_alohaBackOff);

    writeRF_CSBK(csbk);
}

/// <summary>
/// Helper to write a TSCC Ann-Wd broadcast packet on the RF interface.
/// </summary>
/// <param name="channelNo"></param>
/// <param name="annWd"></param>
void ControlSignaling::writeRF_TSCC_Bcast_Ann_Wd(uint32_t channelNo, bool annWd)
{
    if (m_debug) {
        LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_BROADCAST (Broadcast), BCAST_ANNC_ANN_WD_TSCC (Announce-WD TSCC Channel), channelNo = %u, annWd = %u",
            m_slot->m_slotNo, channelNo, annWd);
    }

    m_slot->m_rfSeqNo = 0U;

    lc::CSBK csbk = lc::CSBK(m_slot->m_siteData, m_slot->m_idenEntry, false/*m_slot->m_dumpCSBKData*/);
    csbk.setCdef(false);
    csbk.setCSBKO(CSBKO_BROADCAST);
    csbk.setFID(FID_ETSI);

    csbk.setAnncType(BCAST_ANNC_ANN_WD_TSCC);
    csbk.setLogicalCh1(channelNo);
    csbk.setAnnWdCh1(annWd);

    writeRF_CSBK(csbk);
}

/// <summary>
/// Helper to write a TSCC Sys_Parm broadcast packet on the RF interface.
/// </summary>
void ControlSignaling::writeRF_TSCC_Bcast_Sys_Parm()
{
    if (m_debug) {
        LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_BROADCAST (Broadcast), BCAST_ANNC_SITE_PARMS (Announce Site Parms)", m_slot->m_slotNo);
    }

    lc::CSBK csbk = lc::CSBK(m_slot->m_siteData, m_slot->m_idenEntry, false/*m_slot->m_dumpCSBKData*/);
    csbk.setCSBKO(CSBKO_BROADCAST);
    csbk.setFID(FID_ETSI);

    csbk.setAnncType(BCAST_ANNC_SITE_PARMS);

    writeRF_CSBK(csbk);
}

/// <summary>
/// Helper to write a TSCC Git Hash broadcast packet on the RF interface.
/// </summary>
void ControlSignaling::writeRF_TSCC_Git_Hash()
{
    if (m_debug) {
        LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_DVM_GIT_HASH (DVM Git Hash)", m_slot->m_slotNo);
    }

    lc::CSBK csbk = lc::CSBK(m_slot->m_siteData, m_slot->m_idenEntry, false/*m_slot->m_dumpCSBKData*/);
    csbk.setCSBKO(CSBKO_DVM_GIT_HASH);
    csbk.setFID(FID_DVM);

    writeRF_CSBK(csbk);
}
