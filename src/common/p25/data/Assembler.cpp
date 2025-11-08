// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/data/Assembler.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::data;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

bool Assembler::s_dumpPDUData = false;
bool Assembler::s_verbose = false;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the Assembler class. */

Assembler::Assembler() :
    dataBlocks(nullptr),
    dataHeader(),
    m_extendedAddress(false),
    m_auxiliaryES(false),
    m_dataBlockCnt(0U),
    m_undecodableBlockCnt(0U),
    m_packetCRCFailed(false),
    m_complete(false),
    m_pduUserData(nullptr),
    m_pduUserDataLength(0U),
    m_blockCount(0U),
    m_dataOffset(0U),
    m_usingCustomWriter(false),
    m_blockWriter(nullptr)
{
    dataBlocks = new data::DataBlock[P25_MAX_PDU_BLOCKS];

    m_pduUserData = new uint8_t[P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
    ::memset(m_pduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);

    m_rawPDU = new uint8_t[P25_PDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(m_rawPDU, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);
}

/* Finalizes a instance of the Assembler class. */

Assembler::~Assembler()
{
    if (dataBlocks != nullptr)
        delete[] dataBlocks;
    if (m_pduUserData != nullptr)
        delete[] m_pduUserData;
}

/* Helper to disassemble a P25 PDU frame into user data. */

bool Assembler::disassemble(const uint8_t* pduBlock, uint32_t blockLength, bool resetState)
{
    assert(pduBlock != nullptr);

    if (resetState) {
        resetDisassemblyState();
    }

#if DEBUG_P25_PDU_DATA
    Utils::dump(1U, "P25, PDU Disassembler Block", pduBlock, blockLength);
#endif

    UInt8Array dataArray = std::make_unique<uint8_t[]>(P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);
    uint8_t* data = dataArray.get();
    ::memset(data, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);

    if (m_blockCount == 0U) {
        bool ret = dataHeader.decode(pduBlock);
        if (!ret) {
            LogWarning(LOG_P25, P25_PDU_STR ", unfixable RF 1/2 rate header data");
            Utils::dump(1U, "P25, Unfixable PDU Data", pduBlock, P25_PDU_FEC_LENGTH_BYTES);

            resetDisassemblyState();
            return false;
        }

        if (s_verbose) {
            LogInfoEx(LOG_P25, P25_PDU_STR ", ISP, ack = %u, outbound = %u, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, packetLength = %u, S = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
                dataHeader.getAckNeeded(), dataHeader.getOutbound(), dataHeader.getFormat(), dataHeader.getMFId(), dataHeader.getSAP(), dataHeader.getFullMessage(),
                dataHeader.getBlocksToFollow(), dataHeader.getPadLength(), dataHeader.getPacketLength(), dataHeader.getSynchronize(), dataHeader.getNs(), dataHeader.getFSN(), dataHeader.getLastFragment(),
                dataHeader.getHeaderOffset(), dataHeader.getLLId());
        }

        // make sure we don't get a PDU with more blocks then we support
        if (dataHeader.getBlocksToFollow() >= P25_MAX_PDU_BLOCKS) {
            LogError(LOG_P25, P25_PDU_STR ", ISP, too many PDU blocks to process, %u > %u", dataHeader.getBlocksToFollow(), P25_MAX_PDU_BLOCKS);

            resetDisassemblyState();
            return false;
        }

        m_blockCount++;
        m_complete = false;
        return true;
    }
    else {
        ::memcpy(m_rawPDU + ((m_blockCount - 1U) * blockLength), pduBlock, blockLength);
        m_dataOffset += blockLength;
        m_blockCount++;

        if (m_blockCount - 1U >= dataHeader.getBlocksToFollow()) {
#if DEBUG_P25_PDU_DATA
            LogDebugEx(LOG_P25, "Assembler::disassemble()", "complete PDU, blocksToFollow = %u, blockCount = %u", dataHeader.getBlocksToFollow(), m_blockCount);
            Utils::dump(1U, "Assembler::disassemble() rawPDU", m_rawPDU, m_dataOffset);
#endif
            uint32_t blocksToFollow = dataHeader.getBlocksToFollow();
            uint32_t offset = 0U;
            uint32_t dataOffset = 0U;

            uint8_t buffer[P25_PDU_FEC_LENGTH_BYTES];

            m_dataBlockCnt = 0U;

            // process all blocks in the data stream
            // if the primary header has a header offset ensure data if offset by that amount 
            if (dataHeader.getHeaderOffset() > 0U) {
                offset += dataHeader.getHeaderOffset();
            }

            uint32_t packetLength = dataHeader.getPacketLength();
            uint32_t padLength = dataHeader.getPadLength();
            uint32_t secondHeaderOffset = 0U;

            // decode data blocks
            for (uint32_t i = 0U; i < blocksToFollow; i++) {
                ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
                ::memcpy(buffer, m_rawPDU + offset, P25_PDU_FEC_LENGTH_BYTES);

                bool ret = dataBlocks[i].decode(buffer, dataHeader);
                if (ret) {
                    // if we are getting unconfirmed or confirmed blocks, and if we've reached the total number of blocks
                    // set this block as the last block for full packet CRC
                    if ((dataHeader.getFormat() == PDUFormatType::CONFIRMED) || (dataHeader.getFormat() == PDUFormatType::UNCONFIRMED)) {
                        if ((m_dataBlockCnt + 1U) == blocksToFollow) {
                            dataBlocks[i].setLastBlock(true);
                        }
                    }

                    // fake data block serial number for unconfirmed mode
                    if (dataHeader.getFormat() == PDUFormatType::UNCONFIRMED && dataBlocks[i].getSerialNo() == 0U)
                        dataBlocks[i].setSerialNo(i);

                    // are we processing extended address data from the first block?
                    if (dataHeader.getSAP() == PDUSAP::EXT_ADDR && dataBlocks[i].getSerialNo() == 0U) {
                        uint8_t secondHeader[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                        ::memset(secondHeader, 0x00U, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                        dataBlocks[i].getData(secondHeader);

                        dataHeader.decodeExtAddr(secondHeader);
                        if (s_verbose) {
                            LogInfoEx(LOG_P25, P25_PDU_STR ", ISP, block %u, fmt = $%02X, lastBlock = %u, sap = $%02X, srcLlId = %u",
                                dataBlocks[i].getSerialNo(), dataBlocks[i].getFormat(), dataBlocks[i].getLastBlock(), dataHeader.getEXSAP(), dataHeader.getSrcLLId());
                        }

                        m_extendedAddress = true;
                        if (dataHeader.getFormat() == PDUFormatType::CONFIRMED)
                            secondHeaderOffset += 4U;
                        else
                            secondHeaderOffset += P25_PDU_HEADER_LENGTH_BYTES;
                    }
                    else {
                        // are we processing auxiliary ES data from the first block?
                        if ((dataHeader.getSAP() == PDUSAP::ENC_USER_DATA || dataHeader.getSAP() == PDUSAP::ENC_KMM) &&
                            dataBlocks[i].getSerialNo() == 0U) {
                            uint8_t secondHeader[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                            ::memset(secondHeader, 0x00U, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                            dataBlocks[i].getData(secondHeader);

                            dataHeader.decodeAuxES(secondHeader);
                            if (s_verbose) {
                                LogInfoEx(LOG_P25, P25_PDU_STR ", ISP, block %u, fmt = $%02X, lastBlock = %u, sap = $%02X, algoId = $%02X, kId = $%04X",
                                    dataBlocks[i].getSerialNo(), dataBlocks[i].getFormat(), dataBlocks[i].getLastBlock(), dataHeader.getEXSAP(),
                                    dataHeader.getAlgId(), dataHeader.getKId());

                                if (dataHeader.getAlgId() != ALGO_UNENCRYPT) {
                                    uint8_t mi[MI_LENGTH_BYTES];
                                    ::memset(mi, 0x00U, MI_LENGTH_BYTES);

                                    dataHeader.getMI(mi);

                                    LogInfoEx(LOG_P25, P25_PDU_STR ", ISP, Enc Sync, block %u, MI = %02X %02X %02X %02X %02X %02X %02X %02X %02X", 
                                        dataBlocks[i].getSerialNo(), mi[0U], mi[1U], mi[2U], mi[3U], mi[4U], mi[5U], mi[6U], mi[7U], mi[8U]);
                                }
                            }

                            m_auxiliaryES = true;
                            if (dataHeader.getFormat() == PDUFormatType::CONFIRMED)
                                secondHeaderOffset += 13U;
                            else
                                secondHeaderOffset += P25_PDU_HEADER_LENGTH_BYTES + 1U;
                        }
                        else {
                            if (s_verbose) {
                                LogInfoEx(LOG_P25, P25_PDU_STR ", ISP, block %u, fmt = $%02X, lastBlock = %u",
                                    (dataHeader.getFormat() == PDUFormatType::CONFIRMED) ? dataBlocks[i].getSerialNo() : m_dataBlockCnt, dataBlocks[i].getFormat(),
                                    dataBlocks[i].getLastBlock());
                            }
                        }
                    }

                    dataBlocks[i].getData(m_pduUserData + dataOffset);

                    // is this the first unconfirmed data block after a auxiliary ES header?
                    if (i == 0U && dataHeader.getFormat() == PDUFormatType::UNCONFIRMED && m_auxiliaryES) {
                        uint8_t exSAP = m_pduUserData[0U]; // first byte of the first data block after an aux ES header is the extended SAP
                        dataHeader.setEXSAP(exSAP);
                    }

                    dataOffset += (dataHeader.getFormat() == PDUFormatType::CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
                    m_dataBlockCnt++;
                }
                else {
                    if (dataBlocks[i].getFormat() == PDUFormatType::CONFIRMED) {
                        LogWarning(LOG_P25, P25_PDU_STR ", unfixable PDU data (3/4 rate or CRC), block %u", i);

                        // to prevent data block offset errors fill the bad block with 0's
                        uint8_t blankBuf[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                        ::memset(blankBuf, 0x00U, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                        ::memcpy(m_pduUserData + dataOffset, blankBuf, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);

                        dataOffset += P25_PDU_CONFIRMED_DATA_LENGTH_BYTES;
                        m_dataBlockCnt++;
                        m_undecodableBlockCnt++;
                    }
                    else {
                        LogWarning(LOG_P25, P25_PDU_STR ", unfixable PDU data (1/2 rate or CRC), block %u", i);

                        // to prevent data block offset errors fill the bad block with 0's
                        uint8_t blankBuf[P25_PDU_UNCONFIRMED_LENGTH_BYTES];
                        ::memset(blankBuf, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
                        ::memcpy(m_pduUserData + dataOffset, blankBuf, P25_PDU_UNCONFIRMED_LENGTH_BYTES);

                        dataOffset += P25_PDU_UNCONFIRMED_LENGTH_BYTES;
                        m_dataBlockCnt++;
                        m_undecodableBlockCnt++;
                    }

                    if (s_dumpPDUData) {
                        Utils::dump(1U, "P25, Unfixable PDU Data", buffer, P25_PDU_FEC_LENGTH_BYTES);
                    }
                }

                offset += P25_PDU_FEC_LENGTH_BYTES;
            }

#if DEBUG_P25_PDU_DATA
            LogDebugEx(LOG_P25, "Assembler::disassemble()", "packetLength = %u, secondHeaderOffset = %u, padLength = %u, pduLength = %u", packetLength, secondHeaderOffset, padLength, dataHeader.getPDULength());
#endif
            if (dataHeader.getBlocksToFollow() > 0U) {
                if (padLength > 0U) {
                    // move CRC-32 properly before padding to check CRC of user data
                    uint8_t crcBytes[P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
                    ::memset(crcBytes, 0x00U, packetLength);
                    ::memcpy(crcBytes, m_pduUserData, packetLength);
                    ::memcpy(crcBytes + packetLength, m_pduUserData + packetLength + padLength, 4U);

                    bool crcRet = edac::CRC::checkCRC32(crcBytes, packetLength + 4U);
                    if (!crcRet) {
                        LogWarning(LOG_P25, P25_PDU_STR ", failed CRC-32 check, blocks %u, len %u", dataHeader.getBlocksToFollow(), m_pduUserDataLength);
                        m_packetCRCFailed = true;
                    }
                } else {
                    bool crcRet = edac::CRC::checkCRC32(m_pduUserData, packetLength + 4U);
                    if (!crcRet) {
                        LogWarning(LOG_P25, P25_PDU_STR ", failed CRC-32 check, blocks %u, len %u", dataHeader.getBlocksToFollow(), m_pduUserDataLength);
                        m_packetCRCFailed = true;
                    }
                }
            }

            // reorganize PDU buffer for second header offsetting
            if (secondHeaderOffset > 0U) {
                uint8_t tempBuf[P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
                ::memcpy(tempBuf, m_pduUserData + secondHeaderOffset, packetLength - 4U);
                ::memset(m_pduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);
                ::memcpy(m_pduUserData, tempBuf, packetLength - secondHeaderOffset);
            }

            if (s_dumpPDUData && m_dataBlockCnt > 0U) {
                Utils::dump(1U, "P25, PDU Packet", m_pduUserData, packetLength - secondHeaderOffset);
            }

            if (m_dataBlockCnt < blocksToFollow) {
                LogWarning(LOG_P25, P25_PDU_STR ", incomplete PDU (%d / %d blocks)", m_dataBlockCnt, blocksToFollow);
            }

            m_pduUserDataLength = packetLength - secondHeaderOffset;

            m_blockCount = 0U;
            m_complete = true;
            return true;
        } else {
            return true;
        }
    }

    return false;
}

/* Helper to assemble user data as a P25 PDU packet. */

UInt8Array Assembler::assemble(data::DataHeader& dataHeader, bool extendedAddress, bool auxiliaryES, 
    const uint8_t* pduUserData, uint32_t* assembledBitLength, void* userContext)
{
    assert(pduUserData != nullptr);

    if (assembledBitLength != nullptr)
        *assembledBitLength = 0U;

    uint32_t bitLength = ((dataHeader.getBlocksToFollow() + 1U) * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
    if (dataHeader.getPadLength() > 0U)
        bitLength += (dataHeader.getPadLength() * 8U);

    uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

    UInt8Array dataArray = std::make_unique<uint8_t[]>((bitLength / 8U) + 1U);
    uint8_t* data = dataArray.get();
    ::memset(data, 0x00U, (bitLength / 8U) + 1U);

    uint8_t block[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    uint32_t blocksToFollow = dataHeader.getBlocksToFollow();

    if (s_verbose) {
        LogInfoEx(LOG_P25, P25_PDU_STR ", OSP, ack = %u, outbound = %u, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, packetLength = %u, S = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, bitLength = %u, llId = %u",
            dataHeader.getAckNeeded(), dataHeader.getOutbound(), dataHeader.getFormat(), dataHeader.getMFId(), dataHeader.getSAP(), dataHeader.getFullMessage(),
            dataHeader.getBlocksToFollow(), dataHeader.getPadLength(), dataHeader.getPacketLength(), dataHeader.getSynchronize(), dataHeader.getNs(), dataHeader.getFSN(), dataHeader.getLastFragment(),
            dataHeader.getHeaderOffset(), bitLength, dataHeader.getLLId());
    }

    // generate the PDU header and 1/2 rate Trellis
    dataHeader.encode(block);

#if DEBUG_P25_PDU_DATA
    Utils::dump(1U, "P25, PDU Assembler Block", block, P25_PDU_FEC_LENGTH_BYTES);
#endif

    if (m_usingCustomWriter && m_blockWriter != nullptr)
        m_blockWriter(userContext, 0U, block, P25_PDU_FEC_LENGTH_BYTES, false);
    else
        Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);
    offset += P25_PDU_FEC_LENGTH_BITS;

    if (pduUserData != nullptr && blocksToFollow > 0U) {
        uint32_t dataOffset = 0U;
        uint32_t pduLength = dataHeader.getPDULength() + dataHeader.getPadLength();
        uint32_t dataBlockCnt = 1U;
        uint32_t secondHeaderOffset = 0U;

        // we pad 20 bytes of extra space -- confirmed data will use various extra space in the PDU
        DECLARE_UINT8_ARRAY(packetData, pduLength + 20U);

        // are we processing extended address data from the first block?
        if (dataHeader.getSAP() == PDUSAP::EXT_ADDR && extendedAddress) {
            if (dataHeader.getFormat() == PDUFormatType::CONFIRMED)
                secondHeaderOffset += 4U;
            else
                secondHeaderOffset += P25_PDU_HEADER_LENGTH_BYTES;
            dataHeader.encodeExtAddr(packetData);

            if (s_verbose) {
                LogInfoEx(LOG_P25, P25_PDU_STR ", OSP, extended address, sap = $%02X, srcLlId = %u",
                    dataHeader.getEXSAP(), dataHeader.getSrcLLId());
            }
        }

        // are we processing auxiliary ES data from the first block?
        if ((dataHeader.getSAP() == PDUSAP::ENC_USER_DATA || dataHeader.getSAP() == PDUSAP::ENC_KMM) && auxiliaryES) {
            if (dataHeader.getFormat() == PDUFormatType::CONFIRMED)
                secondHeaderOffset += 13U;
            else
                secondHeaderOffset += P25_PDU_HEADER_LENGTH_BYTES + 1U;
            dataHeader.encodeAuxES(packetData);

            if (s_verbose) {
                LogInfoEx(LOG_P25, P25_PDU_STR ", OSP, auxiliary ES, algId = $%02X, kId = $%04X",
                    dataHeader.getAlgId(), dataHeader.getKId());

                if (dataHeader.getAlgId() != ALGO_UNENCRYPT) {
                    uint8_t mi[MI_LENGTH_BYTES];
                    ::memset(mi, 0x00U, MI_LENGTH_BYTES);

                    dataHeader.getMI(mi);

                    LogInfoEx(LOG_P25, P25_PDU_STR ", OSP, Enc Sync, MI = %02X %02X %02X %02X %02X %02X %02X %02X %02X", 
                        mi[0U], mi[1U], mi[2U], mi[3U], mi[4U], mi[5U], mi[6U], mi[7U], mi[8U]);
                }
            }
        }

        uint32_t packetLength = dataHeader.getPacketLength();
        uint32_t padLength = dataHeader.getPadLength();
#if DEBUG_P25_PDU_DATA
        LogDebugEx(LOG_P25, "Assembler::assemble()", "packetLength = %u, secondHeaderOffset = %u, padLength = %u, pduLength = %u", packetLength, secondHeaderOffset, padLength, pduLength);
#endif
        if (dataHeader.getFormat() != PDUFormatType::AMBT) {
            ::memcpy(packetData + secondHeaderOffset, pduUserData, packetLength);
            edac::CRC::addCRC32(packetData, packetLength + 4U);

            if (padLength > 0U) {
                // move the CRC-32 to the end of the packet data after the padding
                uint8_t crcBytes[4U];
                ::memcpy(crcBytes, packetData + packetLength, 4U);
                ::memset(packetData + packetLength, 0x00U, 4U);
                ::memcpy(packetData + (packetLength + padLength), crcBytes, 4U);
            }
        } else {
            // our AMBTs have a pre-calculated CRC-32 -- we don't need to do it ourselves
            ::memcpy(packetData + secondHeaderOffset, pduUserData, pduLength);
        }

#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25, Assembled PDU User Data", packetData, packetLength + padLength + 4U);
#endif

        // generate the PDU data
        for (uint32_t i = 0U; i < blocksToFollow; i++) {
            DataBlock dataBlock = DataBlock();
            dataBlock.setFormat(dataHeader);
            dataBlock.setSerialNo(i);
            dataBlock.setData(packetData + dataOffset);
            dataBlock.setLastBlock((i + 1U) == blocksToFollow);

            if (s_verbose) {
                LogInfoEx(LOG_P25, P25_PDU_STR ", OSP, block %u, fmt = $%02X, lastBlock = %u",
                    (dataHeader.getFormat() == PDUFormatType::CONFIRMED) ? dataBlock.getSerialNo() : i, dataBlock.getFormat(),
                    dataBlock.getLastBlock());
            }

            ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
            dataBlock.encode(block);

#if DEBUG_P25_PDU_DATA
            Utils::dump(1U, "P25, PDU Assembler Block", block, P25_PDU_FEC_LENGTH_BYTES);
#endif

            if (m_usingCustomWriter && m_blockWriter != nullptr)
                m_blockWriter(userContext, dataBlockCnt, block, P25_PDU_FEC_LENGTH_BYTES, dataBlock.getLastBlock());
            else
                Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

            offset += P25_PDU_FEC_LENGTH_BITS;
            dataOffset += (dataHeader.getFormat() == PDUFormatType::CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
            dataBlockCnt++;
        }
    }

    if (assembledBitLength != nullptr)
        *assembledBitLength = bitLength;

    if (m_usingCustomWriter) {
        return nullptr;
    }
    return dataArray;
}

/* Gets the raw user user data stored. */

uint32_t Assembler::getUserData(uint8_t* buffer) const
{
    assert(buffer != nullptr);
    assert(m_pduUserData != nullptr);

    if (m_complete) {
        ::memcpy(buffer, m_pduUserData, m_pduUserDataLength);
        return m_pduUserDataLength;
    } else {
        return 0U;
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to reset the disassembly state. */

void Assembler::resetDisassemblyState()
{
    dataHeader.reset();

    m_extendedAddress = false;
    m_auxiliaryES = false;

    m_dataBlockCnt = 0U;
    m_undecodableBlockCnt = 0U;

    m_blockCount = 0U;
    m_dataOffset = 0U;

    ::memset(m_pduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);
    ::memset(m_rawPDU, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

    m_packetCRCFailed = false;
    m_complete = false;
}
