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
 * @file SNDCPCtxDeactivation.h
 * @ingroup p25_sndcp
 * @file SNDCPCtxDeactivation.cpp
 * @ingroup p25_sndcp
 */
#if !defined(__P25_SNDCP__SNDCPCTXDEACTIVATION_H__)
#define  __P25_SNDCP__SNDCPCTXDEACTIVATION_H__

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
         * @brief Represents a SNDCP PDU context deactivation.
         * @ingroup p25_sndcp
         */
        class HOST_SW_API SNDCPCtxDeactivation : public SNDCPPacket {
        public:
            /**
             * @brief Initializes a new instance of the SNDCPCtxDeactivation class.
             */
            SNDCPCtxDeactivation();

            /**
             * @brief Decode a SNDCP context deactivation packet.
             * @param[in] data Buffer containing SNDCP packet data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            bool decode(const uint8_t* data);
            /**
             * @brief Encode a SNDCP context deactivation packet.
             * @param[out] data Buffer to encode SNDCP packet data to.
             */
            void encode(uint8_t* data);

        public:
            /**
             * @brief Deactivation Type
             */
            __PROPERTY(uint8_t, deactType, DeactType);

            __COPY(SNDCPCtxDeactivation);
        };
    } // namespace sndcp
} // namespace p25

#endif // __P25_SNDCP__SNDCPCTXDEACTIVATION_H__
