// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file AMBT.h
 * @ingroup p25_lc
 * @file AMBT.cpp
 * @ingroup p25_lc
 */
#if !defined(__P25_LC__AMBT_H__)
#define  __P25_LC__AMBT_H__

#include "common/Defines.h"
#include "common/p25/lc/TSBK.h"
#include "common/p25/data/DataHeader.h"
#include "common/p25/data/DataBlock.h"

namespace p25
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents link control data for Alternate Trunking packets.
         * @ingroup p25_lc
         */
        class HOST_SW_API AMBT : public TSBK {
        public:
            /**
             * @brief Initializes a new instance of the AMBT class.
             */
            AMBT();

            /**
             * @brief Decode a alternate trunking signalling block.
             * @param[in] dataHeader P25 PDU data header
             * @param[in] blocks P25 PDU data blocks
             * @returns bool True, if AMBT decoded, otherwise false.
             */
            virtual bool decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks) = 0;
            /**
             * @brief Encode a alternate trunking signalling block.
             * @param[out] dataHeader P25 PDU data header
             * @param[out] pduUserData P25 PDU user data
             */
            virtual void encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData) = 0;

            /**
             * @brief Decode a trunking signalling block.
             * @param[in] data Buffer containing a TSBK to decode.
             * @param rawTSBK Flag indicating whether or not the passed buffer is raw.
             * @returns bool True, if TSBK decoded, otherwise false.
             */
            bool decode(const uint8_t* data, bool rawTSBK = false);
            /**
             * @brief Encode a trunking signalling block.
             * @param[out] data Buffer to encode a TSBK.
             * @param rawTSBK Flag indicating whether or not the output buffer is raw.
             * @param noTrellis Flag indicating whether or not the encoded data should be Trellis encoded.
             */
            void encode(uint8_t* data, bool rawTSBK = false, bool noTrellis = false);

        protected:
            /**
             * @brief Internal helper to convert AMBT bytes to a 64-bit long value.
             * @param[in] dataHeader P25 PDU data header
             * @param[in] blocks P25 PDU data blocks
             * @returns ulong64_t 64-bit packed value containing the buffer.
             */
            static ulong64_t toValue(const data::DataHeader& dataHeader, const uint8_t* pduUserData);

            /**
             * @brief Internal helper to decode a trunking signalling block.
             * @param[in] dataHeader P25 PDU data header
             * @param[in] blocks P25 PDU data blocks
             * @param[out] pduUserData P25 PDU user data
             * @returns bool True, if AMBT decoded, otherwise false.
             */
            bool decode(const data::DataHeader& dataHeader, const data::DataBlock* blocks, uint8_t* pduUserData);
            /**
             * @brief Internal helper to encode a trunking signalling block.
             * @param[out] dataHeader P25 PDU data header
             * @param[out] pduUserData P25 PDU user data
             */
            void encode(data::DataHeader& dataHeader, uint8_t* pduUserData);
        };
    } // namespace lc
} // namespace p25

#endif // __P25_LC__AMBT_H__
