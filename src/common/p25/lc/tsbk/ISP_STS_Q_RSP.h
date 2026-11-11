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
 * @file ISP_STS_Q_RSP.h
 * @ingroup p25_tsbk
 * @file ISP_STS_Q_RSP.cpp
 * @ingroup p25_tsbk
 */
#if !defined(__P25_LC_TSBK__ISP_STS_Q_RSP_H__)
#define __P25_LC_TSBK__ISP_STS_Q_RSP_H__

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
             * @brief Implements STS Q RSP - Status Query Response (ISP).
             * @ingroup p25_tsbk
             */
            class DVM_COMMON_API ISP_STS_Q_RSP : public IOSP_STS_UPDT {
            public:
                /**
                 * @brief Initializes a new instance of the ISP_STS_Q_RSP class.
                 */
                ISP_STS_Q_RSP();

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

#endif // __P25_LC_TSBK__ISP_STS_Q_RSP_H__
