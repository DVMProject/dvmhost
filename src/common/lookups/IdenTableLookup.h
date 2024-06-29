// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2018-2022,2024 Bryan Biedenkapp, N2PLL
 *  Copyright (c) 2024 Patrick McDonnell, W3AXL
 *
 */
/**
 * @defgroup lookups_iden Channel Identity Table Lookups
 * @brief Implementation for channel identity lookup tables.
 * @ingroup lookups
 * 
 * @file IdenTableLookup.h
 * @ingroup lookups_iden
 * @file IdenTableLookup.cpp
 * @ingroup lookups_iden
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
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents an individual entry in the bandplan identity table.
     * @ingroup lookups_iden
     */
    class HOST_SW_API IdenTable {
    public:
        /**
         * @brief Initializes a new instance of the IdenTable class.
         */
        IdenTable() :
            m_channelId(0U),
            m_baseFrequency(0U),
            m_chSpaceKhz(0.0F),
            m_txOffsetMhz(0.0F),
            m_chBandwidthKhz(0.0F)
        {
            /* stub */
        }
        /**
         * @brief Initializes a new instance of the IdenTable class.
         * @param channelId Channel ID.
         * @param baseFrequency Base Frequency.
         * @param chSpaceKhz Channel Spacing in kHz.
         * @param txOffsetMhz Transmit Offset in MHz.
         * @param chBandwidthKhz Channel Bandwidth in kHz.
         */
        IdenTable(uint8_t channelId, uint32_t baseFrequency, float chSpaceKhz, float txOffsetMhz, float chBandwidthKhz) :
            m_channelId(channelId),
            m_baseFrequency(baseFrequency),
            m_chSpaceKhz(chSpaceKhz),
            m_txOffsetMhz(txOffsetMhz),
            m_chBandwidthKhz(chBandwidthKhz)
        {
            /* stub */
        }

        /**
         * @brief Equals operator.
         * @param data Instance of IdenTable to copy.
         */
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
        /**
         * @brief Channel ID for this entry.
         */
        __READONLY_PROPERTY_PLAIN(uint8_t, channelId);
        /**
         * @brief Base frequency for this entry.
         */
        __READONLY_PROPERTY_PLAIN(uint32_t, baseFrequency);
        /**
         * @brief Channel spacing in kHz for this entry.
         */
        __READONLY_PROPERTY_PLAIN(float, chSpaceKhz);
        /**
         * @brief Channel transmit offset in MHz for this entry.
         */
        __READONLY_PROPERTY_PLAIN(float, txOffsetMhz);
        /**
         * @brief Channel bandwith in kHz for this entry.
         */
        __READONLY_PROPERTY_PLAIN(float, chBandwidthKhz);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
   // ---------------------------------------------------------------------------

    /**
     * @brief Implements a threading lookup table class that contains the bandplan
     *  identity table.
     * @ingroup lookups_iden
     */
    class HOST_SW_API IdenTableLookup : public LookupTable<IdenTable> {
    public:
        /**
         * @brief Initializes a new instance of the IdenTableLookup class.
         * @param filename Full-path to the channel identity table file.
         * @param reloadTime Interval of time to reload the channel identity table.
         */
        IdenTableLookup(const std::string& filename, uint32_t reloadTime);

        /**
         * @brief Clears all entries from the lookup table.
         */
        void clear() override;

        /**
         * @brief Finds a table entry in this lookup table.
         * @param id Unique identifier for table entry.
         * @returns IdenTable Table entry.
         */
        IdenTable find(uint32_t id) override;
        /**
         * @brief Returns the list of entries in this lookup table.
         * @returns std::vector<IdenTable> List of all entries in the lookup table.
         */
        std::vector<IdenTable> list();

    protected:
        /**
         * @brief Loads the table from the passed lookup table file.
         * @returns bool True, if lookup table was loaded, otherwise false.
         */
        bool load() override;

        /**
         * @brief Saves the table to the passed lookup table file.
         * @returns bool True, if lookup table was saved, otherwise false.
         */
        bool save() override;

    private:
        static std::mutex m_mutex;
    };
} // namespace lookups

#endif // __IDEN_TABLE_LOOKUP_H__
