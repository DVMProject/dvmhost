// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
*  Copyright (C) 2015,2016,2017,2018 Jonathan Naylor, G4KLX
*  Copyright (C) 2017-2023 Bryan Biedenkapp, N2PLL
*
*/
/**
 * @defgroup network Host Networking
 * @brief Implementation for the host networking.
 * @ingroup host
 * 
 * @file Network.h
 * @ingroup network
 * @file Network.cpp
 * @ingroup network
 */
#if !defined(__NETWORK_H__)
#define __NETWORK_H__

#include "Defines.h"
#include "common/network/BaseNetwork.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/p25/kmm/KeysetItem.h"

#include <string>
#include <cstdint>
#include <functional>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the core peer networking logic.
     * @ingroup network
     */
    class HOST_SW_API Network : public BaseNetwork {
    public:
        /**
         * @brief Initializes a new instance of the Network class.
         * @param address Network Hostname/IP address to connect to.
         * @param port Network port number.
         * @param localPort 
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
        Network(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
            bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup);
        /**
         * @brief Finalizes a instance of the Network class.
         */
        ~Network() override;

        /**
         * @brief Resets the DMR ring buffer for the given slot.
         * @param slotNo DMR slot ring buffer to reset.
         */
        void resetDMR(uint32_t slotNo) override;
        /**
         * @brief Resets the P25 ring buffer.
         */
        void resetP25() override;
        /**
         * @brief Resets the NXDN ring buffer.
         */
        void resetNXDN() override;

        /**
         * @brief Sets the instances of the Radio ID and Talkgroup ID lookup tables.
         * @param ridLookup Radio ID Lookup Table Instance
         * @param tidLookup Talkgroup Rules Lookup Table Instance
         */
        void setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupRulesLookup* tidLookup);
        /**
         * @brief Sets metadata configuration settings from the modem.
         * @param identity Peer Identity.
         * @param rxFrequency Rx Frequency (Informational Only).
         * @param txFrequency Tx Frequency (Informational Only).
         * @param txOffsetMhz Tx Offset in MHz (Informational Only).
         * @param chBandwidthKhz Channel Bandwidth in kHz (Informational Only).
         * @param channelId Channel ID (Informational Only).
         * @param channelNo Channel Number (Informational Only).
         * @param power RF Power Level (Informational Only).
         * @param latitude Geographic Latitude (Informational Only).
         * @param longitude Geographic Longitude (Informational Only).
         * @param height Station Height in Meters (Informational Only).
         * @param location Textual Location (Informational Only).
         */
        void setMetadata(const std::string& identity, uint32_t rxFrequency, uint32_t txFrequency, float txOffsetMhz, float chBandwidthKhz,
            uint8_t channelId, uint32_t channelNo, uint32_t power, float latitude, float longitude, int height, const std::string& location);
        /**
         * @brief Sets REST API configuration settings from the modem.
         * @param password REST API Password.
         * @param port REST API Port.
         */
        void setRESTAPIData(const std::string& password, uint16_t port);
        /**
         * @brief Sets a flag indicating whether the conventional option is sent to the FNE.
         * @param conv Flag indicating conventional operation.
         */
        void setConventional(bool conv) { m_conventional = conv; }
        /**
         * @brief Sets endpoint preshared encryption key.
         * @param presharedKey Encryption preshared key for networking.
         */
        void setPresharedKey(const uint8_t* presharedKey);

        /**
         * @brief Updates the timer by the passed number of milliseconds.
         * @param ms Number of milliseconds.
         */
        void clock(uint32_t ms) override;

        /**
         * @brief Opens connection to the network.
         * @returns bool True, if networking has started, otherwise false.
         */
        bool open() override;

        /**
         * @brief Closes connection to the network.
         */
        void close() override;

        /**
         * @brief Flag indicating if this network connection enabled.
         * @returns bool Flag indicating if this network connection enabled.
         */
        bool isEnabled() const { return m_enabled; }
        /**
         * @brief Sets flag enabling network communication.
         * @param enabled Flag indicating if this network connection enabled.
         */
        void enable(bool enabled);

        /**
         * @brief Helper to set the DMR In-Call Control callback.
         * @param callback 
         */
        void setDMRICCCallback(std::function<void(NET_ICC::ENUM, uint32_t, uint8_t)>&& callback) { m_dmrInCallCallback = callback; }
        /**
         * @brief Helper to set the P25 In-Call Control callback.
         * @param callback 
         */
        void setP25ICCCallback(std::function<void(NET_ICC::ENUM, uint32_t)>&& callback) { m_p25InCallCallback = callback; }
        /**
         * @brief Helper to set the NXDN In-Call Control callback.
         * @param callback 
         */
        void setNXDNICCCallback(std::function<void(NET_ICC::ENUM, uint32_t)>&& callback) { m_nxdnInCallCallback = callback; }

        /**
         * @brief Helper to set the enc. key response callback.
         * @param callback 
         */
        void setKeyResponseCallback(std::function<void(p25::kmm::KeyItem, uint8_t, uint8_t)>&& callback) { m_keyRespCallback = callback; }

    public:
        /**
         * @brief Last received RTP sequence number.
         */
        __READONLY_PROPERTY_PLAIN(uint16_t, pktLastSeq);

    protected:
        std::string m_address;
        uint16_t m_port;

        std::string m_password;

        bool m_enabled;

        bool m_dmrEnabled;
        bool m_p25Enabled;
        bool m_nxdnEnabled;

        bool m_updateLookup;
        bool m_saveLookup;

        lookups::RadioIdLookup* m_ridLookup;
        lookups::TalkgroupRulesLookup* m_tidLookup;

        uint8_t* m_salt;

        Timer m_retryTimer;
        Timer m_timeoutTimer;

        uint32_t* m_rxDMRStreamId;
        uint32_t m_rxP25StreamId;
        uint32_t m_rxNXDNStreamId;

        uint16_t m_pktSeq;
        uint32_t m_loginStreamId;

        /** station metadata */
        std::string m_identity;
        uint32_t m_rxFrequency;
        uint32_t m_txFrequency;

        float m_txOffsetMhz;
        float m_chBandwidthKhz;
        uint8_t m_channelId;
        uint32_t m_channelNo;

        uint32_t m_power;
        float m_latitude;
        float m_longitude;
        int m_height;
        std::string m_location;

        std::string m_restApiPassword;
        uint16_t m_restApiPort;

        bool m_conventional;

        uint32_t m_remotePeerId;

        bool m_promiscuousPeer;

        std::function<void(NET_ICC::ENUM command, uint32_t dstId, uint8_t slotNo)> m_dmrInCallCallback;
        std::function<void(NET_ICC::ENUM command, uint32_t dstId)> m_p25InCallCallback;
        std::function<void(NET_ICC::ENUM command, uint32_t dstId)> m_nxdnInCallCallback;

        std::function<void(p25::kmm::KeyItem ki, uint8_t algId, uint8_t keyLength)> m_keyRespCallback;

        /**
         * @brief User overrideable handler that allows user code to process network packets not handled by this class.
         * @param peerId Peer ID.
         * @param opcode FNE network opcode pair.
         * @param[in] data Buffer containing message to send to peer.
         * @param length Length of buffer.
         * @param streamId Stream ID.
         */
        virtual void userPacketHandler(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data = nullptr, uint32_t length = 0U,
            uint32_t streamId = 0U);

        /**
         * @brief Writes login request to the network.
         * @returns bool True, if login request was sent, otherwise false.
         */
        bool writeLogin();
        /**
         * @brief Writes network authentication challenge.
         * @returns bool True, if authorization response was sent, otherwise false.
         */
        bool writeAuthorisation();
        /**
         * @brief Writes modem configuration to the network.
         * @returns bool True, if configuration response was sent, otherwise false.
         */
        virtual bool writeConfig();
        /**
         * @brief Writes a network stay-alive ping.
         * @returns bool True, if stay-alive ping was sent, otherwise false.
         */
        bool writePing();
    };
} // namespace network

#endif // __NETWORK_H__
