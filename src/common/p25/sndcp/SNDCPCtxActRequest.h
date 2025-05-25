// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file SNDCPCtxActRequest.h
 * @ingroup p25_sndcp
 * @file SNDCPCtxActRequest.cpp
 * @ingroup p25_sndcp
 */
#if !defined(__P25_SNDCP__SNDCPCTXACTREQUEST_H__)
#define  __P25_SNDCP__SNDCPCTXACTREQUEST_H__

#include "common/Defines.h"
#include "common/p25/sndcp/SNDCPPacket.h"
#include "common/Utils.h"

#include <string>

namespace p25
{
    namespace sndcp
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents a SNDCP PDU context activation request.
         * @ingroup p25_sndcp
         */
        class HOST_SW_API SNDCPCtxActRequest : public SNDCPPacket {
        public:
            /**
             * @brief Initializes a new instance of the SNDCPCtxActRequest class.
             */
            SNDCPCtxActRequest();

            /**
             * @brief Decode a SNDCP context activation request.
             * @param[in] data Buffer containing SNDCP packet data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            bool decode(const uint8_t* data) override;
            /**
             * @brief Encode a SNDCP context activation request.
             * @param[out] data Buffer to encode SNDCP packet data to.
             */
            void encode(uint8_t* data) override;

        public:
            /**
             * @brief Network Address Type
             */
            DECLARE_PROPERTY(uint8_t, nat, NAT);
            
            /**
             * @brief IP Address
             */
            DECLARE_PROPERTY(ulong64_t, ipAddress, IPAddress);

            /**
             * @brief Data Subscriber Unit Type
             */
            DECLARE_PROPERTY(uint8_t, dsut, DSUT);

            /**
             * @brief MDPCO
             */
            DECLARE_PROPERTY(uint8_t, mdpco, MDPCO);

            DECLARE_COPY(SNDCPCtxActRequest);
        };
    } // namespace sndcp
} // namespace p25

#endif // __P25_SNDCP__SNDCPCTXACTREQUEST_H__
