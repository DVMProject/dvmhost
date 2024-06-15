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
    //      Represents an individual entry in the peer ID table.
    // ---------------------------------------------------------------------------

    class HOST_SW_API PeerId {
    public:
        /// <summary>Initializes a new instance of the PeerId class.</summary>
        PeerId() :
            m_peerId(0U),
            m_peerPassword(),
            m_peerDefault(false)
        {
            /* stub */
        }
        /// <summary>Initializes a new instance of the PeerId class.</summary>
        /// <param name="peerId"></param>
        /// <param name="peerPassword"></param>
        /// <param name="peerDefault"></param>
        PeerId(uint32_t peerId, const std::string& peerPassword, bool peerDefault) :
            m_peerId(peerId),
            m_peerPassword(peerPassword),
            m_peerDefault(peerDefault)
        {
            /* stub */
        }

        /// <summary>Equals operator. Copies this PeerId to another PeerId.</summary>
        PeerId& operator=(const PeerId& data)
        {
            if (this != &data) {
                m_peerId = data.m_peerId;
                m_peerPassword = data.m_peerPassword;
                m_peerDefault = data.m_peerDefault;
            }

            return *this;
        }

        /// <summary>Sets flag values.</summary>
        /// <param name="peerId">Peer ID.</param>
        /// <param name="peerPassword">Per Peer Password.</param>
        /// <param name="peerDefault"></param>
        void set(uint32_t peerId, const std::string& peerPassword, bool peerDefault)
        {
            m_peerId = peerId;
            m_peerPassword =  peerPassword;
            m_peerDefault = peerDefault;
        }

    public:
        /// <summary>Peer ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint32_t, peerId);
        /// <summary>Per Peer Password.</summary>
        __READONLY_PROPERTY_PLAIN(std::string, peerPassword);
        /// <summary>Flag indicating if the peer is default.</summary>
        __READONLY_PROPERTY_PLAIN(bool, peerDefault);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements a threading lookup table class that contains peer ID
    //      lookup table.
    // ---------------------------------------------------------------------------

    class HOST_SW_API PeerListLookup : public LookupTable<PeerId> {
    public:
        enum Mode {
            WHITELIST,
            BLACKLIST
        };

        /// <summary>Initializes a new instance of the PeerListLookup class.</summary>
        PeerListLookup(const std::string& filename, Mode mode, uint32_t reloadTime, bool peerAcl);

        /// <summary>Clears all entries from the list.</summary>
        void clear() override;

        /// <summary>Adds a new entry to the list.</summary>
        void addEntry(uint32_t id, const std::string& password = "");
        /// <summary>Removes an existing entry from the list.</summary>
        void eraseEntry(uint32_t id);
        /// <summary>Finds a table entry in this lookup table.</summary>
        PeerId find(uint32_t id) override;

        /// <summary>Commit the table.</summary>
        void commit();

        /// <summary>Gets whether the lookup is enabled.</summary>
        bool getACL() const;

        /// <summary>Checks if a peer ID is in the list.</summary>
        bool isPeerInList(uint32_t id) const;
        /// <summary>Checks if a peer ID is allowed based on the mode and enabled flag.</summary>
        bool isPeerAllowed(uint32_t id) const;

        /// <summary>Sets the mode to either WHITELIST or BLACKLIST.</summary>
        void setMode(Mode mode);
        /// <summary>Gets the current mode.</summary>
        Mode getMode() const;

    protected:
        bool m_acl;

        /// <summary>Loads the table from the passed lookup table file.</summary>
        /// <returns>True, if lookup table was loaded, otherwise false.</returns>
        bool load() override;

        /// <summary>Saves the table to the passed lookup table file.</summary>
        /// <returns>True, if lookup table was saved, otherwise false.</returns>
        bool save() override;

    private:
        Mode m_mode;

        static std::mutex m_mutex;
    };
} // namespace lookups

#endif // __PEER_LIST_LOOKUP_H__