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
#include "common/p25/lc/tdulc/TDULCFactory.h"
#include "common/p25/sndcp/SNDCPFactory.h"
#include "common/p25/P25Utils.h"
#include "common/p25/Sync.h"
#include "common/edac/CRC.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "p25/packet/Data.h"
#include "ActivityLog.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::data;
using namespace p25::sndcp;
using namespace p25::packet;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t MAX_PDU_RETRY_CNT = 2U;
const uint32_t CONV_REG_WAIT_TIMEOUT = 750U; // ms
const uint32_t SNDCP_READY_TIMEOUT = 10U;
const uint32_t SNDCP_STANDBY_TIMEOUT = 60U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Resets the data states for the RF interface. */

void Data::resetRF()
{
    m_rfPDUCount = 0U;
    m_rfPDUBits = 0U;
}

/* Process a data frame from the RF interface. */

bool Data::process(uint8_t* data, uint32_t len)
{
    assert(data != nullptr);

    // decode the NID
    bool valid = m_p25->m_nid.decode(data + 2U);

    if (m_p25->m_rfState == RS_RF_LISTENING && !valid)
        return false;

    if (m_prevRfState != RS_RF_DATA) {
        m_prevRfState = m_p25->m_rfState;
    }

    uint8_t duid = m_p25->m_nid.getDUID();

    // are we interrupting a running CC?
    if (m_p25->m_ccRunning) {
        m_p25->m_ccHalted = true;
    }

    // handle individual DUIDs
    if (duid == DUID::PDU) {
        m_inbound = true;

        if (m_p25->m_rfState != RS_RF_DATA) {
            m_rfPDUCount = 0U;
            m_rfPDUBits = 0U;

            ::memset(m_rfPDU, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

            m_p25->m_rfState = RS_RF_DATA;

            ::memset(m_rfPduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);
            m_rfPduUserDataLength = 0U;
        }

        //Utils::dump(1U, "P25, Data::process(), Raw PDU ISP", data, len);

        uint32_t start = m_rfPDUCount * P25_PDU_FRAME_LENGTH_BITS;

        uint8_t buffer[P25_PDU_FRAME_LENGTH_BYTES];
        ::memset(buffer, 0x00U, P25_PDU_FRAME_LENGTH_BYTES);

        uint32_t bits = P25Utils::decode(data + 2U, buffer, start, start + P25_PDU_FRAME_LENGTH_BITS);
        m_rfPDUBits += Utils::getBits(buffer, m_rfPDU, 0U, bits);

        if (m_rfPDUCount == 0U) {
            ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
            Utils::getBitRange(m_rfPDU, buffer, P25_PREAMBLE_LENGTH_BITS, P25_PDU_FEC_LENGTH_BITS);

            bool ret = m_rfAssembler->disassemble(buffer, P25_PDU_FEC_LENGTH_BYTES, true);
            if (!ret) {
                m_rfPDUCount = 0U;
                m_rfPDUBits = 0U;
                m_p25->m_rfState = m_prevRfState;
                return false;
            }

            // if we're a dedicated CC or in control only mode, we only want to handle AMBTs. Otherwise return
            if ((m_p25->m_dedicatedControl || m_p25->m_controlOnly) && m_rfAssembler->dataHeader.getFormat() != PDUFormatType::AMBT) {
                if (m_debug) {
                    LogDebug(LOG_RF, "CC only mode, ignoring non-AMBT PDU from RF");
                }
                
                m_p25->m_ccHalted = false;
                
                m_rfPDUCount = 0U;
                m_rfPDUBits = 0U;
                m_p25->m_rfState = m_prevRfState;
                return false;
            }

            // did we receive a response header?
            if (m_rfAssembler->dataHeader.getFormat() == PDUFormatType::RSP) {
                LogInfoEx(LOG_RF, P25_PDU_STR ", ISP, response, fmt = $%02X, rspClass = $%02X, rspType = $%02X, rspStatus = $%02X, llId = %u, srcLlId = %u",
                        m_rfAssembler->dataHeader.getFormat(), m_rfAssembler->dataHeader.getResponseClass(), m_rfAssembler->dataHeader.getResponseType(), m_rfAssembler->dataHeader.getResponseStatus(),
                        m_rfAssembler->dataHeader.getLLId(), m_rfAssembler->dataHeader.getSrcLLId());

                if (m_rfAssembler->dataHeader.getResponseClass() == PDUAckClass::ACK && m_rfAssembler->dataHeader.getResponseType() == PDUAckType::ACK) {
                    LogInfoEx(LOG_RF, P25_PDU_STR ", ISP, response, OSP ACK, llId = %u, all blocks received OK, n = %u",
                        m_rfAssembler->dataHeader.getLLId(), m_rfAssembler->dataHeader.getResponseStatus());
                    if (m_retryPDUData != nullptr && m_retryPDUBitLength > 0U) {
                        delete m_retryPDUData;
                        m_retryPDUData = nullptr;

                        m_retryPDUBitLength = 0U;
                        m_retryCount = 0U;
                    }
                } else {
                    if (m_rfAssembler->dataHeader.getResponseClass() == PDUAckClass::NACK) {
                        switch (m_rfAssembler->dataHeader.getResponseType()) {
                            case PDUAckType::NACK_ILLEGAL:
                                LogInfoEx(LOG_RF, P25_PDU_STR ", ISP, response, OSP NACK, illegal format, llId = %u",
                                    m_rfAssembler->dataHeader.getLLId());
                                break;
                            case PDUAckType::NACK_PACKET_CRC:
                                LogInfoEx(LOG_RF, P25_PDU_STR ", ISP, response, OSP NACK, packet CRC error, llId = %u, n = %u",
                                    m_rfAssembler->dataHeader.getLLId(), m_rfAssembler->dataHeader.getResponseStatus());
                                break;
                            case PDUAckType::NACK_SEQ:
                            case PDUAckType::NACK_OUT_OF_SEQ:
                                LogInfoEx(LOG_RF, P25_PDU_STR ", ISP, response, OSP NACK, packet out of sequence, llId = %u, seqNo = %u",
                                    m_rfAssembler->dataHeader.getLLId(), m_rfAssembler->dataHeader.getResponseStatus());
                                break;
                            case PDUAckType::NACK_UNDELIVERABLE:
                                LogInfoEx(LOG_RF, P25_PDU_STR ", ISP, response, OSP NACK, packet undeliverable, llId = %u, n = %u",
                                    m_rfAssembler->dataHeader.getLLId(), m_rfAssembler->dataHeader.getResponseStatus());
                                break;

                            default:
                                break;
                            }
                    } else if (m_rfAssembler->dataHeader.getResponseClass() == PDUAckClass::ACK_RETRY) {
                        LogInfoEx(LOG_RF, P25_PDU_STR ", ISP, response, OSP ACK RETRY, llId = %u",
                            m_rfAssembler->dataHeader.getLLId());

                        // really this is supposed to check the bit field in the included response 
                        // and only return those bits -- but we're responding with the entire previous packet...
                        if (m_retryPDUData != nullptr && m_retryPDUBitLength > 0U) {
                            if (m_retryCount < MAX_PDU_RETRY_CNT) {
                                m_p25->writeRF_Preamble();
                                writeRF_PDU(m_retryPDUData, m_retryPDUBitLength, false, true);
                                m_retryCount++;
                            }
                            else {
                                delete m_retryPDUData;
                                m_retryPDUData = nullptr;

                                m_retryPDUBitLength = 0U;
                                m_retryCount = 0U;

                                LogInfoEx(LOG_RF, P25_PDU_STR ", ISP, response, OSP ACK RETRY, llId = %u, exceeded retries, undeliverable",
                                    m_rfAssembler->dataHeader.getLLId());

                                writeRF_PDU_Ack_Response(PDUAckClass::NACK, PDUAckType::NACK_UNDELIVERABLE, m_rfAssembler->dataHeader.getNs(), m_rfAssembler->dataHeader.getLLId(), m_rfAssembler->dataHeader.getSrcLLId());
                            }
                        }
                    }
                }

                // rewrite the response to the network
                writeNetwork(0U, buffer, P25_PDU_FEC_LENGTH_BYTES, true);

                // only repeat the PDU locally if the packet isn't for the FNE
                if (m_repeatPDU && m_rfAssembler->dataHeader.getLLId() != WUID_FNE) {
                    writeRF_PDU_Ack_Response(m_rfAssembler->dataHeader.getResponseClass(), m_rfAssembler->dataHeader.getResponseType(), m_rfAssembler->dataHeader.getResponseStatus(),
                        m_rfAssembler->dataHeader.getLLId(), m_rfAssembler->dataHeader.getSrcLLId());
                }

                m_rfPDUCount = 0U;
                m_rfPDUBits = 0U;
                m_rfPduUserDataLength = 0U;
                ::memset(m_rfPDU, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

                m_p25->m_rfState = RS_RF_LISTENING;
                m_inbound = false;
                return true;
            }

            m_rfPDUCount++;
        }

        if (m_p25->m_rfState == RS_RF_DATA) {
            uint32_t blocksToFollow = m_rfAssembler->dataHeader.getBlocksToFollow();
            uint32_t bitLength = ((blocksToFollow + 1U) * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;

            if (m_rfPDUBits >= bitLength) {
                for (uint32_t i = P25_PREAMBLE_LENGTH_BITS + P25_PDU_FEC_LENGTH_BITS; i < bitLength; i += P25_PDU_FEC_LENGTH_BITS) {
                    LogDebugEx(LOG_P25, "Data::process()", "blocksToFollow = %u, bitLength = %u, rfPDUBits = %u, rfPDUCount = %u", blocksToFollow, bitLength, m_rfPDUBits, m_rfPDUCount);
                    ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
                    Utils::getBitRange(m_rfPDU, buffer, i, P25_PDU_FEC_LENGTH_BITS);

                    bool ret = m_rfAssembler->disassemble(buffer, P25_PDU_FEC_LENGTH_BYTES);
                    if (!ret) {
                        m_rfPDUCount = 0U;
                        m_rfPDUBits = 0U;
                        m_p25->m_rfState = m_prevRfState;
                        return false;
                    }

                    m_rfPDUCount++;
                }
            } else {
                LogDebugEx(LOG_P25, "Data::process()", "blocksToFollow = %u, bitLength = %u, rfPDUBits = %u, rfPDUCount = %u", blocksToFollow, bitLength, m_rfPDUBits, m_rfPDUCount);
                ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
                Utils::getBitRange(m_rfPDU, buffer, P25_PREAMBLE_LENGTH_BITS + P25_PDU_FEC_LENGTH_BITS, P25_PDU_FEC_LENGTH_BITS);

                bool ret = m_rfAssembler->disassemble(buffer, P25_PDU_FEC_LENGTH_BYTES);
                if (!ret) {
                    m_rfPDUCount = 0U;
                    m_rfPDUBits = 0U;
                    m_p25->m_rfState = m_prevRfState;
                    return false;
                }

                m_rfPDUCount++;
            }

            if (m_rfAssembler->getComplete()) {
                m_rfPduUserDataLength = m_rfAssembler->getUserDataLength();
                m_rfAssembler->getUserData(m_rfPduUserData);

                uint8_t sap = (m_rfAssembler->getExtendedAddress()) ? m_rfAssembler->dataHeader.getEXSAP() : m_rfAssembler->dataHeader.getSAP();
                if (m_rfAssembler->getAuxiliaryES())
                    sap = m_rfAssembler->dataHeader.getEXSAP();

                uint32_t srcId = (m_rfAssembler->getExtendedAddress()) ? m_rfAssembler->dataHeader.getSrcLLId() : m_rfAssembler->dataHeader.getLLId();
                uint32_t dstId = m_rfAssembler->dataHeader.getLLId();

                // handle standard P25 service access points
                switch (sap) {
                case PDUSAP::ARP:
                {
                    /* bryanb: quick and dirty ARP logging */
                    uint8_t arpPacket[P25_PDU_ARP_PCKT_LENGTH];
                    ::memset(arpPacket, 0x00U, P25_PDU_ARP_PCKT_LENGTH);
                    ::memcpy(arpPacket, m_rfPduUserData, P25_PDU_ARP_PCKT_LENGTH);

                    uint16_t opcode = GET_UINT16(arpPacket, 6U);
                    uint32_t srcHWAddr = GET_UINT24(arpPacket, 8U);
                    uint32_t srcProtoAddr = GET_UINT32(arpPacket, 11U);
                    //uint32_t tgtHWAddr = GET_UINT24(arpPacket, 15U);
                    uint32_t tgtProtoAddr = GET_UINT32(arpPacket, 18U);

                    if (m_verbose) {
                        if (opcode == P25_PDU_ARP_REQUEST) {
                            LogInfoEx(LOG_RF, P25_PDU_STR ", ARP request, who has %s? tell %s (%u)", __IP_FROM_UINT(tgtProtoAddr).c_str(), __IP_FROM_UINT(srcProtoAddr).c_str(), srcHWAddr);
                        } else if (opcode == P25_PDU_ARP_REPLY) {
                            LogInfoEx(LOG_RF, P25_PDU_STR ", ARP reply, %s is at %u", __IP_FROM_UINT(srcProtoAddr).c_str(), srcHWAddr);
                        }
                    }

                    writeNet_PDU_User(m_rfAssembler->dataHeader, m_rfAssembler->getExtendedAddress(), m_rfAssembler->getAuxiliaryES(), 
                        m_rfPduUserData);
                    writeRF_PDU_Buffered(); // re-generate buffered PDU and send it on
                }
                break;
                case PDUSAP::SNDCP_CTRL_DATA:
                {
                    if (m_rfAssembler->getUndecodableBlockCount() > 0U)
                        break;

                    if (m_verbose) {
                        LogInfoEx(LOG_RF, P25_PDU_STR ", SNDCP_CTRL_DATA (SNDCP Control Data), blocksToFollow = %u",
                            m_rfAssembler->dataHeader.getBlocksToFollow());
                    }

                    processSNDCPControl(m_rfPduUserData);
                    writeNet_PDU_User(m_rfAssembler->dataHeader, m_rfAssembler->getExtendedAddress(), m_rfAssembler->getAuxiliaryES(), 
                        m_rfPduUserData);
                }
                break;
                case PDUSAP::CONV_DATA_REG:
                {
                    if (m_rfAssembler->getUndecodableBlockCount() > 0U) {
                        break;
                    }

                    if (m_verbose) {
                        LogInfoEx(LOG_RF, P25_PDU_STR ", CONV_DATA_REG (Conventional Data Registration), blocksToFollow = %u",
                            m_rfAssembler->dataHeader.getBlocksToFollow());
                    }

                    processConvDataReg(m_rfPduUserData);
                    writeNet_PDU_User(m_rfAssembler->dataHeader, m_rfAssembler->getExtendedAddress(), m_rfAssembler->getAuxiliaryES(), 
                        m_rfPduUserData);
                }
                break;
                case PDUSAP::UNENC_KMM:
                case PDUSAP::ENC_KMM:
                {
                    if (m_rfAssembler->getUndecodableBlockCount() > 0U)
                        break;

                    if (m_verbose) {
                        LogInfoEx(LOG_RF, P25_PDU_STR ", KMM (Key Management Message), blocksToFollow = %u",
                            m_rfAssembler->dataHeader.getBlocksToFollow());
                    }

                    writeNet_PDU_User(m_rfAssembler->dataHeader, m_rfAssembler->getExtendedAddress(), m_rfAssembler->getAuxiliaryES(), 
                        m_rfPduUserData);
                }
                break;
                case PDUSAP::TRUNK_CTRL:
                {
                    if (m_rfAssembler->getUndecodableBlockCount() > 0U)
                        break;

                    if (m_verbose) {
                        LogInfoEx(LOG_RF, P25_PDU_STR ", TRUNK_CTRL (Alternate MBT Packet), lco = $%02X, blocksToFollow = %u",
                            m_rfAssembler->dataHeader.getAMBTOpcode(), m_rfAssembler->dataHeader.getBlocksToFollow());
                    }

                    m_p25->m_control->processMBT(m_rfAssembler->dataHeader, m_rfAssembler->dataBlocks);
                }
                break;
                default:
                    writeNet_PDU_User(m_rfAssembler->dataHeader, m_rfAssembler->getExtendedAddress(), m_rfAssembler->getAuxiliaryES(), 
                        m_rfPduUserData);

                    // only repeat the PDU locally if the packet isn't for the FNE
                    if (m_repeatPDU && m_rfAssembler->dataHeader.getLLId() != WUID_FNE) {
                        ::ActivityLog("P25", true, "RF data transmission from %u to %u, %u blocks", srcId, dstId, m_rfAssembler->dataHeader.getBlocksToFollow());
                        LogInfoEx(LOG_RF, "P25 Data Call (Local Repeat), srcId = %u, dstId = %u", srcId, dstId);

                        if (m_verbose) {
                            LogInfoEx(LOG_RF, P25_PDU_STR ", repeating PDU, llId = %u", (m_rfAssembler->getExtendedAddress()) ? m_rfAssembler->dataHeader.getSrcLLId() : m_rfAssembler->dataHeader.getLLId());
                        }

                        writeRF_PDU_Buffered(); // re-generate buffered PDU and send it on
                        ::ActivityLog("P25", true, "end of RF data transmission");
                    }
                    break;
                }

                m_rfPDUCount = 0U;
                m_rfPDUBits = 0U;
                m_rfPduUserDataLength = 0U;
                ::memset(m_rfPDU, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

                m_p25->m_rfState = RS_RF_LISTENING;
            }
        }

        m_inbound = false;
        return true;
    }
    else {
        LogError(LOG_RF, "P25 unhandled data DUID, duid = $%02X", duid);
    }

    return false;
}

/* Process a data frame from the network. */

bool Data::processNetwork(uint8_t* data, uint32_t len, uint8_t currentBlock, uint32_t blockLength)
{
    if ((m_p25->m_netState != RS_NET_DATA) || (currentBlock == 0U)) {
        m_p25->m_netState = RS_NET_DATA;
        m_inbound = false;

        bool ret = m_netAssembler->disassemble(data + 24U, blockLength, true);
        if (!ret) {
            m_p25->m_netState = RS_NET_IDLE;
            return false;
        }

        // if we're a dedicated CC or in control only mode, we only want to handle AMBTs. Otherwise return
        if ((m_p25->m_dedicatedControl || m_p25->m_controlOnly) && m_netAssembler->dataHeader.getFormat() != PDUFormatType::AMBT) {
            if (m_debug) {
                LogDebug(LOG_NET, "CC only mode, ignoring non-AMBT PDU from network");
            }

            m_p25->m_netState = RS_NET_IDLE;
            return false;
        }

        // did we receive a response header?
        if (m_netAssembler->dataHeader.getFormat() == PDUFormatType::RSP) {
            m_p25->m_netState = RS_NET_IDLE;

            if (m_verbose) {
                LogInfoEx(LOG_NET, P25_PDU_STR ", FNE ISP, response, fmt = $%02X, rspClass = $%02X, rspType = $%02X, rspStatus = $%02X, llId = %u, srcLlId = %u",
                        m_netAssembler->dataHeader.getFormat(), m_netAssembler->dataHeader.getResponseClass(), m_netAssembler->dataHeader.getResponseType(), m_netAssembler->dataHeader.getResponseStatus(),
                        m_netAssembler->dataHeader.getLLId(), m_netAssembler->dataHeader.getSrcLLId());

                if (m_netAssembler->dataHeader.getResponseClass() == PDUAckClass::ACK && m_netAssembler->dataHeader.getResponseType() == PDUAckType::ACK) {
                    LogInfoEx(LOG_NET, P25_PDU_STR ", FNE ISP, response, OSP ACK, llId = %u, all blocks received OK, n = %u",
                        m_netAssembler->dataHeader.getLLId(), m_netAssembler->dataHeader.getResponseStatus());
                } else {
                    if (m_netAssembler->dataHeader.getResponseClass() == PDUAckClass::NACK) {
                        switch (m_netAssembler->dataHeader.getResponseType()) {
                            case PDUAckType::NACK_ILLEGAL:
                                LogInfoEx(LOG_NET, P25_PDU_STR ", FNE ISP, response, OSP NACK, illegal format, llId = %u",
                                    m_netAssembler->dataHeader.getLLId());
                                break;
                            case PDUAckType::NACK_PACKET_CRC:
                                LogInfoEx(LOG_NET, P25_PDU_STR ", FNE ISP, response, OSP NACK, packet CRC error, llId = %u, n = %u",
                                    m_netAssembler->dataHeader.getLLId(), m_netAssembler->dataHeader.getResponseStatus());
                                break;
                            case PDUAckType::NACK_SEQ:
                            case PDUAckType::NACK_OUT_OF_SEQ:
                                LogInfoEx(LOG_NET, P25_PDU_STR ", FNE ISP, response, OSP NACK, packet out of sequence, llId = %u, seqNo = %u",
                                    m_netAssembler->dataHeader.getLLId(), m_netAssembler->dataHeader.getResponseStatus());
                                break;
                            case PDUAckType::NACK_UNDELIVERABLE:
                                LogInfoEx(LOG_NET, P25_PDU_STR ", FNE ISP, response, OSP NACK, packet undeliverable, llId = %u, n = %u",
                                    m_netAssembler->dataHeader.getLLId(), m_netAssembler->dataHeader.getResponseStatus());
                                break;

                            default:
                                break;
                            }
                    }
                }
            }

            writeRF_PDU_Ack_Response(m_netAssembler->dataHeader.getResponseClass(), m_netAssembler->dataHeader.getResponseType(), m_netAssembler->dataHeader.getResponseStatus(),
                m_netAssembler->dataHeader.getLLId(), (m_netAssembler->dataHeader.getSrcLLId() > 0U), m_netAssembler->dataHeader.getSrcLLId());
        }

        return true;
    }

    if (m_p25->m_netState == RS_NET_DATA) {
        // block 0 is always the PDU header block -- if we got here with that bail bail bail
        if (currentBlock == 0U) {
            return false; // bail
        }

        bool ret = m_netAssembler->disassemble(data + 24U, blockLength);
        if (!ret) {
            m_p25->m_netState = RS_NET_IDLE;
            return false;
        }
        else {
            if (m_netAssembler->getComplete()) {
                m_netPduUserDataLength = m_netAssembler->getUserDataLength();
                m_netAssembler->getUserData(m_netPduUserData);

                uint32_t srcId = (m_netAssembler->getExtendedAddress()) ? m_netAssembler->dataHeader.getSrcLLId() : m_netAssembler->dataHeader.getLLId();
                uint32_t dstId = m_netAssembler->dataHeader.getLLId();

                uint8_t sap = (m_netAssembler->getExtendedAddress()) ? m_netAssembler->dataHeader.getEXSAP() : m_netAssembler->dataHeader.getSAP();
                if (m_netAssembler->getAuxiliaryES())
                    sap = m_netAssembler->dataHeader.getEXSAP();

                // handle standard P25 service access points
                switch (sap) {
                case PDUSAP::ARP:
                {
                    /* bryanb: quick and dirty ARP logging */
                    uint8_t arpPacket[P25_PDU_ARP_PCKT_LENGTH];
                    ::memset(arpPacket, 0x00U, P25_PDU_ARP_PCKT_LENGTH);
                    ::memcpy(arpPacket, m_netPduUserData, P25_PDU_ARP_PCKT_LENGTH);

                    uint16_t opcode = GET_UINT16(arpPacket, 6U);
                    uint32_t srcHWAddr = GET_UINT24(arpPacket, 8U);
                    uint32_t srcProtoAddr = GET_UINT32(arpPacket, 11U);
                    //uint32_t tgtHWAddr = GET_UINT24(arpPacket, 15U);
                    uint32_t tgtProtoAddr = GET_UINT32(arpPacket, 18U);

                    if (m_verbose) {
                        if (opcode == P25_PDU_ARP_REQUEST) {
                            LogInfoEx(LOG_NET, P25_PDU_STR ", ARP request, who has %s? tell %s (%u)", __IP_FROM_UINT(tgtProtoAddr).c_str(), __IP_FROM_UINT(srcProtoAddr).c_str(), srcHWAddr);
                        } else if (opcode == P25_PDU_ARP_REPLY) {
                            LogInfoEx(LOG_NET, P25_PDU_STR ", ARP reply, %s is at %u", __IP_FROM_UINT(srcProtoAddr).c_str(), srcHWAddr);
                        }
                    }

                    writeNet_PDU_Buffered(); // re-generate buffered PDU and send it on
                }
                break;
                default:
                    ::ActivityLog("P25", false, "Net data transmission from %u to %u, %u blocks", srcId, dstId, m_netAssembler->dataHeader.getBlocksToFollow());
                    LogInfoEx(LOG_NET, "P25 Data Call, srcId = %u, dstId = %u", srcId, dstId);

                    if (m_verbose) {
                        LogInfoEx(LOG_NET, P25_PDU_STR ", transmitting network PDU, llId = %u", (m_netAssembler->getExtendedAddress()) ? m_netAssembler->dataHeader.getSrcLLId() : m_netAssembler->dataHeader.getLLId());
                    }

                    writeNet_PDU_Buffered(); // re-generate buffered PDU and send it on

                    ::ActivityLog("P25", false, "end of Net data transmission");
                    break;
                }

                m_netPduUserDataLength = 0U;
                m_p25->m_netState = RS_NET_IDLE;
                m_p25->m_network->resetP25();
            }
        }
    }

    return true;
}

/* Helper to check if a logical link ID has registered with data services. */

bool Data::hasLLIdFNEReg(uint32_t llId) const
{
    // lookup dynamic FNE registration table entry
    try {
        ulong64_t tblIpAddr = m_fneRegTable.at(llId);
        if (tblIpAddr != 0U) {
            return true;
        }
        else {
            return false;
        }
    } catch (...) {
        return false;
    }
}

/* Helper to write user data as a P25 PDU packet. */

void Data::writeRF_PDU_User(data::DataHeader& dataHeader, bool extendedAddress, bool auxiliaryES, uint8_t* pduUserData, bool imm)
{
    assert(pduUserData != nullptr);

    m_p25->writeRF_TDU(true, imm);

    uint32_t bitLength = 0U;
    UInt8Array data = m_rfAssembler->assemble(dataHeader, extendedAddress, auxiliaryES, pduUserData, &bitLength);

    writeRF_PDU(data.get(), bitLength, imm);
}

/* Helper to write user data as a P25 PDU packet. */

void Data::writeNet_PDU_User(data::DataHeader& dataHeader, bool extendedAddress, bool auxiliaryES, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    uint32_t bitLength = 0U;
    m_netAssembler->assemble(dataHeader, extendedAddress, auxiliaryES, pduUserData, &bitLength);
}

/* Updates the processor by the passed number of milliseconds. */

void Data::clock(uint32_t ms)
{
    if (m_p25->m_sndcpSupport) {
        // clock all the SNDCP ready timers
        std::vector<uint32_t> sndcpReadyExpired = std::vector<uint32_t>();
        for (auto entry : m_sndcpReadyTimers) {
            uint32_t llId = entry.first;

            m_sndcpReadyTimers[llId].clock(ms);
            if (m_sndcpReadyTimers[llId].isRunning() && m_sndcpReadyTimers[llId].hasExpired()) {
                sndcpReadyExpired.push_back(llId);
            }
        }

        // clock all the SNDCP standby timers
        std::vector<uint32_t> sndcpStandbyExpired = std::vector<uint32_t>();
        for (auto entry : m_sndcpStandbyTimers) {
            uint32_t llId = entry.first;

            m_sndcpStandbyTimers[llId].clock(ms);
            if (m_sndcpStandbyTimers[llId].isRunning() && m_sndcpStandbyTimers[llId].hasExpired()) {
                sndcpStandbyExpired.push_back(llId);
            }
        }

        // process and SNDCP enabled LLIDs
        for (auto entry : m_sndcpStateTable) {
            uint32_t llId = entry.first;
            SNDCPState::E state = entry.second;

            switch (state) {
            case SNDCPState::CLOSED:
                break;
            case SNDCPState::IDLE:
                {
                    if (m_p25->m_permittedDstId == llId) {
                        m_sndcpReadyTimers[llId].start();
                        m_sndcpStateTable[llId] = SNDCPState::READY_S;
                        if (m_verbose) {
                            LogInfoEx(LOG_RF, P25_PDU_STR ", SNDCP, llId = %u, state = %u", llId, (uint8_t)state);
                        }
                    }
                }
                break;
            case SNDCPState::READY_S:
                {
                    // has the LLID reached ready state expiration?
                    if (std::find(sndcpReadyExpired.begin(), sndcpReadyExpired.end(), llId) != sndcpReadyExpired.end()) {
                        m_sndcpStateTable[llId] = SNDCPState::IDLE;

                        if (m_verbose) {
                            LogInfoEx(LOG_RF, P25_TDULC_STR ", CALL_TERM (Call Termination), llId = %u", llId);
                        }

                        std::unique_ptr<lc::TDULC> lc = std::make_unique<lc::tdulc::LC_CALL_TERM>();
                        lc->setDstId(llId);
                        m_p25->m_control->writeRF_TDULC(lc.get(), true);
                        for (uint8_t i = 0U; i < 8U; i++) {
                            m_p25->writeRF_TDU(true);
                        }

                        if (m_p25->m_notifyCC) {
                            m_p25->notifyCC_ReleaseGrant(llId);
                        }
                    }
                }
                break;
            case SNDCPState::STANDBY:
                {
                    // has the LLID reached standby state expiration?
                    if (std::find(sndcpStandbyExpired.begin(), sndcpStandbyExpired.end(), llId) != sndcpStandbyExpired.end()) {
                        sndcpReset(llId);
                    }
                }
                break;
            case SNDCPState::READY:
                break;
            default:
                break;
            }
        }
    }
}

/* Helper to initialize the SNDCP state for a logical link ID. */

void Data::sndcpInitialize(uint32_t llId)
{
    if (!isSNDCPInitialized(llId)) {
        m_sndcpStateTable[llId] = SNDCPState::IDLE;
        m_sndcpReadyTimers[llId] = Timer(1000U, SNDCP_READY_TIMEOUT);
        m_sndcpStandbyTimers[llId] = Timer(1000U, SNDCP_STANDBY_TIMEOUT);

        if (m_verbose) {
            LogInfoEx(LOG_RF, P25_PDU_STR ", SNDCP, first initialize, llId = %u, state = %u", llId, (uint8_t)SNDCPState::IDLE);
        }
    }
}

/* Helper to determine if the logical link ID has been SNDCP initialized. */

bool Data::isSNDCPInitialized(uint32_t llId) const
{
    // lookup dynamic affiliation table entry
    if (m_sndcpStateTable.find(llId) != m_sndcpStateTable.end()) {
        return true;
    }

    return false;
}

/* Helper to reset the SNDCP state for a logical link ID. */

void Data::sndcpReset(uint32_t llId, bool callTerm)
{
    if (isSNDCPInitialized(llId)) {
        if (m_verbose) {
            LogInfoEx(LOG_RF, P25_PDU_STR ", SNDCP, reset, llId = %u, state = %u", llId, (uint8_t)m_sndcpStateTable[llId]);
        }

        m_sndcpStateTable[llId] = SNDCPState::CLOSED;
        m_sndcpReadyTimers[llId].stop();
        m_sndcpStandbyTimers[llId].stop();

        if (callTerm) {
            if (m_verbose) {
                LogInfoEx(LOG_RF, P25_TDULC_STR ", CALL_TERM (Call Termination), llId = %u", llId);
            }

            std::unique_ptr<lc::TDULC> lc = std::make_unique<lc::tdulc::LC_CALL_TERM>();
            lc->setDstId(llId);
            m_p25->m_control->writeRF_TDULC(lc.get(), true);
            m_p25->writeRF_Preamble();

            if (m_p25->m_notifyCC) {
                m_p25->notifyCC_ReleaseGrant(llId);
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the Data class. */

Data::Data(Control* p25, bool dumpPDUData, bool repeatPDU, bool debug, bool verbose) :
    m_p25(p25),
    m_prevRfState(RS_RF_LISTENING),
    m_rfAssembler(nullptr),
    m_rfPDU(nullptr),
    m_rfPDUCount(0U),
    m_rfPDUBits(0U),
    m_netAssembler(nullptr),
    m_retryPDUData(nullptr),
    m_retryPDUBitLength(0U),
    m_retryCount(0U),
    m_rfPduUserData(nullptr),
    m_rfPduUserDataLength(0U),
    m_netPduUserData(nullptr),
    m_netPduUserDataLength(0U),
    m_fneRegTable(),
    m_sndcpStateTable(),
    m_sndcpReadyTimers(),
    m_sndcpStandbyTimers(),
    m_inbound(false),
    m_dumpPDUData(dumpPDUData),
    m_repeatPDU(repeatPDU),
    m_verbose(verbose),
    m_debug(debug)
{
    data::Assembler::setVerbose(verbose);
    data::Assembler::setDumpPDUData(dumpPDUData);

    m_rfAssembler = new Assembler();
    m_netAssembler = new Assembler();

    m_rfPDU = new uint8_t[P25_PDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(m_rfPDU, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

    m_netAssembler->setBlockWriter([=](const void* userContext, const uint8_t currentBlock, const uint8_t *data, uint32_t len, bool lastBlock) {
        writeNetwork(currentBlock, data, len, lastBlock);
    });

    m_rfPduUserData = new uint8_t[P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
    ::memset(m_rfPduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);
    m_netPduUserData = new uint8_t[P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
    ::memset(m_netPduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);

    m_fneRegTable.clear();

    m_sndcpStateTable.clear();
    m_sndcpReadyTimers.clear();
    m_sndcpStandbyTimers.clear();
}

/* Finalizes a instance of the Data class. */

Data::~Data()
{
    delete m_rfAssembler;
    delete m_netAssembler;

    delete[] m_rfPDU;

    if (m_retryPDUData != nullptr)
        delete m_retryPDUData;

    delete[] m_rfPduUserData;
    delete[] m_netPduUserData;
}

/* Helper used to process conventional data registration from PDU data. */

bool Data::processConvDataReg(const uint8_t* pduUserData)
{
    uint8_t regType = (pduUserData[0U] >> 4) & 0x0F;
    switch (regType) {
    case PDURegType::CONNECT:
    {
        uint32_t llId = (pduUserData[1U] << 16) + (pduUserData[2U] << 8) + pduUserData[3U];
        uint32_t ipAddr = (pduUserData[8U] << 24) + (pduUserData[9U] << 16) + (pduUserData[10U] << 8) + pduUserData[11U];

        if (m_verbose) {
            LogInfoEx(LOG_RF, P25_PDU_STR ", CONNECT (Registration Request Connect), llId = %u, ipAddr = %s", llId, __IP_FROM_UINT(ipAddr).c_str());
        }

        if (!acl::AccessControl::validateSrcId(llId)) {
            LogWarning(LOG_RF, P25_PDU_STR ", DENY (Registration Response Deny), llId = %u, ipAddr = %s", llId, __IP_FROM_UINT(ipAddr).c_str());
            writeRF_PDU_Reg_Response(PDURegType::DENY, llId, ipAddr);
        }
        else {
            if (!hasLLIdFNEReg(llId)) {
                // update dynamic FNE registration table entry
                m_fneRegTable[llId] = ipAddr;
            }

            if (m_verbose) {
                LogInfoEx(LOG_RF, P25_PDU_STR ", ACCEPT (Registration Response Accept), llId = %u, ipAddr = %s", llId, __IP_FROM_UINT(ipAddr).c_str());
            }

            writeRF_PDU_Reg_Response(PDURegType::ACCEPT, llId, ipAddr);
        }
    }
    break;
    case PDURegType::DISCONNECT:
    {
        uint32_t llId = (pduUserData[1U] << 16) + (pduUserData[2U] << 8) + pduUserData[3U];

        if (m_verbose) {
            LogInfoEx(LOG_RF, P25_PDU_STR ", DISCONNECT (Registration Request Disconnect), llId = %u", llId);
        }

        // acknowledge
        writeRF_PDU_Ack_Response(PDUAckClass::ACK, PDUAckType::ACK, m_rfAssembler->dataHeader.getNs(), llId, false);

        if (hasLLIdFNEReg(llId)) {
            // remove dynamic FNE registration table entry
            try {
                m_fneRegTable.at(llId);
                m_fneRegTable.erase(llId);
            }
            catch (...) {
                // stub
            }
        }
    }
    break;
    default:
        LogError(LOG_RF, "P25 unhandled PDU registration type, regType = $%02X", regType);
        break;
    }

    return true;
}

/* Helper used to process SNDCP control data from PDU data. */

bool Data::processSNDCPControl(const uint8_t* pduUserData)
{
    if (!m_p25->m_sndcpSupport) {
        return false;
    }

    uint8_t txPduUserData[P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES];
    ::memset(txPduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES);

    std::unique_ptr<sndcp::SNDCPPacket> packet = SNDCPFactory::create(pduUserData);
    if (packet == nullptr) {
        LogWarning(LOG_RF, P25_PDU_STR ", undecodable SNDCP packet");
        return false;
    }

    uint32_t llId = m_rfAssembler->dataHeader.getLLId();

    switch (packet->getPDUType()) {
        case SNDCP_PDUType::ACT_TDS_CTX:
        {
            SNDCPCtxActRequest* isp = static_cast<SNDCPCtxActRequest*>(packet.get());
            if (m_verbose) {
                LogInfoEx(LOG_RF, P25_PDU_STR ", SNDCP context activation request, llId = %u, nsapi = %u, ipAddr = %s, nat = $%02X, dsut = $%02X, mdpco = $%02X", llId,
                    isp->getNSAPI(), __IP_FROM_UINT(isp->getIPAddress()).c_str(), isp->getNAT(), isp->getDSUT(), isp->getMDPCO());
            }

            m_p25->writeRF_Preamble();

            DataHeader rspHeader = DataHeader();
            rspHeader.setFormat(PDUFormatType::CONFIRMED);
            rspHeader.setMFId(MFG_STANDARD);
            rspHeader.setAckNeeded(true);
            rspHeader.setOutbound(true);
            rspHeader.setSAP(PDUSAP::SNDCP_CTRL_DATA);
            rspHeader.setNs(m_rfAssembler->dataHeader.getNs());
            rspHeader.setLLId(llId);
            rspHeader.setBlocksToFollow(1U);

            if (!isSNDCPInitialized(llId)) {
                std::unique_ptr<SNDCPCtxActReject> osp = std::make_unique<SNDCPCtxActReject>();
                osp->setNSAPI(DEFAULT_NSAPI);
                osp->setRejectCode(SNDCPRejectReason::SU_NOT_PROVISIONED);

                osp->encode(txPduUserData);

                rspHeader.calculateLength(2U);
                writeRF_PDU_User(rspHeader, false, false, txPduUserData);
                return true;
            }

            // which network address type is this?
            switch (isp->getNAT()) {
                case SNDCPNAT::IPV4_STATIC_ADDR:
                {
                    std::unique_ptr<SNDCPCtxActReject> osp = std::make_unique<SNDCPCtxActReject>();
                    osp->setNSAPI(DEFAULT_NSAPI);
                    osp->setRejectCode(SNDCPRejectReason::STATIC_IP_ALLOCATION_UNSUPPORTED);

                    osp->encode(txPduUserData);

                    rspHeader.calculateLength(2U);
                    writeRF_PDU_User(rspHeader, false, false, txPduUserData);

                    sndcpReset(llId, true);
                }
                break;

                case SNDCPNAT::IPV4_DYN_ADDR:
                {
                    std::unique_ptr<SNDCPCtxActReject> osp = std::make_unique<SNDCPCtxActReject>();
                    osp->setNSAPI(DEFAULT_NSAPI);
                    osp->setRejectCode(SNDCPRejectReason::DYN_IP_ALLOCATION_UNSUPPORTED);

                    osp->encode(txPduUserData);

                    rspHeader.calculateLength(2U);
                    writeRF_PDU_User(rspHeader, false, false, txPduUserData);

                    sndcpReset(llId, true);

                    // TODO TODO TODO
/*
                    std::unique_ptr<SNDCPCtxActAccept> osp = std::make_unique<SNDCPCtxActAccept>();
                    osp->setNSAPI(DEFAULT_NSAPI);
                    osp->setReadyTimer(SNDCPReadyTimer::TEN_SECONDS);
                    osp->setStandbyTimer(SNDCPStandbyTimer::ONE_MINUTE);
                    osp->setNAT(SNDCPNAT::IPV4_DYN_ADDR);
                    osp->setIPAddress(__IP_FROM_STR(std::string("10.10.1.10")));
                    osp->setMTU(SNDCP_MTU_510);
                    osp->setMDPCO(isp->getMDPCO());

                    osp->encode(txPduUserData);

                    rspHeader.calculateLength(13U);
                    writeRF_PDU_User(rspHeader, rspHeader, false, txPduUserData);

                    m_sndcpStateTable[llId] = SNDCPState::STANDBY;
                    m_sndcpReadyTimers[llId].stop();
                    m_sndcpStandbyTimers[llId].start();
*/
                }
                break;

                default:
                {
                    std::unique_ptr<SNDCPCtxActReject> osp = std::make_unique<SNDCPCtxActReject>();
                    osp->setNSAPI(DEFAULT_NSAPI);
                    osp->setRejectCode(SNDCPRejectReason::ANY_REASON);

                    osp->encode(txPduUserData);

                    rspHeader.calculateLength(2U);
                    writeRF_PDU_User(rspHeader, false, false, txPduUserData);

                    sndcpReset(llId, true);
                }
                break;
            }
        }
        break;

        case SNDCP_PDUType::DEACT_TDS_CTX_REQ:
        {
            SNDCPCtxDeactivation* isp = static_cast<SNDCPCtxDeactivation*>(packet.get());
            if (m_verbose) {
                LogInfoEx(LOG_RF, P25_PDU_STR ", SNDCP context deactivation request, llId = %u, deactType = %02X", llId,
                    isp->getDeactType());
            }

            writeRF_PDU_Ack_Response(PDUAckClass::ACK, PDUAckType::ACK, m_rfAssembler->dataHeader.getNs(), llId, false);
            sndcpReset(llId, true);
        }
        break;

        default:
        {
            LogError(LOG_RF, P25_PDU_STR ", unhandled SNDCP PDU Type, pduType = $%02X", packet->getPDUType());
            sndcpReset(llId, true);
        }
        break;
    } // switch (packet->getPDUType())

    return true;
}

/* Write data processed from RF to the network. */

void Data::writeNetwork(const uint8_t currentBlock, const uint8_t* data, uint32_t len, bool lastBlock)
{
    assert(data != nullptr);

    if (m_p25->m_network == nullptr)
        return;

    if (m_p25->m_rfTimeout.isRunning() && m_p25->m_rfTimeout.hasExpired())
        return;

    m_p25->m_network->writeP25PDU(m_rfAssembler->dataHeader, currentBlock, data, len, lastBlock);
}

/* Helper to write a P25 PDU packet. */

void Data::writeRF_PDU(const uint8_t* pdu, uint32_t bitLength, bool imm, bool ackRetry)
{
    assert(pdu != nullptr);
    assert(bitLength > 0U);

    m_p25->writeRF_TDU(true, imm);

    for (uint8_t i = 0U; i < 5U; i++)
        m_p25->writeRF_Nulls();

    if (!ackRetry) {
        if (m_retryPDUData != nullptr)
            delete m_retryPDUData;

        // store PDU for ACK RETRY logic
        m_retryCount = 0U;
        m_retryPDUBitLength = bitLength;
        uint32_t retryByteLength = bitLength / 8U;
        if ((retryByteLength % 8U) > 0U)
            retryByteLength++;

        m_retryPDUData = new uint8_t[retryByteLength];
        ::memcpy(m_retryPDUData, pdu, retryByteLength);
    } else {
        LogInfoEx(LOG_RF, P25_PDU_STR ", OSP, ack retry, bitLength = %u",
                   m_retryPDUBitLength);
    }

    uint8_t data[P25_PDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(data, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

    // add the data
    uint32_t newBitLength = P25Utils::encodeByLength(pdu, data + 2U, bitLength);
    uint32_t newByteLength = newBitLength / 8U;
    if ((newBitLength % 8U) > 0U)
        newByteLength++;

    // generate Sync
    Sync::addP25Sync(data + 2U);

    // generate NID
    m_p25->m_nid.encode(data + 2U, DUID::PDU);

    // add status bits
    P25Utils::addStatusBits(data + 2U, newBitLength, m_inbound, true);
    P25Utils::setStatusBitsStartIdle(data + 2U);

    //Utils::dump("P25, Data::writeRF_PDU(), Raw PDU OSP", data, newByteLength + 2U);

    if (m_p25->m_duplex) {
        data[0U] = modem::TAG_DATA;
        data[1U] = 0x00U;

        m_p25->addFrame(data, newByteLength + 2U, false, imm);
    }

    m_p25->writeRF_TDU(true, imm);
}

/* Helper to write a network P25 PDU packet. */

void Data::writeNet_PDU_Buffered()
{
    uint32_t bitLength = 0U;
    UInt8Array data = m_rfAssembler->assemble(m_netAssembler->dataHeader, m_netAssembler->getExtendedAddress(), m_netAssembler->getAuxiliaryES(), m_netPduUserData, &bitLength);
    writeRF_PDU(data.get(), bitLength);
}

/* Helper to re-write a received P25 PDU packet. */

void Data::writeRF_PDU_Buffered()
{
    uint32_t bitLength = 0U;
    UInt8Array data = m_rfAssembler->assemble(m_rfAssembler->dataHeader, m_rfAssembler->getExtendedAddress(), m_rfAssembler->getAuxiliaryES(), m_rfPduUserData, &bitLength);
    writeRF_PDU(data.get(), bitLength);
}

/* Helper to write a PDU registration response. */

void Data::writeRF_PDU_Reg_Response(uint8_t regType, uint32_t llId, uint32_t ipAddr)
{
    if ((regType != PDURegType::ACCEPT) && (regType != PDURegType::DENY))
        return;

    uint8_t pduUserData[P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES];
    ::memset(pduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES);

    DataHeader rspHeader = DataHeader();
    rspHeader.setFormat(PDUFormatType::CONFIRMED);
    rspHeader.setMFId(m_rfAssembler->dataHeader.getMFId());
    rspHeader.setAckNeeded(true);
    rspHeader.setOutbound(true);
    rspHeader.setSAP(PDUSAP::CONV_DATA_REG);
    rspHeader.setSynchronize(true);
    rspHeader.setLLId(llId);
    rspHeader.setBlocksToFollow(1U);

    pduUserData[0U] = ((regType & 0x0FU) << 4);                                 // Registration Type & Options
    pduUserData[1U] = (llId >> 16) & 0xFFU;                                     // Logical Link ID
    pduUserData[2U] = (llId >> 8) & 0xFFU;
    pduUserData[3U] = (llId >> 0) & 0xFFU;
    if (regType == PDURegType::ACCEPT) {
        pduUserData[8U] = (ipAddr >> 24) & 0xFFU;                               // IP Address
        pduUserData[9U] = (ipAddr >> 16) & 0xFFU;
        pduUserData[10U] = (ipAddr >> 8) & 0xFFU;
        pduUserData[11U] = (ipAddr >> 0) & 0xFFU;
    }

    if (m_dumpPDUData)
        Utils::dump(1U, "P25, PDU Registration Response", pduUserData, 12U);

    rspHeader.calculateLength(12U);
    writeRF_PDU_User(rspHeader, false, false, pduUserData);
}

/* Helper to write a PDU acknowledge response. */

void Data::writeRF_PDU_Ack_Response(uint8_t ackClass, uint8_t ackType, uint8_t ackStatus, uint32_t llId, bool extendedAddress, uint32_t srcLlId)
{
    if (ackClass == PDUAckClass::ACK && ackType != PDUAckType::ACK)
        return;

    uint32_t bitLength = (1U * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
    uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

    DECLARE_UINT8_ARRAY(data, (bitLength / 8U) + 1U);

    uint8_t block[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    DataHeader rspHeader = DataHeader();
    rspHeader.setFormat(PDUFormatType::RSP);
    rspHeader.setMFId(m_rfAssembler->dataHeader.getMFId());
    rspHeader.setOutbound(true);
    rspHeader.setResponseClass(ackClass);
    rspHeader.setResponseType(ackType);
    rspHeader.setResponseStatus(ackStatus);
    rspHeader.setLLId(llId);
    if (srcLlId > 0U) {
        rspHeader.setSrcLLId(srcLlId);
    }

    if (extendedAddress)
        rspHeader.setFullMessage(false);
    else
        rspHeader.setFullMessage(true);

    rspHeader.setBlocksToFollow(0U);

    // generate the PDU header and 1/2 rate Trellis
    rspHeader.encode(block);
    Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

    if (m_verbose) {
        LogInfoEx(LOG_RF, P25_PDU_STR ", OSP, response, rspClass = $%02X, rspType = $%02X, rspStatus = $%02X, llId = %u, srcLLId = %u",
            rspHeader.getResponseClass(), rspHeader.getResponseType(), rspHeader.getResponseStatus(), rspHeader.getLLId(), rspHeader.getSrcLLId());
    }

    writeRF_PDU(data, bitLength);
}
