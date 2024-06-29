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
 * @file SNDCPFactory.h
 * @ingroup p25_sndcp
 * @file SNDCPFactory.cpp
 * @ingroup p25_sndcp
 */
#if !defined(__P25_SNDCP__SNDCP_FACTORY_H__)
#define  __P25_SNDCP__SNDCP_FACTORY_H__

#include "common/Defines.h"

#include "common/p25/sndcp/SNDCPPacket.h"
#include "common/p25/sndcp/SNDCPCtxActAccept.h"
#include "common/p25/sndcp/SNDCPCtxActReject.h"
#include "common/p25/sndcp/SNDCPCtxActRequest.h"
#include "common/p25/sndcp/SNDCPCtxDeactivation.h"

namespace p25
{
    namespace sndcp
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Helper class to instantiate an instance of a SNDCP packet.
         * @ingroup p25_sndcp
         */
        class HOST_SW_API SNDCPFactory {
        public:
            /**
             * @brief Initializes a new instance of the SNDCPFactory class.
             */
            SNDCPFactory();
            /**
             * @brief Finalizes a instance of the SNDCPFactory class.
             */
            ~SNDCPFactory();

            /**
             * @brief Create an instance of a SNDCPPacket.
             * @param[in] data Buffer containing SNDCP packet data to decode.
             * @returns SNDCPPacket* Instance of a SNDCPPacket representing the decoded data.
             */
            static std::unique_ptr<SNDCPPacket> create(const uint8_t* data);

        private:
            /**
             * @brief Decode a SNDCP packet.
             * @param packet Instance of SNDCPPacket.
             * @param[in] data Buffer containing SNDCP packet data to decode.
             * @returns SNDCPPacket* Instance of a SNDCPPacket representing the decoded data.
             */
            static std::unique_ptr<SNDCPPacket> decode(SNDCPPacket* packet, const uint8_t* data);
        };
    } // namespace sndcp
} // namespace p25

#endif // __P25_SNDCP__SNDCP_FACTORY_H__
