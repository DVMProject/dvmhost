// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - IPSC Gateway
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup ipsc_protocol IPSC Protocol
 * @brief Motorola IPSC protocol definitions.
 * @ingroup ipsc
 *
 * @file IPSCDefines.h
 * @ingroup ipsc_protocol
 */
#if !defined(__IPSC_PROTOCOL_DEFINES_H__)
#define __IPSC_PROTOCOL_DEFINES_H__

#include <cstdint>

namespace ipsc
{
    namespace defines
    {
        // Voice message flags
        constexpr uint8_t SLOT_ONE_FLAG = 0x00U;
        constexpr uint8_t SLOT_TWO_FLAG = 0x20U;

        constexpr uint8_t LAST_PACKET_FLAG = 0x40U;

        // Burst flags and masks
        constexpr uint8_t BURST_TYPE_MASK = 0x3FU;

        constexpr uint8_t SLOT_ONE_BURST_FLAG = 0x00U;
        constexpr uint8_t SLOT_TWO_BURST_FLAG = 0x80U;

        constexpr uint8_t VOICE_SYNC_FLAG = 0x40U;
        constexpr uint8_t VOICE_EMBEDDED_LC_FLAG = 0x10U;
        constexpr uint8_t VOICE_EMB_FLAG = 0x04U;
        constexpr uint8_t VOICE_LC_HARD_BITS_FLAG = 0x02U;

        // Peer mode and slot capability flags
        constexpr uint8_t PEER_DIGITAL_MODE = 0x20U;
        constexpr uint8_t PEER_ENABLED = 0x40U;

        constexpr uint8_t SLOT_ONE_IPSC_SUPPORT = 0x08U;
        constexpr uint8_t SLOT_TWO_IPSC_SUPPORT = 0x02U;

        // RTP payload type
        constexpr uint8_t RTP_PAYLOAD_AMBE = 120U;

        // BurstData extended flags
        constexpr uint16_t BURST_SLOT_TYPE_FLAG = 0x0008U;
        constexpr uint16_t BURST_SYNC_VOICE = 0x0001U;

        // IPSC wire-format lengths
        constexpr uint32_t RETR_HEADER_LENGTH = 5U;
        constexpr uint32_t PEER_REGISTRATION_LENGTH = 9U;
        constexpr uint32_t PEER_KEEP_ALIVE_LENGTH = 10U;
        constexpr uint32_t STATUS_MESSAGE_LENGTH = 14U;

        constexpr uint32_t VOICE_HEADER_LENGTH = 18U;
        constexpr uint32_t VOICE_PAYLOAD_OFFSET = 30U;

        constexpr uint32_t CALL_PLUS_TRAILER_LENGTH = 5U;

        constexpr uint32_t VOICE_BURST_BASE_LENGTH = 22U;
        constexpr uint32_t VOICE_LC_HARD_BITS_LENGTH = 4U;
        constexpr uint32_t VOICE_EMBEDDED_LC_LENGTH = 9U;
        constexpr uint32_t AUTHENTICATION_TAG_LENGTH = 10U;
        constexpr uint32_t AUTHENTICATION_KEY_LENGTH = 20U;
        constexpr uint32_t MAX_PACKET_LENGTH = 2048U;

        // Protocol versions and timing defaults
        constexpr uint16_t DEFAULT_PORT = 50000U;

        constexpr uint8_t PROTOCOL_MIN_VERSION = 1U;
        constexpr uint8_t PROTOCOL_MAX_VERSION = 8U;

        constexpr uint16_t PROTOCOL_TYPE_IPSC = 1U;
        constexpr uint16_t PROTOCOL_TYPE_SHIFT = 10U;
        constexpr uint16_t PROTOCOL_VERSION_MASK = 0x03FFU;

        /**
         * @brief Packs an IPSC protocol type and version into the wire-format protocol field.
         * @param[in] version Ten-bit IPSC protocol version.
         * @returns Packed 16-bit IPSC protocol field.
         */
        constexpr uint16_t encodeProtocolVersion(uint16_t version)
        {
            return uint16_t((PROTOCOL_TYPE_IPSC << PROTOCOL_TYPE_SHIFT) | (version & PROTOCOL_VERSION_MASK));
        }

        constexpr uint32_t REGISTRATION_RETRY_SECONDS = 5U;
        constexpr uint32_t KEEP_ALIVE_SECONDS = 6U;
        constexpr uint32_t MASTER_TIMEOUT_SECONDS = 45U;
        constexpr uint32_t PEER_TIMEOUT_SECONDS = 60U;

        constexpr uint32_t RTP_TIMESTAMP_STEP = 480U;

        constexpr uint8_t MAX_COLOR_CODE = 15U;

        constexpr uint32_t DEFAULT_CALL_TIMEOUT_SECONDS = 2U;

        /** @brief IPSC message opcodes. */
        enum class MessageType : uint8_t {
            CALL_CONTROL_REQUEST = 0x04U,
            COMMON_CSBK_CALL = 0x05U,
            PRIVATE_CALL_RESPONSE = 0x06U,
            CALL_CONTROL_RESPONSE = 0x08U,
            CALL_STATUS = 0x0AU,
            RCM_CALL_TRANSMISSION_STATUS = 0x61U,
            RCM_CALL_CONTROL_NOTIFICATION = 0x62U,
            RCM_REPEAT_BLOCKED_INDICATION = 0x63U,
            GROUP_VOICE = 0x80U,
            PRIVATE_VOICE = 0x81U,
            GROUP_VOICE_PLUS = 0x87U,
            PRIVATE_VOICE_PLUS = 0x88U,
            MASTER_REGISTRATION_REQUEST = 0x90U,
            MASTER_REGISTRATION_RESPONSE = 0x91U,
            MASTER_MAP_REQUEST = 0x92U,
            MASTER_MAP_BROADCAST = 0x93U,
            PEER_REGISTRATION_REQUEST = 0x94U,
            PEER_REGISTRATION_RESPONSE = 0x95U,
            MASTER_KEEP_ALIVE_REQUEST = 0x96U,
            MASTER_KEEP_ALIVE_RESPONSE = 0x97U,
            PEER_KEEP_ALIVE_REQUEST = 0x98U,
            PEER_KEEP_ALIVE_RESPONSE = 0x99U,
            DEREGISTRATION_REQUEST = 0x9AU,
            DEREGISTRATION_RESPONSE = 0x9BU
        };

        /** @brief IPSC RTP burst payload types. */
        enum class BurstType : uint8_t {
            VOICE_HEADER = 1U,
            VOICE_TERMINATOR = 2U,
            VOICE = 10U,
            VOICE_PLUS = 39U
        };

        /** @brief IPSC peer service capability flags. */
        enum class PeerService : uint32_t {
            VOICE_CALL = 0x00000004U,
            PACKET_AUTHENTICATION = 0x00000010U,
            CONSOLE_APPLICATION = 0x00002000U,
            CALL_MONITORING = 0x00004000U
        };
    } // namespace defines
} // namespace ipsc

#endif // __IPSC_PROTOCOL_DEFINES_H__
