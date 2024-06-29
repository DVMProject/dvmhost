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
 * @defgroup lookups Lookup Tables
 * @brief Implementation for various data lookup tables.
 * @ingroup common
 * 
 * @file LookupTable.h
 * @ingroup lookups
 */
#if !defined(__LOOKUP_TABLE_H__)
#define __LOOKUP_TABLE_H__

#include "common/Defines.h"
#include "common/Thread.h"
#include "common/Timer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <mutex>
#include <unordered_map>

namespace lookups
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements a abstract threading class that contains base logic for
     *  building tables of data.
     * @tparam T Atomic type this lookup table is for.
     * @ingroup lookups
     */
    template <class T>
    class HOST_SW_API LookupTable : public Thread {
    public:
        /**
         * @brief Initializes a new instance of the LookupTable class.
         * @param filename Full-path to the lookup table file.
         * @param reloadTime Interval of time to reload the channel identity table.
         */
        LookupTable(const std::string& filename, uint32_t reloadTime) :
            Thread(),
            m_filename(filename),
            m_reloadTime(reloadTime),
            m_table(),
            m_stop(false)
        {
            /* stub */
        }

        /**
         * @brief Thread entry point. This function is provided to run the thread
         *  for the lookup table.
         */
        void entry() override
        {
            if (m_reloadTime == 0U) {
                return;
            }

            Timer timer(1U, 60U * m_reloadTime);
            timer.start();

            while (!m_stop) {
                sleep(1000U);

                timer.clock();
                if (timer.hasExpired()) {
                    load();
                    timer.start();
                }
            }
        }

        /**
         * @brief Stops and unloads this lookup table.
         */
        virtual void stop()
        {
            if (m_reloadTime == 0U) {
                delete this;
                return;
            }

            m_stop = true;

            wait();
        }

        /**
         * @brief Reads the lookup table from the specified lookup table file.
         * @return True, if lookup table was read, otherwise false.
         */
        virtual bool read()
        {
            bool ret = load();

            if (m_reloadTime > 0U)
                run();
            setName("host:lookup-tbl");

            return ret;
        }

        /**
         * @brief Reads the lookup table from the specified lookup table file.
         * @return True, if lookup table was read, otherwise false.
         */
        virtual bool reload()
        {
            return load();
        }

        /**
         * @brief Clears all entries from the lookup table.
         */
        virtual void clear()
        {
            // bryanb: this is not thread-safe and thread saftey should be implemented
            // on the derived class
            m_table.clear();
        }

        /**
         * @brief Helper to check if this lookup table has the specified unique ID.
         * @param id Unique ID to check for.
         * @returns bools True, if the lookup table has an entry by the specified unique ID, otherwise false.
         */
        virtual bool hasEntry(uint32_t id)
        {
            try {
                m_table.at(id);
                return true;
            }
            catch (...) {
                return false;
            }

            return false;
        }

        /**
         * @brief Finds a table entry in this lookup table.
         * @param id Unique identifier for table entry.
         * @returns T Table entry.
         */
        virtual T find(uint32_t id) = 0;

        /**
         * @brief Helper to return the lookup table.
         * @returns std::unordered_map<uint32_t, T> Table.
         */
        virtual std::unordered_map<uint32_t, T> table() { return m_table; }

    protected:
        std::string m_filename;
        uint32_t m_reloadTime;
        std::unordered_map<uint32_t, T> m_table;
        bool m_stop;

        /**
         * @brief Loads the table from the passed lookup table file.
         * @returns bool True, if lookup table was loaded, otherwise false.
         */
        virtual bool load() = 0;

        /**
         * @brief Saves the table from the lookup table in memory.
         * @returns bool True, if lookup table was saved, otherwise false.
         */
        virtual bool save() = 0;
    };
} // namespace lookups

#endif // __LOOKUP_TABLE_H__