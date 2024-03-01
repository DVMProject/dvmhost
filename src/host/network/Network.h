// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2015,2016,2017,2018 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2023 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__NETWORK_H__)
#define __NETWORK_H__

#include "Defines.h"
#include "common/network/BaseNetwork.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"

#include <string>
#include <cstdint>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements the core peer networking logic.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Network : public BaseNetwork {
    public:
        /// <summary>Initializes a new instance of the Network class.</summary>
        Network(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
            bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup);
        /// <summary>Finalizes a instance of the Network class.</summary>
        ~Network() override;

        /// <summary>Resets the DMR ring buffer for the given slot.</summary>
        void resetDMR(uint32_t slotNo) override;
        /// <summary>Resets the P25 ring buffer.</summary>
        void resetP25() override;
        /// <summary>Resets the NXDN ring buffer.</summary>
        void resetNXDN() override;

        /// <summary>Sets the instances of the Radio ID and Talkgroup ID lookup tables.</summary>
        void setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupRulesLookup* tidLookup);
        /// <summary>Sets metadata configuration settings from the modem.</summary>
        void setMetadata(const std::string& callsign, uint32_t rxFrequency, uint32_t txFrequency, float txOffsetMhz, float chBandwidthKhz,
            uint8_t channelId, uint32_t channelNo, uint32_t power, float latitude, float longitude, int height, const std::string& location);
        /// <summary>Sets REST API configuration settings from the modem.</summary>
        void setRESTAPIData(const std::string& password, uint16_t port);
        /// <summary>Sets endpoint preshared encryption key.</summary>
        void setPresharedKey(const uint8_t* presharedKey);

        /// <summary>Updates the timer by the passed number of milliseconds.</summary>
        void clock(uint32_t ms) override;

        /// <summary>Opens connection to the network.</summary>
        bool open() override;

        /// <summary>Closes connection to the network.</summary>
        void close() override;

        /// <summary>Sets flag enabling network communication.</summary>
        void enable(bool enabled);

    public:
        /// <summary>Last received RTP sequence number.</summary>
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

        uint32_t m_remotePeerId;

        /// <summary>Writes login request to the network.</summary>
        bool writeLogin();
        /// <summary>Writes network authentication challenge.</summary>
        bool writeAuthorisation();
        /// <summary>Writes modem configuration to the network.</summary>
        virtual bool writeConfig();
        /// <summary>Writes a network stay-alive ping.</summary>
        bool writePing();
    };
} // namespace network

#endif // __NETWORK_H__
