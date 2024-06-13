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
*   Copyright (C) 2017-2022 Bryan Biedenkapp, N2PLL
*   Copyright (c) 2024 Caleb, KO4UYJ
*
*/
#if !defined(__PEER_LIST_LOOKUP_H__)
#define __PEER_LIST_LOOKUP_H__

#include "common/Defines.h"
#include "common/lookups/LookupTable.h"

#include <string>
#include <vector>
#include <mutex>

namespace lookups
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements a threading lookup table class that contains peer whitelist
    //      and blacklist.
    // ---------------------------------------------------------------------------

    class HOST_SW_API PeerListLookup : public LookupTable<uint32_t> {
    public:
        enum Mode {
            WHITELIST,
            BLACKLIST
        };

        /// <summary>Initializes a new instance of the PeerListLookup class.</summary>
        PeerListLookup(const std::string& listFile, Mode mode, uint32_t reloadTime, bool enabled);

        /// <summary>Clears all entries from the list.</summary>
        void clear() override;

        /// <summary>Adds a new entry to the list.</summary>
        void addEntry(uint32_t peerId);

        /// <summary>Removes an existing entry from the list.</summary>
        void removeEntry(uint32_t peerId);

        /// <summary>Checks if a peer ID is in the list.</summary>
        bool isPeerInList(uint32_t peerId) const;

        /// <summary>Checks if a peer ID is allowed based on the mode and enabled flag.</summary>
        bool isPeerAllowed(uint32_t peerId) const;

        /// <summary>Sets the mode to either WHITELIST or BLACKLIST.</summary>
        void setMode(Mode mode);

        /// <summary>Gets the current mode.</summary>
        Mode getMode() const;

        /// <summary>Sets whether the lookup is enabled.</summary>
        void setEnabled(bool enabled);

        /// <summary>Finds a table entry in this lookup table.</summary>
        uint32_t find(uint32_t id) override;

    protected:
        bool load() override;
        bool save() override;

    private:
        std::string m_listFile;
        Mode m_mode;
        bool m_enabled;
        std::vector<uint32_t> m_list;
        static std::mutex m_mutex;

        bool loadList();
        bool saveList() const;
    };
} // namespace lookups

#endif // __PEER_LIST_LOOKUP_H__