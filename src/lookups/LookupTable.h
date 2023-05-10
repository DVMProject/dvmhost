/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2018-2022 by Bryan Biedenkapp N2PLL
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
#if !defined(__LOOKUP_TABLE_H__)
#define __LOOKUP_TABLE_H__

#if _MSC_VER > 1910
#pragma warning(disable : 4834) // [[nodiscard]] warning -- we don't care about this here
#endif

#include "Defines.h"
#include "Log.h"
#include "Thread.h"
#include "Timer.h"
#include "Mutex.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
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
            m_table()
        {
            /* stub */
        }
        /// <summary>Finalizes a instance of the LookupTable class.</summary>
        virtual ~LookupTable()
        {
            /* stub */
        }

        /// <summary></summary>
        virtual void entry()
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
            m_mutex.lock();
            {
                m_table.clear();
            }
            m_mutex.unlock();
        }

        /// <summary>Helper to check if this lookup table has the specified unique ID.</summary>
        /// <param name="id">Unique ID to check for.</param>
        /// <returns>True, if the lookup table has an entry by the specified unique ID, otherwise false.</returns>
        virtual bool hasEntry(uint32_t id)
        {
            m_mutex.lock();
            {
                try {
                    m_table.at(id);
                    return true;
                }
                catch (...) {
                    return false;
                }
            }
            m_mutex.unlock();

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
        Mutex m_mutex;
        bool m_stop;

        bool m_acl;

        /// <summary>Loads the table from the passed lookup table file.</summary>
        /// <returns>True, if lookup table was loaded, otherwise false.</returns>
        virtual bool load() = 0;
    };
} // namespace lookups

#endif // __LOOKUP_TABLE_H__
