// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file OSP_STS_Q.h
 * @ingroup p25_tsbk
 * @file OSP_STS_Q.cpp
 * @ingroup p25_tsbk
 */
#if !defined(__P25_LC_TSBK__OSP_STS_Q_H__)
#define __P25_LC_TSBK__OSP_STS_Q_H__

#include "common/Defines.h"
#include "common/p25/lc/tsbk/IOSP_STS_UPDT.h"

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
             * @brief Implements STS Q - Status Query (OSP).
             * @ingroup p25_tsbk
             */
            class DVM_COMMON_API OSP_STS_Q : public IOSP_STS_UPDT {
            public:
                /**
                 * @brief Initializes a new instance of the OSP_STS_Q class.
                 */
                OSP_STS_Q();

                /**
                 * @brief Encodes a status query trunking signalling block.
                 * @param[out] data Buffer to encode a TSBK.
                 * @param[in] rawTSBK Flag indicating whether or not the output buffer is raw.
                 * @param[in] noTrellis Flag indicating whether or not Trellis encoding is suppressed.
                 */
                void encode(uint8_t* data, bool rawTSBK = false, bool noTrellis = false) override;

                /**
                 * @brief Returns a string that represents the current TSBK.
                 * @param[in] isp Flag indicating ISP formatting; ignored for this fixed-direction TSBK.
                 * @returns std::string String representation of the TSBK.
                 */
                std::string toString(bool isp = false) override;
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__OSP_STS_Q_H__
