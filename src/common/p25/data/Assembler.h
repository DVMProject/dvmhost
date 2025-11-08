// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file Assembler.h
 * @ingroup p25_pdu
 * @file Assembler.cpp
 * @ingroup p25_pdu
 */
#if !defined(__P25_DATA__ASSEMBLER_H__)
#define  __P25_DATA__ASSEMBLER_H__

#include "common/Defines.h"
#include "common/p25/data/DataBlock.h"
#include "common/p25/data/DataHeader.h"

#include <string>
#include <functional>

namespace p25
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements a packet assembler for P25 PDU packet streams.
         * @ingroup p25_pdu
         */
        class HOST_SW_API Assembler {
        public:
            /**
             * @brief Initializes a new instance of the Assembler class.
             */
            Assembler();
            /**
             * @brief Finalizes a instance of the Assembler class.
             */
            ~Assembler();

            /**
             * @brief Helper to disassemble a P25 PDU frame into user data.
             * @param[in] pduBlock Buffer containing a PDU block to disassemble.
             * @param blockLength Length of PDU block buffer.
             * @param resetState Flag indicating the current disassembly state should be reset.
             * @returns bool True, if entire P25 PDU packet is disassembled, otherwise false.
             */
            bool disassemble(const uint8_t* pduBlock, uint32_t blockLength, bool resetState = false);
            /**
             * @brief Helper to assemble user data as a P25 PDU packet.
             * @note When using a custom block writer, this will return null.
             * @param dataHeader Instance of a PDU data header.
             * @param extendedAddress Flag indicating whether or not extended addressing is in use.
             * @param auxiliaryES Flag indicating whether or not an auxiliary ES is included.
             * @param[in] pduUserData Buffer containing user data to assemble.
             * @param[out] assembledBitLength Length of assembled packet in bits.
             * @param[in] userContext User supplied context data to pass to custom block writer.
             * @returns UInt8Array Assembled PDU buffer.
             */
            UInt8Array assemble(data::DataHeader& dataHeader, bool extendedAddress, bool auxiliaryES, const uint8_t* pduUserData,
                uint32_t* assembledBitLength, void* userContext = nullptr);

            /**
             * @brief Helper to set the custom block writer callback.
             * @param callback 
             */
            void setBlockWriter(std::function<void(const void*, const uint8_t, const uint8_t*, uint32_t, bool)>&& callback)
            { 
                m_blockWriter = callback;
                if (callback == nullptr)
                    m_usingCustomWriter = false;
                else
                    m_usingCustomWriter = true;
            }

            /**
             * @brief Gets the raw user user data stored.
             * @param[out] buffer Buffer to copy bytes in data block to.
             * @returns uint32_t Number of bytes copied.
             */
            uint32_t getUserData(uint8_t* buffer) const;
            /**
             * @brief Gets the length of the raw user data stored. (This will include the 4 bytes for the CRC-32).
             * @returns uint32_t Length of raw user data stored.
             */
            uint32_t getUserDataLength() const { return m_pduUserDataLength; }

            /**
             * @brief Sets the flag indicating whether or not the assembler will dump PDU data.
             * @param dumpPDUData Flag indicating PDU log dumping.
             */
            static void setDumpPDUData(bool dumpPDUData) { s_dumpPDUData = dumpPDUData; }
            /**
             * @brief Sets the flag indicating verbose log output.
             * @param verbose Flag indicating verbose log output.
             */
            static void setVerbose(bool verbose) { s_verbose = verbose; }

        public:
            /**
             * @brief Data Blocks in disassmbled packet.
             */
            data::DataBlock* dataBlocks;
            /**
             * @brief Data Header from disassembled packet.
             */
            data::DataHeader dataHeader;

            /**
             * @brief Flag indicating the disassembled packet contains extended addressing.
             */
            DECLARE_PROPERTY(bool, extendedAddress, ExtendedAddress);
            /**
             * @brief Flag indicating the disassembled packet contains an auxiliary ES.
             */
            DECLARE_PROPERTY(bool, auxiliaryES, AuxiliaryES);
            /**
             * @brief Count of data blocks in disassmebled packet.
             */
            DECLARE_PROPERTY(uint8_t, dataBlockCnt, DataBlockCount);
            /**
             * @brief Count of data blocks in disassmebled packet that were undecodable.
             */
            DECLARE_PROPERTY(uint8_t, undecodableBlockCnt, UndecodableBlockCount);

            /**
             * @brief Flag indicating resulting user data failed the CRC-32 check.
             */
            DECLARE_PROPERTY(bool, packetCRCFailed, PacketCRCFailed);
            /**
             * @brief Flag indicating disassembly is complete and user data is available.
             */
            DECLARE_PROPERTY(bool, complete, Complete);

        private:
            uint8_t* m_pduUserData;
            uint32_t m_pduUserDataLength;

            uint8_t* m_rawPDU;

            uint32_t m_blockCount;
            uint32_t m_dataOffset;

            static bool s_dumpPDUData;
            static bool s_verbose;
            bool m_usingCustomWriter;

            /**
             * @brief Custom block writing callback.
             */
            std::function<void(const void* userContext, const uint8_t currentBlock, const uint8_t* data, uint32_t len, bool lastBlock)> m_blockWriter;

            /**
             * @brief Internal helper to reset the disassembly state.
             */
            void resetDisassemblyState();
        };
    } // namespace data
} // namespace p25

#endif // __P25_DATA__ASSEMBLER_H__
