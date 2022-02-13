/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2018-2019 by Bryan Biedenkapp N2PLL
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
#if !defined(__IDEN_TABLE_LOOKUP_H__)
#define __IDEN_TABLE_LOOKUP_H__

#include "Defines.h"
#include "lookups/LookupTable.h"
#include "Thread.h"
#include "Mutex.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace lookups
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Represents an individual entry in the bandplan identity table.
    // ---------------------------------------------------------------------------

    class HOST_SW_API IdenTable {
    public:
        /// <summary>Initializes a new insatnce of the IdenTable class.</summary>
        IdenTable() :
            m_channelId(0U),
            m_baseFrequency(0U),
            m_chSpaceKhz(0.0F),
            m_txOffsetMhz(0.0F),
            m_chBandwidthKhz(0.0F)
        {
            /* stub */
        }
        /// <summary>Initializes a new insatnce of the IdenTable class.</summary>
        /// <param name="channelId"></param>
        /// <param name="baseFrequency"></param>
        /// <param name="chSpaceKhz"></param>
        /// <param name="txOffsetMhz"></param>
        /// <param name="chBandwidthKhz"></param>
        IdenTable(uint8_t channelId, uint32_t baseFrequency, float chSpaceKhz, float txOffsetMhz, float chBandwidthKhz) :
            m_channelId(channelId),
            m_baseFrequency(baseFrequency),
            m_chSpaceKhz(chSpaceKhz),
            m_txOffsetMhz(txOffsetMhz),
            m_chBandwidthKhz(chBandwidthKhz)
        {
            /* stub */
        }

        /// <summary>Equals operator.</summary>
        /// <param name="data"></param>
        /// <returns></returns>
        IdenTable& operator=(const IdenTable& data)
        {
            if (this != &data) {
                m_channelId = data.m_channelId;
                m_baseFrequency = data.m_baseFrequency;
                m_chSpaceKhz = data.m_chSpaceKhz;
                m_txOffsetMhz = data.m_txOffsetMhz;
                m_chBandwidthKhz = data.m_chBandwidthKhz;
            }

            return *this;
        }

    public:
        /// <summary>Channel ID for this entry.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, channelId, channelId);
        /// <summary>Base frequency for this entry.</summary>
        __READONLY_PROPERTY_PLAIN(uint32_t, baseFrequency, baseFrequency);
        /// <summary>Channel spacing in kHz for this entry.</summary>
        __READONLY_PROPERTY_PLAIN(float, chSpaceKhz, chSpaceKhz);
        /// <summary>Channel transmit offset in MHz for this entry.</summary>
        __READONLY_PROPERTY_PLAIN(float, txOffsetMhz, txOffsetMhz);
        /// <summary>Channel bandwith in kHz for this entry.</summary>
        __READONLY_PROPERTY_PLAIN(float, chBandwidthKhz, chBandwidthKhz);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements a threading lookup table class that contains the bandplan 
    //      identity table.
    // ---------------------------------------------------------------------------

    class HOST_SW_API IdenTableLookup : public LookupTable<IdenTable> {
    public:
        /// <summary>Initializes a new instance of the IdenTableLookup class.</summary>
        IdenTableLookup(const std::string& filename, uint32_t reloadTime);
        /// <summary>Finalizes a instance of the IdenTableLookup class.</summary>
        virtual ~IdenTableLookup();

        /// <summary>Finds a table entry in this lookup table.</summary>
        virtual IdenTable find(uint32_t id);
        /// <summary>Returns the list of entries in this lookup table.</summary>
        std::vector<IdenTable> list();

    private:
        /// <summary>Parses a table entry from the passed comma delimited string.</summary>
        IdenTable parse(std::string tableEntry);
    };
} // namespace lookups

#endif // __IDEN_TABLE_LOOKUP_H__
