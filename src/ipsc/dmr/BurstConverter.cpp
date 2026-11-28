// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - IPSC Gateway
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "dmr/BurstConverter.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/SlotType.h"
#include "common/dmr/Sync.h"
#include "common/dmr/data/EMB.h"
#include "common/dmr/data/EmbeddedData.h"
#include "common/dmr/lc/FullLC.h"
#include "common/dmr/lc/LC.h"
#include "common/edac/AMBEFEC.h"
#include "common/edac/Golay24128.h"
#include "common/edac/RS129.h"
#include "protocol/IPSCDefines.h"

#include <cstring>

using namespace dmr::defines;
using namespace ipsc::dmr;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Converts DVM network DMR data into an IPSC burst. */

bool BurstConverter::fromDMR(const ::dmr::data::NetData& data, uint8_t colorCode, uint8_t* burst, uint32_t capacity, uint32_t& length)
{
    length = 0U;
    if (burst == nullptr || capacity < defines::VOICE_BURST_BASE_LENGTH)
        return false;

    uint8_t frame[DMR_FRAME_LENGTH_BYTES];
    data.getData(frame);
    const DataType::E type = data.getDataType();

    // handle link-control header and terminator bursts first
    if (type == DataType::VOICE_LC_HEADER || type == DataType::TERMINATOR_WITH_LC) {
        ::dmr::lc::FullLC fullLC;
        std::unique_ptr<::dmr::lc::LC> lc = fullLC.decode(frame, type);
        if (lc == nullptr)
            return false;

        uint8_t lcData[12U];
        ::memset(lcData, 0x00U, sizeof(lcData));
        lc->getData(lcData);

        uint8_t parity[4U];
        edac::RS129::encode(lcData, 9U, parity);

        const uint8_t* mask = type == DataType::VOICE_LC_HEADER ? VOICE_LC_HEADER_CRC_MASK : TERMINATOR_WITH_LC_CRC_MASK;
        lcData[9U] = parity[2U] ^ mask[0U];
        lcData[10U] = parity[1U] ^ mask[1U];
        lcData[11U] = parity[0U] ^ mask[2U];

        ::memset(burst, 0x00U, defines::VOICE_BURST_BASE_LENGTH);

        burst[0U] = uint8_t(type == DataType::VOICE_LC_HEADER ? defines::BurstType::VOICE_HEADER : defines::BurstType::VOICE_TERMINATOR);
        burst[1U] = data.getSlotNo() == 2U ? defines::SLOT_TWO_BURST_FLAG : defines::SLOT_ONE_BURST_FLAG;
        burst[2U] = 0U;
        burst[3U] = 9U;
        burst[4U] = uint8_t((defines::BURST_SLOT_TYPE_FLAG | defines::BURST_SYNC_VOICE) >> 8U);
        burst[5U] = uint8_t(defines::BURST_SLOT_TYPE_FLAG | defines::BURST_SYNC_VOICE);
        burst[6U] = 0U;
        burst[7U] = 96U;

        ::memcpy(burst + 8U, lcData, sizeof(lcData));

        burst[20U] = colorCode << 4U;

        const defines::BurstType slotType = type == DataType::VOICE_LC_HEADER ? defines::BurstType::VOICE_HEADER : defines::BurstType::VOICE_TERMINATOR;

        burst[21U] = uint8_t((colorCode << 4U) | uint8_t(slotType));

        length = defines::VOICE_BURST_BASE_LENGTH;
        return true;
    }

    // handle regular voice bursts
    if (type != DataType::VOICE && type != DataType::VOICE_SYNC)
        return false;

    uint8_t raw[27U];
    ::memcpy(raw, frame, 13U);

    raw[13U] = (frame[13U] & 0xF0U) | (frame[19U] & 0x0FU);
    ::memcpy(raw + 14U, frame + 20U, 13U);

    ::memset(burst, 0x00U, defines::VOICE_BURST_BASE_LENGTH);
    burst[0U] = uint8_t(defines::BurstType::VOICE) | (data.getSlotNo() == 2U ? defines::SLOT_TWO_BURST_FLAG : defines::SLOT_ONE_BURST_FLAG);
    burst[1U] = 20U;

    if (type == DataType::VOICE_SYNC)
        burst[2U] = defines::VOICE_SYNC_FLAG;

    // convert each 9-byte AMBE+2 frame into 49-bit voice payload and write to burst
    for (uint32_t frameNo = 0U; frameNo < 3U; ++frameNo) {
        uint8_t ambe[7U];
        decodeAMBE(raw + (frameNo * 9U), ambe);

        // write each bit of the 49-bit AMBE+2 frame into the burst payload
        for (uint32_t bit = 0U; bit < 49U; ++bit)
            WRITE_BIT(burst, 24U + frameNo * 49U + bit, READ_BIT(ambe, bit));
    }

    length = defines::VOICE_BURST_BASE_LENGTH;
    return true;
}

/* Converts an IPSC burst into DVM network DMR data. */

bool BurstConverter::toDMR(const uint8_t* burst, uint32_t length, uint32_t srcId, uint32_t dstId, bool group, uint8_t slotNo, uint8_t seqNo, uint8_t n, 
    uint8_t colorCode, ::dmr::data::EmbeddedData& embeddedData, ::dmr::data::NetData& data)
{
    if (burst == nullptr || length < 2U)
        return false;

    const uint8_t burstType = burst[0U] & defines::BURST_TYPE_MASK;
    uint8_t frame[DMR_FRAME_LENGTH_BYTES];
    ::memset(frame, 0x00U, sizeof(frame));

    DataType::E dataType;

    // handle voice header and terminator bursts first
    if ((burstType == uint8_t(defines::BurstType::VOICE_HEADER) || burstType == uint8_t(defines::BurstType::VOICE_TERMINATOR)) && length >= 20U) {
        ::dmr::lc::LC lc(burst + 8U);
        if (burstType == uint8_t(defines::BurstType::VOICE_HEADER))
            embeddedData.setLC(lc);

        dataType = burstType == uint8_t(defines::BurstType::VOICE_HEADER) ? DataType::VOICE_LC_HEADER : DataType::TERMINATOR_WITH_LC;

        ::dmr::Sync::addDMRDataSync(frame, true);

        ::dmr::SlotType slotType;
        slotType.setColorCode(colorCode);
        slotType.setDataType(dataType);
        slotType.encode(frame);

        ::dmr::lc::FullLC().encode(lc, frame, dataType);
    } else if (burstType == uint8_t(defines::BurstType::VOICE) && length >= 22U) {
        uint8_t raw[27U];
        ::memset(raw, 0x00U, sizeof(raw));

        // extract each 49-bit AMBE+2 frame from the burst and encode it into raw format
        for (uint32_t frameNo = 0U; frameNo < 3U; ++frameNo) {
            uint8_t ambe[7U];
            ::memset(ambe, 0x00U, sizeof(ambe));

            // extract 49 bits for the current AMBE+2 frame
            for (uint32_t bit = 0U; bit < 49U; ++bit)
                WRITE_BIT(ambe, bit, READ_BIT(burst, 24U + frameNo * 49U + bit));

            encodeAMBE(ambe, raw + frameNo * 9U);
        }

        ::memcpy(frame, raw, 13U);
        frame[13U] = raw[13U] & 0xF0U;
        frame[19U] = raw[13U] & 0x0FU;
        ::memcpy(frame + 20U, raw + 14U, 13U);

        // determine if this is a voice sync frame or a regular voice frame
        if ((burst[2U] & defines::VOICE_SYNC_FLAG) != 0U || n == 0U) {
            ::dmr::Sync::addDMRAudioSync(frame, true);
            dataType = DataType::VOICE_SYNC;
        } else {
            const uint8_t lcss = embeddedData.getData(frame, n);
            ::dmr::data::EMB emb;
            emb.setColorCode(colorCode);
            emb.setLCSS(lcss);
            emb.encode(frame);

            dataType = DataType::VOICE;
        }
    } else {
        return false;
    }

    data.setSlotNo(slotNo);
    data.setSrcId(srcId);
    data.setDstId(dstId);
    data.setFLCO(group ? FLCO::GROUP : FLCO::PRIVATE);
    data.setSeqNo(seqNo);
    data.setN(n);
    data.setDataType(dataType);
    data.setBER(0U);
    data.setRSSI(0U);
    data.setData(frame);

    return true;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Extracts a 49-bit AMBE+2 codec frame from a 72-bit FEC codeword. */

void BurstConverter::decodeAMBE(const uint8_t* codeword, uint8_t* ambe)
{
    uint8_t corrected[9U];
    ::memcpy(corrected, codeword, sizeof(corrected));
    edac::AMBEFEC().regenerateNXDN(corrected);

    uint32_t a = 0U;
    for (uint32_t i = 0U; i < 24U; ++i)
        a = (a << 1U) | (READ_BIT(corrected, edac::AMBE_A_TABLE[i]) != 0U);
    uint32_t aData = 0U;
    edac::Golay24128::decode24128(a, aData);

    uint32_t b = 0U;
    for (uint32_t i = 0U; i < 23U; ++i)
        b = (b << 1U) | (READ_BIT(corrected, edac::AMBE_B_TABLE[i]) != 0U);
    b ^= edac::PRNG_TABLE[aData] >> 1U;
    uint32_t bData = edac::Golay24128::decode23127(b);

    ::memset(ambe, 0x00U, 7U);
    for (uint32_t i = 0U; i < 12U; ++i) {
        WRITE_BIT(ambe, i, (aData >> (11U - i)) & 1U);
        WRITE_BIT(ambe, i + 12U, (bData >> (11U - i)) & 1U);
    }

    for (uint32_t i = 0U; i < 25U; ++i)
        WRITE_BIT(ambe, i + 24U, READ_BIT(corrected, edac::AMBE_C_TABLE[i]));
}

/* Generates a 72-bit FEC codeword from a 49-bit AMBE+2 codec frame. */

void BurstConverter::encodeAMBE(const uint8_t* ambe, uint8_t* codeword)
{
    uint32_t aData = 0U, bData = 0U, cData = 0U;
    for (uint32_t i = 0U; i < 12U; ++i) {
        aData = (aData << 1U) | (READ_BIT(ambe, i) != 0U);
        bData = (bData << 1U) | (READ_BIT(ambe, i + 12U) != 0U);
    }

    for (uint32_t i = 0U; i < 25U; ++i)
        cData = (cData << 1U) | (READ_BIT(ambe, i + 24U) != 0U);

    const uint32_t a = edac::Golay24128::encode24128(aData);
    const uint32_t b = (edac::Golay24128::encode23127(bData) >> 1U) ^ (edac::PRNG_TABLE[aData] >> 1U);
    ::memset(codeword, 0x00U, 9U);

    for (uint32_t i = 0U; i < 24U; ++i)
        WRITE_BIT(codeword, edac::AMBE_A_TABLE[i], (a >> (23U - i)) & 1U);
    for (uint32_t i = 0U; i < 23U; ++i)
        WRITE_BIT(codeword, edac::AMBE_B_TABLE[i], (b >> (22U - i)) & 1U);
    for (uint32_t i = 0U; i < 25U; ++i)
        WRITE_BIT(codeword, edac::AMBE_C_TABLE[i], (cData >> (24U - i)) & 1U);
}
