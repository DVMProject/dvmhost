// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2022,2024,2025 Bryan Biedenkapp, N2PLL
 *  Copyright (c) 2024 Patrick McDonnell, W3AXL
 *  Copyright (c) 2024 Caleb, KO4UYJ
 *
 */
/**
 * @defgroup lookups_peer Peer List Lookups
 * @brief Implementation for peer list lookup tables.
 * @ingroup lookups
 * 
 * @file PeerListLookup.h
 * @ingroup lookups_peer
 * @file PeerListLookup.cpp
 * @ingroup lookups_peer
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
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents an individual entry in the peer ID table.
     * @ingroup lookups_peer
     */
    class HOST_SW_API PeerId {
    public:
        /**
         * @brief Initializes a new instance of the PeerId class.
         */
        PeerId() :
            m_peerId(0U),
            m_peerAlias(),
            m_peerPassword(),
            m_peerLink(false),
            m_canRequestKeys(false),
            m_peerDefault(false)
        {
            /* stub */
        }
        /**
         * @brief Initializes a new instance of the PeerId class.
         * @param peerId Peer ID.
         * @param peerAlias Peer alias
         * @param peerPassword Per Peer Password.
         * @param sendConfiguration Flag indicating this peer participates in peer link and should be sent configuration.
         * @param peerLink lag indicating if the peer participates in peer link and should be sent configuration.
         * @param canRequestKeys Flag indicating if the peer can request encryption keys.
         * @param peerDefault Flag indicating this is a "default" (i.e. undefined) peer.
         */
        PeerId(uint32_t peerId, const std::string& peerAlias, const std::string& peerPassword, bool peerLink, bool canRequestKeys, bool peerDefault) :
            m_peerId(peerId),
            m_peerAlias(peerAlias),
            m_peerPassword(peerPassword),
            m_peerLink(peerLink),
            m_canRequestKeys(canRequestKeys),
            m_peerDefault(peerDefault)
        {
            /* stub */
        }

        /**
         * @brief Equals operator. Copies this PeerId to another PeerId.
         * @param data Instance of PeerId to copy.
         */
        PeerId& operator=(const PeerId& data)
        {
            if (this != &data) {
                m_peerId = data.m_peerId;
                m_peerAlias = data.m_peerAlias;
                m_peerPassword = data.m_peerPassword;
                m_peerLink = data.m_peerLink;
                m_canRequestKeys = data.m_canRequestKeys;
                m_peerDefault = data.m_peerDefault;
            }

            return *this;
        }

        /**
         * @brief Sets flag values.
         * @param peerId Peer ID.
         * @param peerAlias Peer Alias
         * @param peerPassword Per Peer Password.
         * @param sendConfiguration Flag indicating this peer participates in peer link and should be sent configuration.
         * @param peerLink lag indicating if the peer participates in peer link and should be sent configuration.
         * @param canRequestKeys Flag indicating if the peer can request encryption keys.
         * @param peerDefault Flag indicating this is a "default" (i.e. undefined) peer.
         */
        void set(uint32_t peerId, const std::string& peerAlias, const std::string& peerPassword, bool peerLink, bool canRequestKeys, bool peerDefault)
        {
            m_peerId = peerId;
            m_peerAlias = peerAlias;
            m_peerPassword =  peerPassword;
            m_peerLink = peerLink;
            m_canRequestKeys = canRequestKeys;
            m_peerDefault = peerDefault;
        }

    public:
        /**
         * @brief Peer ID.
         */
        __PROPERTY_PLAIN(uint32_t, peerId);
        /**
         * @breif Peer Alias
         */
        __PROPERTY_PLAIN(std::string, peerAlias);
        /**
         * @brief Per Peer Password.
         */
        __PROPERTY_PLAIN(std::string, peerPassword);
        /**
         * @brief Flag indicating if the peer participates in peer link and should be sent configuration.
         */
        __PROPERTY_PLAIN(bool, peerLink);
        /**
         * @brief Flag indicating if the peer can request encryption keys.
         */
        __PROPERTY_PLAIN(bool, canRequestKeys);
        /**
         * @brief Flag indicating if the peer is default.
         */
        __READONLY_PROPERTY_PLAIN(bool, peerDefault);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements a threading lookup table class that contains peer ID
     *  lookup table.
     * @ingroup lookups_peer
     */
    class HOST_SW_API PeerListLookup : public LookupTable<PeerId> {
    public:
        /**
         * @brief Peer List Mode
         */
        enum Mode {
            WHITELIST,      //! Peers listed are whitelisted
            BLACKLIST       //! Peers listed are blacklisted
        };

        /**
         * @brief Initializes a new instance of the PeerListLookup class.
         * @param filename Full-path to the list file.
         * @param mode Mode to operate in (WHITELIST or BLACKLIST).
         * @param peerAcl Flag indicating if the lookup is enabled.
         */
        PeerListLookup(const std::string& filename, Mode mode, uint32_t reloadTime, bool peerAcl);

        /**
         * @brief Clears all entries from the list.
         */
        void clear() override;

        /**
         * @brief Adds a new entry to the list.
         * @param peerId Unique peer ID to add.
         * @param password Per Peer Password.
         * @param peerLink Flag indicating this peer will participate in peer link and should be sent configuration.
         * @param canRequestKeys Flag indicating if the peer can request encryption keys.
         */
        void addEntry(uint32_t id, const std::string& alias = "", const std::string& password = "", bool peerLink = false, bool canRequestKeys = false);
        /**
         * @brief Removes an existing entry from the list.
         * @param peerId Unique peer ID to remove.
         */
        void eraseEntry(uint32_t id);
        /**
         * @brief Finds a table entry in this lookup table.
         * @param id Unique identifier for table entry.
         * @returns PeerId Table entry.
         */
        PeerId find(uint32_t id) override;

        /**
         * @brief Commit the table.
         */
        void commit();

        /**
         * @brief Gets whether the lookup is enabled.
         * @returns True, if this lookup table is enabled, otherwise false.
         */
        bool getACL() const;

        /**
         * @brief Checks if a peer ID is in the list.
         * @param id Unique peer ID to check.
         * @returns bool True, if the peer ID is in the list, otherwise false.
         */
        bool isPeerInList(uint32_t id) const;
        /**
         * @brief Checks if a peer ID is allowed based on the mode and enabled flag.
         * @param id Unique peer ID to check.
         * @returns bool True, if the peer ID is allowed, otherwise false.
         */
        bool isPeerAllowed(uint32_t id) const;

        /**
         * @brief Checks if the peer list is empty.
         * @returns bool True, if list is empty, otherwise false.
         */
        bool isPeerListEmpty() const { return m_table.size() == 0U; }

        /**
         * @brief Sets the mode to either WHITELIST or BLACKLIST.
         * @param mode The mode to set.
         */
        void setMode(Mode mode);
        /**
         * @brief Gets the current mode.
         * @returns Mode Current peer list operational mode.
         */
        Mode getMode() const;

        /**
         * @brief Gets the entire peer ID table.
         * @returns std::unordered_map<uint32_t, PeerId> 
         */
        std::unordered_map<uint32_t, PeerId> table() const { return m_table; }
        /**
         * @brief Gets the entire peer ID table.
         * @returns std::vector<PeerId> 
         */
        std::vector<PeerId> tableAsList() const;

    protected:
        bool m_acl;

        /**
         * @brief Loads the table from the passed lookup table file.
         * @return True, if lookup table was loaded, otherwise false.
         */
        bool load() override;

        /**
         * @brief Saves the table to the passed lookup table file.
         * @return True, if lookup table was saved, otherwise false.
         */
        bool save() override;

    private:
        Mode m_mode;

        static std::mutex m_mutex;  //! Mutex used for hard locking.
        static bool m_locked;       //! Flag used for soft locking (prevents find lookups), should be used when atomic operations (add/erase/etc) are being used.
    };
} // namespace lookups

#endif // __PEER_LIST_LOOKUP_H__