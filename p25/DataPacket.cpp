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
*   Copyright (C) 2017-2020 by Bryan Biedenkapp N2PLL
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
#include "p25/DataPacket.h"
#include "p25/acl/AccessControl.h"
#include "p25/P25Utils.h"
#include "p25/Sync.h"
#include "edac/CRC.h"
#include "HostMain.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::data;

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>

#if !defined(_WIN32) && !defined(_WIN64)
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#else
#include <winsock.h>
#endif

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Resets the data states for the RF interface.
/// </summary>
void DataPacket::resetRF()
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
bool DataPacket::process(uint8_t* data, uint32_t len)
{
    assert(data != NULL);

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
        g_interruptP25Control = true;
    }

    // handle individual DUIDs
    if (duid == P25_DUID_PDU) {
        m_p25->m_trunk->resetStatusCommand();

        if (m_p25->m_rfState != RS_RF_DATA) {
            m_rfDataHeader.reset();
            m_rfDataBlockCnt = 0U;
            m_rfPDUCount = 0U;
            m_rfPDUBits = 0U;
            
            m_p25->m_rfState = RS_RF_DATA;

            ::memset(m_pduUserData, 0x00U, P25_MAX_PDU_COUNT * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);
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
                m_rfDataBlockCnt = 0U;
                m_rfPDUCount = 0U;
                m_rfPDUBits = 0U;
                m_p25->m_rfState = m_prevRfState;
                return false;
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_PDU_STR ", ack = %u, outbound = %u, fmt = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padCount = %u, n = %u, seqNo = %u, hdrOffset = %u",
                    m_rfDataHeader.getAckNeeded(), m_rfDataHeader.getOutbound(), m_rfDataHeader.getFormat(), m_rfDataHeader.getSAP(), m_rfDataHeader.getFullMessage(), 
                    m_rfDataHeader.getBlocksToFollow(), m_rfDataHeader.getPadCount(), m_rfDataHeader.getN(), m_rfDataHeader.getSeqNo(),
                    m_rfDataHeader.getHeaderOffset());
            }

            // make sure we don't get a PDU with more blocks then we support
            if (m_rfDataHeader.getBlocksToFollow() >= P25_MAX_PDU_COUNT) {
                LogError(LOG_RF, P25_PDU_STR ", too many PDU blocks to process, %u > %u", m_rfDataHeader.getBlocksToFollow(), P25_MAX_PDU_COUNT);

                m_rfDataHeader.reset();
                m_rfDataBlockCnt = 0U;
                m_rfPDUCount = 0U;
                m_rfPDUBits = 0U;
                m_p25->m_rfState = m_prevRfState;
                return false;
            }

            writeNetworkRF(P25_DT_DATA_HEADER, buffer, P25_PDU_FEC_LENGTH_BYTES);
        }

        if (m_p25->m_rfState == RS_RF_DATA) {
            uint32_t blocksToFollow = m_rfDataHeader.getBlocksToFollow();

            // confirmed extended addressing is unsupported
            if (m_rfDataHeader.getSAP() == PDU_SAP_EXT_ADDR &&
                m_rfDataHeader.getFormat() == PDU_FMT_CONFIRMED) {
                LogWarning(LOG_RF, P25_PDU_STR ", unsupported confirmed enhanced addressing");

                m_rfDataHeader.reset();
                m_rfSecondHeader.reset();
                m_rfDataBlockCnt = 0U;
                m_rfPDUCount = 0U;
                m_rfPDUBits = 0U;
                m_p25->m_rfState = m_prevRfState;
                return false;
            }

            // process second header if we're using enhanced addressing
            if (m_rfDataHeader.getSAP() == PDU_SAP_EXT_ADDR) {
                ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
                Utils::getBitRange(m_rfPDU, buffer, offset, P25_PDU_FEC_LENGTH_BITS);
                bool ret = m_rfSecondHeader.decode(buffer);
                if (!ret) {
                    LogWarning(LOG_RF, P25_PDU_STR ", unfixable RF 1/2 rate second header data");
                    Utils::dump(1U, "Unfixable PDU Data", m_rfPDU + offset, P25_PDU_HEADER_LENGTH_BYTES);

                    m_rfDataHeader.reset();
                    m_rfSecondHeader.reset();
                    m_rfDataBlockCnt = 0U;
                    m_rfPDUCount = 0U;
                    m_rfPDUBits = 0U;
                    m_p25->m_rfState = m_prevRfState;
                    return false;
                }

                if (m_verbose) {
                    LogMessage(LOG_RF, P25_PDU_STR ", fmt = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padCount = %u, n = %u, seqNo = %u, hdrOffset = %u, srcId = %u",
                        m_rfSecondHeader.getFormat(), m_rfSecondHeader.getSAP(), m_rfSecondHeader.getFullMessage(),
                        m_rfSecondHeader.getBlocksToFollow(), m_rfSecondHeader.getPadCount(), m_rfSecondHeader.getN(), m_rfSecondHeader.getSeqNo(),
                        m_rfSecondHeader.getHeaderOffset(), m_rfSecondHeader.getLLId());
                }

                writeNetworkRF(P25_DT_DATA_SEC_HEADER, buffer, P25_PDU_FEC_LENGTH_BYTES);

                offset += P25_PDU_FEC_LENGTH_BITS;
                m_rfPDUCount++;
                blocksToFollow--;
            }

            m_rfPDUCount++;
            uint32_t bitLength = ((blocksToFollow + 1U) * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;

            if (m_rfPDUBits >= bitLength) {
                uint32_t dataOffset = 0U;
                for (uint32_t i = 0U; i < blocksToFollow; i++) {
                    ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
                    Utils::getBitRange(m_rfPDU, buffer, offset, P25_PDU_FEC_LENGTH_BITS);
                    bool ret = m_rfData[i].decode(buffer, (m_rfDataHeader.getSAP() == PDU_SAP_EXT_ADDR) ? m_rfSecondHeader : m_rfDataHeader);
                    if (ret) {
                        if (m_verbose) {
                            LogMessage(LOG_RF, P25_PDU_STR ", block %u, fmt = $%02X",
                                (m_rfDataHeader.getFormat() == PDU_FMT_CONFIRMED) ? m_rfData[i].getSerialNo() : m_rfDataBlockCnt, m_rfData[i].getFormat());
                        }

                        m_rfData[i].getData(m_pduUserData + dataOffset);
                        m_rfDataBlockCnt++;

                        writeNetworkRF(P25_DT_DATA, buffer, P25_PDU_FEC_LENGTH_BYTES);
                    }
                    else {
                        if (m_rfData[i].getHalfRateTrellis())
                            LogWarning(LOG_RF, P25_PDU_STR ", unfixable PDU data (1/2 rate or CRC)");
                        else
                            LogWarning(LOG_RF, P25_PDU_STR ", unfixable PDU data (3/4 rate or CRC)");

                        if (m_dumpPDUData) {
                            Utils::dump(1U, "Unfixable PDU Data", buffer, P25_PDU_FEC_LENGTH_BYTES);
                        }
                    }

                    offset += P25_PDU_FEC_LENGTH_BITS;
                    dataOffset += (m_rfDataHeader.getFormat() == PDU_FMT_CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
                }

                if (m_dumpPDUData) {
                    Utils::dump(1U, "PDU Packet", m_pduUserData, dataOffset);
                }

                if (m_rfDataBlockCnt < blocksToFollow) {
                    LogWarning(LOG_RF, P25_PDU_STR ", incomplete PDU (%d / %d blocks)", m_rfDataBlockCnt, blocksToFollow);
                }

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
                            LogMessage(LOG_RF, P25_PDU_STR ", PDU_REG_TYPE_REQ_CNCT (Registration Request Connect), llId = %u, ipAddr = %u", llId, ipAddr);
                        }

                        if (!acl::AccessControl::validateSrcId(llId)) {
                            LogWarning(LOG_RF, P25_PDU_STR ", PDU_REG_TYPE_RSP_DENY (Registration Response Deny), llId = %u, ipAddr = %u", llId, ipAddr);
                            writeRF_PDU_Reg_Response(PDU_REG_TYPE_RSP_DENY, llId, ipAddr);
                        }
                        else {
                            if (!hasLLIdFNEReg(llId)) {
                                // update dynamic FNE registration table entry
                                m_fneRegTable[llId] = ipAddr;

                                if (m_verbose) {
                                    LogMessage(LOG_RF, P25_PDU_STR ", PDU_REG_TYPE_RSP_ACCPT (Registration Response Accept), llId = %u, ipAddr = %u", llId, ipAddr);
                                }
                                writeRF_PDU_Reg_Response(PDU_REG_TYPE_RSP_ACCPT, llId, ipAddr);
                            }
                        }
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
                default:
                    ::ActivityLog("P25", true, "received RF data transmission from %u to %u, %u blocks", m_rfDataHeader.getLLId(), m_rfDataHeader.getLLId(), m_rfDataHeader.getBlocksToFollow());

                    if (m_repeatPDU) {
                        if (m_verbose) {
                            LogMessage(LOG_RF, P25_PDU_STR ", repeating PDU, llId = %u", (m_rfDataHeader.getSAP() == PDU_SAP_EXT_ADDR) ? m_rfSecondHeader.getLLId() : m_rfDataHeader.getLLId());
                        }

                        writeRF_PDU(); // re-generate PDU and send it on
                    }

                    ::ActivityLog("P25", true, "end RF data transmission");
                    break;
                }

                m_rfDataHeader.reset();
                m_rfDataBlockCnt = 0U;
                m_rfPDUCount = 0U;
                m_rfPDUBits = 0U;

                m_p25->m_rfState = m_prevRfState;
            }
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
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="duid"></param>
/// <returns></returns>
bool DataPacket::processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, uint8_t& duid)
{
    if (m_p25->m_rfState != RS_RF_LISTENING && m_p25->m_netState == RS_NET_IDLE)
        return false;

    switch (duid) {
        case P25_DUID_PDU:
            {
                uint32_t pduLen = control.getDstId();               // PDU's use dstId as the PDU len
                ::memset(m_netPDU, 0x00U, pduLen + 2U);
                ::memcpy(m_netPDU, data, pduLen);

                uint8_t dataType = control.getLCO();
                if (dataType == P25_DT_DATA_HEADER) {
                    writeNet_PDU_Header();
                }
                else if (dataType == P25_DT_DATA_SEC_HEADER) {
                    writeNet_PDU_Sec_Header();
                }
                else if (dataType == P25_DT_DATA) {
                    writeNet_PDU();
                }

                if (m_p25->m_netState == RS_NET_DATA) {
                    if (m_netDataBlockCnt >= m_netBlocksToFollow) {
                        if (m_dumpPDUData) {
                            Utils::dump(1U, "PDU Packet", m_pduUserData, m_netDataOffset);
                        }
                    }

                    // write data to RF interface?
                    if (m_repeatPDU) {
                        if (m_netDataBlockCnt >= m_netBlocksToFollow) {
                            uint32_t bitLength = ((m_netDataHeader.getBlocksToFollow() + 1U) * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
                            uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

                            ::memset(m_netPDU, 0x00U, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);

                            uint8_t buffer[P25_PDU_FEC_LENGTH_BYTES];
                            ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

                            uint32_t blocksToFollow = m_netDataHeader.getBlocksToFollow();

                            // Generate the PDU header and 1/2 rate Trellis
                            m_netDataHeader.encode(buffer);
                            Utils::setBitRange(buffer, m_netPDU, offset, P25_PDU_FEC_LENGTH_BITS);
                            offset += P25_PDU_FEC_LENGTH_BITS;

                            // Generate the second PDU header
                            if (m_netDataHeader.getSAP() == PDU_SAP_EXT_ADDR) {
                                m_netSecondHeader.encode(buffer);
                                Utils::setBitRange(buffer, m_netPDU, offset, P25_PDU_FEC_LENGTH_BITS);

                                offset += P25_PDU_FEC_LENGTH_BITS;
                                blocksToFollow--;
                            }

                            // Generate the PDU data
                            bool halfRate = m_netData[0].getHalfRateTrellis();
                            uint32_t dataOffset = 0U;
                            for (uint32_t i = 0U; i < blocksToFollow; i++) {
                                m_netData[i].setFormat((m_netDataHeader.getSAP() == PDU_SAP_EXT_ADDR) ? m_netSecondHeader : m_netDataHeader);
                                m_netData[i].setSerialNo(i);
                                m_netData[i].setHalfRateTrellis(halfRate);
                                m_netData[i].setData(m_pduUserData + dataOffset);

                                ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
                                m_netData[i].encode(buffer);
                                Utils::setBitRange(buffer, m_netPDU, offset, P25_PDU_FEC_LENGTH_BITS);

                                offset += P25_PDU_FEC_LENGTH_BITS;
                                dataOffset += (m_netDataHeader.getFormat() == PDU_FMT_CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
                            }

                            if (m_debug) {
                                Utils::dump(2U, "!!! *Raw PDU Frame Data - P25_DUID_PDU", m_netPDU, bitLength / 8U);
                            }

                            uint8_t pdu[P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U];

                            // Add the data
                            uint32_t newBitLength = P25Utils::encode(m_netPDU, pdu + 2U, bitLength);
                            uint32_t newByteLength = newBitLength / 8U;
                            if ((newBitLength % 8U) > 0U)
                                newByteLength++;

                            // Regenerate Sync
                            Sync::addP25Sync(pdu + 2U);

                            // Regenerate NID
                            m_p25->m_nid.encode(pdu + 2U, P25_DUID_PDU);

                            // Add busy bits
                            m_p25->addBusyBits(pdu + 2U, newBitLength, false, true);

                            ::memset(m_netPDU, 0x00U, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);

                            if (m_p25->m_duplex) {
                                pdu[0U] = TAG_DATA;
                                pdu[1U] = 0x00U;
                                m_p25->writeQueueNet(pdu, newByteLength + 2U);
                            }

                            // add trailing null pad; only if control data isn't being transmitted
                            if (!m_p25->m_ccRunning) {
                                m_p25->writeRF_Nulls();
                            }

                            if (m_debug) {
                                Utils::dump(2U, "!!! *TX P25 Frame - P25_DUID_PDU", pdu + 2U, newByteLength);
                            }

                            ::ActivityLog("P25", true, "end RF data transmission");

                            m_netDataHeader.reset();
                            m_netSecondHeader.reset();
                            m_netBlocksToFollow = 0U;
                            m_netDataBlockCnt = 0U;
                            m_netBitOffset = 0U;
                            m_netDataOffset = 0U;
                            m_p25->m_netState = RS_NET_IDLE;
                        }
                    }
                } // if (m_netState == RS_NET_DATA)
            }
            break;
        default:
            return false;
    }

    return true;
}

/// <summary>
/// Helper to check if a logical link ID has registered with data services.
/// </summary>
/// <param name="llId">Logical Link ID.</param>
/// <returns>True, if ID has registered, otherwise false.</returns>
bool DataPacket::hasLLIdFNEReg(uint32_t llId) const
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

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the DataPacket class.
/// </summary>
/// <param name="p25">Instance of the Control class.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="dumpPDUData"></param>
/// <param name="repeatPDU"></param>
/// <param name="debug">Flag indicating whether P25 debug is enabled.</param>
/// <param name="verbose">Flag indicating whether P25 verbose logging is enabled.</param>
DataPacket::DataPacket(Control* p25, network::BaseNetwork* network, bool dumpPDUData, bool repeatPDU, bool debug, bool verbose) :
    m_p25(p25),
    m_network(network),
    m_prevRfState(RS_RF_LISTENING),
    m_rfData(NULL),
    m_rfDataHeader(),
    m_rfSecondHeader(),
    m_rfDataBlockCnt(0U),
    m_rfPDU(NULL),
    m_rfPDUCount(0U),
    m_rfPDUBits(0U),
    m_netData(NULL),
    m_netDataHeader(),
    m_netSecondHeader(),
    m_netBlocksToFollow(0U),
    m_netDataBlockCnt(0U),
    m_netBitOffset(0U),
    m_netDataOffset(0U),
    m_netPDU(NULL),
    m_pduUserData(NULL),
    m_fneRegTable(),
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
}

/// <summary>
/// Finalizes a instance of the DataPacket class.
/// </summary>
DataPacket::~DataPacket()
{
    delete[] m_rfPDU;
    delete[] m_netPDU;
    delete[] m_pduUserData;
}

/// <summary>
/// Write data processed from RF to the network.
/// </summary>
/// <param name="dataType"></param>
/// <param name="data"></param>
/// <param name="len"></param>
void DataPacket::writeNetworkRF(const uint8_t dataType, const uint8_t *data, uint32_t len)
{
    assert(data != NULL);

    if (m_network == NULL)
        return;

    if (m_p25->m_rfTimeout.isRunning() && m_p25->m_rfTimeout.hasExpired())
        return;

    m_network->writeP25PDU((m_rfDataHeader.getSAP() == PDU_SAP_EXT_ADDR) ? m_rfSecondHeader.getLLId() : m_rfDataHeader.getLLId(), dataType, data, len);
}

/// <summary>
/// Helper to write a P25 PDU packet.
/// </summary>
void DataPacket::writeRF_PDU()
{
    uint32_t bitLength = ((m_rfDataHeader.getBlocksToFollow() + 1U) * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
    uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

    ::memset(m_rfPDU, 0x00U, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);

    uint8_t buffer[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    uint32_t blocksToFollow = m_rfDataHeader.getBlocksToFollow();

    // Generate the PDU header and 1/2 rate Trellis
    m_rfDataHeader.encode(buffer);
    Utils::setBitRange(buffer, m_rfPDU, offset, P25_PDU_FEC_LENGTH_BITS);
    offset += P25_PDU_FEC_LENGTH_BITS;

    // Generate the second PDU header
    if (m_rfDataHeader.getSAP() == PDU_SAP_EXT_ADDR) {
        ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

        m_rfSecondHeader.encode(buffer);
        Utils::setBitRange(buffer, m_rfPDU, offset, P25_PDU_FEC_LENGTH_BITS);
        
        offset += P25_PDU_FEC_LENGTH_BITS;
        blocksToFollow--;
    }

    // Generate the PDU data
    bool halfRate = m_rfData[0].getHalfRateTrellis();
    uint32_t dataOffset = 0U;
    for (uint32_t i = 0U; i < blocksToFollow; i++) {
        m_rfData[i].setFormat((m_rfDataHeader.getSAP() == PDU_SAP_EXT_ADDR) ? m_rfSecondHeader : m_rfDataHeader);
        m_rfData[i].setSerialNo(i);
        m_rfData[i].setHalfRateTrellis(halfRate);
        m_rfData[i].setData(m_pduUserData + dataOffset);

        ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
        m_rfData[i].encode(buffer);
        Utils::setBitRange(buffer, m_rfPDU, offset, P25_PDU_FEC_LENGTH_BITS);

        offset += P25_PDU_FEC_LENGTH_BITS;
        dataOffset += (m_rfDataHeader.getFormat() == PDU_FMT_CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
    }

    if (m_debug) {
        Utils::dump(2U, "!!! *Raw PDU Frame Data - P25_DUID_PDU", m_rfPDU, bitLength / 8U);
    }

    uint8_t pdu[P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U];

    // Add the data
    uint32_t newBitLength = P25Utils::encode(m_rfPDU, pdu + 2U, bitLength);
    uint32_t newByteLength = newBitLength / 8U;
    if ((newBitLength % 8U) > 0U)
        newByteLength++;

    // Regenerate Sync
    Sync::addP25Sync(pdu + 2U);

    // Regenerate NID
    m_p25->m_nid.encode(pdu + 2U, P25_DUID_PDU);

    // Add busy bits
    m_p25->addBusyBits(pdu + 2U, newBitLength, false, true);

    ::memset(m_rfPDU, 0x00U, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);

    if (m_p25->m_duplex) {
        pdu[0U] = TAG_DATA;
        pdu[1U] = 0x00U;
        m_p25->writeQueueRF(pdu, newByteLength + 2U);
    }

    // add trailing null pad; only if control data isn't being transmitted
    if (!m_p25->m_ccRunning) {
        m_p25->writeRF_Nulls();
    }

    if (m_debug) {
        Utils::dump(2U, "!!! *TX P25 Frame - P25_DUID_PDU", pdu + 2U, newByteLength);
    }
}

/// <summary>
/// Helper to write a PDU registration response.
/// </summary>
/// <param name="regType"></param>
/// <param name="llId"></param>
/// <param name="ipAddr"></param>
void DataPacket::writeRF_PDU_Reg_Response(uint8_t regType, uint32_t llId, ulong64_t ipAddr)
{
    if ((regType != PDU_REG_TYPE_RSP_ACCPT) && (regType != PDU_REG_TYPE_RSP_DENY))
        return;

    uint32_t bitLength = (2U * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
    uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

    uint8_t buffer[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    DataRspHeader rspHeader = DataRspHeader();
    rspHeader.setOutbound(true);
    rspHeader.setClass(PDU_ACK_CLASS_ACK);
    rspHeader.setType(PDU_ACK_TYPE_ACK);
    rspHeader.setLLId(llId);
    rspHeader.setSrcLLId(P25_WUID_FNE);
    rspHeader.setBlocksToFollow(1U);

    // Generate the PDU header and 1/2 rate Trellis
    rspHeader.encode(buffer);
    Utils::setBitRange(buffer, m_rfPDU, offset, P25_PDU_FEC_LENGTH_BITS);
    offset += P25_PDU_FEC_LENGTH_BITS;

    // build registration response data
    ::memset(buffer, 0x00U, P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);

    buffer[0U] = (regType << 4);                                                    // Registration Type & Options
    buffer[1U] = (llId >> 16) & 0xFFU;                                              // Logical Link ID
    buffer[2U] = (llId >> 8) & 0xFFU;
    buffer[3U] = (llId >> 0) & 0xFFU;
    if (regType == PDU_REG_TYPE_RSP_ACCPT) {
        buffer[8U] = (ipAddr >> 24) & 0xFFU;                                        // IP Address
        buffer[9U] = (ipAddr >> 16) & 0xFFU;
        buffer[10U] = (ipAddr >> 8) & 0xFFU;
        buffer[11U] = (ipAddr >> 0) & 0xFFU;
    }

    edac::CRC::addCRC32(buffer, P25_PDU_CONFIRMED_LENGTH_BYTES);

    // Generate the PDU data
    m_rfData[0].setFormat(PDU_FMT_RSP);
    m_rfData[0].setSerialNo(0U);
    m_rfData[0].setHalfRateTrellis(true);
    m_rfData[0].setData(buffer);

    ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
    m_rfData[0].encode(buffer);
    Utils::setBitRange(buffer, m_rfPDU, offset, P25_PDU_FEC_LENGTH_BITS);

    if (m_debug) {
        Utils::dump(2U, "!!! *Raw PDU Frame Data - P25_DUID_PDU", m_rfPDU, bitLength / 8U);
    }

    uint8_t pdu[P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U];

    // Add the data
    uint32_t newBitLength = P25Utils::encode(m_rfPDU, pdu + 2U, bitLength);
    uint32_t newByteLength = newBitLength / 8U;
    if ((newBitLength % 8U) > 0U)
        newByteLength++;

    // Regenerate Sync
    Sync::addP25Sync(pdu + 2U);

    // Regenerate NID
    m_p25->m_nid.encode(pdu + 2U, P25_DUID_PDU);

    // Add busy bits
    m_p25->addBusyBits(pdu + 2U, newBitLength, false, true);

    ::memset(m_rfPDU, 0x00U, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);

    if (m_p25->m_duplex) {
        pdu[0U] = TAG_DATA;
        pdu[1U] = 0x00U;
        m_p25->writeQueueRF(pdu, newByteLength + 2U);
    }

    if (m_debug) {
        Utils::dump(2U, "!!! *TX P25 Frame - P25_DUID_PDU", pdu + 2U, newByteLength);
    }
}

/// <summary>
/// Helper to write a network P25 PDU header packet.
/// </summary>
void DataPacket::writeNet_PDU_Header()
{
    if (m_p25->m_netState != RS_NET_DATA) {
        m_netDataHeader.reset();
        m_netSecondHeader.reset();
        m_netBlocksToFollow = 0U;
        m_netDataBlockCnt = 0U;
        m_netBitOffset = 0U;
        m_netDataOffset = 0U;

        m_p25->m_netState = RS_NET_DATA;

        ::memset(m_pduUserData, 0x00U, P25_MAX_PDU_COUNT * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);

        uint8_t buffer[P25_PDU_FEC_LENGTH_BYTES];
        ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
        ::memcpy(buffer, m_netPDU, P25_PDU_FEC_LENGTH_BYTES);
        
        bool ret = m_netDataHeader.decode(buffer);
        if (!ret) {
            LogWarning(LOG_NET, P25_PDU_STR ", unfixable RF 1/2 rate header data");
            Utils::dump(1U, "Unfixable PDU Data", buffer, P25_PDU_FEC_LENGTH_BYTES);

            m_netDataHeader.reset();
            m_netSecondHeader.reset();
            m_netBlocksToFollow = 0U;
            m_netDataBlockCnt = 0U;
            m_netBitOffset = 0U;
            m_netDataOffset = 0U;
            m_p25->m_netState = RS_NET_IDLE;
            return;
        }

        ::ActivityLog("P25", true, "received network data transmission from %u to %u, %u blocks", m_netDataHeader.getLLId(), m_netDataHeader.getLLId(), m_netDataHeader.getBlocksToFollow());

        if (m_verbose) {
            LogMessage(LOG_NET, P25_PDU_STR ", ack = %u, outbound = %u, fmt = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padCount = %u, n = %u, seqNo = %u, hdrOffset = %u",
                m_netDataHeader.getAckNeeded(), m_netDataHeader.getOutbound(), m_netDataHeader.getFormat(), m_netDataHeader.getSAP(), m_netDataHeader.getFullMessage(),
                m_netDataHeader.getBlocksToFollow(), m_netDataHeader.getPadCount(), m_netDataHeader.getN(), m_netDataHeader.getSeqNo(),
                m_netDataHeader.getHeaderOffset());
        }

        // make sure we don't get a PDU with more blocks then we support
        if (m_netDataHeader.getBlocksToFollow() >= P25_MAX_PDU_COUNT) {
            LogError(LOG_NET, P25_PDU_STR ", too many PDU blocks to process, %u > %u", m_netDataHeader.getBlocksToFollow(), P25_MAX_PDU_COUNT);

            m_netDataHeader.reset();
            m_netBlocksToFollow = 0U;
            m_netDataBlockCnt = 0U;
            m_netBitOffset = 0U;
            m_netDataOffset = 0U;
            return;
        }

        if (m_netDataHeader.getSAP() == PDU_SAP_EXT_ADDR &&
            m_netDataHeader.getFormat() == PDU_FMT_CONFIRMED) {
            LogWarning(LOG_NET, P25_PDU_STR ", unsupported confirmed enhanced addressing");

            m_netDataHeader.reset();
            m_netSecondHeader.reset();
            m_netBlocksToFollow = 0U;
            m_netDataBlockCnt = 0U;
            m_netBitOffset = 0U;
            m_netDataOffset = 0U;
            m_p25->m_netState = RS_NET_IDLE;
            return;
        }

        m_netBlocksToFollow = m_netDataHeader.getBlocksToFollow();

        //uint32_t bitLength = ((m_netDataHeader.getBlocksToFollow() + 1U) * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
        m_netBitOffset = P25_PREAMBLE_LENGTH_BITS;

        ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

        // Generate the PDU header and 1/2 rate Trellis
        m_netDataHeader.encode(buffer);
        Utils::setBitRange(buffer, m_netPDU, m_netBitOffset, P25_PDU_FEC_LENGTH_BITS);
        m_netBitOffset += P25_PDU_FEC_LENGTH_BITS;
    }
}

/// <summary>
/// Helper to write a network P25 PDU secondary header packet.
/// </summary>
void DataPacket::writeNet_PDU_Sec_Header()
{
    if (m_p25->m_netState == RS_NET_DATA) {
        // process second header if we're using enhanced addressing
        if (m_netDataHeader.getSAP() == PDU_SAP_EXT_ADDR) {
            uint8_t buffer[P25_PDU_FEC_LENGTH_BYTES];
            ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
            ::memcpy(buffer, m_netPDU, P25_PDU_FEC_LENGTH_BYTES);

            bool ret = m_netSecondHeader.decode(buffer);
            if (!ret) {
                LogWarning(LOG_NET, P25_PDU_STR ", unfixable RF 1/2 rate second header data");
                Utils::dump(1U, "Unfixable PDU Data", buffer, P25_PDU_HEADER_LENGTH_BYTES);

                m_netDataHeader.reset();
                m_netSecondHeader.reset();
                m_netBlocksToFollow = 0U;
                m_netDataBlockCnt = 0U;
                m_netBitOffset = 0U;
                m_netDataOffset = 0U;
                m_p25->m_netState = RS_NET_IDLE;
                return;
            }

            if (m_verbose) {
                LogMessage(LOG_NET, P25_PDU_STR ", fmt = $%02X, sap = $%02X, srcId = %u",
                    m_netSecondHeader.getFormat(), m_netSecondHeader.getSAP(), m_netSecondHeader.getLLId());
            }

            ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

            // Generate the PDU header and 1/2 rate Trellis
            m_netDataHeader.encode(buffer);
            Utils::setBitRange(buffer, m_netPDU, m_netBitOffset, P25_PDU_FEC_LENGTH_BITS);
            m_netBitOffset += P25_PDU_FEC_LENGTH_BITS;

            m_netBlocksToFollow--;
        }
    }
}

/// <summary>
/// Helper to write a network P25 PDU data packet.
/// </summary>
void DataPacket::writeNet_PDU()
{
    if (m_p25->m_netState == RS_NET_DATA) {
        uint8_t buffer[P25_PDU_FEC_LENGTH_BYTES];
        ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
        ::memcpy(buffer, m_netPDU, P25_PDU_FEC_LENGTH_BYTES);

        bool ret = m_netData[m_netDataBlockCnt].decode(buffer, (m_netDataHeader.getSAP() == PDU_SAP_EXT_ADDR) ? m_netSecondHeader : m_netDataHeader);
        if (ret) {
            if (m_verbose) {
                LogMessage(LOG_NET, P25_PDU_STR ", block %u, fmt = $%02X",
                    (m_netDataHeader.getFormat() == PDU_FMT_CONFIRMED) ? m_netData[m_netDataBlockCnt].getSerialNo() : m_netDataBlockCnt, m_netData[m_netDataBlockCnt].getFormat());
            }

            m_netData[m_netDataBlockCnt].getData(m_pduUserData + m_netDataOffset);
            m_netDataBlockCnt++;
        }
        else {
            if (m_netData[m_netDataBlockCnt].getHalfRateTrellis())
                LogWarning(LOG_NET, P25_PDU_STR ", unfixable PDU data (1/2 rate or CRC)");
            else
                LogWarning(LOG_NET, P25_PDU_STR ", unfixable PDU data (3/4 rate or CRC)");

            if (m_dumpPDUData) {
                Utils::dump(1U, "Unfixable PDU Data", buffer, P25_PDU_FEC_LENGTH_BYTES);
            }
        }

        m_netBitOffset += P25_PDU_FEC_LENGTH_BITS;
        m_netDataOffset += (m_netDataHeader.getFormat() == PDU_FMT_CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
    }
}
