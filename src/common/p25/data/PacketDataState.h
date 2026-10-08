// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file PacketDataState.h
 * @ingroup p25_pdu
 * @file PacketDataState.cpp
 * @ingroup p25_pdu
 */
#if !defined(__P25_DATA__PACKETDATASTATE_H__)
#define  __P25_DATA__PACKETDATASTATE_H__

#include "common/Defines.h"

#include <cstdint>
#include <unordered_map>

namespace p25
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        /**
         * @addtogroup p25_pdu
         * @{
         */
        
        /** 
         * @brief P25 packet data access mode. 
         */
        enum class DataAccessMode : uint8_t {
            CONVENTIONAL,               //!< Conventional access mode
            TRUNKED                     //!< Trunked access mode
        };

        /** 
         * @brief P25 packet data convergence mode. 
         */
        enum class DataConvergenceMode : uint8_t {
            CAI_DATA,                   //!< CAI data convergence mode
            SCEP,                       //!< SCEP data convergence mode
            SNDCP                       //!< SNDCP data convergence mode
        };

        /** 
         * @brief Source from which an IP binding was learned. 
         */
        enum class DataBindingOrigin : uint8_t {
            PROVISIONED,                //!< Provisioned binding origin
            CONVENTIONAL_REGISTRATION,  //!< Conventional registration binding origin
            SNDCP_STATIC,               //!< SNDCP static binding origin
            SNDCP_DYNAMIC,              //!< SNDCP dynamic binding origin
            ARP_LEARNED                 //!< ARP learned binding origin
        };

        /** 
         * @brief Result of LLC receive sequence processing. 
         */
        enum class ReceiveSequenceResult : uint8_t {
            ACCEPTED,                   //!< Accepted receive sequence result
            DUPLICATE,                  //!< Duplicate receive sequence result
            OUT_OF_SEQUENCE             //!< Out of sequence receive sequence result
        };
        /* @} */

        // ---------------------------------------------------------------------------
        //  Struct Declaration
        // ---------------------------------------------------------------------------

        /** 
         * @brief Identifies a P25 logical data link.
         * @ingroup p25_pdu
         */
        struct DVM_COMMON_API DataLinkKey {
            /**
             * @brief The logical link identifier.
             */
            uint32_t llId = 0U;

            /**
             * @brief Compares this DataLinkKey with another for equality.
             * @param other The other DataLinkKey to compare with.
             * @return True if the DataLinkKeys are equal; otherwise, false.
             */
            bool operator==(const DataLinkKey& other) const noexcept
            {
                return llId == other.llId;
            }
        };

        // ---------------------------------------------------------------------------
        //  Struct Declaration
        // ---------------------------------------------------------------------------

        /** 
         * @brief Associates a logical data link with an authorized IP address. 
         * @ingroup p25_pdu
         */
        struct DVM_COMMON_API IPBinding {
            /**
             * @brief The logical data link associated with this IP binding.
             */
            DataLinkKey link;
            /**
             * @brief The IP address associated with this binding.
             */
            uint32_t ipAddress = 0U;
            /**
             * @brief The access mode for this binding.
             */
            DataAccessMode accessMode = DataAccessMode::CONVENTIONAL;
            /**
             * @brief The convergence mode for this binding.
             */
            DataConvergenceMode convergenceMode = DataConvergenceMode::SCEP;
            /**
             * @brief The origin from which this binding was learned.
             */
            DataBindingOrigin origin = DataBindingOrigin::PROVISIONED;
            /**
             * @brief The NSAPI associated with this binding.
             */
            uint8_t nsapi = 0U;
            /**
             * @brief Indicates whether the NSAPI is valid.
             */
            bool hasNSAPI = false;
            /**
             * @brief Indicates whether this binding is authorized.
             */
            bool authorized = false;
        };

        // ---------------------------------------------------------------------------
        //  Struct Declaration
        // ---------------------------------------------------------------------------

        /** 
         * @brief LLC sequencing and readiness state for one logical link. 
         * @ingroup p25_pdu
         */
        struct DVM_COMMON_API DataLinkState {
            /**
             * @brief The send sequence number for this logical link.
             */
            uint8_t sendSequence = 0U;
            /**
             * @brief The receive sequence number for this logical link.
             */
            uint8_t receiveSequence = 0U;
            /**
             * @brief The receive fingerprint for this logical link.
             */
            uint32_t receiveFingerprint = 0U;
            /**
             * @brief Indicates whether the send sequence has been initialized.
             */
            bool sendInitialized = false;
            /**
             * @brief Indicates whether the receive sequence has been initialized.
             */
            bool receiveInitialized = false;
            /**
             * @brief Indicates whether the link is ready for the next packet.
             */
            bool readyForNextPacket = true;
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /** 
         * @brief Stores typed P25 packet data IP bindings. 
         * @ingroup p25_pdu
         */
        class DVM_COMMON_API DataBindingRegistry {
        public:
            /**
             * @brief Inserts or updates an IP binding in the registry. Returns true if the operation was successful.
             * @param binding The IP binding to insert or update.
             * @return True if the operation was successful, false otherwise.
             */
            bool upsert(const IPBinding& binding);
            /**
             * @brief Erases an IP binding from the registry by its logical link ID. Returns true if the operation was successful.
             * @param llId The logical link ID of the IP binding to erase.
             * @return True if the operation was successful, false otherwise.
             */
            bool erase(uint32_t llId);
            /**
             * @brief Clears all IP bindings from the registry.
             */
            void clear();

            /**
             * @brief Finds an IP binding by its logical link ID.
             * @param llId The logical link ID of the IP binding to find.
             * @return A pointer to the IP binding if found, nullptr otherwise.
             */
            const IPBinding* findByLLId(uint32_t llId) const;
            /**
             * @brief Finds an IP binding by its IP address.
             * @param ipAddress The IP address of the IP binding to find.
             * @return A pointer to the IP binding if found, nullptr otherwise.
             */
            const IPBinding* findByIPAddress(uint32_t ipAddress) const;
            /**
             * @brief Returns the number of IP bindings in the registry.
             * @return The number of IP bindings.
             */
            size_t size() const noexcept { return m_bindings.size(); }

        private:
            std::unordered_map<uint32_t, IPBinding> m_bindings;
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /** 
         * @brief Stores LLC state independently for each logical link. 
         * @ingroup p25_pdu
         */
        class DVM_COMMON_API DataLinkManager {
        public:
            /**
             * @brief Returns the next send sequence number for the specified logical link.
             * @param llId The logical link ID.
             * @param synchronize Indicates whether to synchronize the sequence.
             * @return The next send sequence number.
             */
            uint8_t nextSendSequence(uint32_t llId, bool& synchronize);
            /**
             * @brief Accepts a received sequence number for the specified logical link.
             * @param llId The logical link ID.
             * @param sequence The received sequence number.
             * @param synchronize Indicates whether to synchronize the sequence.
             * @param packetFingerprint The fingerprint of the received packet.
             * @param expectedSequence The expected sequence number.
             * @return The result of the receive sequence acceptance.
             */
            ReceiveSequenceResult acceptReceiveSequence(uint32_t llId, uint8_t sequence,
                bool synchronize, uint32_t packetFingerprint, uint8_t& expectedSequence);

            /**
             * @brief Computes the fingerprint of the given data.
             * @param data Pointer to the data.
             * @param length Length of the data.
             * @return The computed fingerprint.
             */
            static uint32_t fingerprint(const uint8_t* data, uint32_t length);

            /**
             * @brief Sets the readiness state for the specified logical link.
             * @param llId The logical link ID.
             * @param ready The readiness state to set.
             */
            void setReady(uint32_t llId, bool ready);
            /**
             * @brief Checks if the specified logical link is ready.
             * @param llId The logical link ID.
             * @return True if the logical link is ready, false otherwise.
             */
            bool isReady(uint32_t llId) const;
            /**
             * @brief Checks if the specified logical link has an associated state.
             * @param llId The logical link ID.
             * @return True if the logical link has an associated state, false otherwise.
             */
            bool hasState(uint32_t llId) const;
            /**
             * @brief Erases the state associated with the specified logical link.
             * @param llId The logical link ID.
             */
            void erase(uint32_t llId);
            /**
             * @brief Clears all logical link states.
             */
            void clear();

        private:
            std::unordered_map<uint32_t, DataLinkState> m_states;
        };
    } // namespace data
} // namespace p25

#endif // __P25_DATA__PACKETDATASTATE_H__
