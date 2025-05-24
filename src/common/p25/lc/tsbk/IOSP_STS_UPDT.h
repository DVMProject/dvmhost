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
 * @file IOSP_STS_UPDT.h
 * @ingroup p25_tsbk
 * @file IOSP_STS_UPDT.cpp
 * @ingroup p25_tsbk
 */
#if !defined(__P25_LC_TSBK__IOSP_STS_UPDT_H__)
#define  __P25_LC_TSBK__IOSP_STS_UPDT_H__

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
             * @brief Implements STS UPDT REQ - Status Update Request (ISP) and
             *  STS UPDT - Status Update (OSP)
             * @ingroup p25_tsbk
             */
            class HOST_SW_API IOSP_STS_UPDT : public TSBK {
            public:
                /**
                 * @brief Initializes a new instance of the IOSP_STS_UPDT class.
                 */
                IOSP_STS_UPDT();

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
                 * @brief Status value.
                 */
                DECLARE_PROPERTY(uint8_t, statusValue, Status);

                DECLARE_COPY(IOSP_STS_UPDT);
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__IOSP_STS_UPDT_H__
