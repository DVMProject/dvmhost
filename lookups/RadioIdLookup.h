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
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2019 by Bryan Biedenkapp N2PLL
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
#if !defined(__RADIO_ID_LOOKUP_H__)
#define __RADIO_ID_LOOKUP_H__

#include "Defines.h"
#include "lookups/LookupTable.h"
#include "Thread.h"
#include "Mutex.h"

#include <string>
#include <unordered_map>

namespace lookups
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Represents an individual entry in the radio ID table.
    // ---------------------------------------------------------------------------

    class HOST_SW_API RadioId {
    public:
        /// <summary>Initializes a new insatnce of the RadioId class.</summary>
        RadioId() :
            m_radioEnabled(false),
            m_radioDefault(false)
        {
            /* stub */
        }
        /// <summary>Initializes a new insatnce of the RadioId class.</summary>
        /// <param name="radioEnabled"></param>
        /// <param name="radioDefault"></param>
        RadioId(bool radioEnabled, bool radioDefault) :
            m_radioEnabled(radioEnabled),
            m_radioDefault(radioDefault)
        {
            /* stub */
        }

        /// <summary>Equals operator. Copies this RadioId to another RadioId.</summary>
        RadioId& operator=(const RadioId& data)
        {
            if (this != &data) {
                m_radioEnabled = data.m_radioEnabled;
                m_radioDefault = data.m_radioDefault;
            }

            return *this;
        }

        /// <summary>Sets flag values.</summary>
        /// <param name="radioEnabled">Radio enabled.</param>
        /// <param name="radioDefault">Radio default.</param>
        void set(bool radioEnabled, bool radioDefault)
        {
            m_radioEnabled = radioEnabled;
            m_radioDefault = radioDefault;
        }

    public:
        /// <summary>Flag indicating if the radio is enabled.</summary>
        __READONLY_PROPERTY_PLAIN(bool, radioEnabled, radioEnabled);
        /// <summary>Flag indicating if the radio is default.</summary>
        __READONLY_PROPERTY_PLAIN(bool, radioDefault, radioDefault);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements a threading lookup table class that contains a radio ID
    //      lookup table.
    // ---------------------------------------------------------------------------

    class HOST_SW_API RadioIdLookup : public LookupTable<RadioId> {
    public:
        /// <summary>Initializes a new instance of the RadioIdLookup class.</summary>
        RadioIdLookup(const std::string& filename, uint32_t reloadTime, bool ridAcl);
        /// <summary>Finalizes a instance of the RadioIdLookup class.</summary>
        virtual ~RadioIdLookup();

        /// <summary>Toggles the specified radio ID enabled or disabled.</summary>
        void toggleEntry(uint32_t id, bool enabled);

        /// <summary>Adds a new entry to the lookup table by the specified unique ID.</summary>
        void addEntry(uint32_t id, bool enabled);
        /// <summary>Finds a table entry in this lookup table.</summary>
        virtual RadioId find(uint32_t id);

        /// <summary>Flag indicating whether radio ID access control is enabled or not.</summary>
        bool getACL();

    private:
        bool m_acl;

        /// <summary>Parses a table entry from the passed comma delimited string.</summary>
        virtual RadioId parse(std::string tableEntry);
    };
} // namespace lookups

#endif // __RADIO_ID_LOOKUP_H__
