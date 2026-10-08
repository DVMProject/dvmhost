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
 * @file ConventionalDataService.h
 * @ingroup p25_pdu
 * @file ConventionalDataService.cpp
 * @ingroup p25_pdu
 */
#if !defined(__P25_DATA__CONVENTIONALDATASERVICE_H__)
#define  __P25_DATA__CONVENTIONALDATASERVICE_H__

#include "common/Defines.h"
#include "common/p25/data/ConventionalRegistration.h"
#include "common/p25/data/PacketDataState.h"

#include <cstdint>
#include <functional>
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
         * @brief Conventional packet data registration decision. 
         */
        enum class RegistrationDecision : uint8_t {
            ACCEPT,                 //!< Registration request accepted.
            DENY,                   //!< Registration request denied.
            DISCONNECTED            //!< Registration disconnected.
        };

        /** 
         * @brief Reason a conventional packet data registration was denied. 
         */
        enum class RegistrationDenyReason : uint8_t {
            NONE,                   //!< No specific reason.
            MALFORMED,              //!< Registration request was malformed.
            IDENTITY_MISMATCH,      //!< Identity did not match expected value.
            NOT_PROVISIONED,        //!< Device not provisioned.
            DISABLED,               //!< Registration disabled.
            UNSUPPORTED_OPTIONS,    //!< Registration request contained unsupported options.
            ADDRESS_UNAVAILABLE,    //!< Requested address unavailable.
            ADDRESS_CONFLICT        //!< Address conflict detected.
        };
        /* @} */

        // ---------------------------------------------------------------------------
        //  Struct Declaration
        // ---------------------------------------------------------------------------

        /** 
         * @brief Provisioning inputs used to decide a registration request. 
         * @ingroup p25_pdu
         */
        struct DVM_COMMON_API ConventionalRegistrationProvisioning {
            /**
             * @brief Indicates whether the device is known.
             */
            bool known = false;
            /**
             * @brief Indicates whether the device is enabled.
             */
            bool enabled = false;
            /**
             * @brief Indicates whether dynamic address allocation is allowed.
             */
            bool allowDynamicAddress = false;
            /**
             * @brief The static IP address of the device.
             */
            uint32_t staticIPAddress = 0U;
        };

        // ---------------------------------------------------------------------------
        //  Struct Declaration
        // ---------------------------------------------------------------------------

        /** 
         * @brief Local policy applied to conventional registration requests.
         * @ingroup p25_pdu
         */
        struct DVM_COMMON_API ConventionalRegistrationPolicy {
            /**
             * @brief Indicates whether CMS scan is allowed.
             */
            bool allowCMSScan = false;
            /**
             * @brief The lifetime of a registration in milliseconds.
             */
            uint64_t registrationLifetimeMs = 0U;
            /**
             * @brief Function to allocate a dynamic IP address.
             */
            std::function<uint32_t(uint32_t, uint32_t)> allocateDynamicAddress;
        };

        // ---------------------------------------------------------------------------
        //  Struct Declaration
        // ---------------------------------------------------------------------------

        /** 
         * @brief Result of a conventional registration transaction.
         * @ingroup p25_pdu
         */
        struct DVM_COMMON_API ConventionalRegistrationResult {
            /**
             * @brief The decision of the registration request.
             */
            RegistrationDecision decision = RegistrationDecision::DENY;
            /**
             * @brief The reason for denying the registration request.
             */
            RegistrationDenyReason denyReason = RegistrationDenyReason::MALFORMED;
            /**
             * @brief The response to the registration request.
             */
            ConventionalRegistration response;
            /**
             * @brief Indicates whether the binding has changed as a result of the registration request.
             */
            bool bindingChanged { false };
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements conventional packet data registration and binding state.
         * @ingroup p25_pdu
         */
        class DVM_COMMON_API ConventionalDataService {
        public:
            /**
             * @brief Initializes a new instance of the ConventionalDataService class.
             * @param bindings The data binding registry.
             * @param links The data link manager.
             * @param policy The conventional registration policy.
             */
            ConventionalDataService(DataBindingRegistry& bindings, DataLinkManager& links,
                const ConventionalRegistrationPolicy& policy = ConventionalRegistrationPolicy());

            /**
             * @brief Processes a conventional registration request.
             * @param data The raw registration request data.
             * @param length The length of the registration request data.
             * @param headerLlId The logical link identifier from the header.
             * @param provisioning The conventional registration provisioning information.
             * @param nowMs The current time in milliseconds.
             * @return The result of the registration request.
             */
            ConventionalRegistrationResult process(const uint8_t* data, uint32_t length,
                uint32_t headerLlId, const ConventionalRegistrationProvisioning& provisioning,
                uint64_t nowMs = 0U);

            /**
             * @brief Installs a static registration for the specified logical link identifier.
             * @param llId The logical link identifier.
             * @param ipAddress The IP address associated with the registration.
             * @param authorized Indicates whether the registration is authorized.
             * @return True if the static registration was successfully installed; otherwise, false.
             */
            bool installStatic(uint32_t llId, uint32_t ipAddress, bool authorized = true);
            /**
             * @brief Expires registrations that have passed their expiration time.
             * @param nowMs The current time in milliseconds.
             */
            void expire(uint64_t nowMs);
            /**
             * @brief Checks if the specified logical link identifier is registered.
             * @param llId The logical link identifier.
             * @return True if the logical link identifier is registered; otherwise, false.
             */
            bool isRegistered(uint32_t llId) const;
            /**
             * @brief Gets the number of current registrations.
             * @return The number of current registrations.
             */
            size_t registrationCount() const noexcept { return m_registrations.size(); }

            /**
             * @brief Converts a registration deny reason to a string representation.
             * @param reason The registration deny reason.
             * @return A string representation of the registration deny reason.
             */
            static std::string denyReasonToString(RegistrationDenyReason reason);

        private:
            /**
             * @brief Represents the state of a registration.
             */
            struct RegistrationState {
                /**
                 * @brief The IP address associated with the registration.
                 */
                uint32_t ipAddress = 0U;
                /**
                 * @brief The options associated with the registration.
                 */
                uint8_t options = 0U;
                /**
                 * @brief The expiration time of the registration in milliseconds.
                 */
                uint64_t expiresAtMs = 0U;
                /**
                 * @brief Indicates whether the registration is dynamic.
                 */
                bool dynamic = false;
            };

            DataBindingRegistry& m_bindings;
            DataLinkManager& m_links;
            ConventionalRegistrationPolicy m_policy;
            std::unordered_map<uint32_t, RegistrationState> m_registrations;

            /**
             * @brief Denies a registration request.
             * @param request The registration request to deny.
             * @param reason The reason for denying the registration.
             * @return The result of the denial operation.
             */
            ConventionalRegistrationResult deny(const ConventionalRegistration& request,
                RegistrationDenyReason reason) const;
        };
    } // namespace data
} // namespace p25

#endif // __P25_DATA__CONVENTIONALDATASERVICE_H__
