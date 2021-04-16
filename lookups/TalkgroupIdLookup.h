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
#if !defined(__TALKGROUP_ID_LOOKUP_H__)
#define __TALKGROUP_ID_LOOKUP_H__

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
    //     Represents an individual entry in the talkgroup ID table.
    // ---------------------------------------------------------------------------

    class HOST_SW_API TalkgroupId {
    public:
        /// <summary>Initializes a new insatnce of the TalkgroupId class.</summary>
        TalkgroupId() :
            m_tgEnabled(false),
            m_tgSlot(0U),
            m_tgDefault(false)
        {
            /* stub */
        }
        /// <summary>Initializes a new insatnce of the TalkgroupId class.</summary>
        /// <param name="tgEnabled"></param>
        /// <param name="tgSlot"></param>
        /// <param name="tgDefault"></param>
        TalkgroupId(bool tgEnabled, uint8_t tgSlot, bool tgDefault) :
            m_tgEnabled(tgEnabled),
            m_tgSlot(tgSlot),
            m_tgDefault(tgDefault)
        {
            /* stub */
        }

        /// <summary>Equals operator. Copies this TalkgroupId to another TalkgroupId.</summary>
        TalkgroupId& operator=(const TalkgroupId& data)
        {
            if (this != &data) {
                m_tgEnabled = data.m_tgEnabled;
                m_tgSlot = data.m_tgSlot;
                m_tgDefault = data.m_tgDefault;
            }

            return *this;
        }

        /// <summary>Sets talkgroup values.</summary>
        /// <param name="tgEnabled">Talkgroup Enabled.</param>
        /// <param name="tgSlot">Talkgroup DMR slot.</param>
        /// <param name="tgDefault">Talkgroup Default.</param>
        void set(bool tgEnabled, uint8_t tgSlot, bool tgDefault)
        {
            m_tgEnabled = tgEnabled;
            m_tgSlot = tgSlot;
            m_tgDefault = tgDefault;
        }

    public:
        /// <summary>Flag indicating if the talkgroup is enabled.</summary>
        __READONLY_PROPERTY_PLAIN(bool, tgEnabled, tgEnabled);
        /// <summary>Talkgroup DMR slot.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, tgSlot, tgSlot);
        /// <summary>Flag indicating if the talkgroup is default.</summary>
        __READONLY_PROPERTY_PLAIN(bool, tgDefault, tgDefault);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements a threading lookup table class that contains a talkgroup
    //      ID lookup table.
    // ---------------------------------------------------------------------------

    class HOST_SW_API TalkgroupIdLookup : public LookupTable<TalkgroupId> {
    public:
        /// <summary>Initializes a new instance of the TalkgroupIdLookup class.</summary>
        TalkgroupIdLookup(const std::string& filename, uint32_t reloadTime, bool tidAcl);
        /// <summary>Finalizes a instance of the TalkgroupIdLookup class.</summary>
        virtual ~TalkgroupIdLookup();

        /// <summary>Adds a new entry to the lookup table by the specified unique ID.</summary>
        void addEntry(uint32_t id, unsigned char slot, bool enabled);
        /// <summary>Finds a table entry in this lookup table.</summary>
        virtual TalkgroupId find(uint32_t id);

        /// <summary>Flag indicating whether talkgroup ID access control is enabled or not.</summary>
        bool getACL();

    private:
        bool m_acl;

        /// <summary>Parses a table entry from the passed comma delimited string.</summary>
        virtual TalkgroupId parse(std::string tableEntry);
    };
} // namespace lookups

#endif // __TALKGROUP_ID_LOOKUP_H__
