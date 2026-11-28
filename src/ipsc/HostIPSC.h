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
 * @file HostIPSC.h
 * @ingroup ipsc
 * @file HostIPSC.cpp
 * @ingroup ipsc
 */
#if !defined(__HOST_IPSC_H__)
#define __HOST_IPSC_H__

#include "Defines.h"
#include "common/dmr/data/EmbeddedData.h"
#include "common/network/Network.h"
#include "common/yaml/Yaml.h"
#include "protocol/IPSCClient.h"

#include <chrono>
#include <string>
#include <unordered_map>

namespace ipsc
{
    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents the active call state for one DMR slot.
     * @ingroup ipsc
     */
    struct CallState {
        /** Flag indicating that the call originated from the IPSC network. */
        bool fromIPSC;
        /** Flag indicating that the call is active. */
        bool active;
        /** DMR voice burst index. */
        uint8_t n;
        /** IPSC call sequence number. */
        uint8_t sequence;
        /** DMR source radio ID. */
        uint32_t sourceId;
        /** DMR destination radio or talkgroup ID. */
        uint32_t destinationId;
        /** Flag indicating group-call operation. */
        bool group;
        /** DMR color code used to reconstruct air-interface bursts. */
        uint8_t colorCode;
        /** Time at which the most recent packet for the call was processed. */
        std::chrono::steady_clock::time_point lastActivity;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the standalone IPSC-to-DVM gateway application.
     * @ingroup ipsc
     */
    class HOST_SW_API HostIPSC {
    public:
        /**
         * @brief Initializes a new instance of the HostIPSC class.
         * @param configurationFile Full path to the configuration file.
         */
        explicit HostIPSC(const std::string& configurationFile);
        /** @brief Finalizes an instance of the HostIPSC class. */
        ~HostIPSC();

        /**
         * @brief Executes the main IPSC gateway processing loop.
         * @returns int Zero if successful, otherwise an error occurred.
         */
        int run();

    private:
        std::string m_confFile;
        yaml::Node m_conf;

        network::Network* m_network;
        IPSCClient* m_ipsc;

        CallState m_calls[2U];
        dmr::data::EmbeddedData m_embeddedData[2U];

        std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> m_recent;

        uint8_t m_colorCode;
        uint32_t m_callTimeout;

        /** @brief Creates and opens the DVM network connection. */
        bool createNetwork();
        /** @brief Creates and opens the Motorola IPSC connection. */
        bool createIPSC();

        /**
         * @brief Generates a fingerprint used to suppress reflected voice packets.
         * @param sourceId DMR source radio ID.
         * @param destinationId DMR destination radio or talkgroup ID.
         * @param slotNo DMR slot number.
         * @param data Voice frame data.
         * @param length Length of voice frame data in bytes.
         * @returns uint64_t Packet fingerprint.
         */
        static uint64_t fingerprint(uint32_t sourceId, uint32_t destinationId, uint8_t slotNo, const uint8_t* data, uint32_t length);
        /** @brief Adds a packet fingerprint to the loop-suppression cache. */
        void remember(uint64_t value);
        /** @brief Determines whether a packet fingerprint is present in the loop-suppression cache. */
        bool remembered(uint64_t value) const;

        /** @brief Removes expired packet fingerprints from the loop-suppression cache. */
        void expireRecent(std::chrono::steady_clock::time_point now);
        /** @brief Releases calls which have exceeded the configured inactivity timeout. */
        void expireCalls(std::chrono::steady_clock::time_point now);

        /** @brief Converts and forwards a received IPSC voice packet to the DVM network. */
        void processIPSC(const VoicePacket& packet);
        /** @brief Converts and forwards available DVM DMR traffic to the IPSC network. */
        void processNetwork();
    };
} // namespace ipsc

#endif // __HOST_IPSC_H__
