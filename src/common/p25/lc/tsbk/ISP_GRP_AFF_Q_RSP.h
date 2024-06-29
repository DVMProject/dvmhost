// SPDX-License-Identifier: GPL-2.0-only
/**
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file ISP_GRP_AFF_Q_RSP.h
 * @ingroup p25_tsbk
 * @file ISP_GRP_AFF_Q_RSP.cpp
 * @ingroup p25_tsbk
 */
#if !defined(__P25_LC_TSBK__ISP_GRP_AFF_Q_RSP_H__)
#define  __P25_LC_TSBK__ISP_GRP_AFF_Q_RSP_H__

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
             * @brief Implements GRP AFF Q RSP - Group Affiliation Query Response
             * @ingroup p25_tsbk
             */
            class HOST_SW_API ISP_GRP_AFF_Q_RSP : public TSBK {
            public:
                /**
                 * @brief Initializes a new instance of the ISP_GRP_AFF_Q_RSP class.
                 */
                ISP_GRP_AFF_Q_RSP();

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
                 * @brief Announcement group.
                 */
                __PROPERTY(uint32_t, announceGroup, AnnounceGroup);

                __COPY(ISP_GRP_AFF_Q_RSP);
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__ISP_GRP_AFF_Q_RSP_H__
