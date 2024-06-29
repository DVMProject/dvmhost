// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file OSP_TSBK_RAW.h
 * @ingroup p25_tsbk
 * @file OSP_TSBK_RAW.cpp
 * @ingroup p25_tsbk
 */
#if !defined(__P25_LC_TSBK__OSP_TSBK_RAW_H__)
#define  __P25_LC_TSBK__OSP_TSBK_RAW_H__

#include "common/Defines.h"
#include "common/p25/lc/TSBK.h"

namespace p25
{
    namespace lc
    {
        namespace tsbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Implements a mechanism to generate raw TSBK data from bytes.
             * @ingroup p25_tsbk
             */
            class HOST_SW_API OSP_TSBK_RAW : public TSBK {
            public:
                /**
                 * @brief Initializes a new instance of the OSP_TSBK_RAW class.
                 */
                OSP_TSBK_RAW();
                /**
                 * @brief Finalizes a instance of the OSP_TSBK_RAW class.
                 */
                ~OSP_TSBK_RAW();

                /**
                 * @brief Decode a trunking signalling block.
                 * @param[in] data Buffer containing a TSBK to decode.
                 * @param rawTSBK Flag indicating whether or not the passed buffer is raw.
                 * @returns bool True, if TSBK decoded, otherwise false.
                 */
                bool decode(const uint8_t* data, bool rawTSBK = false) override;
                /**
                 * @brief Encode a trunking signalling block.
                 * @param[out] data Buffer to encode a TSBK.
                 * @param rawTSBK Flag indicating whether or not the output buffer is raw.
                 * @param noTrellis Flag indicating whether or not the encoded data should be Trellis encoded.
                 */
                void encode(uint8_t* data, bool rawTSBK = false, bool noTrellis = false) override;

                /**
                 * @brief Sets the TSBK to encode.
                 * @param[in] tsbk Buffer containing TSBK to encode.
                 */
                void setTSBK(const uint8_t* tsbk);

            private:
                uint8_t* m_tsbk;
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__OSP_TSBK_RAW_H__
