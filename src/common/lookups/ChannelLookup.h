// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__CHANNEL_LOOKUP_H__)
#define __CHANNEL_LOOKUP_H__

#include "common/Defines.h"
#include "common/Timer.h"

#include <cstdio>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <functional>

namespace lookups
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Represents voice channel data.
    // ---------------------------------------------------------------------------

    class HOST_SW_API VoiceChData {
    public:
        /// <summary>Initializes a new instance of the VoiceChData class.</summary>
        VoiceChData() :
            m_chId(0U),
            m_chNo(0U),
            m_address(),
            m_port(),
            m_password(),
            m_ssl()
        {
            /* stub */
        }
        /// <summary>Initializes a new instance of the VoiceChData class.</summary>
        /// <param name="chId">Voice Channel Identity.</param>
        /// <param name="chNo">Voice Channel Number.</param>
        /// <param name="address">REST API Address.</param>
        /// <param name="port">REST API Port.</param>
        /// <param name="password">REST API Password.</param>
        /// <param name="ssl">Flag indicating REST is using SSL.</param>
        VoiceChData(uint8_t chId, uint32_t chNo, std::string address, uint16_t port, std::string password, bool ssl) :
            m_chId(chId),
            m_chNo(chNo),
            m_address(address),
            m_port(port),
            m_password(password),
            m_ssl(ssl)
        {
            /* stub */
        }

        /// <summary>Equals operator.</summary>
        /// <param name="data"></param>
        /// <returns></returns>
        VoiceChData & operator=(const VoiceChData & data)
        {
            if (this != &data) {
                m_chId = data.m_chId;
                m_chNo = data.m_chNo;
                m_address = data.m_address;
                m_port = data.m_port;
                m_password = data.m_password;
                m_ssl = data.m_ssl;
            }

            return *this;
        }

        /// <summary>Helper to determine if the channel identity is valid.</summary>
        bool isValidChId() const { return m_chId != 0U; }
        /// <summary>Helper to determine if the channel is valid.</summary>
        bool isValidCh() const { return m_chNo != 0U; }

    public:
        /// <summary>Voice Channel Identity.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, chId);
        /// <summary>Voice Channel Number.</summary>
        __READONLY_PROPERTY_PLAIN(uint32_t, chNo);
        /// <summary>REST API Address.</summary>
        __PROPERTY_PLAIN(std::string, address);
        /// <summary>REST API Port.</summary>
        __PROPERTY_PLAIN(uint16_t, port);
        /// <summary>REST API Password.</summary>
        __READONLY_PROPERTY_PLAIN(std::string, password);
        /// <summary>Flag indicating REST is using SSL.</summary>
        __PROPERTY_PLAIN(bool, ssl);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements a lookup table class that contains RF channel information.
    // ---------------------------------------------------------------------------

    class HOST_SW_API ChannelLookup {
    public:
        /// <summary>Initializes a new instance of the ChannelLookup class.</summary>
        ChannelLookup();
        /// <summary>Finalizes a instance of the ChannelLookup class.</summary>
        virtual ~ChannelLookup();

        /** RF Channel Data */
        /// <summary>Gets the count of RF channel data.</summary>
        uint8_t rfChDataSize() const { return m_rfChDataTable.size(); }
        /// <summary>Gets the RF channel data table.</summary>
        std::unordered_map<uint32_t, VoiceChData> rfChDataTable() const { return m_rfChDataTable; }
        /// <summary>Helper to set RF channel data.</summary>
        void setRFChData(const std::unordered_map<uint32_t, VoiceChData>& chData) { m_rfChDataTable = chData; }
        /// <summary>Helper to set RF channel data.</summary>
        void setRFChData(uint32_t chNo, VoiceChData chData) { m_rfChDataTable[chNo] = chData; }
        /// <summary>Helper to get RF channel data.</summary>
        VoiceChData getRFChData(uint32_t chNo) const;
        /// <summary>Helper to get first available channel number.</summary>
        uint32_t getFirstRFChannel() const { return m_rfChTable.at(0); }

        /// <summary>Gets the count of RF channels.</summary>
        uint8_t rfChSize() const { return m_rfChTable.size(); }
        /// <summary>Gets the RF channels table.</summary>
        std::vector<uint32_t> rfChTable() const { return m_rfChTable; }
        /// <summary>Helper to add a RF channel.</summary>
        bool addRFCh(uint32_t chNo, bool force = false);
        /// <summary>Helper to remove a RF channel.</summary>
        void removeRFCh(uint32_t chNo);
        /// <summary>Helper to determine if there are any RF channels available..</summary>
        bool isRFChAvailable() const { return !m_rfChTable.empty(); }

    private:
        std::vector<uint32_t> m_rfChTable;
        std::unordered_map<uint32_t, VoiceChData> m_rfChDataTable;
    };
} // namespace lookups

#endif // __CHANNEL_LOOKUP_H__
