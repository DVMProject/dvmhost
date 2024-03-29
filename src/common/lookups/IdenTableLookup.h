// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2018-2022,2024 Bryan Biedenkapp, N2PLL
*   Copyright (c) 2024 Patrick McDonnell, W3AXL
*
*/
#if !defined(__IDEN_TABLE_LOOKUP_H__)
#define __IDEN_TABLE_LOOKUP_H__

#include "common/Defines.h"
#include "common/lookups/LookupTable.h"

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
        /// <summary>Initializes a new instance of the IdenTable class.</summary>
        IdenTable() :
            m_channelId(0U),
            m_baseFrequency(0U),
            m_chSpaceKhz(0.0F),
            m_txOffsetMhz(0.0F),
            m_chBandwidthKhz(0.0F)
        {
            /* stub */
        }
        /// <summary>Initializes a new instance of the IdenTable class.</summary>
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
        __READONLY_PROPERTY_PLAIN(uint8_t, channelId);
        /// <summary>Base frequency for this entry.</summary>
        __READONLY_PROPERTY_PLAIN(uint32_t, baseFrequency);
        /// <summary>Channel spacing in kHz for this entry.</summary>
        __READONLY_PROPERTY_PLAIN(float, chSpaceKhz);
        /// <summary>Channel transmit offset in MHz for this entry.</summary>
        __READONLY_PROPERTY_PLAIN(float, txOffsetMhz);
        /// <summary>Channel bandwith in kHz for this entry.</summary>
        __READONLY_PROPERTY_PLAIN(float, chBandwidthKhz);
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

        /// <summary>Clears all entries from the lookup table.</summary>
        void clear() override;

        /// <summary>Finds a table entry in this lookup table.</summary>
        IdenTable find(uint32_t id) override;
        /// <summary>Returns the list of entries in this lookup table.</summary>
        std::vector<IdenTable> list();

    protected:
        /// <summary>Loads the table from the passed lookup table file.</summary>
        /// <returns>True, if lookup table was loaded, otherwise false.</returns>
        bool load() override;

        /// <summary>Saves the table to the passed lookup table file.</summary>
        /// <returns>True, if lookup table was saved, otherwise false.</returns>
        bool save() override;

    private:
        static std::mutex m_mutex;
    };
} // namespace lookups

#endif // __IDEN_TABLE_LOOKUP_H__
