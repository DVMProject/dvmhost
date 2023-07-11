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
#include "p25/packet/Data.h"
#include "p25/packet/Trunk.h"
#include "p25/acl/AccessControl.h"
#include "p25/P25Utils.h"
#include "p25/Sync.h"
#include "edac/CRC.h"
#include "HostMain.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::data;
using namespace p25::packet;

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t CONN_WAIT_TIMEOUT = 1U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Resets the data states for the RF interface.
/// </summary>
void Data::resetRF()
{
    m_rfDataBlockCnt = 0U;
    m_rfPDUCount = 0U;
    m_rfPDUBits = 0U;

    m_rfDataHeader.reset();
}

/// <summary>
/// Process a data frame from the RF interface.
/// </summary>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
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
    if (duid == P25_DUID_PDU) {
        if (m_p25->m_rfState != RS_RF_DATA) {
            m_rfDataHeader.reset();
            m_rfDataBlockCnt = 0U;
            m_rfPDUCount = 0U;
            m_rfPDUBits = 0U;

            ::memset(m_rfPDU, 0x00U, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);

            m_p25->m_rfState = RS_RF_DATA;

            ::memset(m_pduUserData, 0x00U, P25_MAX_PDU_COUNT * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);
            m_pduUserDataLength = 0U;
        }

        uint32_t start = m_rfPDUCount * P25_LDU_FRAME_LENGTH_BITS;

        uint8_t buffer[P25_MAX_PDU_LENGTH];
        ::memset(buffer, 0x00U, P25_MAX_PDU_LENGTH);

        uint32_t bits = P25Utils::decode(data + 2U, buffer, start, start + P25_LDU_FRAME_LENGTH_BITS);
        m_rfPDUBits = Utils::getBits(buffer, m_rfPDU, 0U, bits);

        // Utils::dump(2U, "* !!! P25_DUID_PDU - m_rfPDU", m_rfPDU, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);

        uint32_t offset = P25_PREAMBLE_LENGTH_BITS + P25_PDU_FEC_LENGTH_BITS;
        if (m_rfPDUCount == 0U) {
            ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
            Utils::getBitRange(m_rfPDU, buffer, P25_PREAMBLE_LENGTH_BITS, P25_PDU_FEC_LENGTH_BITS);
            bool ret = m_rfDataHeader.decode(buffer);
            if (!ret) {
                LogWarning(LOG_RF, P25_PDU_STR ", unfixable RF 1/2 rate header data");
                Utils::dump(1U, "Unfixable PDU Data", buffer, P25_PDU_FEC_LENGTH_BYTES);

                m_rfDataHeader.reset();
                m_rfSecondHeader.reset();
                m_rfDataBlockCnt = 0U;
                m_rfPDUCount = 0U;
                m_rfPDUBits = 0U;
                m_p25->m_rfState = m_prevRfState;
                return false;
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_PDU_STR ", ack = %u, outbound = %u, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padCount = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
                    m_rfDataHeader.getAckNeeded(), m_rfDataHeader.getOutbound(), m_rfDataHeader.getFormat(), m_rfDataHeader.getMFId(), m_rfDataHeader.getSAP(), m_rfDataHeader.getFullMessage(),
                    m_rfDataHeader.getBlocksToFollow(), m_rfDataHeader.getPadCount(), m_rfDataHeader.getNs(), m_rfDataHeader.getFSN(), m_rfDataHeader.getLastFragment(),
                    m_rfDataHeader.getHeaderOffset(), m_rfDataHeader.getLLId());
            }

            // make sure we don't get a PDU with more blocks then we support
            if (m_rfDataHeader.getBlocksToFollow() >= P25_MAX_PDU_COUNT) {
                LogError(LOG_RF, P25_PDU_STR ", too many PDU blocks to process, %u > %u", m_rfDataHeader.getBlocksToFollow(), P25_MAX_PDU_COUNT);

                m_rfDataHeader.reset();
                m_rfSecondHeader.reset();
                m_rfDataBlockCnt = 0U;
                m_rfPDUCount = 0U;
                m_rfPDUBits = 0U;
                m_p25->m_rfState = m_prevRfState;
                return false;
            }

            // if we're a dedicated CC or in control only mode, we only want to handle AMBTs. Otherwise return
            if ((m_p25->m_dedicatedControl || m_p25->m_controlOnly) && m_rfDataHeader.getMFId() != PDU_FMT_AMBT) {
                if (m_debug) {
                    LogDebug(LOG_RF, "CC only mode, ignoring non-AMBT PDU from RF");
                }
                
                m_p25->m_ccHalted = false;
                
                m_rfDataHeader.reset();
                m_rfSecondHeader.reset();
                m_rfDataBlockCnt = 0U;
                m_rfPDUCount = 0U;
                m_rfPDUBits = 0U;
                m_p25->m_rfState = m_prevRfState;
                return false;
            }

            // only send data blocks across the network, if we're not an AMBT,
            // an RSP or a registration service
            if ((m_rfDataHeader.getFormat() != PDU_FMT_AMBT) &&
                (m_rfDataHeader.getFormat() != PDU_FMT_RSP) &&
                (m_rfDataHeader.getSAP() != PDU_SAP_REG)) {
                writeNetwork(0, buffer, P25_PDU_FEC_LENGTH_BYTES);
            }
        }

        if (m_p25->m_rfState == RS_RF_DATA) {
            uint32_t blocksToFollow = m_rfDataHeader.getBlocksToFollow();
            // process second header if we're using enhanced addressing
            if (m_rfDataHeader.getSAP() == PDU_SAP_EXT_ADDR &&
                m_rfDataHeader.getFormat() == PDU_FMT_UNCONFIRMED) {
                ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
                Utils::getBitRange(m_rfPDU, buffer, offset, P25_PDU_FEC_LENGTH_BITS);
                bool ret = m_rfSecondHeader.decode(buffer);
                if (!ret) {
                    LogWarning(LOG_RF, P25_PDU_STR ", unfixable RF 1/2 rate second header data");
                    Utils::dump(1U, "Unfixable PDU Data", m_rfPDU + offset, P25_PDU_HEADER_LENGTH_BYTES);

                    m_rfDataHeader.reset();
                    m_rfSecondHeader.reset();
                    m_rfUseSecondHeader = false;
                    m_rfDataBlockCnt = 0U;
                    m_rfPDUCount = 0U;
                    m_rfPDUBits = 0U;
                    m_p25->m_rfState = m_prevRfState;
                    return false;
                }

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_PDU_STR ", fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padCount = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
                        m_rfSecondHeader.getFormat(), m_rfSecondHeader.getMFId(), m_rfSecondHeader.getSAP(), m_rfSecondHeader.getFullMessage(),
                        m_rfSecondHeader.getBlocksToFollow(), m_rfSecondHeader.getPadCount(), m_rfSecondHeader.getNs(), m_rfSecondHeader.getFSN(), m_rfSecondHeader.getLastFragment(),
                        m_rfSecondHeader.getHeaderOffset(), m_rfSecondHeader.getLLId());
                }

                m_rfUseSecondHeader = true;

                // only send data blocks across the network, if we're not an AMBT,
                // an RSP or a registration service
                if ((m_rfDataHeader.getFormat() != PDU_FMT_AMBT) &&
                    (m_rfDataHeader.getFormat() != PDU_FMT_RSP) &&
                    (m_rfDataHeader.getSAP() != PDU_SAP_REG)) {
                    writeNetwork(1, buffer, P25_PDU_FEC_LENGTH_BYTES);
                }

                offset += P25_PDU_FEC_LENGTH_BITS;
                m_rfPDUCount++;
                blocksToFollow--;
            }

            uint32_t srcId = m_rfDataHeader.getLLId();
            uint32_t dstId = (m_rfUseSecondHeader || m_rfExtendedAddress) ? m_rfSecondHeader.getLLId() : m_rfDataHeader.getLLId();

            m_rfPDUCount++;
            uint32_t bitLength = ((blocksToFollow + 1U) * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;

            if (m_rfPDUBits >= bitLength) {
                // process all blocks in the data stream
                uint32_t dataOffset = 0U;

                // if we are using a secondary header place it in the PDU user data buffer
                if (m_rfUseSecondHeader) {
                    m_rfSecondHeader.getData(m_pduUserData + dataOffset);
                    dataOffset += P25_PDU_HEADER_LENGTH_BYTES;
                    m_pduUserDataLength += P25_PDU_HEADER_LENGTH_BYTES;
                }

                // decode data blocks
                for (uint32_t i = 0U; i < blocksToFollow; i++) {
                    ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
                    Utils::getBitRange(m_rfPDU, buffer, offset, P25_PDU_FEC_LENGTH_BITS);
                    bool ret = m_rfData[i].decode(buffer, (m_rfUseSecondHeader) ? m_rfSecondHeader : m_rfDataHeader);
                    if (ret) {
                        // if we are getting unconfirmed or confirmed blocks, and if we've reached the total number of blocks
                        // set this block as the last block for full packet CRC
                        if ((m_rfDataHeader.getFormat() == PDU_FMT_CONFIRMED) || (m_rfDataHeader.getFormat() == PDU_FMT_UNCONFIRMED)) {
                            if ((m_rfDataBlockCnt + 1U) == blocksToFollow) {
                                m_rfData[i].setLastBlock(true);
                            }
                        }

                        // are we processing extended address data from the first block?
                        if (m_rfDataHeader.getSAP() == PDU_SAP_EXT_ADDR && m_rfDataHeader.getFormat() == PDU_FMT_CONFIRMED &&
                            m_rfData[i].getSerialNo() == 0U) {
                            LogMessage(LOG_RF, P25_PDU_STR ", block %u, fmt = $%02X, lastBlock = %u, sap = $%02X, llId = %u",
                                m_rfData[i].getSerialNo(), m_rfData[i].getFormat(), m_rfData[i].getLastBlock(), m_rfData[i].getSAP(), m_rfData[i].getLLId());
                            m_rfSecondHeader.reset();
                            m_rfSecondHeader.setAckNeeded(true);
                            m_rfSecondHeader.setFormat(m_rfData[i].getFormat());
                            m_rfSecondHeader.setLLId(m_rfData[i].getLLId());
                            m_rfSecondHeader.setSAP(m_rfData[i].getSAP());
                            dstId = m_rfSecondHeader.getLLId();
                            m_rfExtendedAddress = true;
                        }
                        else {
                            LogMessage(LOG_RF, P25_PDU_STR ", block %u, fmt = $%02X, lastBlock = %u",
                                (m_rfDataHeader.getFormat() == PDU_FMT_CONFIRMED) ? m_rfData[i].getSerialNo() : m_rfDataBlockCnt, m_rfData[i].getFormat(),
                                m_rfData[i].getLastBlock());
                        }

                        m_rfData[i].getData(m_pduUserData + dataOffset);
                        dataOffset += (m_rfDataHeader.getFormat() == PDU_FMT_CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
                        m_pduUserDataLength = dataOffset;

                        // only send data blocks across the network, if we're not an AMBT,
                        // an RSP or a registration service
                        if ((m_rfDataHeader.getFormat() != PDU_FMT_AMBT) &&
                            (m_rfDataHeader.getFormat() != PDU_FMT_RSP) &&
                            (m_rfDataHeader.getSAP() != PDU_SAP_REG)) {
                            writeNetwork(m_rfDataBlockCnt, buffer, P25_PDU_FEC_LENGTH_BYTES);
                        }

                        m_rfDataBlockCnt++;

                        // is this the last block?
                        if (m_rfData[i].getLastBlock() && m_rfDataBlockCnt == blocksToFollow) {
                            bool crcRet = edac::CRC::checkCRC32(m_pduUserData, m_pduUserDataLength);
                            if (!crcRet) {
                                LogWarning(LOG_RF, P25_PDU_STR ", failed CRC-32 check, blocks %u, len %u", blocksToFollow, m_pduUserDataLength);

                                // does the packet require ack?
                                if (m_rfDataHeader.getAckNeeded()) {
                                    if (m_rfExtendedAddress) {
                                        writeRF_PDU_Ack_Response(PDU_ACK_CLASS_NACK, PDU_ACK_TYPE_NACK_PACKET_CRC, dstId, srcId);
                                    }
                                    else {
                                        writeRF_PDU_Ack_Response(PDU_ACK_CLASS_NACK, PDU_ACK_TYPE_NACK_PACKET_CRC, srcId);
                                    }
                                }
                            }
                            else {
                                // does the packet require ack?
                                if (m_rfDataHeader.getAckNeeded()) {
                                    if (m_rfExtendedAddress) {
                                        writeRF_PDU_Ack_Response(PDU_ACK_CLASS_ACK, PDU_ACK_TYPE_ACK, dstId, srcId);
                                    }
                                    else {
                                        writeRF_PDU_Ack_Response(PDU_ACK_CLASS_ACK, PDU_ACK_TYPE_ACK, srcId);
                                    }
                                }
                            }
                        }
                    }
                    else {
                        if (m_rfData[i].getFormat() == PDU_FMT_CONFIRMED)
                            LogWarning(LOG_RF, P25_PDU_STR ", unfixable PDU data (3/4 rate or CRC), block %u", i);
                        else
                            LogWarning(LOG_RF, P25_PDU_STR ", unfixable PDU data (1/2 rate or CRC), block %u", i);

                        if (m_dumpPDUData) {
                            Utils::dump(1U, "Unfixable PDU Data", buffer, P25_PDU_FEC_LENGTH_BYTES);
                        }
                    }

                    offset += P25_PDU_FEC_LENGTH_BITS;
                }

                if (m_dumpPDUData && m_rfDataBlockCnt > 0U) {
                    Utils::dump(1U, "PDU Packet", m_pduUserData, m_pduUserDataLength);
                }

                if (m_rfDataBlockCnt < blocksToFollow) {
                    LogWarning(LOG_RF, P25_PDU_STR ", incomplete PDU (%d / %d blocks)", m_rfDataBlockCnt, blocksToFollow);
                }

                // did we receive a response header?
                if (m_rfDataHeader.getFormat() == PDU_FMT_RSP) {
                    if (m_verbose) {
                        LogMessage(LOG_RF, P25_PDU_STR ", response, fmt = $%02X, rspClass = $%02X, rspType = $%02X, rspStatus = $%02X",
                                m_rfDataHeader.getFormat(), m_rfDataHeader.getResponseClass(), m_rfDataHeader.getResponseType(), m_rfDataHeader.getResponseStatus());
                    }
                }
                else {
                    // handle standard P25 service access points
                    switch (m_rfDataHeader.getSAP()) {
                    case PDU_SAP_REG:
                    {
                        uint8_t regType = (m_pduUserData[0] >> 4) & 0x0F;
                        switch (regType) {
                        case PDU_REG_TYPE_REQ_CNCT:
                        {
                            uint32_t llId = (m_pduUserData[1U] << 16) + (m_pduUserData[2U] << 8) + m_pduUserData[3U];
                            ulong64_t ipAddr = (m_pduUserData[8U] << 24) + (m_pduUserData[9U] << 16) +
                                (m_pduUserData[10U] << 8) + m_pduUserData[11U];

                            if (m_verbose) {
                                LogMessage(LOG_RF, P25_PDU_STR ", PDU_REG_TYPE_REQ_CNCT (Registration Request Connect), llId = %u, ipAddr = %s", llId, __IP_FROM_ULONG(ipAddr).c_str());
                            }

                            m_connQueueTable[llId] = std::make_tuple(m_rfDataHeader.getMFId(), ipAddr);

                            m_connTimerTable[llId] = Timer(1000U, CONN_WAIT_TIMEOUT);
                            m_connTimerTable[llId].start();
                        }
                        break;
                        case PDU_REG_TYPE_REQ_DISCNCT:
                        {
                            uint32_t llId = (m_pduUserData[1U] << 16) + (m_pduUserData[2U] << 8) + m_pduUserData[3U];

                            if (m_verbose) {
                                LogMessage(LOG_RF, P25_PDU_STR ", PDU_REG_TYPE_REQ_DISCNCT (Registration Request Disconnect), llId = %u", llId);
                            }

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
                    }
                    break;
                    case PDU_SAP_TRUNK_CTRL:
                    {
                        if (m_verbose) {
                            LogMessage(LOG_RF, P25_PDU_STR ", PDU_SAP_TRUNK_CTRL (Alternate MBT Packet), lco = $%02X, blocksToFollow = %u",
                                m_rfDataHeader.getAMBTOpcode(), m_rfDataHeader.getBlocksToFollow());
                        }

                        m_p25->m_trunk->processMBT(m_rfDataHeader, m_rfData);
                    }
                    break;
                    default:
                        ::ActivityLog("P25", true, "RF data transmission from %u to %u, %u blocks", srcId, dstId, m_rfDataHeader.getBlocksToFollow());

                        if (m_repeatPDU) {
                            if (m_verbose) {
                                LogMessage(LOG_RF, P25_PDU_STR ", repeating PDU, llId = %u", (m_rfUseSecondHeader || m_rfExtendedAddress) ? m_rfSecondHeader.getLLId() : m_rfDataHeader.getLLId());
                            }

                            writeRF_PDU_Buffered(); // re-generate buffered PDU and send it on
                        }

                        ::ActivityLog("P25", true, "end of RF data transmission");
                        break;
                    }
                }

                m_rfDataHeader.reset();
                m_rfSecondHeader.reset();
                m_rfUseSecondHeader = false;
                m_rfDataBlockCnt = 0U;
                m_rfPDUCount = 0U;
                m_rfPDUBits = 0U;
                m_pduUserDataLength = 0U;

                m_p25->m_rfState = m_prevRfState;
            } // switch (m_rfDataHeader.getSAP())
        }

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
/// <param name="blockLength"></param>
/// <returns></returns>
bool Data::processNetwork(uint8_t* data, uint32_t len, uint32_t blockLength)
{
    if (m_p25->m_rfState != RS_RF_LISTENING && m_p25->m_netState == RS_NET_IDLE)
        return false;

    if (m_p25->m_netState != RS_NET_DATA) {
        m_netDataHeader.reset();
        m_netSecondHeader.reset();
        m_netDataOffset = 0U;
        m_netDataBlockCnt = 0U;
        m_netPDUCount = 0U;

        m_p25->m_netState = RS_NET_DATA;

        uint8_t buffer[P25_PDU_FEC_LENGTH_BYTES];
        ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
        ::memcpy(buffer, data + 24U, P25_PDU_FEC_LENGTH_BYTES);

        bool ret = m_netDataHeader.decode(buffer);
        if (!ret) {
            LogWarning(LOG_NET, P25_PDU_STR ", unfixable RF 1/2 rate header data");
            Utils::dump(1U, "Unfixable PDU Data", buffer, P25_PDU_FEC_LENGTH_BYTES);

            m_netDataHeader.reset();
            m_netSecondHeader.reset();
            m_netDataBlockCnt = 0U;
            m_netPDUCount = 0U;
            m_p25->m_netState = RS_NET_IDLE;
            return false;
        }

        if (m_verbose) {
            LogMessage(LOG_NET, P25_PDU_STR ", ack = %u, outbound = %u, fmt = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padCount = %u, n = %u, seqNo = %u, hdrOffset = %u, llId = %u",
                m_netDataHeader.getAckNeeded(), m_netDataHeader.getOutbound(), m_netDataHeader.getFormat(), m_netDataHeader.getSAP(), m_netDataHeader.getFullMessage(),
                m_netDataHeader.getBlocksToFollow(), m_netDataHeader.getPadCount(), m_netDataHeader.getNs(), m_netDataHeader.getFSN(),
                m_netDataHeader.getHeaderOffset(), m_netDataHeader.getLLId());
        }

        // make sure we don't get a PDU with more blocks then we support
        if (m_netDataHeader.getBlocksToFollow() >= P25_MAX_PDU_COUNT) {
            LogError(LOG_NET, P25_PDU_STR ", too many PDU blocks to process, %u > %u", m_netDataHeader.getBlocksToFollow(), P25_MAX_PDU_COUNT);

            m_netDataHeader.reset();
            m_netSecondHeader.reset();
            m_netDataOffset = 0U;
            m_netDataBlockCnt = 0U;
            m_netPDUCount = 0U;
            m_p25->m_netState = RS_NET_IDLE;
            return false;
        }

        // if we're a dedicated CC or in control only mode, we only want to handle AMBTs. Otherwise return
        if ((m_p25->m_dedicatedControl || m_p25->m_controlOnly) && m_rfDataHeader.getMFId() != PDU_FMT_AMBT) {
            if (m_debug) {
                LogDebug(LOG_NET, "CC only mode, ignoring non-AMBT PDU from network");
            }

            m_netDataHeader.reset();
            m_netSecondHeader.reset();
            m_netDataOffset = 0U;
            m_netDataBlockCnt = 0U;
            m_netPDUCount = 0U;
            m_p25->m_netState = RS_NET_IDLE;
            return false;
        }

        m_netPDUCount++;
        return true;
    }

    if (m_p25->m_netState == RS_NET_DATA) {
        // Utils::dump(1U, "Incoming Network PDU Frame", data + 24U, blockLength);

        ::memcpy(m_netPDU + m_netDataOffset, data + 24U, blockLength);
        m_netDataOffset += blockLength;
        m_netPDUCount++;
        m_netDataBlockCnt++;

        if (m_netDataBlockCnt >= m_netDataHeader.getBlocksToFollow()) {
            uint32_t blocksToFollow = m_netDataHeader.getBlocksToFollow();
            uint32_t offset = 0U;

            uint8_t buffer[P25_PDU_FEC_LENGTH_BYTES];

            // process second header if we're using enhanced addressing
            if (m_netDataHeader.getSAP() == PDU_SAP_EXT_ADDR &&
                m_netDataHeader.getFormat() == PDU_FMT_UNCONFIRMED) {
                ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
                ::memcpy(buffer, m_netPDU, P25_PDU_FEC_LENGTH_BYTES);

                bool ret = m_netSecondHeader.decode(buffer);
                if (!ret) {
                    LogWarning(LOG_NET, P25_PDU_STR ", unfixable RF 1/2 rate second header data");
                    Utils::dump(1U, "Unfixable PDU Data", buffer, P25_PDU_HEADER_LENGTH_BYTES);

                    m_netDataHeader.reset();
                    m_netSecondHeader.reset();
                    m_netUseSecondHeader = false;
                    m_netDataBlockCnt = 0U;
                    m_netPDUCount = 0U;
                    m_p25->m_netState = RS_NET_IDLE;
                    return false;
                }

                if (m_verbose) {
                    LogMessage(LOG_NET, P25_PDU_STR ", fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padCount = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
                        m_netSecondHeader.getFormat(), m_netSecondHeader.getMFId(), m_netSecondHeader.getSAP(), m_netSecondHeader.getFullMessage(),
                        m_netSecondHeader.getBlocksToFollow(), m_netSecondHeader.getPadCount(), m_netSecondHeader.getNs(), m_netSecondHeader.getFSN(), m_netSecondHeader.getLastFragment(),
                        m_netSecondHeader.getHeaderOffset(), m_netSecondHeader.getLLId());
                }

                m_netUseSecondHeader = true;

                offset += P25_PDU_FEC_LENGTH_BYTES;
                blocksToFollow--;
            }

            m_netDataBlockCnt = 0U;

            // process all blocks in the data stream
            uint32_t dataOffset = 0U;

            // if we are using a secondary header place it in the PDU user data buffer
            if (m_netUseSecondHeader) {
                m_netSecondHeader.getData(m_pduUserData + dataOffset);
                dataOffset += P25_PDU_HEADER_LENGTH_BYTES;
                m_pduUserDataLength += P25_PDU_HEADER_LENGTH_BYTES;
            }

            // decode data blocks
            for (uint32_t i = 0U; i < blocksToFollow; i++) {
                ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
                ::memcpy(buffer, m_netPDU + offset, P25_PDU_FEC_LENGTH_BYTES);

                bool ret = m_netData[i].decode(buffer, (m_netUseSecondHeader) ? m_netSecondHeader : m_netDataHeader);
                if (ret) {
                    // if we are getting unconfirmed or confirmed blocks, and if we've reached the total number of blocks
                    // set this block as the last block for full packet CRC
                    if ((m_netDataHeader.getFormat() == PDU_FMT_CONFIRMED) || (m_netDataHeader.getFormat() == PDU_FMT_UNCONFIRMED)) {
                        if ((m_netDataBlockCnt + 1U) == blocksToFollow) {
                            m_netData[i].setLastBlock(true);
                        }
                    }

                    // are we processing extended address data from the first block?
                    if (m_netDataHeader.getSAP() == PDU_SAP_EXT_ADDR && m_netDataHeader.getFormat() == PDU_FMT_CONFIRMED &&
                        m_netData[i].getSerialNo() == 0U) {
                        LogMessage(LOG_NET, P25_PDU_STR ", block %u, fmt = $%02X, lastBlock = %u, sap = $%02X, llId = %u",
                            m_netData[i].getSerialNo(), m_netData[i].getFormat(), m_netData[i].getLastBlock(), m_netData[i].getSAP(), m_netData[i].getLLId());
                        m_netSecondHeader.reset();
                        m_netSecondHeader.setAckNeeded(true);
                        m_netSecondHeader.setFormat(m_netData[i].getFormat());
                        m_netSecondHeader.setLLId(m_netData[i].getLLId());
                        m_netSecondHeader.setSAP(m_netData[i].getSAP());
                        m_netExtendedAddress = true;
                    }
                    else {
                        LogMessage(LOG_NET, P25_PDU_STR ", block %u, fmt = $%02X, lastBlock = %u",
                            (m_netDataHeader.getFormat() == PDU_FMT_CONFIRMED) ? m_netData[i].getSerialNo() : m_netDataBlockCnt, m_netData[i].getFormat(),
                            m_netData[i].getLastBlock());
                    }

                    m_netData[i].getData(m_pduUserData + dataOffset);
                    dataOffset += (m_rfDataHeader.getFormat() == PDU_FMT_CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
                    m_pduUserDataLength = dataOffset;

                    m_netDataBlockCnt++;

                    // is this the last block?
                    if (m_netData[i].getLastBlock() && m_netDataBlockCnt == blocksToFollow) {
                        bool crcRet = edac::CRC::checkCRC32(m_pduUserData, m_pduUserDataLength);
                        if (!crcRet) {
                            LogWarning(LOG_NET, P25_PDU_STR ", failed CRC-32 check, blocks %u, len %u", blocksToFollow, m_pduUserDataLength);
                        }
                    }
                }
                else {
                    if (m_netData[i].getFormat() == PDU_FMT_CONFIRMED)
                        LogWarning(LOG_NET, P25_PDU_STR ", unfixable PDU data (3/4 rate or CRC), block %u", i);
                    else
                        LogWarning(LOG_NET, P25_PDU_STR ", unfixable PDU data (1/2 rate or CRC), block %u", i);

                    if (m_dumpPDUData) {
                        Utils::dump(1U, "Unfixable PDU Data", buffer, P25_PDU_FEC_LENGTH_BYTES);
                    }
                }

                offset += P25_PDU_FEC_LENGTH_BYTES;
            }

            if (m_dumpPDUData && m_netDataBlockCnt > 0U) {
                Utils::dump(1U, "PDU Packet", m_pduUserData, m_pduUserDataLength);
            }

            if (m_netDataBlockCnt < blocksToFollow) {
                LogWarning(LOG_NET, P25_PDU_STR ", incomplete PDU (%d / %d blocks)", m_netDataBlockCnt, blocksToFollow);
            }

            uint32_t srcId = (m_netUseSecondHeader || m_netExtendedAddress) ? m_netSecondHeader.getLLId() : m_netDataHeader.getLLId();
            uint32_t dstId = m_netDataHeader.getLLId();

            ::ActivityLog("P25", false, "Net data transmission from %u to %u, %u blocks", srcId, dstId, m_netDataHeader.getBlocksToFollow());

            if (m_repeatPDU) {
                if (m_verbose) {
                    LogMessage(LOG_NET, P25_PDU_STR ", repeating PDU, llId = %u", (m_netUseSecondHeader || m_netExtendedAddress) ? m_netSecondHeader.getLLId() : m_netDataHeader.getLLId());
                }

                writeNet_PDU_Buffered(); // re-generate buffered PDU and send it on
            }

            ::ActivityLog("P25", false, "end of Net data transmission");

            m_netDataHeader.reset();
            m_netSecondHeader.reset();
            m_netDataOffset = 0U;
            m_netDataBlockCnt = 0U;
            m_netPDUCount = 0U;
            m_pduUserDataLength = 0U;

            m_p25->m_netState = RS_NET_IDLE;
        }
    }

    return true;
}

/// <summary>
/// Helper to check if a logical link ID has registered with data services.
/// </summary>
/// <param name="llId">Logical Link ID.</param>
/// <returns>True, if ID has registered, otherwise false.</returns>
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

/// <summary>
/// Helper to write user data as a P25 PDU packet.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="pduUserData"></param>
/// <param name="clearBeforeWrite"></param>
void Data::writeRF_PDU_User(data::DataHeader& dataHeader, const uint8_t* pduUserData, bool clearBeforeWrite)
{
    assert(pduUserData != nullptr);

    uint32_t bitLength = ((dataHeader.getBlocksToFollow() + 1U) * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
    uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

    uint8_t data[bitLength / 8U];
    ::memset(data, 0x00U, bitLength / 8U);
    uint8_t block[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    // Generate the PDU header and 1/2 rate Trellis
    dataHeader.encode(block);
    Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);
    offset += P25_PDU_FEC_LENGTH_BITS;

    // Generate the PDU data
    DataBlock rspBlock = DataBlock();
    uint32_t dataOffset = 0U;
    for (uint8_t i = 0; i < dataHeader.getBlocksToFollow(); i++) {
        rspBlock.setFormat(PDU_FMT_UNCONFIRMED);
        rspBlock.setSerialNo(0U);
        rspBlock.setData(pduUserData + dataOffset);

        ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
        rspBlock.encode(block);
        Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);
        offset += P25_PDU_FEC_LENGTH_BITS;
        dataOffset += P25_PDU_UNCONFIRMED_LENGTH_BYTES;
    }

    if (clearBeforeWrite) {
        m_p25->m_modem->clearP25Frame();
        m_p25->m_txQueue.clear();
    }

    writeRF_PDU(data, bitLength);
}

/// <summary>
/// Updates the processor by the passed number of milliseconds.
/// </summary>
/// <param name="ms"></param>
void Data::clock(uint32_t ms)
{
    // clock all the connect timers
    std::vector<uint32_t> connToClear = std::vector<uint32_t>();
    for (auto entry : m_connQueueTable) {
        uint32_t llId = entry.first;

        m_connTimerTable[llId].clock(ms);
        if (m_connTimerTable[llId].isRunning() && m_connTimerTable[llId].hasExpired()) {
            connToClear.push_back(llId);
        }
    }

    // handle PDU connection registration
    for (uint32_t llId : connToClear) {
        uint8_t mfId = std::get<0>(m_connQueueTable[llId]);
        uint64_t ipAddr = std::get<1>(m_connQueueTable[llId]);

        if (!acl::AccessControl::validateSrcId(llId)) {
            LogWarning(LOG_RF, P25_PDU_STR ", PDU_REG_TYPE_RSP_DENY (Registration Response Deny), llId = %u, ipAddr = %s", llId, __IP_FROM_ULONG(ipAddr).c_str());
            writeRF_PDU_Reg_Response(PDU_REG_TYPE_RSP_DENY, mfId, llId, ipAddr);
        }
        else {
            if (!hasLLIdFNEReg(llId)) {
                // update dynamic FNE registration table entry
                m_fneRegTable[llId] = ipAddr;
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_PDU_STR ", PDU_REG_TYPE_RSP_ACCPT (Registration Response Accept), llId = %u, ipAddr = %s", llId, __IP_FROM_ULONG(ipAddr).c_str());
            }

            writeRF_PDU_Reg_Response(PDU_REG_TYPE_RSP_ACCPT, mfId, llId, ipAddr);
        }

        m_connQueueTable.erase(llId);
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Data class.
/// </summary>
/// <param name="p25">Instance of the Control class.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="dumpPDUData"></param>
/// <param name="repeatPDU"></param>
/// <param name="debug">Flag indicating whether P25 debug is enabled.</param>
/// <param name="verbose">Flag indicating whether P25 verbose logging is enabled.</param>
Data::Data(Control* p25, network::BaseNetwork* network, bool dumpPDUData, bool repeatPDU, bool debug, bool verbose) :
    m_p25(p25),
    m_network(network),
    m_prevRfState(RS_RF_LISTENING),
    m_rfData(nullptr),
    m_rfDataHeader(),
    m_rfSecondHeader(),
    m_rfUseSecondHeader(false),
    m_rfExtendedAddress(false),
    m_rfDataBlockCnt(0U),
    m_rfPDU(nullptr),
    m_rfPDUCount(0U),
    m_rfPDUBits(0U),
    m_netData(nullptr),
    m_netDataHeader(),
    m_netSecondHeader(),
    m_netUseSecondHeader(false),
    m_netExtendedAddress(false),
    m_netDataOffset(0U),
    m_netDataBlockCnt(0U),
    m_netPDU(nullptr),
    m_netPDUCount(0U),
    m_pduUserData(nullptr),
    m_pduUserDataLength(0U),
    m_fneRegTable(),
    m_connQueueTable(),
    m_connTimerTable(),
    m_dumpPDUData(dumpPDUData),
    m_repeatPDU(repeatPDU),
    m_verbose(verbose),
    m_debug(debug)
{
    m_rfData = new data::DataBlock[P25_MAX_PDU_COUNT];

    m_rfPDU = new uint8_t[P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(m_rfPDU, 0x00U, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);

    m_netData = new data::DataBlock[P25_MAX_PDU_COUNT];

    m_netPDU = new uint8_t[P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(m_netPDU, 0x00U, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);

    m_pduUserData = new uint8_t[P25_MAX_PDU_COUNT * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
    ::memset(m_pduUserData, 0x00U, P25_MAX_PDU_COUNT * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);

    m_fneRegTable.clear();
    m_connQueueTable.clear();
    m_connTimerTable.clear();
}

/// <summary>
/// Finalizes a instance of the Data class.
/// </summary>
Data::~Data()
{
    delete[] m_rfData;
    delete[] m_netData;
    delete[] m_rfPDU;
    delete[] m_netPDU;
    delete[] m_pduUserData;
}

/// <summary>
/// Write data processed from RF to the network.
/// </summary>
/// <param name="currentBlock"></param>
/// <param name="data"></param>
/// <param name="len"></param>
void Data::writeNetwork(const uint8_t currentBlock, const uint8_t *data, uint32_t len)
{
    assert(data != nullptr);

    if (m_network == nullptr)
        return;

    if (m_p25->m_rfTimeout.isRunning() && m_p25->m_rfTimeout.hasExpired())
        return;

    // Utils::dump(1U, "Outgoing Network PDU Frame", data, len);

    m_network->writeP25PDU(m_rfDataHeader, currentBlock, data, len);
}

/// <summary>
/// Helper to write a P25 PDU packet.
/// </summary>
/// <param name="pdu"></param>
/// <param name="bitlength"></param>
/// <param name="noNulls"></param>
/// <remarks>This simply takes data packed into m_rfPDU and transmits it.</remarks>
void Data::writeRF_PDU(const uint8_t* pdu, uint32_t bitLength, bool noNulls)
{
    assert(pdu != nullptr);
    assert(bitLength > 0U);

    uint8_t data[P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(data, 0x00U, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);

    if (m_debug) {
        Utils::dump(2U, "!!! *Raw PDU Frame Data - P25_DUID_PDU", pdu, bitLength / 8U);
    }

    // Add the data
    uint32_t newBitLength = P25Utils::encode(pdu, data + 2U, bitLength);
    uint32_t newByteLength = newBitLength / 8U;
    if ((newBitLength % 8U) > 0U)
        newByteLength++;

    // Regenerate Sync
    Sync::addP25Sync(data + 2U);

    // Regenerate NID
    m_p25->m_nid.encode(data + 2U, P25_DUID_PDU);

    // Add busy bits
    P25Utils::addBusyBits(data + 2U, newBitLength, false, true);

    if (m_p25->m_duplex) {
        data[0U] = modem::TAG_DATA;
        data[1U] = 0x00U;

        m_p25->addFrame(data, newByteLength + 2U);
    }

    // add trailing null pad; only if control data isn't being transmitted
    if (!m_p25->m_ccRunning && !noNulls) {
        m_p25->writeRF_Nulls();
    }
}

/// <summary>
/// Helper to write a network P25 PDU packet.
/// </summary>
/// <remarks>This will take buffered network PDU data and repeat it over the air.</remarks>
void Data::writeNet_PDU_Buffered()
{
    uint32_t bitLength = ((m_netDataHeader.getBlocksToFollow() + 1U) * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
    uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

    uint8_t data[bitLength / 8U];
    ::memset(data, 0x00U, bitLength / 8U);
    uint8_t block[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    uint32_t blocksToFollow = m_netDataHeader.getBlocksToFollow();

    // generate the PDU header and 1/2 rate Trellis
    m_netDataHeader.encode(block);
    Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);
    offset += P25_PDU_FEC_LENGTH_BITS;

    uint32_t dataOffset = 0U;
    edac::CRC::addCRC32(m_pduUserData, m_pduUserDataLength);

    // generate the second PDU header
    if (m_netUseSecondHeader) {
        ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

        m_netSecondHeader.encode(block);
        Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

        offset += P25_PDU_FEC_LENGTH_BITS;
        dataOffset += P25_PDU_HEADER_LENGTH_BYTES;
        blocksToFollow--;
    }

    // generate the PDU data
    for (uint32_t i = 0U; i < blocksToFollow; i++) {
        m_netData[i].setFormat((m_netUseSecondHeader) ? m_netSecondHeader : m_netDataHeader);
        m_netData[i].setSerialNo(i);
        m_netData[i].setData(m_pduUserData + dataOffset);

        ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
        m_netData[i].encode(block);
        Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

        offset += P25_PDU_FEC_LENGTH_BITS;
        dataOffset += (m_netDataHeader.getFormat() == PDU_FMT_CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
    }

    writeRF_PDU(data, bitLength);
}

/// <summary>
/// Helper to re-write a received P25 PDU packet.
/// </summary>
/// <remarks>This will take buffered received PDU data and repeat it over the air.</remarks>
void Data::writeRF_PDU_Buffered()
{
    uint32_t bitLength = ((m_rfDataHeader.getBlocksToFollow() + 1U) * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
    uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

    uint8_t data[bitLength / 8U];
    ::memset(data, 0x00U, bitLength / 8U);
    uint8_t block[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    uint32_t blocksToFollow = m_rfDataHeader.getBlocksToFollow();

    // generate the PDU header and 1/2 rate Trellis
    m_rfDataHeader.encode(block);
    Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);
    offset += P25_PDU_FEC_LENGTH_BITS;

    uint32_t dataOffset = 0U;
    edac::CRC::addCRC32(m_pduUserData, m_pduUserDataLength);

    // generate the second PDU header
    if (m_rfUseSecondHeader) {
        ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

        m_rfSecondHeader.encode(block);
        Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

        offset += P25_PDU_FEC_LENGTH_BITS;
        dataOffset += P25_PDU_HEADER_LENGTH_BYTES;
        blocksToFollow--;
    }

    // generate the PDU data
    for (uint32_t i = 0U; i < blocksToFollow; i++) {
        m_rfData[i].setFormat((m_rfUseSecondHeader) ? m_rfSecondHeader : m_rfDataHeader);
        m_rfData[i].setSerialNo(i);
        m_rfData[i].setData(m_pduUserData + dataOffset);

        ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
        m_rfData[i].encode(block);
        Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

        offset += P25_PDU_FEC_LENGTH_BITS;
        dataOffset += (m_rfDataHeader.getFormat() == PDU_FMT_CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
    }

    writeRF_PDU(data, bitLength);
}

/// <summary>
/// Helper to write a PDU registration response.
/// </summary>
/// <param name="regType"></param>
/// <param name="mfId"></param>
/// <param name="llId"></param>
/// <param name="ipAddr"></param>
void Data::writeRF_PDU_Reg_Response(uint8_t regType, uint8_t mfId, uint32_t llId, ulong64_t ipAddr)
{
    if ((regType != PDU_REG_TYPE_RSP_ACCPT) && (regType != PDU_REG_TYPE_RSP_DENY))
        return;

    uint32_t bitLength = (2U * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
    uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

    uint8_t data[bitLength / 8U];
    ::memset(data, 0x00U, bitLength / 8U);
    uint8_t block[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    DataHeader rspHeader = DataHeader();
    rspHeader.setFormat(PDU_FMT_CONFIRMED);
    rspHeader.setMFId(mfId);
    rspHeader.setAckNeeded(true);
    rspHeader.setOutbound(true);
    rspHeader.setSAP(PDU_SAP_REG);
    rspHeader.setLLId(llId);
    rspHeader.setBlocksToFollow(1U);

    // Generate the PDU header and 1/2 rate Trellis
    rspHeader.encode(block);
    Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);
    offset += P25_PDU_FEC_LENGTH_BITS;

    // build registration response data
    uint8_t rspData[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
    ::memset(rspData, 0x00U, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);

    rspData[0U] = ((regType & 0x0FU) << 4);                                     // Registration Type & Options
    rspData[1U] = (llId >> 16) & 0xFFU;                                         // Logical Link ID
    rspData[2U] = (llId >> 8) & 0xFFU;
    rspData[3U] = (llId >> 0) & 0xFFU;
    if (regType == PDU_REG_TYPE_RSP_ACCPT) {
        rspData[8U] = (ipAddr >> 24) & 0xFFU;                                   // IP Address
        rspData[9U] = (ipAddr >> 16) & 0xFFU;
        rspData[10U] = (ipAddr >> 8) & 0xFFU;
        rspData[11U] = (ipAddr >> 0) & 0xFFU;
    }

    edac::CRC::addCRC32(rspData, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);

    // Generate the PDU data
    DataBlock rspBlock = DataBlock();
    rspBlock.setFormat(PDU_FMT_CONFIRMED);
    rspBlock.setSerialNo(0U);
    rspBlock.setData(rspData);

    ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
    rspBlock.encode(block);
    Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

    writeRF_PDU(data, bitLength);
}

/// <summary>
/// Helper to write a PDU acknowledge response.
/// </summary>
/// <param name="ackClass"></param>
/// <param name="ackType"></param>
/// <param name="llId"></param>
/// <param name="srcLlId"></param>
/// <param name="noNulls"></param>
void Data::writeRF_PDU_Ack_Response(uint8_t ackClass, uint8_t ackType, uint32_t llId, uint32_t srcLlId, bool noNulls)
{
    if (ackClass == PDU_ACK_CLASS_ACK && ackType != PDU_ACK_TYPE_ACK)
        return;

    uint32_t bitLength = (2U * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
    uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

    uint8_t data[bitLength / 8U];
    ::memset(data, 0x00U, bitLength / 8U);
    uint8_t block[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    DataHeader rspHeader = DataHeader();
    rspHeader.setFormat(PDU_FMT_RSP);
    rspHeader.setMFId(m_rfDataHeader.getMFId());
    rspHeader.setOutbound(true);
    rspHeader.setResponseClass(ackClass);
    rspHeader.setResponseType(ackType);
    rspHeader.setResponseStatus(m_rfDataHeader.getNs());
    rspHeader.setLLId(llId);
    if (m_rfDataHeader.getSAP() == PDU_SAP_EXT_ADDR) {
        rspHeader.setSrcLLId(srcLlId);
        rspHeader.setFullMessage(false);
    }
    else {
        rspHeader.setFullMessage(true);
    }
    rspHeader.setBlocksToFollow(0U);

    // Generate the PDU header and 1/2 rate Trellis
    rspHeader.encode(block);
    Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

    if (m_verbose) {
        LogMessage(LOG_RF, P25_PDU_STR ", response, ackClass = $%02X, ackType = $%02X, llId = %u, srcLLId = %u",
            rspHeader.getResponseClass(), rspHeader.getResponseType(), rspHeader.getLLId(), rspHeader.getSrcLLId());
    }

    writeRF_PDU(data, bitLength, noNulls);
}
