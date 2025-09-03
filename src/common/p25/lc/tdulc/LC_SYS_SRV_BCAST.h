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
 * @file LC_SYS_SRV_BCAST.h
 * @ingroup p25_lc
 * @file LC_SYS_SRV_BCAST.cpp
 * @ingroup p25_lc
 */
#if !defined(__P25_LC_TDULC__LC_SYS_SRV_BCAST_H__)
#define  __P25_LC_TDULC__LC_SYS_SRV_BCAST_H__

#include "common/Defines.h"
#include "common/p25/lc/TDULC.h"

namespace p25
{
    namespace lc
    {
        namespace tdulc
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Implements SYS SRV BCAST - System Service Broadcast
             * @ingroup p25_lc
             */
            class HOST_SW_API LC_SYS_SRV_BCAST : public TDULC {
            public:
                /**
                 * @brief Initializes a new instance of the LC_SYS_SRV_BCAST class.
                 */
                LC_SYS_SRV_BCAST();

                /**
                 * @brief Decode a terminator data unit w/ link control.
                 * @param[in] data Buffer containing a TDULC to decode.
                 * @returns bool True, if TDULC decoded, otherwise false.
                 */
                bool decode(const uint8_t* data) override;
                /**
                 * @brief Encode a terminator data unit w/ link control.
                 * @param[out] data Buffer to encode a TDULC.
                 */
                void encode(uint8_t* data) override;
            };
        } // namespace tdulc
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TDULC__LC_SYS_SRV_BCAST_H__
