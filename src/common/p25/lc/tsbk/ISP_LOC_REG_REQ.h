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
 * @file ISP_LOC_REG_REQ.h
 * @ingroup p25_tsbk
 * @file ISP_LOC_REG_REQ.cpp
 * @ingroup p25_tsbk
 */
#if !defined(__P25_LC_TSBK__ISP_LOC_REG_REQ_H__)
#define  __P25_LC_TSBK__ISP_LOC_REG_REQ_H__

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
             * @brief Implements LOC REG REQ - Location Registration Request
             * @ingroup p25_tsbk
             */
            class HOST_SW_API ISP_LOC_REG_REQ : public TSBK {
            public:
                /**
                 * @brief Initializes a new instance of the ISP_LOC_REG_REQ class.
                 */
                ISP_LOC_REG_REQ();

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
                 * @brief Location registration area.
                 */
                __PROPERTY(uint8_t, lra, LRA);

                __COPY(ISP_LOC_REG_REQ);
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__ISP_LOC_REG_REQ_H__
