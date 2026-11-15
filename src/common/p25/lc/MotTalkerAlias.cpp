// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 * 
 *  Based on code from sdrtrunk project (https://github.com/sdrtrunk/sdrtrunk)
 *  Portions Copyright (C) 2014-2024 Dennis Sheirer
 *  sdrtrunk/src/main/java/io/github/dsheirer/module/decode/p25/phase1/message/lc/motorola/LCMotorolaTalkerAliasHeader.java
 *  sdrtrunk/src/main/java/io/github/dsheirer/module/decode/p25/phase1/message/lc/motorola/LCMotorolaTalkerAliasDataBlock.java
 *  sdrtrunk/src/main/java/io/github/dsheirer/module/decode/p25/phase1/message/lc/motorola/LCMotorolaTalkerAliasAssembler.java
 *
 */
#include "common/p25/lc/MotTalkerAlias.h"
#include "common/edac/CRC.h"
#include "common/p25/P25Defines.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

static const uint8_t LOOKUP_TABLE[256U] = {
    0xF2U, 0x2EU, 0x66U, 0x90U, 0x74U, 0x8AU, 0x6FU, 0x78U, 0xBBU, 0x53U, 0x03U, 0x11U, 0x68U, 0xCDU, 0x44U, 0x17U,
    0x28U, 0x5FU, 0x1EU, 0x84U, 0x75U, 0x79U, 0x6EU, 0x9BU, 0x2CU, 0xBEU, 0x62U, 0x2DU, 0xF1U, 0x7CU, 0xB8U, 0x83U,
    0xD9U, 0x4EU, 0x6DU, 0x02U, 0x61U, 0x3DU, 0xA8U, 0x06U, 0xB9U, 0xF8U, 0x9CU, 0x37U, 0x3AU, 0x23U, 0xC1U, 0x50U,
    0xEDU, 0x9FU, 0xAFU, 0x3BU, 0xBDU, 0x82U, 0xBAU, 0xA0U, 0xDFU, 0xC2U, 0x47U, 0x22U, 0xF0U, 0xEEU, 0xA1U, 0xFEU,
    0xA2U, 0x10U, 0x5BU, 0x48U, 0x57U, 0xA3U, 0x05U, 0x60U, 0x7BU, 0x0DU, 0xF9U, 0x6CU, 0xB3U, 0x56U, 0x4CU, 0xBCU,
    0x29U, 0xA4U, 0x0FU, 0xECU, 0xB6U, 0xA5U, 0xA6U, 0x3CU, 0x7FU, 0x6BU, 0xB4U, 0x21U, 0xADU, 0xAEU, 0xC4U, 0xC8U,
    0xC5U, 0x5DU, 0xDEU, 0xE0U, 0x1DU, 0x19U, 0x4BU, 0xC6U, 0x0CU, 0x3FU, 0x5AU, 0xC7U, 0xE1U, 0x59U, 0x55U, 0x54U,
    0x4AU, 0x43U, 0x42U, 0xE2U, 0xE3U, 0xFAU, 0x00U, 0xE4U, 0xE5U, 0x18U, 0x41U, 0x0BU, 0x0AU, 0xE6U, 0xFCU, 0xFDU,
    0xD2U, 0xF6U, 0xD4U, 0x2BU, 0x63U, 0x49U, 0x94U, 0x5EU, 0xA7U, 0x5CU, 0x70U, 0x69U, 0xF7U, 0x08U, 0xB1U, 0x7DU,
    0x38U, 0xCFU, 0xCCU, 0xD8U, 0x51U, 0x8FU, 0xD5U, 0x93U, 0x6AU, 0xF3U, 0xEFU, 0x7EU, 0xFBU, 0x64U, 0xF4U, 0x35U,
    0x27U, 0x07U, 0x31U, 0x14U, 0x87U, 0x98U, 0x76U, 0x34U, 0xCAU, 0x92U, 0x33U, 0x1BU, 0x4FU, 0x8CU, 0x09U, 0x40U,
    0x32U, 0x36U, 0x77U, 0x12U, 0xD3U, 0xC3U, 0x01U, 0xABU, 0x72U, 0x81U, 0x95U, 0xC9U, 0xC0U, 0xE9U, 0x65U, 0x52U,
    0x24U, 0x30U, 0x1CU, 0xDBU, 0x88U, 0xE8U, 0x97U, 0x9DU, 0x58U, 0x26U, 0x04U, 0x39U, 0xACU, 0x2AU, 0x9EU, 0xAAU,
    0x25U, 0xD7U, 0xCEU, 0xEBU, 0x96U, 0xF5U, 0x0EU, 0x8DU, 0xDCU, 0xA9U, 0x2FU, 0xDDU, 0x1FU, 0xEAU, 0x91U, 0xB7U,
    0xD6U, 0x89U, 0x8BU, 0xD1U, 0xB0U, 0x99U, 0x13U, 0x7AU, 0xE7U, 0x9AU, 0xB5U, 0x86U, 0xFFU, 0x46U, 0x85U, 0xB2U,
    0x73U, 0xDAU, 0xBFU, 0xD0U, 0x71U, 0xCBU, 0x4DU, 0x80U, 0x15U, 0x67U, 0x16U, 0x1AU, 0x20U, 0x8EU, 0x45U, 0x3EU
};

static const uint16_t MOT_TALKER_ALIAS_HEADER_CRC_MASK = 0x01AU;
static const uint16_t MOT_TALKER_ALIAS_HEADER_CRC_LENGTH_BITS = 128U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes an new instance of the MotTalkerAlias class. */

MotTalkerAlias::MotTalkerAlias() :
    m_dstId(0U),
    m_blockCount(0U),
    m_paddingCount(0U),
    m_sequence(0U),
    m_haveHeader(false),
    m_complete(false),
    m_blocks(),
    m_result()
{
    /* stub */
}

/* Clears all partially assembled alias state. */

void MotTalkerAlias::reset()
{
    m_dstId = 0U;
    m_blockCount = 0U;
    m_paddingCount = 0U;
    m_sequence = 0U;
    m_haveHeader = false;
    m_complete = false;
    m_blocks.clear();
    m_result = MotTalkerAliasResult();
}

/* Decodes and assembles an alias. This uses the nine-byte Motorola link control word,
   which may be either a header or a data block. The assembler maintains state across multiple calls
   until a complete CRC-valid alias is assembled. */

bool MotTalkerAlias::decode(const uint8_t* lc, MotTalkerAliasResult& result)
{
    if (lc == nullptr || lc[1U] != MFG_MOT)
        return false;

    // check if this is a header or data block and process accordingly
    if (lc[0U] == LCO::MOT_TALKER_ALIAS_HEADER) {
        const uint16_t transmittedCRC = (uint16_t)(((lc[7U] & 0x01U) << 8U) | lc[8U]);
        if (headerCRC(lc) != transmittedCRC)
            return false;

        const uint8_t sequence = lc[7U] >> 4U;
        if (m_sequence != sequence || m_haveHeader || m_complete) {
            reset();
            m_sequence = sequence;
        }

        m_dstId = (uint16_t)((lc[2U] << 8U) | lc[3U]);
        m_blockCount = lc[4U];
        m_paddingCount = (lc[7U] >> 1U) & 0x07U;
        m_haveHeader = m_blockCount > 0U;
    }
    else if (lc[0U] == LCO::MOT_TALKER_ALIAS_DATA) {
        // process a data block
        const uint8_t sequence = lc[3U] >> 4U;
        if (m_sequence != sequence) {
            reset();
            m_sequence = sequence;
        }

        const uint8_t blockNumber = lc[2U];
        if (blockNumber == 0U)
            return false;

        std::vector<bool>& fragment = m_blocks[blockNumber];
        fragment.assign(MOT_TALKER_ALIAS_FRAGMENT_LENGTH_BITS, false);

        // extract the fragment bits from the LC word
        for (uint8_t bit = 0U; bit < MOT_TALKER_ALIAS_FRAGMENT_LENGTH_BITS; ++bit) {
            const uint8_t sourceBit = bit + 28U;
            if ((lc[sourceBit / 8U] & (0x80U >> (sourceBit % 8U))) != 0U)
                fragment[bit] = true;
        }
    }
    else {
        return false;
    }

    assemble();
    if (!m_complete)
        return false;

    result = m_result;
    return true;
}

/* Encodes an alias into a header followed by numbered data-block LC words. */

std::vector<std::vector<uint8_t>> MotTalkerAlias::encode(uint32_t netId, uint16_t sysId, uint32_t srcId,
    uint16_t dstId, const std::string& alias, uint8_t sequence)
{
    // assemble the payload with the fully-qualified source address and alias data
    uint8_t payload[MOT_TALKER_ALIAS_MAX_PAYLOAD_BYTES] = { 0U };
    uint16_t payloadLength = 7U;
    payload[0U] = (uint8_t)(netId >> 12U);
    payload[1U] = (uint8_t)(netId >> 4U);
    payload[2U] = (uint8_t)((netId << 4U) | ((sysId >> 8U) & 0x0FU));
    payload[3U] = (uint8_t)sysId;
    payload[4U] = (uint8_t)(srcId >> 16U);
    payload[5U] = (uint8_t)(srcId >> 8U);
    payload[6U] = (uint8_t)srcId;

    // convert the UTF-8 alias to UTF-16 and then to Motorola-obfuscated bytes
    uint16_t utf16[MOT_TALKER_ALIAS_MAX_CHARS] = { 0U };
    const uint16_t utf16Length = utf8ToUtf16(alias, utf16, MOT_TALKER_ALIAS_MAX_CHARS);

    // encode the decoded alias bytes into Motorola-obfuscated format
    const uint16_t aliasByteCount = utf16Length * 2U;
    uint16_t accumulator = aliasByteCount;
    for (uint16_t i = 0U; i < utf16Length; ++i) {
        const uint8_t decoded[2U] = { (uint8_t)(utf16[i] >> 8U), (uint8_t)utf16[i] };
        for (uint8_t byte = 0U; byte < 2U; ++byte) {
            for (uint16_t encoded = 0U; encoded <= 0xFFU; ++encoded) {
                if (decodeByte((uint8_t)encoded, accumulator) == decoded[byte]) {
                    payload[payloadLength++] = (uint8_t)encoded;
                    accumulator = (uint16_t)(accumulator + encoded + 1U);
                    break;
                }
            }
        }
    }

    payloadLength += 2U;

    // calculate the CRC for the payload and find the correct high and low bytes for the checksum
    uint16_t prefixCRC = 0U;
    for (uint16_t i = 0U; i + 2U < payloadLength; ++i)
        prefixCRC = crcUpdate(prefixCRC, payload[i]);

    for (uint16_t high = 0U; high <= 0xFFU; ++high) {
        const uint16_t highCRC = crcUpdate(prefixCRC, (uint8_t)high);
        for (uint16_t low = 0U; low <= 0xFFU; ++low) {
            if (crcUpdate(highCRC, (uint8_t)low) == 0x1D0FU) {
                payload[payloadLength - 2U] = (uint8_t)high;
                payload[payloadLength - 1U] = (uint8_t)low;
                high = 0x100U;
                break;
            }
        }
    }

    const uint32_t payloadBits = (uint32_t)payloadLength * 8U;
    const uint8_t blockCount = (uint8_t)((payloadBits + MOT_TALKER_ALIAS_FRAGMENT_LENGTH_BITS - 1U) / MOT_TALKER_ALIAS_FRAGMENT_LENGTH_BITS);

    // assemble the header word for the talker-alias message
    std::vector<std::vector<uint8_t>> words;
    uint8_t header[9U] = { 0U };
    header[0U] = LCO::MOT_TALKER_ALIAS_HEADER;
    header[1U] = MFG_MOT;
    header[2U] = (uint8_t)(dstId >> 8U);
    header[3U] = (uint8_t)dstId;
    header[4U] = blockCount;
    header[5U] = 0x01U; // always use BMP UTF-16

    // calculate the number of available bits and the padding count for the last data block
    const uint32_t availableBits = (uint32_t)blockCount * MOT_TALKER_ALIAS_FRAGMENT_LENGTH_BITS;
    const uint8_t paddingCount = (uint8_t)((availableBits - payloadBits + 7U) / 8U);
    header[7U] = (uint8_t)(((sequence & 0x0FU) << 4U) | (paddingCount << 1U));

    // calculate the header CRC and populate the CRC fields in the header
    const uint16_t crc = headerCRC(header);
    header[7U] |= (uint8_t)(crc >> 8U);
    header[8U] = (uint8_t)crc;
    words.emplace_back(header, header + sizeof(header));

    // create the data blocks for the payload
    for (uint8_t block = 0U; block < blockCount; ++block) {
        uint8_t word[9U] = { 0U };
        word[0U] = LCO::MOT_TALKER_ALIAS_DATA;
        word[1U] = MFG_MOT;
        word[2U] = block + 1U;
        word[3U] = (uint8_t)((sequence & 0x0FU) << 4U);

        // copy the payload bits into the data block
        for (uint8_t bit = 0U; bit < MOT_TALKER_ALIAS_FRAGMENT_LENGTH_BITS; ++bit) {
            const uint32_t payloadBit = (uint32_t)block * MOT_TALKER_ALIAS_FRAGMENT_LENGTH_BITS + bit;
            if (payloadBit < payloadBits && (payload[payloadBit / 8U] & (0x80U >> (payloadBit % 8U))) != 0U) {
                const uint8_t targetBit = bit + 28U;
                word[targetBit / 8U] |= 0x80U >> (targetBit % 8U);
            }
        }

        words.emplace_back(word, word + sizeof(word));
    }

    return words;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Calculates the nine-bit CRC carried by a Motorola talker-alias header. */

uint16_t MotTalkerAlias::headerCRC(const uint8_t* lc)
{
    // copy the first 8 bytes of the header into the CRC buffer
    uint8_t crcBuffer[MOT_TALKER_ALIAS_HEADER_CRC_LENGTH_BITS / 8U] = { 0U };
    for (uint8_t i = 0U; i < 8U; ++i)
        crcBuffer[i] = lc[i];

    crcBuffer[7U] &= 0xFEU;

    // calculate the CRC9 for the header and apply the header CRC mask
    return edac::CRC::createCRC9(crcBuffer, MOT_TALKER_ALIAS_HEADER_CRC_LENGTH_BITS) ^
        MOT_TALKER_ALIAS_HEADER_CRC_MASK;
}

/* Updates the Motorola payload CRC remainder with one octet. */

uint16_t MotTalkerAlias::crcUpdate(uint16_t crc, uint8_t value)
{
    // update the CRC remainder with each bit of the input value
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
        const bool input = (value & (0x80U >> bit)) != 0U;
        const bool top = (crc & 0x8000U) != 0U;

        crc <<= 1U;

        if (input != top)
            crc ^= 0x1021U;
    }

    return crc;
}

/* Calculates the Motorola payload CRC remainder. */

uint16_t MotTalkerAlias::crcRemainder(const uint8_t* data, uint16_t length)
{
    uint16_t crc = 0U;
    for (uint16_t i = 0U; i < length; ++i)
        crc = crcUpdate(crc, data[i]);

    return crc;
}

/* Decodes one Motorola-obfuscated alias octet. */

uint8_t MotTalkerAlias::decodeByte(uint8_t encoded, uint16_t accumulator)
{
    const uint16_t accumMult = (uint16_t)(accumulator * 293U + 0x72E9U);
    
    // Java's signed byte plus 128 maps the transmitted octet to this rotated table index.
    const uint8_t mult1 = (uint8_t)(LOOKUP_TABLE[(uint8_t)(encoded + 128U)] - (uint8_t)(accumMult >> 8U));
    uint8_t mult2 = 1U;
    uint8_t shortstop = (uint8_t)(accumMult | 0x01U);
    const uint8_t increment = (uint8_t)(shortstop << 1U);
    while (mult2 != 0xFFU && shortstop != 1U) {
        shortstop = (uint8_t)(shortstop + increment);
        mult2 = (uint8_t)(mult2 + 2U);
    }

    return (uint8_t)(mult1 * mult2);
}

/* Converts UTF-8 text into BMP UTF-16 code units. */

uint16_t MotTalkerAlias::utf8ToUtf16(const std::string& text, uint16_t* utf16, uint16_t capacity)
{
    uint16_t length = 0U;

    // iterate over each byte in the UTF-8 text and decode it into a UTF-16 code unit
    for (std::size_t i = 0U; i < text.size() && length < capacity;) {
        const uint8_t first = (uint8_t)text[i];
        uint32_t codepoint = '?';

        std::size_t count = 1U;
        if (first < 0x80U) {
            codepoint = first;
        }
        else if ((first & 0xE0U) == 0xC0U && i + 1U < text.size()) {
            codepoint = ((first & 0x1FU) << 6U) | ((uint8_t)text[i + 1U] & 0x3FU);
            count = 2U;
        }
        else if ((first & 0xF0U) == 0xE0U && i + 2U < text.size()) {
            codepoint = ((first & 0x0FU) << 12U) | (((uint8_t)text[i + 1U] & 0x3FU) << 6U) |
                ((uint8_t)text[i + 2U] & 0x3FU);
            count = 3U;
        }

        utf16[length++] = codepoint <= 0xFFFFU ? (uint16_t)codepoint : (uint16_t)'?';

        i += count;
    }

    return length;
}

/* Appends one BMP UTF-16 code unit to a UTF-8 string. */

void MotTalkerAlias::appendUtf8(std::string& text, uint16_t codepoint)
{
    if (codepoint < 0x80U) {
        text.push_back((char)codepoint);
    }
    else if (codepoint < 0x800U) {
        text.push_back((char)(0xC0U | (codepoint >> 6U)));
        text.push_back((char)(0x80U | (codepoint & 0x3FU)));
    }
    else {
        text.push_back((char)(0xE0U | (codepoint >> 12U)));
        text.push_back((char)(0x80U | ((codepoint >> 6U) & 0x3FU)));
        text.push_back((char)(0x80U | (codepoint & 0x3FU)));
    }
}

/* Attempts to assemble the current header and data blocks. */

void MotTalkerAlias::assemble()
{
    if (!m_haveHeader)
        return;

    // check that all expected blocks are present before attempting assembly
    for (uint16_t block = 1U; block <= m_blockCount; ++block) {
        if (m_blocks.find((uint8_t)block) == m_blocks.end())
            return;
    }

    // concatenate all block bits into a byte-aligned payload buffer
    uint8_t payload[MOT_TALKER_ALIAS_MAX_PAYLOAD_BYTES] = { 0U };
    const uint32_t availableBits = (uint32_t)m_blockCount * MOT_TALKER_ALIAS_FRAGMENT_LENGTH_BITS;
    for (uint16_t block = 1U; block <= m_blockCount; ++block) {
        const std::vector<bool>& fragment = m_blocks[(uint8_t)block];
        for (uint8_t bit = 0U; bit < MOT_TALKER_ALIAS_FRAGMENT_LENGTH_BITS; ++bit) {
            if (fragment[bit]) {
                const uint32_t payloadBit = ((uint32_t)block - 1U) * MOT_TALKER_ALIAS_FRAGMENT_LENGTH_BITS + bit;
                payload[payloadBit / 8U] |= 0x80U >> (payloadBit % 8U);
            }
        }
    }

    // use the header padding count to recover the exact byte-aligned payload length
    const uint16_t availableBytes = (uint16_t)((availableBits + 7U) / 8U);
    if (m_paddingCount > availableBytes)
        return;

    const uint16_t payloadLength = availableBytes - m_paddingCount;
    if (payloadLength < 11U || crcRemainder(payload, payloadLength) != 0x1D0FU)
        return;

    m_result.netId = ((uint32_t)payload[0U] << 12U) | ((uint32_t)payload[1U] << 4U) | (payload[2U] >> 4U);
    m_result.sysId = (uint16_t)(((payload[2U] & 0x0FU) << 8U) | payload[3U]);
    m_result.srcId = ((uint32_t)payload[4U] << 16U) | ((uint32_t)payload[5U] << 8U) | payload[6U];
    m_result.dstId = m_dstId;
    m_result.sequence = m_sequence;
    m_result.alias.clear();

    // decode the payload starting from the 7th byte, using the encoded length and accumulator for decoding
    const uint16_t encodedLength = payloadLength - 7U;
    const uint16_t decodedLength = encodedLength - 2U;
    if (decodedLength > MOT_TALKER_ALIAS_MAX_CHARS * 2U)
        return;

    uint16_t accumulator = decodedLength;

    // decode each obfuscated alias byte into the fixed scratch buffer
    uint8_t decoded[MOT_TALKER_ALIAS_MAX_CHARS * 2U] = { 0U };
    for (uint16_t i = 0U; i < decodedLength; ++i) {
        decoded[i] = decodeByte(payload[7U + i], accumulator);
        accumulator = (uint16_t)(accumulator + payload[7U + i] + 1U);
    }

    // convert the decoded bytes into UTF-16 code units and append them to the alias string
    for (uint16_t i = 0U; i + 1U < decodedLength; i += 2U)
        appendUtf8(m_result.alias, (uint16_t)((decoded[i] << 8U) | decoded[i + 1U]));

    m_complete = true;
}
