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
/**
 * @file MotTalkerAlias.h
 * @ingroup p25_lc
 * @file MotTalkerAlias.cpp
 * @ingroup p25_lc
 */
#if !defined(__P25_LC_MOT_TALKER_ALIAS_H__)
#define __P25_LC_MOT_TALKER_ALIAS_H__

#include "common/Defines.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace p25
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Structure Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Contains a decoded Motorola talker alias and its fully-qualified source address.
         * @ingroup p25_lc
         */
        struct MotTalkerAliasResult {
            uint32_t netId;       //!< 20-bit WACN.
            uint16_t sysId;       //!< 12-bit system ID.
            uint32_t srcId;       //!< 24-bit source radio ID.
            uint16_t dstId;       //!< 16-bit talkgroup ID from the header.
            uint8_t sequence;     //!< 4-bit message sequence.
            std::string alias;    //!< Decoded UTF-8 alias.
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Encodes and assembles Motorola P25 Phase 1 talker-alias link control words.
         * @ingroup p25_lc
         */
        class DVM_COMMON_API MotTalkerAlias {
        public:
            /** 
             * @brief Initializes an new instance of the MotTalkerAlias class. 
             */
            MotTalkerAlias();

            /**
             * @brief Clears all partially assembled alias state.
             */
            void reset();

            /**
             * @brief Decodes and assembles an alias. This uses the nine-byte Motorola link control word,
             *  which may be either a header or a data block. The assembler maintains state across multiple calls
             *  until a complete CRC-valid alias is assembled.
             * @param[in] lc Link control bytes, including LCO and MFId.
             * @param[out] result Completed talker-alias fields.
             * @returns bool True when this word completes a CRC-valid alias and result was populated.
             */
            bool decode(const uint8_t* lc, MotTalkerAliasResult& result);

            /**
             * @brief Encodes an alias into a header followed by numbered data-block LC words.
             * @param netId 20-bit WACN placed in the fully-qualified source address.
             * @param sysId 12-bit system ID placed in the fully-qualified source address.
             * @param srcId 24-bit source radio ID.
             * @param dstId 16-bit talkgroup ID.
             * @param alias UTF-8 alias; characters outside the BMP are replaced.
             * @param sequence 4-bit message sequence.
             * @returns std::vector<std::vector<uint8_t>> LC words in transmit order.
             */
            static std::vector<std::vector<uint8_t>> encode(uint32_t netId, uint16_t sysId, uint32_t srcId,
                uint16_t dstId, const std::string& alias, uint8_t sequence);

        private:
            uint16_t m_dstId;
            uint8_t m_blockCount;
            uint8_t m_paddingCount;
            uint8_t m_sequence;
            bool m_haveHeader;
            bool m_complete;
            std::map<uint8_t, std::vector<bool>> m_blocks;
            MotTalkerAliasResult m_result;

            /**
             * @brief Calculates the nine-bit CRC carried by a Motorola talker-alias header.
             * @param[in] lc Nine-byte header LC with the CRC field either populated or cleared.
             * @returns uint16_t Calculated nine-bit header CRC.
             */
            static uint16_t headerCRC(const uint8_t* lc);
            /**
             * @brief Updates the Motorola payload CRC remainder with one octet.
             * @param crc Current CRC remainder.
             * @param value Octet to process.
             * @returns uint16_t Updated CRC remainder.
             */
            static uint16_t crcUpdate(uint16_t crc, uint8_t value);
            /**
             * @brief Calculates the Motorola payload CRC remainder.
             * @param[in] data Payload bytes including the transmitted checksum.
             * @param length Payload length in bytes.
             * @returns uint16_t CRC remainder.
             */
            static uint16_t crcRemainder(const uint8_t* data, uint16_t length);
            /**
             * @brief Decodes one Motorola-obfuscated alias octet.
             * @param encoded Encoded alias octet.
             * @param accumulator Current 16-bit alias accumulator.
             * @returns uint8_t Decoded UTF-16 octet.
             */
            static uint8_t decodeByte(uint8_t encoded, uint16_t accumulator);
            /**
             * @brief Converts UTF-8 text into BMP UTF-16 code units.
             * @param[in] text UTF-8 text to convert.
             * @param[out] utf16 Destination buffer for UTF-16 code units.
             * @param capacity Destination capacity in code units.
             * @returns uint16_t Number of UTF-16 code units written; unsupported characters are replaced.
             */
            static uint16_t utf8ToUtf16(const std::string& text, uint16_t* utf16, uint16_t capacity);
            /**
             * @brief Appends one BMP UTF-16 code unit to a UTF-8 string.
             * @param[in,out] text UTF-8 output string.
             * @param codepoint BMP code unit to append.
             */
            static void appendUtf8(std::string& text, uint16_t codepoint);

            /** 
             * @brief Attempts to assemble the current header and data blocks. 
             */
            void assemble();
        };
    } // namespace lc
} // namespace p25

#endif // __P25_LC_MOT_TALKER_ALIAS_H__
