// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *
 */
/**
 * @file ShortLC.h
 * @ingroup dmr_lc
 * @file ShortLC.cpp
 * @ingroup dmr_lc
 */
#if !defined(__DMR_LC__SHORT_LC_H__)
#define __DMR_LC__SHORT_LC_H__

#include "common/Defines.h"

namespace dmr
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents short DMR link control.
         * @ingroup dmr_lc
         */
        class HOST_SW_API ShortLC {
        public:
            /**
             * @brief Initializes a new instance of the ShortLC class.
             */
            ShortLC();
            /**
             * @brief Finalizes a instance of the ShortLC class.
             */
            ~ShortLC();

            /**
             * @brief Decode DMR short-link control data.
             * @param[in] in Buffer containing encoded short-link control data.
             * @param[out] out Buffer to copy raw short-link control data.
             */
            bool decode(const uint8_t* in, uint8_t* out);
            /**
             * @brief Encode DMR short-link control data.
             * @param[in] in Buffer containing raw short-link control data.
             * @param[out] out Buffer to copy encoded short-link control data.
             */
            void encode(const uint8_t* in, uint8_t* out);

        private:
            bool* m_rawData;
            bool* m_deInterData;

            /**
             * @brief 
             * @param[in] in 
             */
            void decodeExtractBinary(const uint8_t* in);
            /**
             * @brief 
             */
            void decodeDeInterleave();
            /**
             * @brief 
             */
            bool decodeErrorCheck();
            /**
             * @brief 
             * @param[out] data 
             */
            void decodeExtractData(uint8_t* data) const;

            /**
             * @brief 
             * @param[in] in 
             */
            void encodeExtractData(const uint8_t* in) const;
            /**
             * @brief 
             */
            void encodeErrorCheck();
            /**
             * @brief 
             */
            void encodeInterleave();
            /**
             * @brief 
             * @param[out] data
             */
            void encodeExtractBinary(uint8_t* data);
        };
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC__SHORT_LC_H__
