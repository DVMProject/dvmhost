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
 * @file OSP_ADJ_STS_BCAST.h
 * @ingroup p25_tsbk
 * @file OSP_ADJ_STS_BCAST.cpp
 * @ingroup p25_tsbk
 */
#if !defined(__P25_LC_TSBK__OSP_ADJ_STS_BCAST_H__)
#define  __P25_LC_TSBK__OSP_ADJ_STS_BCAST_H__

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
             * @brief Implements ADJ STS BCAST - Adjacent Site Status Broadcast
             * @ingroup p25_tsbk
             */
            class HOST_SW_API OSP_ADJ_STS_BCAST : public TSBK {
            public:
                /**
                 * @brief Initializes a new instance of the OSP_ADJ_STS_BCAST class.
                 */
                OSP_ADJ_STS_BCAST();

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
                // Adjacent Site Data
                /**
                 * @brief Adjacent site CFVA flags.
                 */
                DECLARE_PROPERTY(uint8_t, adjCFVA, AdjSiteCFVA);
                /**
                 * @brief Adjacent site system ID.
                 */
                DECLARE_PROPERTY(uint32_t, adjSysId, AdjSiteSysId);
                /**
                 * @brief Adjacent site RFSS ID.
                 */
                DECLARE_PROPERTY(uint8_t, adjRfssId, AdjSiteRFSSId);
                /**
                 * @brief Adjacent site ID.
                 */
                DECLARE_PROPERTY(uint8_t, adjSiteId, AdjSiteId);
                /**
                 * @brief Adjacent site channel ID.
                 */
                DECLARE_PROPERTY(uint8_t, adjChannelId, AdjSiteChnId);
                /**
                 * @brief Adjacent site channel number.
                 */
                DECLARE_PROPERTY(uint32_t, adjChannelNo, AdjSiteChnNo);
                /**
                 * @brief Adjacent site service class.
                 */
                DECLARE_PROPERTY(uint8_t, adjServiceClass, AdjSiteSvcClass);

                DECLARE_COPY(OSP_ADJ_STS_BCAST);
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__OSP_ADJ_STS_BCAST_H__
