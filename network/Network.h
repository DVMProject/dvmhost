/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2015,2016,2017,2018 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2020 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__NETWORK_H__)
#define __NETWORK_H__

#include "Defines.h"
#include "network/BaseNetwork.h"
#include "lookups/RadioIdLookup.h"
#include "lookups/TalkgroupIdLookup.h"

#include <string>
#include <cstdint>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements the core networking logic.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Network : public BaseNetwork {
    public:
        /// <summary>Initializes a new instance of the Network class.</summary>
        Network(const std::string& address, uint16_t port, uint16_t local, uint32_t id, const std::string& password,
            bool duplex, bool debug, bool dmr, bool p25, bool slot1, bool slot2, bool transferActivityLog, bool transferDiagnosticLog, bool updateLookup);
        /// <summary>Finalizes a instance of the Network class.</summary>
        ~Network();

        /// <summary>Sets the instances of the Radio ID and Talkgroup ID lookup tables.</summary>
        void setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupIdLookup* tidLookup);
        /// <summary>Sets metadata configuration settings from the modem.</summary>
        void setMetadata(const std::string& callsign, uint32_t rxFrequency, uint32_t txFrequency, float txOffsetMhz, float chBandwidthKhz, 
            uint8_t channelId, uint32_t channelNo, uint32_t power, float latitude, float longitude, int height, const std::string& location);
        /// <summary>Sets RCON configuration settings from the modem.</summary>
        void setRconData(const std::string& password, uint16_t port);
        /// <summary>Gets the current status of the network.</summary>
        uint8_t getStatus();

        /// <summary>Updates the timer by the passed number of milliseconds.</summary>
        void clock(uint32_t ms);

        /// <summary>Opens connection to the network.</summary>
        bool open();

        /// <summary>Sets flag enabling network communication.</summary>
        void enable(bool enabled);

        /// <summary>Closes connection to the network.</summary>
        void close();

    private:
        std::string m_address;
        uint16_t m_port;

        std::string m_password;

        bool m_enabled;

        bool m_dmrEnabled;
        bool m_p25Enabled;

        bool m_updateLookup;

        lookups::RadioIdLookup* m_ridLookup;
        lookups::TalkgroupIdLookup* m_tidLookup;

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

        std::string m_rconPassword;
        uint16_t m_rconPort;

        /// <summary>Writes login request to the network.</summary>
        bool writeLogin();
        /// <summary>Writes network authentication challenge.</summary>
        bool writeAuthorisation();
        /// <summary>Writes modem configuration to the network.</summary>
        bool writeConfig();
        /// <summary>Writes a network stay-alive ping.</summary>
        bool writePing();
    };
} // namespace network

#endif // __NETWORK_H__
