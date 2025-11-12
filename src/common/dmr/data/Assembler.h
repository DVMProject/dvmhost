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
 * @ingroup dmr_pdu
 * @file Assembler.cpp
 * @ingroup dmr_pdu
 */
#if !defined(__DMR_DATA__ASSEMBLER_H__)
#define  __DMR_DATA__ASSEMBLER_H__

#include "common/Defines.h"
#include "common/dmr/data/DataBlock.h"
#include "common/dmr/data/DataHeader.h"

#include <string>
#include <functional>

namespace dmr
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements a packet assembler for DMR PDU packet streams.
         * @ingroup dmr_pdu
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
             * @brief Helper to assemble user data as a DMR PDU packet.
             * @note When using a custom block writer, this will return null.
             * @param dataHeader Instance of a PDU data header.
             * @param dataType DMR packet data type.
             * @param[in] pduUserData Buffer containing user data to assemble.
             * @param[out] assembledBitLength Length of assembled packet in bits.
             * @param[in] userContext User supplied context data to pass to custom block writer.
             * @returns UInt8Array Assembled PDU buffer.
             */
            void assemble(data::DataHeader& dataHeader, DMRDEF::DataType::E dataType, const uint8_t* pduUserData,
                uint32_t* assembledBitLength, void* userContext = nullptr);

            /**
             * @brief Helper to set the custom block writer callback.
             * @param callback 
             */
            void setBlockWriter(std::function<void(const void*, const uint8_t, const uint8_t*, uint32_t, bool)>&& callback)
            { 
                m_blockWriter = callback;
            }

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

        private:
            static bool s_dumpPDUData;
            static bool s_verbose;

            /**
             * @brief Custom block writing callback.
             */
            std::function<void(const void* userContext, const uint8_t currentBlock, const uint8_t* data, uint32_t len, bool lastBlock)> m_blockWriter;
        };
    } // namespace data
} // namespace dmr

#endif // __DMR_DATA__ASSEMBLER_H__
