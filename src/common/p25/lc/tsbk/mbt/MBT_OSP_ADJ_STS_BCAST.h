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
 * @file MBT_OSP_ADJ_STS_BCAST.h
 * @ingroup p25_ambt
 * @file MBT_OSP_ADJ_STS_BCAST.cpp
 * @ingroup p25_ambt
 */
#if !defined(__P25_LC_TSBK__MBT_OSP_ADJ_STS_BCAST_H__)
#define  __P25_LC_TSBK__MBT_OSP_ADJ_STS_BCAST_H__

#include "common/Defines.h"
#include "common/p25/lc/AMBT.h"

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
             * @ingroup p25_ambt
             */
            class HOST_SW_API MBT_OSP_ADJ_STS_BCAST : public AMBT {
            public:
                /**
                 * @brief Initializes a new instance of the MBT_OSP_ADJ_STS_BCAST class.
                 */
                MBT_OSP_ADJ_STS_BCAST();

                /**
                 * @brief Decode a alternate trunking signalling block.
                 * @param[in] dataHeader P25 PDU data header
                 * @param[in] blocks P25 PDU data blocks
                 * @returns bool True, if AMBT decoded, otherwise false.
                 */
                bool decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks) override;
                /**
                 * @brief Encode a alternate trunking signalling block.
                 * @param[out] dataHeader P25 PDU data header
                 * @param[out] pduUserData P25 PDU user data
                 */
                void encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData) override;

                /**
                 * @brief Returns a string that represents the current AMBT.
                 * @returns std::string String representation of the AMBT.
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

                DECLARE_COPY(MBT_OSP_ADJ_STS_BCAST);
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__MBT_OSP_ADJ_STS_BCAST_H__
