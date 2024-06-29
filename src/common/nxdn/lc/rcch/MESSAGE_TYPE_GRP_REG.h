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
 * @file MESSAGE_TYPE_GRP_REG.h
 * @ingroup nxdn_rcch
 * @file MESSAGE_TYPE_GRP_REG.cpp
 * @ingroup nxdn_rcch
 */
#if !defined(__NXDN_LC_RCCH__MESSAGE_TYPE_GRP_REG_H__)
#define  __NXDN_LC_RCCH__MESSAGE_TYPE_GRP_REG_H__

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
             * @brief Implements GRP_REG - Group Registration Request (ISP) and
             *  Group Registration Response (OSP)
             * @ingroup nxdn_rcch 
             */
            class HOST_SW_API MESSAGE_TYPE_GRP_REG : public RCCH {
            public:
                /**
                 * @brief Initializes a new instance of the MESSAGE_TYPE_GRP_REG class.
                 */
                MESSAGE_TYPE_GRP_REG();

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
            };
        } // namespace rcch
    } // namespace lc
} // namespace nxdn

#endif // __NXDN_LC_RCCH__MESSAGE_TYPE_GRP_REG_H__
