// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2016 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2022,2024 Bryan Biedenkapp, N2PLL
*   Copyright (c) 2024 Patrick McDonnell, W3AXL
*
*/
#if !defined(__RADIO_ID_LOOKUP_H__)
#define __RADIO_ID_LOOKUP_H__

#include "common/Defines.h"
#include "common/lookups/LookupTable.h"

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
        /// <summary>Initializes a new instance of the RadioId class.</summary>
        RadioId() :
            m_radioEnabled(false),
            m_radioDefault(false),
            m_radioAlias("")
        {
            /* stub */
        }
        /// <summary>Initializes a new instance of the RadioId class.</summary>
        /// <param name="radioEnabled"></param>
        /// <param name="radioDefault"></param>
        RadioId(bool radioEnabled, bool radioDefault) :
            m_radioEnabled(radioEnabled),
            m_radioDefault(radioDefault),
            m_radioAlias("")
        {
            /* stub */
        }

        /// <summary>Initializes a new instance of the RadioId class.</summary>
        /// <param name="radioEnabled"></param>
        /// <param name="radioDefault"></param>
        /// <param name="radioAlias"></param>
        RadioId(bool radioEnabled, bool radioDefault, const std::string& radioAlias) :
            m_radioEnabled(radioEnabled),
            m_radioDefault(radioDefault),
            m_radioAlias(radioAlias)
        {
            /* stub */
        }

        /// <summary>Equals operator. Copies this RadioId to another RadioId.</summary>
        RadioId& operator=(const RadioId& data)
        {
            if (this != &data) {
                m_radioEnabled = data.m_radioEnabled;
                m_radioDefault = data.m_radioDefault;
                m_radioAlias = data.m_radioAlias;
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

        /// <summary>Sets flag values.</summary>
        /// <param name="radioEnabled">Radio enabled.</param>
        /// <param name="radioDefault">Radio default.</param>
        /// <param name="radioAlias"></param>
        void set(bool radioEnabled, bool radioDefault, const std::string& radioAlias)
        {
            m_radioEnabled = radioEnabled;
            m_radioDefault = radioDefault;
            m_radioAlias = radioAlias;
        }

    public:
        /// <summary>Flag indicating if the radio is enabled.</summary>
        __READONLY_PROPERTY_PLAIN(bool, radioEnabled);
        /// <summary>Flag indicating if the radio is default.</summary>
        __READONLY_PROPERTY_PLAIN(bool, radioDefault);
        /// <summary>Alias for the radio.</summary>
        __READONLY_PROPERTY_PLAIN(std::string, radioAlias);
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

        /// <summary>Clears all entries from the lookup table.</summary>
        void clear() override;

        /// <summary>Toggles the specified radio ID enabled or disabled.</summary>
        void toggleEntry(uint32_t id, bool enabled);

        /// <summary>Adds a new entry to the lookup table by the specified unique ID, with an alias.</summary>
        void addEntry(uint32_t id, bool enabled, const std::string& alias);

        /// <summary>Erases an existing entry from the lookup table by the specified unique ID.</summary>
        void eraseEntry(uint32_t id);
        /// <summary>Finds a table entry in this lookup table.</summary>
        RadioId find(uint32_t id) override;

        /// <summary>Saves loaded radio ID lookups.</summary>
        void commit();

        /// <summary>Flag indicating whether radio ID access control is enabled or not.</summary>
        bool getACL();

    protected:
        bool m_acl;

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

#endif // __RADIO_ID_LOOKUP_H__
