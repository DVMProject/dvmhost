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
 * @file SNDCPCtxActAccept.h
 * @ingroup p25_sndcp
 * @file SNDCPCtxActAccept.cpp
 * @ingroup p25_sndcp
 */
#if !defined(__P25_SNDCP__SNDCPCTXACTACCEPT_H__)
#define  __P25_SNDCP__SNDCPCTXACTACCEPT_H__

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
         * @brief Represents a SNDCP PDU context accept response.
         * @ingroup p25_sndcp
         */
        class HOST_SW_API SNDCPCtxActAccept : public SNDCPPacket {
        public:
            /**
             * @brief Initializes a new instance of the SNDCPCtxActAccept class.
             */
            SNDCPCtxActAccept();

            /**
             * @brief Decode a SNDCP context activation response.
             * @param[in] data Buffer containing SNDCP packet data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            bool decode(const uint8_t* data) override;
            /**
             * @brief Encode a SNDCP context activation response.
             * @param[out] data Buffer to encode SNDCP packet data to.
             */
            void encode(uint8_t* data) override;

        public:
            /**
             * @brief Priority
             */
            __PROPERTY(uint8_t, priority, Priority);
            /**
             * @brief Ready Timer
             */
            __PROPERTY(uint8_t, readyTimer, ReadyTimer);
            /**
             * @brief Ready Timer
             */
            __PROPERTY(uint8_t, standbyTimer, StandbyTimer);
            /**
             * @brief Network Address Type
             */
            __PROPERTY(uint8_t, nat, NAT);

            /**
             * @brief IP Address
             */
            __PROPERTY(uint32_t, ipAddress, IPAddress);

            /**
             * @brief MTU
             */
            __PROPERTY(uint8_t, mtu, MTU);

            /**
             * @brief MDPCO
             */
            __PROPERTY(uint8_t, mdpco, MDPCO);

            __COPY(SNDCPCtxActAccept);

        private:
            uint16_t m_sndcpDAC;
        };
    } // namespace sndcp
} // namespace p25

#endif // __P25_SNDCP__SNDCPCTXACTACCEPT_H__
