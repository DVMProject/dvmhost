// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - FNE System View
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file IOSP_GRP_AFF.h
 * @ingroup fneSysView
 * @file IOSP_GRP_AFF.cpp
 * @ingroup fneSysView
 */
#if !defined(__SYSVIEW_P25_TSBK__IOSP_GRP_AFF_H__)
#define  __SYSVIEW_P25_TSBK__IOSP_GRP_AFF_H__

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
             * @brief Implements GRP AFF RSP - Group Affiliation Response (OSP)
             * @ingroup fneSysView
             */
            class HOST_SW_API OSP_GRP_AFF : public TSBK {
            public:
                /**
                 * @brief Initializes a new instance of the OSP_GRP_AFF class.
                 */
                OSP_GRP_AFF();

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
                DECLARE_PROPERTY(uint32_t, announceGroup, AnnounceGroup);

                DECLARE_COPY(OSP_GRP_AFF);
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __SYSVIEW_P25_TSBK__IOSP_GRP_AFF_H__
