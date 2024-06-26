// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016,2017,2018 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
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

const uint32_t CONN_WAIT_TIMEOUT = 1U;
const uint32_t SNDCP_READY_TIMEOUT = 10U;
const uint32_t SNDCP_STANDBY_TIMEOUT = 60U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Resets the data states for the RF interface. */

void Data::resetRF()
{
    m_rfDataBlockCnt = 0U;
    m_rfPDUCount = 0U;
    m_rfPDUBits = 0U;

    m_rfDataHeader.reset();
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
        if (m_p25->m_rfState != RS_RF_DATA) {
            m_rfDataHeader.reset();
            m_rfDataBlockCnt = 0U;
            m_rfPDUCount = 0U;
            m_rfPDUBits = 0U;

            ::memset(m_rfPDU, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

            m_p25->m_rfState = RS_RF_DATA;

            ::memset(m_pduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);
            m_pduUserDataLength = 0U;
        }

        uint32_t start = m_rfPDUCount * P25_PDU_FRAME_LENGTH_BITS;

        uint8_t buffer[P25_PDU_FRAME_LENGTH_BYTES];
        ::memset(buffer, 0x00U, P25_PDU_FRAME_LENGTH_BYTES);

        uint32_t bits = P25Utils::decode(data + 2U, buffer, start, start + P25_PDU_FRAME_LENGTH_BITS);
        m_rfPDUBits = Utils::getBits(buffer, m_rfPDU, 0U, bits);

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
                LogMessage(LOG_RF, P25_PDU_STR ", ISP, ack = %u, outbound = %u, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, packetLength = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
                    m_rfDataHeader.getAckNeeded(), m_rfDataHeader.getOutbound(), m_rfDataHeader.getFormat(), m_rfDataHeader.getMFId(), m_rfDataHeader.getSAP(), m_rfDataHeader.getFullMessage(),
                    m_rfDataHeader.getBlocksToFollow(), m_rfDataHeader.getPadLength(), m_rfDataHeader.getPacketLength(), m_rfDataHeader.getNs(), m_rfDataHeader.getFSN(), m_rfDataHeader.getLastFragment(),
                    m_rfDataHeader.getHeaderOffset(), m_rfDataHeader.getLLId());
            }

            // make sure we don't get a PDU with more blocks then we support
            if (m_rfDataHeader.getBlocksToFollow() >= P25_MAX_PDU_BLOCKS) {
                LogError(LOG_RF, P25_PDU_STR ", ISP, too many PDU blocks to process, %u > %u", m_rfDataHeader.getBlocksToFollow(), P25_MAX_PDU_BLOCKS);

                m_rfDataHeader.reset();
                m_rfSecondHeader.reset();
                m_rfDataBlockCnt = 0U;
                m_rfPDUCount = 0U;
                m_rfPDUBits = 0U;
                m_p25->m_rfState = m_prevRfState;
                return false;
            }

            // if we're a dedicated CC or in control only mode, we only want to handle AMBTs. Otherwise return
            if ((m_p25->m_dedicatedControl || m_p25->m_controlOnly) && m_rfDataHeader.getFormat() != PDUFormatType::AMBT) {
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
            if ((m_rfDataHeader.getFormat() != PDUFormatType::AMBT) &&
                (m_rfDataHeader.getFormat() != PDUFormatType::RSP) &&
                (m_rfDataHeader.getSAP() != PDUSAP::REG)) {
                writeNetwork(0U, buffer, P25_PDU_FEC_LENGTH_BYTES, false);
            }
        }

        if (m_p25->m_rfState == RS_RF_DATA) {
            uint32_t blocksToFollow = m_rfDataHeader.getBlocksToFollow();
            // process second header if we're using enhanced addressing
            if (m_rfDataHeader.getSAP() == PDUSAP::EXT_ADDR &&
                m_rfDataHeader.getFormat() == PDUFormatType::UNCONFIRMED) {
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
                    LogMessage(LOG_RF, P25_PDU_STR ", ISP, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
                        m_rfSecondHeader.getFormat(), m_rfSecondHeader.getMFId(), m_rfSecondHeader.getSAP(), m_rfSecondHeader.getFullMessage(),
                        m_rfSecondHeader.getBlocksToFollow(), m_rfSecondHeader.getPadLength(), m_rfSecondHeader.getNs(), m_rfSecondHeader.getFSN(), m_rfSecondHeader.getLastFragment(),
                        m_rfSecondHeader.getHeaderOffset(), m_rfSecondHeader.getLLId());
                }

                m_rfUseSecondHeader = true;

                // only send data blocks across the network, if we're not an AMBT,
                // an RSP or a registration service
                if ((m_rfDataHeader.getFormat() != PDUFormatType::AMBT) &&
                    (m_rfDataHeader.getFormat() != PDUFormatType::RSP) &&
                    (m_rfDataHeader.getSAP() != PDUSAP::REG)) {
                    writeNetwork(1U, buffer, P25_PDU_FEC_LENGTH_BYTES, false);
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

                // if the primary header has a header offset ensure data if offset by that amount 
                if (m_rfDataHeader.getHeaderOffset() > 0U) {
                    offset += m_rfDataHeader.getHeaderOffset() * 8;
                    m_pduUserDataLength -= m_rfDataHeader.getHeaderOffset();
                }

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
                        if ((m_rfDataHeader.getFormat() == PDUFormatType::CONFIRMED) || (m_rfDataHeader.getFormat() == PDUFormatType::UNCONFIRMED)) {
                            if ((m_rfDataBlockCnt + 1U) == blocksToFollow) {
                                m_rfData[i].setLastBlock(true);
                            }
                        }

                        // are we processing extended address data from the first block?
                        if (m_rfDataHeader.getSAP() == PDUSAP::EXT_ADDR && m_rfDataHeader.getFormat() == PDUFormatType::CONFIRMED &&
                            m_rfData[i].getSerialNo() == 0U) {
                            if (m_verbose) {
                                LogMessage(LOG_RF, P25_PDU_STR ", ISP, block %u, fmt = $%02X, lastBlock = %u, sap = $%02X, llId = %u",
                                    m_rfData[i].getSerialNo(), m_rfData[i].getFormat(), m_rfData[i].getLastBlock(), m_rfData[i].getSAP(), m_rfData[i].getLLId());

                                if (m_dumpPDUData) {
                                    uint8_t dataBlock[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                                    ::memset(dataBlock, 0xAAU, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                                    m_rfData[i].getData(dataBlock);
                                    Utils::dump(2U, "Data Block", dataBlock, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                                }
                            }

                            m_rfSecondHeader.reset();
                            m_rfSecondHeader.setAckNeeded(true);
                            m_rfSecondHeader.setFormat(m_rfData[i].getFormat());
                            m_rfSecondHeader.setLLId(m_rfData[i].getLLId());
                            m_rfSecondHeader.setSAP(m_rfData[i].getSAP());
                            dstId = m_rfSecondHeader.getLLId();
                            m_rfExtendedAddress = true;
                        }
                        else {
                            if (m_verbose) {
                                LogMessage(LOG_RF, P25_PDU_STR ", ISP, block %u, fmt = $%02X, lastBlock = %u",
                                    (m_rfDataHeader.getFormat() == PDUFormatType::CONFIRMED) ? m_rfData[i].getSerialNo() : m_rfDataBlockCnt, m_rfData[i].getFormat(),
                                    m_rfData[i].getLastBlock());

                                if (m_dumpPDUData) {
                                    uint8_t dataBlock[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                                    ::memset(dataBlock, 0xAAU, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                                    m_rfData[i].getData(dataBlock);
                                    Utils::dump(2U, "Data Block", dataBlock, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                                }
                            }
                        }

                        m_rfData[i].getData(m_pduUserData + dataOffset);
                        dataOffset += (m_rfDataHeader.getFormat() == PDUFormatType::CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
                        m_pduUserDataLength = dataOffset;

                        // only send data blocks across the network, if we're not an AMBT,
                        // an RSP or a registration service
                        if ((m_rfDataHeader.getFormat() != PDUFormatType::AMBT) &&
                            (m_rfDataHeader.getFormat() != PDUFormatType::RSP) &&
                            (m_rfDataHeader.getSAP() != PDUSAP::REG)) {
                            writeNetwork(m_rfDataBlockCnt, buffer, P25_PDU_FEC_LENGTH_BYTES, m_rfData[i].getLastBlock());
                        }

                        m_rfDataBlockCnt++;

                        // is this the last block?
                        if (m_rfData[i].getLastBlock() && m_rfDataBlockCnt == blocksToFollow) {
                            bool crcRet = edac::CRC::checkCRC32(m_pduUserData, m_pduUserDataLength);
                            if (!crcRet) {
                                LogWarning(LOG_RF, P25_PDU_STR ", failed CRC-32 check, blocks %u, len %u", blocksToFollow, m_pduUserDataLength);
                            }
                        }
                    }
                    else {
                        if (m_rfData[i].getFormat() == PDUFormatType::CONFIRMED) {
                            LogWarning(LOG_RF, P25_PDU_STR ", unfixable PDU data (3/4 rate or CRC), block %u", i);

                            // to prevent data block offset errors fill the bad block with 0's
                            uint8_t blankBuf[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                            ::memset(blankBuf, 0x00U, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                            ::memcpy(m_pduUserData + dataOffset, blankBuf, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                            dataOffset += P25_PDU_CONFIRMED_DATA_LENGTH_BYTES;
                            m_pduUserDataLength = dataOffset;
                        }
                        else {
                            LogWarning(LOG_RF, P25_PDU_STR ", unfixable PDU data (1/2 rate or CRC), block %u", i);

                            // to prevent data block offset errors fill the bad block with 0's
                            uint8_t blankBuf[P25_PDU_UNCONFIRMED_LENGTH_BYTES];
                            ::memset(blankBuf, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
                            ::memcpy(m_pduUserData + dataOffset, blankBuf, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
                            dataOffset += P25_PDU_UNCONFIRMED_LENGTH_BYTES;
                            m_pduUserDataLength = dataOffset;
                        }

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
                if (m_rfDataHeader.getFormat() == PDUFormatType::RSP) {
                    if (m_verbose) {
                        LogMessage(LOG_RF, P25_PDU_STR ", ISP, response, fmt = $%02X, rspClass = $%02X, rspType = $%02X, rspStatus = $%02X, llId = %u, srcLlId = %u",
                                m_rfDataHeader.getFormat(), m_rfDataHeader.getResponseClass(), m_rfDataHeader.getResponseType(), m_rfDataHeader.getResponseStatus(),
                                m_rfDataHeader.getLLId(), m_rfDataHeader.getSrcLLId());

                        if (m_rfDataHeader.getResponseClass() == PDUAckClass::ACK && m_rfDataHeader.getResponseType() == PDUAckType::ACK) {
                            LogMessage(LOG_RF, P25_PDU_STR ", ISP, response, OSP ACK, llId = %u",
                                m_rfDataHeader.getLLId());
                        } else {
                            if (m_rfDataHeader.getResponseClass() == PDUAckClass::NACK) {
                                switch (m_rfDataHeader.getResponseType()) {
                                    case PDUAckType::NACK_ILLEGAL:
                                        LogMessage(LOG_RF, P25_PDU_STR ", ISP, response, OSP NACK, illegal format, llId = %u",
                                            m_rfDataHeader.getLLId());
                                        break;
                                    case PDUAckType::NACK_PACKET_CRC:
                                        LogMessage(LOG_RF, P25_PDU_STR ", ISP, response, OSP NACK, packet CRC error, llId = %u",
                                            m_rfDataHeader.getLLId());
                                        break;
                                    case PDUAckType::NACK_SEQ:
                                    case PDUAckType::NACK_OUT_OF_SEQ:
                                        LogMessage(LOG_RF, P25_PDU_STR ", ISP, response, OSP NACK, packet out of sequence, llId = %u",
                                            m_rfDataHeader.getLLId());
                                        break;
                                    case PDUAckType::NACK_UNDELIVERABLE:
                                        LogMessage(LOG_RF, P25_PDU_STR ", ISP, response, OSP NACK, packet undeliverable, llId = %u",
                                            m_rfDataHeader.getLLId());
                                        break;

                                    default:
                                        break;
                                    }
                            }
                        }
                    }

                    if (m_repeatPDU) {
                        if (!m_rfDataHeader.getFullMessage()) {
                            m_rfDataHeader.setSAP(PDUSAP::EXT_ADDR);
                        }

                        writeRF_PDU_Ack_Response(m_rfDataHeader.getResponseClass(), m_rfDataHeader.getResponseType(), m_rfDataHeader.getResponseStatus(), 
                                                 m_rfDataHeader.getLLId(), m_rfDataHeader.getSrcLLId());
                   }
                }
                else {
                    // handle standard P25 service access points
                    switch (m_rfDataHeader.getSAP()) {
                    case PDUSAP::REG:
                    {
                        uint8_t regType = (m_pduUserData[0] >> 4) & 0x0F;
                        switch (regType) {
                        case PDURegType::CONNECT:
                        {
                            uint32_t llId = (m_pduUserData[1U] << 16) + (m_pduUserData[2U] << 8) + m_pduUserData[3U];
                            ulong64_t ipAddr = (m_pduUserData[8U] << 24) + (m_pduUserData[9U] << 16) +
                                (m_pduUserData[10U] << 8) + m_pduUserData[11U];

                            if (m_verbose) {
                                LogMessage(LOG_RF, P25_PDU_STR ", CONNECT (Registration Request Connect), llId = %u, ipAddr = %s", llId, __IP_FROM_ULONG(ipAddr).c_str());
                            }

                            m_connQueueTable[llId] = std::make_tuple(m_rfDataHeader.getMFId(), ipAddr);

                            m_connTimerTable[llId] = Timer(1000U, CONN_WAIT_TIMEOUT);
                            m_connTimerTable[llId].start();
                        }
                        break;
                        case PDURegType::DISCONNECT:
                        {
                            uint32_t llId = (m_pduUserData[1U] << 16) + (m_pduUserData[2U] << 8) + m_pduUserData[3U];

                            if (m_verbose) {
                                LogMessage(LOG_RF, P25_PDU_STR ", DISCONNECT (Registration Request Disconnect), llId = %u", llId);
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
                    case PDUSAP::SNDCP_CTRL_DATA:
                    {
                        if (m_verbose) {
                            LogMessage(LOG_RF, P25_PDU_STR ", SNDCP_CTRL_DATA (SNDCP Control Data), lco = $%02X, blocksToFollow = %u",
                                m_rfDataHeader.getAMBTOpcode(), m_rfDataHeader.getBlocksToFollow());
                        }

                        processSNDCPControl();
                    }
                    break;
                    case PDUSAP::TRUNK_CTRL:
                    {
                        if (m_verbose) {
                            LogMessage(LOG_RF, P25_PDU_STR ", TRUNK_CTRL (Alternate MBT Packet), lco = $%02X, blocksToFollow = %u",
                                m_rfDataHeader.getAMBTOpcode(), m_rfDataHeader.getBlocksToFollow());
                        }

                        m_p25->m_control->processMBT(m_rfDataHeader, m_rfData);
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

/* Process a data frame from the network. */

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

        ::memset(m_netPDU, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

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
            LogMessage(LOG_NET, P25_PDU_STR ", ack = %u, outbound = %u, fmt = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, packetLength = %u, n = %u, seqNo = %u, hdrOffset = %u, llId = %u",
                m_netDataHeader.getAckNeeded(), m_netDataHeader.getOutbound(), m_netDataHeader.getFormat(), m_netDataHeader.getSAP(), m_netDataHeader.getFullMessage(),
                m_netDataHeader.getBlocksToFollow(), m_netDataHeader.getPadLength(), m_netDataHeader.getPacketLength(), m_netDataHeader.getNs(), m_netDataHeader.getFSN(),
                m_netDataHeader.getHeaderOffset(), m_netDataHeader.getLLId());
        }

        // make sure we don't get a PDU with more blocks then we support
        if (m_netDataHeader.getBlocksToFollow() >= P25_MAX_PDU_BLOCKS) {
            LogError(LOG_NET, P25_PDU_STR ", too many PDU blocks to process, %u > %u", m_netDataHeader.getBlocksToFollow(), P25_MAX_PDU_BLOCKS);

            m_netDataHeader.reset();
            m_netSecondHeader.reset();
            m_netDataOffset = 0U;
            m_netDataBlockCnt = 0U;
            m_netPDUCount = 0U;
            m_p25->m_netState = RS_NET_IDLE;
            return false;
        }

        // if we're a dedicated CC or in control only mode, we only want to handle AMBTs. Otherwise return
        if ((m_p25->m_dedicatedControl || m_p25->m_controlOnly) && m_netDataHeader.getFormat() != PDUFormatType::AMBT) {
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
        ::memcpy(m_netPDU + m_netDataOffset, data + 24U, blockLength);
        m_netDataOffset += blockLength;
        m_netPDUCount++;
        m_netDataBlockCnt++;

        if (m_netDataBlockCnt >= m_netDataHeader.getBlocksToFollow()) {
            uint32_t blocksToFollow = m_netDataHeader.getBlocksToFollow();
            uint32_t offset = 0U;

            uint8_t buffer[P25_PDU_FEC_LENGTH_BYTES];

            // process second header if we're using enhanced addressing
            if (m_netDataHeader.getSAP() == PDUSAP::EXT_ADDR &&
                m_netDataHeader.getFormat() == PDUFormatType::UNCONFIRMED) {
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
                    LogMessage(LOG_NET, P25_PDU_STR ", fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
                        m_netSecondHeader.getFormat(), m_netSecondHeader.getMFId(), m_netSecondHeader.getSAP(), m_netSecondHeader.getFullMessage(),
                        m_netSecondHeader.getBlocksToFollow(), m_netSecondHeader.getPadLength(), m_netSecondHeader.getNs(), m_netSecondHeader.getFSN(), m_netSecondHeader.getLastFragment(),
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
                    if ((m_netDataHeader.getFormat() == PDUFormatType::CONFIRMED) || (m_netDataHeader.getFormat() == PDUFormatType::UNCONFIRMED)) {
                        if ((m_netDataBlockCnt + 1U) == blocksToFollow) {
                            m_netData[i].setLastBlock(true);
                        }
                    }

                    // are we processing extended address data from the first block?
                    if (m_netDataHeader.getSAP() == PDUSAP::EXT_ADDR && m_netDataHeader.getFormat() == PDUFormatType::CONFIRMED &&
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
                            (m_netDataHeader.getFormat() == PDUFormatType::CONFIRMED) ? m_netData[i].getSerialNo() : m_netDataBlockCnt, m_netData[i].getFormat(),
                            m_netData[i].getLastBlock());
                    }

                    m_netData[i].getData(m_pduUserData + dataOffset);
                    dataOffset += (m_rfDataHeader.getFormat() == PDUFormatType::CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
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
                    if (m_netData[i].getFormat() == PDUFormatType::CONFIRMED)
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

void Data::writeRF_PDU_User(data::DataHeader& dataHeader, data::DataHeader& secondHeader, bool useSecondHeader, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    uint32_t bitLength = ((dataHeader.getBlocksToFollow() + 1U) * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
    uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

    uint8_t data[bitLength / 8U];
    ::memset(data, 0x00U, bitLength / 8U);
    uint8_t block[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    uint32_t blocksToFollow = dataHeader.getBlocksToFollow();

    if (m_verbose) {
        LogMessage(LOG_RF, P25_PDU_STR ", OSP, ack = %u, outbound = %u, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
            dataHeader.getAckNeeded(), dataHeader.getOutbound(), dataHeader.getFormat(), dataHeader.getMFId(), dataHeader.getSAP(), dataHeader.getFullMessage(),
            dataHeader.getBlocksToFollow(), dataHeader.getPadLength(), dataHeader.getNs(), dataHeader.getFSN(), dataHeader.getLastFragment(),
            dataHeader.getHeaderOffset(), dataHeader.getLLId());
    }

    // generate the PDU header and 1/2 rate Trellis
    dataHeader.encode(block);
    Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);
    offset += P25_PDU_FEC_LENGTH_BITS;

    uint32_t dataOffset = 0U;

    // generate the second PDU header
    if (useSecondHeader) {
        secondHeader.encode(m_pduUserData, true);

        ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
        secondHeader.encode(block);
        Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

        bitLength += P25_PDU_FEC_LENGTH_BITS;
        offset += P25_PDU_FEC_LENGTH_BITS;
        dataOffset += P25_PDU_HEADER_LENGTH_BYTES;
        blocksToFollow--;

        if (m_verbose) {
            LogMessage(LOG_RF, P25_PDU_STR ", OSP, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
                secondHeader.getFormat(), secondHeader.getMFId(), secondHeader.getSAP(), secondHeader.getFullMessage(),
                secondHeader.getBlocksToFollow(), secondHeader.getPadLength(), secondHeader.getNs(), secondHeader.getFSN(), secondHeader.getLastFragment(),
                secondHeader.getHeaderOffset(), secondHeader.getLLId());
        }
    }

    if (dataHeader.getFormat() != PDUFormatType::AMBT) {
        edac::CRC::addCRC32(pduUserData, m_pduUserDataLength);
    }

    // generate the PDU data
    for (uint32_t i = 0U; i < blocksToFollow; i++) {
        DataBlock dataBlock = DataBlock();
        dataBlock.setFormat((useSecondHeader) ? secondHeader : dataHeader);
        dataBlock.setSerialNo(i);
        dataBlock.setData(pduUserData + dataOffset);

        // are we processing extended address data from the first block?
        if (dataHeader.getSAP() == PDUSAP::EXT_ADDR && dataHeader.getFormat() == PDUFormatType::CONFIRMED &&
            dataBlock.getSerialNo() == 0U) {
            if (m_verbose) {
                LogMessage(LOG_RF, P25_PDU_STR ", OSP, block %u, fmt = $%02X, lastBlock = %u, sap = $%02X, llId = %u",
                    dataBlock.getSerialNo(), dataBlock.getFormat(), dataBlock.getLastBlock(), dataBlock.getSAP(), dataBlock.getLLId());

                if (m_dumpPDUData) {
                    uint8_t rawDataBlock[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                    ::memset(rawDataBlock, 0xAAU, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                    dataBlock.getData(rawDataBlock);
                    Utils::dump(2U, "Data Block", rawDataBlock, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                }
            }
        }
        else {
            if (m_verbose) {
                LogMessage(LOG_RF, P25_PDU_STR ", OSP, block %u, fmt = $%02X, lastBlock = %u",
                    (dataHeader.getFormat() == PDUFormatType::CONFIRMED) ? dataBlock.getSerialNo() : i, dataBlock.getFormat(),
                    dataBlock.getLastBlock());

                if (m_dumpPDUData) {
                    uint8_t rawDataBlock[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                    ::memset(rawDataBlock, 0xAAU, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                    dataBlock.getData(rawDataBlock);
                    Utils::dump(2U, "Data Block", rawDataBlock, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                }
            }
        }

        ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
        dataBlock.encode(block);
        Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

        offset += P25_PDU_FEC_LENGTH_BITS;
        dataOffset += (dataHeader.getFormat() == PDUFormatType::CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
    }

    writeRF_PDU(data, bitLength);
}

/* Updates the processor by the passed number of milliseconds. */

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
            LogWarning(LOG_RF, P25_PDU_STR ", DENY (Registration Response Deny), llId = %u, ipAddr = %s", llId, __IP_FROM_ULONG(ipAddr).c_str());
            writeRF_PDU_Reg_Response(PDURegType::DENY, mfId, llId, ipAddr);
        }
        else {
            if (!hasLLIdFNEReg(llId)) {
                // update dynamic FNE registration table entry
                m_fneRegTable[llId] = ipAddr;
            }

            if (m_verbose) {
                LogMessage(LOG_RF, P25_PDU_STR ", ACCPT (Registration Response Accept), llId = %u, ipAddr = %s", llId, __IP_FROM_ULONG(ipAddr).c_str());
            }

            writeRF_PDU_Reg_Response(PDURegType::ACCPT, mfId, llId, ipAddr);
        }

        m_connQueueTable.erase(llId);
    }

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
                            LogMessage(LOG_RF, P25_PDU_STR ", SNDCP, llId = %u, state = %u", llId, (uint8_t)state);
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
                            LogMessage(LOG_RF, P25_TDULC_STR ", CALL_TERM (Call Termination), llId = %u", llId);
                        }

                        std::unique_ptr<lc::TDULC> lc = std::make_unique<lc::tdulc::LC_CALL_TERM>();
                        m_p25->m_control->writeRF_TDULC(lc.get(), true);

                        if (m_p25->m_notifyCC) {
                            m_p25->notifyCC_ReleaseGrant(llId);
                        }
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
            LogMessage(LOG_RF, P25_PDU_STR ", SNDCP, first initialize, llId = %u, state = %u", llId, (uint8_t)SNDCPState::IDLE);
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
            LogMessage(LOG_RF, P25_PDU_STR ", SNDCP, reset, llId = %u, state = %u", llId, (uint8_t)m_sndcpStateTable[llId]);
        }

        m_sndcpStateTable[llId] = SNDCPState::CLOSED;
        m_sndcpReadyTimers[llId].stop();
        m_sndcpStandbyTimers[llId].stop();

        if (callTerm) {
            if (m_verbose) {
                LogMessage(LOG_RF, P25_TDULC_STR ", CALL_TERM (Call Termination), llId = %u", llId);
            }

            std::unique_ptr<lc::TDULC> lc = std::make_unique<lc::tdulc::LC_CALL_TERM>();
            m_p25->m_control->writeRF_TDULC(lc.get(), true);

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
    m_sndcpStateTable(),
    m_sndcpReadyTimers(),
    m_sndcpStandbyTimers(),
    m_dumpPDUData(dumpPDUData),
    m_repeatPDU(repeatPDU),
    m_verbose(verbose),
    m_debug(debug)
{
    m_rfData = new data::DataBlock[P25_MAX_PDU_BLOCKS];

    m_rfPDU = new uint8_t[P25_PDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(m_rfPDU, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

    m_netData = new data::DataBlock[P25_MAX_PDU_BLOCKS];

    m_netPDU = new uint8_t[P25_PDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(m_netPDU, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

    m_pduUserData = new uint8_t[P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
    ::memset(m_pduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);

    m_fneRegTable.clear();
    m_connQueueTable.clear();
    m_connTimerTable.clear();

    m_sndcpStateTable.clear();
    m_sndcpReadyTimers.clear();
    m_sndcpStandbyTimers.clear();
}

/* Finalizes a instance of the Data class. */

Data::~Data()
{
    delete[] m_rfData;
    delete[] m_netData;
    delete[] m_rfPDU;
    delete[] m_netPDU;
    delete[] m_pduUserData;
}

/* Helper used to process SNDCP control data from PDU data. */

bool Data::processSNDCPControl()
{
    if (!m_p25->m_sndcpSupport) {
        return false;
    }

    uint8_t data[P25_PDU_UNCONFIRMED_LENGTH_BYTES];
    ::memset(data, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES);

    std::unique_ptr<sndcp::SNDCPPacket> packet = SNDCPFactory::create(m_pduUserData);
    if (packet == nullptr) {
        LogWarning(LOG_RF, P25_PDU_STR ", undecodable SNDCP packet");
        return false;
    }

    uint32_t llId = m_rfDataHeader.getLLId();

    switch (packet->getPDUType()) {
        case SNDCP_PDUType::ACT_TDS_CTX:
        {
            SNDCPCtxActRequest* isp = static_cast<SNDCPCtxActRequest*>(packet.get());
            if (m_verbose) {
                LogMessage(LOG_RF, P25_PDU_STR ", SNDCP context activation request, llId = %u, ipAddr = %08lX, nat = $%02X, dsut = $%02X", llId,
                    isp->getIPAddress(), isp->getNAT(), isp->getDSUT());
            }

            DataHeader rspHeader = DataHeader();
            rspHeader.setFormat(PDUFormatType::CONFIRMED);
            rspHeader.setMFId(MFG_STANDARD);
            rspHeader.setAckNeeded(true);
            rspHeader.setOutbound(true);
            rspHeader.setSAP(PDUSAP::SNDCP_CTRL_DATA);
            rspHeader.setLLId(llId);
            rspHeader.setBlocksToFollow(1U);

            // which network address type is this?
            switch (isp->getNAT()) {
                case SNDCPNAT::IPV4_STATIC_ADDR:
                {
                    std::unique_ptr<SNDCPCtxActReject> osp = std::make_unique<SNDCPCtxActReject>();
                    osp->setNSAPI(packet->getNSAPI());
                    osp->setRejectCode(SNDCPRejectReason::STATIC_IP_ALLOCATION_UNSUPPORTED);

                    osp->encode(data);
                    writeRF_PDU_User(rspHeader, rspHeader, false, data);

                    sndcpReset(llId, true);

                    // TODO TODO TODO
                }
                break;

                case SNDCPNAT::IPV4_DYN_ADDR:
                {
                    std::unique_ptr<SNDCPCtxActReject> osp = std::make_unique<SNDCPCtxActReject>();
                    osp->setNSAPI(packet->getNSAPI());
                    osp->setRejectCode(SNDCPRejectReason::DYN_IP_ALLOCATION_UNSUPPORTED);

                    osp->encode(data);
                    writeRF_PDU_User(rspHeader, rspHeader, false, data);

                    sndcpReset(llId, true);

                    // TODO TODO TODO
                }
                break;

                default:
                {
                    std::unique_ptr<SNDCPCtxActReject> osp = std::make_unique<SNDCPCtxActReject>();
                    osp->setNSAPI(packet->getNSAPI());
                    osp->setRejectCode(SNDCPRejectReason::ANY_REASON);

                    osp->encode(data);
                    writeRF_PDU_User(rspHeader, rspHeader, false, data);

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
                LogMessage(LOG_RF, P25_PDU_STR ", SNDCP context deactivation request, llId = %u, deactType = %02X", llId,
                    isp->getDeactType());
            }

            writeRF_PDU_Ack_Response(PDUAckClass::ACK, PDUAckType::ACK, 0U, llId);
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

void Data::writeNetwork(const uint8_t currentBlock, const uint8_t *data, uint32_t len, bool lastBlock)
{
    assert(data != nullptr);

    if (m_p25->m_network == nullptr)
        return;

    if (m_p25->m_rfTimeout.isRunning() && m_p25->m_rfTimeout.hasExpired())
        return;

    m_p25->m_network->writeP25PDU(m_rfDataHeader, currentBlock, data, len, lastBlock);
}

/* Helper to write a P25 PDU packet. */

void Data::writeRF_PDU(const uint8_t* pdu, uint32_t bitLength, bool noNulls)
{
    assert(pdu != nullptr);
    assert(bitLength > 0U);

    uint8_t data[P25_PDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(data, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

    // Add the data
    uint32_t newBitLength = P25Utils::encode(pdu, data + 2U, bitLength);
    uint32_t newByteLength = newBitLength / 8U;
    if ((newBitLength % 8U) > 0U)
        newByteLength++;

    // Regenerate Sync
    Sync::addP25Sync(data + 2U);

    // Regenerate NID
    m_p25->m_nid.encode(data + 2U, DUID::PDU);

    // Add busy bits
    P25Utils::addBusyBits(data + 2U, newBitLength, true, false);

    // Add idle bits
    P25Utils::addIdleBits(data + 2U, newBitLength, true, true);

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

/* Helper to write a network P25 PDU packet. */

void Data::writeNet_PDU_Buffered()
{
    uint32_t bitLength = ((m_netDataHeader.getBlocksToFollow() + 1U) * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
    uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

    uint8_t data[bitLength / 8U];
    ::memset(data, 0x00U, bitLength / 8U);
    uint8_t block[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    uint32_t blocksToFollow = m_netDataHeader.getBlocksToFollow();

    if (m_verbose) {
        LogMessage(LOG_NET, P25_PDU_STR ", OSP, ack = %u, outbound = %u, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
            m_netDataHeader.getAckNeeded(), m_netDataHeader.getOutbound(), m_netDataHeader.getFormat(), m_netDataHeader.getMFId(), m_netDataHeader.getSAP(), m_netDataHeader.getFullMessage(),
            m_netDataHeader.getBlocksToFollow(), m_netDataHeader.getPadLength(), m_netDataHeader.getNs(), m_netDataHeader.getFSN(), m_netDataHeader.getLastFragment(),
            m_netDataHeader.getHeaderOffset(), m_netDataHeader.getLLId());
    }

    // generate the PDU header and 1/2 rate Trellis
    m_netDataHeader.encode(block);
    Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);
    offset += P25_PDU_FEC_LENGTH_BITS;

    uint32_t dataOffset = 0U;

    // generate the second PDU header
    if (m_netUseSecondHeader) {
        m_netSecondHeader.encode(m_pduUserData, true);

        ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
        m_netSecondHeader.encode(block);
        Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

        bitLength += P25_PDU_FEC_LENGTH_BITS;
        offset += P25_PDU_FEC_LENGTH_BITS;
        dataOffset += P25_PDU_HEADER_LENGTH_BYTES;
        blocksToFollow--;

        if (m_verbose) {
            LogMessage(LOG_NET, P25_PDU_STR ", OSP, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
                m_netSecondHeader.getFormat(), m_netSecondHeader.getMFId(), m_netSecondHeader.getSAP(), m_netSecondHeader.getFullMessage(),
                m_netSecondHeader.getBlocksToFollow(), m_netSecondHeader.getPadLength(), m_netSecondHeader.getNs(), m_netSecondHeader.getFSN(), m_netSecondHeader.getLastFragment(),
                m_netSecondHeader.getHeaderOffset(), m_netSecondHeader.getLLId());
        }
    }

    edac::CRC::addCRC32(m_pduUserData, m_pduUserDataLength);

    // generate the PDU data
    for (uint32_t i = 0U; i < blocksToFollow; i++) {
        m_netData[i].setFormat((m_netUseSecondHeader) ? m_netSecondHeader : m_netDataHeader);
        m_netData[i].setSerialNo(i);
        m_netData[i].setData(m_pduUserData + dataOffset);

        ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
        m_netData[i].encode(block);
        Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

        // are we processing extended address data from the first block?
        if (m_netDataHeader.getSAP() == PDUSAP::EXT_ADDR && m_netDataHeader.getFormat() == PDUFormatType::CONFIRMED &&
            m_netData[i].getSerialNo() == 0U) {
            if (m_verbose) {
                LogMessage(LOG_NET, P25_PDU_STR ", OSP, block %u, fmt = $%02X, lastBlock = %u, sap = $%02X, llId = %u",
                    m_netData[i].getSerialNo(), m_netData[i].getFormat(), m_netData[i].getLastBlock(), m_netData[i].getSAP(), m_netData[i].getLLId());

                if (m_dumpPDUData) {
                    uint8_t dataBlock[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                    ::memset(dataBlock, 0xAAU, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                    m_netData[i].getData(dataBlock);
                    Utils::dump(2U, "Data Block", dataBlock, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                }
            }
        }
        else {
            if (m_verbose) {
                LogMessage(LOG_NET, P25_PDU_STR ", OSP, block %u, fmt = $%02X, lastBlock = %u",
                    (m_netDataHeader.getFormat() == PDUFormatType::CONFIRMED) ? m_netData[i].getSerialNo() : i, m_netData[i].getFormat(),
                    m_netData[i].getLastBlock());

                if (m_dumpPDUData) {
                    uint8_t dataBlock[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                    ::memset(dataBlock, 0xAAU, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                    m_netData[i].getData(dataBlock);
                    Utils::dump(2U, "Data Block", dataBlock, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                }
            }
        }

        offset += P25_PDU_FEC_LENGTH_BITS;
        dataOffset += (m_netDataHeader.getFormat() == PDUFormatType::CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
    }

    writeRF_PDU(data, bitLength);
}

/* Helper to re-write a received P25 PDU packet. */

void Data::writeRF_PDU_Buffered()
{
    uint32_t bitLength = ((m_rfDataHeader.getBlocksToFollow() + 1U) * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
    uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

    uint8_t data[bitLength / 8U];
    ::memset(data, 0x00U, bitLength / 8U);
    uint8_t block[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    uint32_t blocksToFollow = m_rfDataHeader.getBlocksToFollow();

    if (m_verbose) {
        LogMessage(LOG_RF, P25_PDU_STR ", OSP, ack = %u, outbound = %u, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
            m_rfDataHeader.getAckNeeded(), m_rfDataHeader.getOutbound(), m_rfDataHeader.getFormat(), m_rfDataHeader.getMFId(), m_rfDataHeader.getSAP(), m_rfDataHeader.getFullMessage(),
            m_rfDataHeader.getBlocksToFollow(), m_rfDataHeader.getPadLength(), m_rfDataHeader.getNs(), m_rfDataHeader.getFSN(), m_rfDataHeader.getLastFragment(),
            m_rfDataHeader.getHeaderOffset(), m_rfDataHeader.getLLId());
    }

    // generate the PDU header and 1/2 rate Trellis
    m_rfDataHeader.encode(block);
    Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);
    offset += P25_PDU_FEC_LENGTH_BITS;

    uint32_t dataOffset = 0U;

    // generate the second PDU header
    if (m_rfUseSecondHeader) {
        m_rfSecondHeader.encode(m_pduUserData, true);

        ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
        m_rfSecondHeader.encode(block);
        Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

        bitLength += P25_PDU_FEC_LENGTH_BITS;
        offset += P25_PDU_FEC_LENGTH_BITS;
        dataOffset += P25_PDU_HEADER_LENGTH_BYTES;
        blocksToFollow--;

        if (m_verbose) {
            LogMessage(LOG_RF, P25_PDU_STR ", OSP, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
                m_rfSecondHeader.getFormat(), m_rfSecondHeader.getMFId(), m_rfSecondHeader.getSAP(), m_rfSecondHeader.getFullMessage(),
                m_rfSecondHeader.getBlocksToFollow(), m_rfSecondHeader.getPadLength(), m_rfSecondHeader.getNs(), m_rfSecondHeader.getFSN(), m_rfSecondHeader.getLastFragment(),
                m_rfSecondHeader.getHeaderOffset(), m_rfSecondHeader.getLLId());
        }
    }

    edac::CRC::addCRC32(m_pduUserData, m_pduUserDataLength);

    // generate the PDU data
    for (uint32_t i = 0U; i < blocksToFollow; i++) {
        m_rfData[i].setFormat((m_rfUseSecondHeader) ? m_rfSecondHeader : m_rfDataHeader);
        m_rfData[i].setSerialNo(i);
        m_rfData[i].setData(m_pduUserData + dataOffset);

        // are we processing extended address data from the first block?
        if (m_rfDataHeader.getSAP() == PDUSAP::EXT_ADDR && m_rfDataHeader.getFormat() == PDUFormatType::CONFIRMED &&
            m_rfData[i].getSerialNo() == 0U) {
            if (m_verbose) {
                LogMessage(LOG_RF, P25_PDU_STR ", OSP, block %u, fmt = $%02X, lastBlock = %u, sap = $%02X, llId = %u",
                    m_rfData[i].getSerialNo(), m_rfData[i].getFormat(), m_rfData[i].getLastBlock(), m_rfData[i].getSAP(), m_rfData[i].getLLId());

                if (m_dumpPDUData) {
                    uint8_t dataBlock[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                    ::memset(dataBlock, 0xAAU, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                    m_rfData[i].getData(dataBlock);
                    Utils::dump(2U, "Data Block", dataBlock, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                }
            }
        }
        else {
            if (m_verbose) {
                LogMessage(LOG_RF, P25_PDU_STR ", OSP, block %u, fmt = $%02X, lastBlock = %u",
                    (m_rfDataHeader.getFormat() == PDUFormatType::CONFIRMED) ? m_rfData[i].getSerialNo() : i, m_rfData[i].getFormat(),
                    m_rfData[i].getLastBlock());

                if (m_dumpPDUData) {
                    uint8_t dataBlock[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                    ::memset(dataBlock, 0xAAU, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                    m_rfData[i].getData(dataBlock);
                    Utils::dump(2U, "Data Block", dataBlock, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                }
            }
        }

        ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
        m_rfData[i].encode(block);
        Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

        offset += P25_PDU_FEC_LENGTH_BITS;
        dataOffset += (m_rfDataHeader.getFormat() == PDUFormatType::CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
    }

    writeRF_PDU(data, bitLength);
}

/* Helper to write a PDU registration response. */

void Data::writeRF_PDU_Reg_Response(uint8_t regType, uint8_t mfId, uint32_t llId, ulong64_t ipAddr)
{
    if ((regType != PDURegType::ACCPT) && (regType != PDURegType::DENY))
        return;

    uint32_t bitLength = (2U * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
    uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

    uint8_t data[bitLength / 8U];
    ::memset(data, 0x00U, bitLength / 8U);
    uint8_t block[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    DataHeader rspHeader = DataHeader();
    rspHeader.setFormat(PDUFormatType::CONFIRMED);
    rspHeader.setMFId(mfId);
    rspHeader.setAckNeeded(true);
    rspHeader.setOutbound(true);
    rspHeader.setSAP(PDUSAP::REG);
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
    if (regType == PDURegType::ACCPT) {
        rspData[8U] = (ipAddr >> 24) & 0xFFU;                                   // IP Address
        rspData[9U] = (ipAddr >> 16) & 0xFFU;
        rspData[10U] = (ipAddr >> 8) & 0xFFU;
        rspData[11U] = (ipAddr >> 0) & 0xFFU;
    }

    edac::CRC::addCRC32(rspData, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);

    // Generate the PDU data
    DataBlock rspBlock = DataBlock();
    rspBlock.setFormat(PDUFormatType::CONFIRMED);
    rspBlock.setSerialNo(0U);
    rspBlock.setData(rspData);

    ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
    rspBlock.encode(block);
    Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

    writeRF_PDU(data, bitLength);
}

/* Helper to write a PDU acknowledge response. */

void Data::writeRF_PDU_Ack_Response(uint8_t ackClass, uint8_t ackType, uint8_t ackStatus, uint32_t llId, uint32_t srcLlId, bool noNulls)
{
    if (ackClass == PDUAckClass::ACK && ackType != PDUAckType::ACK)
        return;

    uint32_t bitLength = (1U * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
    uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

    uint8_t data[bitLength / 8U];
    ::memset(data, 0x00U, bitLength / 8U);
    uint8_t block[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    DataHeader rspHeader = DataHeader();
    rspHeader.setFormat(PDUFormatType::RSP);
    rspHeader.setMFId(m_rfDataHeader.getMFId());
    rspHeader.setOutbound(true);
    rspHeader.setResponseClass(ackClass);
    rspHeader.setResponseType(ackType);
    rspHeader.setResponseStatus(ackStatus);
    rspHeader.setLLId(llId);
    if (m_rfDataHeader.getSAP() == PDUSAP::EXT_ADDR) {
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
        LogMessage(LOG_RF, P25_PDU_STR ", OSP, response, ackClass = $%02X, ackType = $%02X, llId = %u, srcLLId = %u",
            rspHeader.getResponseClass(), rspHeader.getResponseType(), rspHeader.getLLId(), rspHeader.getSrcLLId());
    }

    writeRF_PDU(data, bitLength, noNulls);
}
