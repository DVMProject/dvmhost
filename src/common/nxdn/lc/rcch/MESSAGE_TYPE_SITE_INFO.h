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
 * @file MESSAGE_TYPE_SITE_INFO.h
 * @ingroup nxdn_rcch
 * @file MESSAGE_TYPE_SITE_INFO.cpp
 * @ingroup nxdn_rcch
 */
#if !defined(__NXDN_LC_RCCH__MESSAGE_TYPE_SITE_INFO_H__)
#define  __NXDN_LC_RCCH__MESSAGE_TYPE_SITE_INFO_H__

#include "common/Defines.h"
#include "common/nxdn/lc/RCCH.h"

namespace nxdn
{
    namespace lc
    {
        namespace rcch
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Implements SITE_INFO - Site Information
             * @ingroup nxdn_rcch
             */
            class HOST_SW_API MESSAGE_TYPE_SITE_INFO : public RCCH {
            public:
                /**
                 * @brief Initializes a new instance of the MESSAGE_TYPE_SITE_INFO class.
                 */
                MESSAGE_TYPE_SITE_INFO();

                /**
                 * @brief Decode RCCH data.
                 * @param[in] data Buffer containing a RCCH to decode.
                 * @param length Length of data buffer.
                 * @param offset Offset for RCCH in data buffer.
                 */
                void decode(const uint8_t* data, uint32_t length, uint32_t offset = 0U) override;
                /**
                 * @brief Encode RCCH data.
                 * @param[out] data Buffer to encode a RCCH.
                 * @param length Length of data buffer.
                 * @param offset Offset for RCCH in data buffer.
                 */
                void encode(uint8_t* data, uint32_t length, uint32_t offset = 0U) override;

                /**
                 * @brief Returns a string that represents the current RCCH.
                 * @returns std::string String representation of the RCCH.
                 */
                std::string toString(bool isp = false) override;

            public:
                // Channel Structure Data
                /**
                 * @brief Count of BCCH frames per RCCH superframe.
                 */
                __PROPERTY(uint8_t, bcchCnt, BcchCnt);
                /**
                 * @brief Count of RCCH frame groupings per RCCH superframe.
                 */
                __PROPERTY(uint8_t, rcchGroupingCnt, RcchGroupingCnt);
                /**
                 * @brief Count of CCCH/UPCH paging frames per RCCH superframe.
                 */
                __PROPERTY(uint8_t, ccchPagingCnt, CcchPagingCnt);
                /**
                 * @brief Count of CCCH/UPCH multi-purpose frames per RCCH superframe.
                 */
                __PROPERTY(uint8_t, ccchMultiCnt, CcchMultiCnt);
                /**
                 * @brief Count of group iterations per RCCH superframe.
                 */
                __PROPERTY(uint8_t, rcchIterateCnt, RcchIterateCount);

                __COPY(MESSAGE_TYPE_SITE_INFO);
            };
        } // namespace rcch
    } // namespace lc
} // namespace nxdn

#endif // __NXDN_LC_RCCH__MESSAGE_TYPE_SITE_INFO_H__
