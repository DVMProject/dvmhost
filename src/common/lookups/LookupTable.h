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
    //      Implements a abstract threading class that contains base logic for
    //      building tables of data.
    // ---------------------------------------------------------------------------

    template <class T>
    class HOST_SW_API LookupTable : public Thread {
    public:
        /// <summary>Initializes a new instance of the LookupTable class.</summary>
        /// <param name="filename">Full-path to the lookup table file.</param>
        /// <param name="reloadTime">Interval of time to reload the channel identity table.</param>
        LookupTable(const std::string& filename, uint32_t reloadTime) :
            Thread(),
            m_filename(filename),
            m_reloadTime(reloadTime),
            m_table(),
            m_stop(false)
        {
            /* stub */
        }

        /// <summary></summary>
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

        /// <summary>Stops and unloads this lookup table.</summary>
        virtual void stop()
        {
            if (m_reloadTime == 0U) {
                delete this;
                return;
            }

            m_stop = true;

            wait();
        }

        /// <summary>Reads the lookup table from the specified lookup table file.</summary>
        /// <returns>True, if lookup table was read, otherwise false.</returns>
        virtual bool read()
        {
            bool ret = load();

            if (m_reloadTime > 0U)
                run();

            return ret;
        }

        /// <summary>Clears all entries from the lookup table.</summary>
        virtual void clear()
        {
            // bryanb: this is not thread-safe and thread saftey should be implemented
            // on the derived class
            m_table.clear();
        }

        /// <summary>Helper to check if this lookup table has the specified unique ID.</summary>
        /// <param name="id">Unique ID to check for.</param>
        /// <returns>True, if the lookup table has an entry by the specified unique ID, otherwise false.</returns>
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

        /// <summary>Finds a table entry in this lookup table.</summary>
        /// <param name="id">Unique identifier for table entry.</param>
        /// <returns>Table entry.</returns>
        virtual T find(uint32_t id) = 0;

        /// <summary>Helper to return the lookup table.</summary>
        /// <returns>Table.</returns>
        virtual std::unordered_map<uint32_t, T> table() { return m_table; }

    protected:
        std::string m_filename;
        uint32_t m_reloadTime;
        std::unordered_map<uint32_t, T> m_table;
        bool m_stop;

        /// <summary>Loads the table from the passed lookup table file.</summary>
        /// <returns>True, if lookup table was loaded, otherwise false.</returns>
        virtual bool load() = 0;

        /// <summary>Saves the table from the lookup table in memory.</summary>
        /// <returns>True, if lookup table was saved, otherwise false.</returns>
        virtual bool save() = 0;
    };
} // namespace lookups

#endif // __LOOKUP_TABLE_H__
