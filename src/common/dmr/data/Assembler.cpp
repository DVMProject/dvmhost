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
#include "dmr/DMRDefines.h"
#include "dmr/data/Assembler.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::data;

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
    m_blockWriter(nullptr)
{
    /* stub */
}

/* Finalizes a instance of the Assembler class. */

Assembler::~Assembler() = default;

/* Helper to assemble user data as a DMR PDU packet. */

void Assembler::assemble(data::DataHeader& dataHeader, DataType::E dataType, const uint8_t* pduUserData, 
    uint32_t* assembledBitLength, void* userContext)
{
    assert(pduUserData != nullptr);
    assert(m_blockWriter != nullptr);

    if (assembledBitLength != nullptr)
        *assembledBitLength = 0U;

    uint32_t bitLength = ((dataHeader.getBlocksToFollow() + 1U) * DMR_FRAME_LENGTH_BITS);
    if (dataHeader.getPadLength() > 0U)
        bitLength += (dataHeader.getPadLength() * 8U);

    UInt8Array dataArray = std::make_unique<uint8_t[]>((bitLength / 8U) + 1U);
    uint8_t* data = dataArray.get();
    ::memset(data, 0x00U, (bitLength / 8U) + 1U);

    uint8_t block[DMR_FRAME_LENGTH_BYTES];
    ::memset(block, 0x00U, DMR_FRAME_LENGTH_BYTES);

    uint32_t blocksToFollow = dataHeader.getBlocksToFollow();

    if (s_verbose) {
        LogInfoEx(LOG_DMR, DMR_DT_DATA_HEADER ", dpf = $%02X, ack = %u, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, packetLength = %u, seqNo = %u, dstId = %u, srcId = %u, group = %u",
            dataHeader.getDPF(), dataHeader.getA(), dataHeader.getSAP(), dataHeader.getFullMesage(), dataHeader.getBlocksToFollow(), dataHeader.getPadLength(), dataHeader.getPacketLength(dataType),
            dataHeader.getFSN(), dataHeader.getDstId(), dataHeader.getSrcId(), dataHeader.getGI());
    }

    // generate the PDU header
    dataHeader.encode(block);

#if DEBUG_DMR_PDU_DATA
    Utils::dump(1U, "DMR, PDU Assembler Block", block, DMR_FRAME_LENGTH_BYTES);
#endif

    m_blockWriter(userContext, 0U, block, DMR_FRAME_LENGTH_BYTES, false);

    if (pduUserData != nullptr && blocksToFollow > 0U) {
        uint32_t dataOffset = 0U;
        uint32_t pduLength = dataHeader.getPDULength(dataType) + dataHeader.getPadLength();
        uint32_t dataBlockCnt = 1U;
        uint32_t secondHeaderOffset = 0U;

        // we pad 20 bytes of extra space -- confirmed data will use various extra space in the PDU
        DECLARE_UINT8_ARRAY(packetData, pduLength + 20U);

        uint32_t packetLength = dataHeader.getPacketLength(dataType);
        uint32_t padLength = dataHeader.getPadLength();
#if DEBUG_DMR_PDU_DATA
        LogDebugEx(LOG_DMR, "Assembler::assemble()", "packetLength = %u, secondHeaderOffset = %u, padLength = %u, pduLength = %u", packetLength, secondHeaderOffset, padLength, pduLength);
#endif
        ::memcpy(packetData + secondHeaderOffset, pduUserData, packetLength);
        edac::CRC::addCRC32(packetData, packetLength + 4U);

        if (padLength > 0U) {
            // move the CRC-32 to the end of the packet data after the padding
            uint8_t crcBytes[4U];
            ::memcpy(crcBytes, packetData + packetLength, 4U);
            ::memset(packetData + packetLength, 0x00U, 4U);
            ::memcpy(packetData + (packetLength + padLength), crcBytes, 4U);
        }

#if DEBUG_DMR_PDU_DATA
        Utils::dump(1U, "DMR, Assembled PDU User Data", packetData, packetLength + padLength + 4U);
#endif

        // generate the PDU data
        for (uint32_t i = 0U; i < blocksToFollow; i++) {
            DataBlock dataBlock = DataBlock();
            dataBlock.setFormat(dataHeader);
            dataBlock.setSerialNo(i);
            dataBlock.setData(packetData + dataOffset);
            dataBlock.setLastBlock((i + 1U) == blocksToFollow);

            if (s_verbose) {
                if (dataType == DataType::RATE_34_DATA) {
                    LogInfoEx(LOG_DMR, DMR_DT_RATE_34_DATA ", ISP, block %u, dataType = $%02X, dpf = $%02X",
                        (dataHeader.getDPF() == DPF::CONFIRMED_DATA) ? dataBlock.getSerialNo() : i, dataBlock.getDataType(), dataBlock.getFormat());
                } else if (dataType == DataType::RATE_12_DATA) {
                    LogInfoEx(LOG_DMR, DMR_DT_RATE_12_DATA ", ISP, block %u, dataType = $%02X, dpf = $%02X", 
                        (dataHeader.getDPF() == DPF::CONFIRMED_DATA) ? dataBlock.getSerialNo() : i, dataBlock.getDataType(), dataBlock.getFormat());
                }
                else {
                    LogInfoEx(LOG_DMR, DMR_DT_RATE_1_DATA ", ISP, block %u, dataType = $%02X, dpf = $%02X", 
                        (dataHeader.getDPF() == DPF::CONFIRMED_DATA) ? dataBlock.getSerialNo() : i, dataBlock.getDataType(), dataBlock.getFormat());
                }
            }

            ::memset(block, 0x00U, DMR_FRAME_LENGTH_BYTES);
            dataBlock.encode(block);

#if DEBUG_P25_PDU_DATA
            Utils::dump(1U, "DMR, PDU Assembler Block", block, DMR_FRAME_LENGTH_BYTES);
#endif

            m_blockWriter(userContext, dataBlockCnt, block, DMR_FRAME_LENGTH_BYTES, dataBlock.getLastBlock());

            if (dataHeader.getDPF() == DPF::CONFIRMED_DATA) {
                if (dataType == DataType::RATE_34_DATA) {
                    dataOffset += DMR_PDU_CONFIRMED_TQ_DATA_LENGTH_BYTES;
                } else if (dataType == DataType::RATE_12_DATA) {
                    dataOffset += DMR_PDU_CONFIRMED_HR_DATA_LENGTH_BYTES;
                }
                else {
                    dataOffset += DMR_PDU_CONFIRMED_UNCODED_DATA_LENGTH_BYTES;
                }
            } else {
                if (dataType == DataType::RATE_34_DATA) {
                    dataOffset += DMR_PDU_THREEQUARTER_LENGTH_BYTES;
                } else if (dataType == DataType::RATE_12_DATA) {
                    dataOffset += DMR_PDU_HALFRATE_LENGTH_BYTES;
                }
                else {
                    dataOffset += DMR_PDU_UNCODED_LENGTH_BYTES;
                }
            }

            dataBlockCnt++;
        }
    }

    if (assembledBitLength != nullptr)
        *assembledBitLength = bitLength;
}
