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
 * @file IOSP_EXT_FNCT.h
 * @ingroup p25_tsbk
 * @file IOSP_EXT_FNCT.cpp
 * @ingroup p25_tsbk
 */
#if !defined(__P25_LC_TSBK__IOSP_EXT_FNCT_H__)
#define  __P25_LC_TSBK__IOSP_EXT_FNCT_H__

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
             * @brief Implements EXT FNCT RSP - Extended Function Response (ISP) and
             *  EXT FNCT CMD - Extended Function Command (OSP)
             * @ingroup p25_tsbk
             */
            class HOST_SW_API IOSP_EXT_FNCT : public TSBK {
            public:
                /**
                 * @brief Initializes a new instance of the IOSP_EXT_FNCT class.
                 */
                IOSP_EXT_FNCT();

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
                 * @brief Returns a string that represents the current TSBK.
                 * @returns std::string String representation of the TSBK.
                 */
                std::string toString(bool isp = false) override;

            public:
                /**
                 * @brief Extended function opcode.
                 */
                DECLARE_PROPERTY(uint32_t, extendedFunction, ExtendedFunction);

                DECLARE_COPY(IOSP_EXT_FNCT);
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__IOSP_EXT_FNCT_H__
