// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - TG Patch
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup patch_network Networking
 * @brief Implementation for the bridge networking.
 * @ingroup patch
 *
 * @file PeerNetwork.h
 * @ingroup patch_network
 * @file PeerNetwork.cpp
 * @ingroup patch_network
 */
#if !defined(__PEER_NETWORK_H__)
#define __PEER_NETWORK_H__

#include "Defines.h"
#include "common/dmr/data/EmbeddedData.h"
#include "common/network/Network.h"

#include <string>
#include <cstdint>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements the core peer networking logic.
    // ---------------------------------------------------------------------------

    class HOST_SW_API PeerNetwork : public Network {
    public:
        /**
         * @brief Initializes a new instance of the PeerNetwork class.
         * @param address Network Hostname/IP address to connect to.
         * @param port Network port number.
         * @param local 
         * @param peerId Unique ID on the network.
         * @param password Network authentication password.
         * @param duplex Flag indicating full-duplex operation.
         * @param debug Flag indicating whether network debug is enabled.
         * @param dmr Flag indicating whether DMR is enabled.
         * @param p25 Flag indicating whether P25 is enabled.
         * @param nxdn Flag indicating whether NXDN is enabled.
         * @param slot1 Flag indicating whether DMR slot 1 is enabled for network traffic.
         * @param slot2 Flag indicating whether DMR slot 2 is enabled for network traffic.
         * @param allowActivityTransfer Flag indicating that the system activity logs will be sent to the network.
         * @param allowDiagnosticTransfer Flag indicating that the system diagnostic logs will be sent to the network.
         * @param updateLookup Flag indicating that the system will accept radio ID and talkgroup ID lookups from the network.
         */
        PeerNetwork(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
            bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup);

        /**
         * @brief Writes P25 LDU1 frame data to the network.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         * @param[in] lsd Instance of p25::data::LowSpeedData containing low speed data.
         * @param[in] data Buffer containing P25 LDU1 data to send.
         * @param[in] frameType DVM P25 frame type.
         * @returns bool True, if message was sent, otherwise false.
         */
        bool writeP25LDU1(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data, 
            p25::defines::FrameType::E frameType) override;
        /**
         * @brief Writes P25 LDU2 frame data to the network.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         * @param[in] lsd Instance of p25::data::LowSpeedData containing low speed data.
         * @param[in] data Buffer containing P25 LDU2 data to send.
         * @returns bool True, if message was sent, otherwise false.
         */
        bool writeP25LDU2(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data) override;

        /**
         * @brief Helper to send a DMR terminator with LC message.
         * @param data 
         * @param seqNo 
         * @param dmrN 
         * @param embeddedData 
         */
        void writeDMRTerminator(dmr::data::NetData& data, uint32_t* seqNo, uint8_t* dmrN, dmr::data::EmbeddedData& embeddedData);

    protected:
        /**
         * @brief Writes configuration to the network.
         * @returns bool True, if configuration was sent, otherwise false.
         */
        bool writeConfig() override;

    private:
        /**
         * @brief Creates an P25 LDU1 frame message.
         * 
         *  The data packed into a P25 LDU1 frame message is near standard DFSI messaging, just instead of
         *  9 individual frames, they are packed into a single message one right after another.
         * 
         * @param[out] length Length of network message buffer.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         * @param[in] lsd Instance of p25::data::LowSpeedData containing low speed data.
         * @param[in] data Buffer containing P25 LDU1 data to send.
         * @param[in] frameType DVM P25 frame type.
         * @returns UInt8Array Buffer containing the built network message.
         */
        UInt8Array createP25_LDU1Message_Raw(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
            const uint8_t* data, p25::defines::FrameType::E frameType);
        /**
         * @brief Creates an P25 LDU2 frame message.
         * 
         *  The data packed into a P25 LDU2 frame message is near standard DFSI messaging, just instead of
         *  9 individual frames, they are packed into a single message one right after another.
         * 
         * @param[out] length Length of network message buffer.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         * @param[in] lsd Instance of p25::data::LowSpeedData containing low speed data.
         * @param[in] data Buffer containing P25 LDU2 data to send.
         * @returns UInt8Array Buffer containing the built network message.
         */
        UInt8Array createP25_LDU2Message_Raw(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
            const uint8_t* data);
    };
} // namespace network

#endif // __PEER_NETWORK_H__
