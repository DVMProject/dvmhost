// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022 by Jason-UWU
 *  Copyright (C) 2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file IOSP_RAD_MON.h
 * @ingroup p25_tsbk
 * @file IOSP_RAD_MON.cpp
 * @ingroup p25_tsbk
 */
#if !defined(__P25_LC_TSBK__IOSP_RAD_MON_H__)
#define  __P25_LC_TSBK__IOSP_RAD_MON_H__

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
             * @brief Implements RAD MON REQ - Radio Unit Monitor Request (ISP) and
             *  RAD MON CMD - Radio Monitor Command (OSP)
             * @ingroup p25_tsbk
             */
            class HOST_SW_API IOSP_RAD_MON : public TSBK {
            public:
                /**
                 * @brief Initializes a new instance of the IOSP_RAD_MON class.
                 */
                IOSP_RAD_MON();

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
                 * @brief Radio Unit Monitor.
                 */
                DECLARE_PROPERTY(uint8_t, txMult, TxMult);

                DECLARE_COPY(IOSP_RAD_MON);
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__IOSP_RAD_MON_H__
