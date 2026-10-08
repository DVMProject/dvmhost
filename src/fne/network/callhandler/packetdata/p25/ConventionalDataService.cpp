// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "network/callhandler/packetdata/p25/ConventionalDataService.h"
#include "common/p25/P25Defines.h"

using namespace network::callhandler::packetdata::p25data;
using namespace ::p25::data;
using namespace ::p25::defines;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ConventionalDataService class. */

ConventionalDataService::ConventionalDataService(DataBindingRegistry& bindings,
    DataLinkManager& links, const ConventionalRegistrationPolicy& policy) :
    m_bindings(bindings),
    m_links(links),
    m_policy(policy),
    m_registrations()
{
    /* stub */
}

/* Processes a conventional registration request. */

ConventionalRegistrationResult ConventionalDataService::process(const uint8_t* data, uint32_t length, uint32_t headerLlId,
    const ConventionalRegistrationProvisioning& provisioning, uint64_t nowMs)
{
    ConventionalRegistration request;
    if (!ConventionalRegistration::decode(data, length, request))
        return deny(request, RegistrationDenyReason::MALFORMED);

    if (headerLlId != 0U && request.llId != headerLlId)
        return deny(request, RegistrationDenyReason::IDENTITY_MISMATCH);

    // handle disconnect requests first
    if (request.type == PDURegType::DISCONNECT) {
        bool changed = m_bindings.erase(request.llId);
        changed = m_registrations.erase(request.llId) > 0U || changed;
        m_links.erase(request.llId);

        ConventionalRegistrationResult result;
        result.decision = RegistrationDecision::DISCONNECTED;
        result.denyReason = RegistrationDenyReason::NONE;
        result.response = request;
        result.bindingChanged = changed;
        return result;
    }

    if (request.type != PDURegType::CONNECT)
        return deny(request, RegistrationDenyReason::MALFORMED);
    if (!provisioning.known)
        return deny(request, RegistrationDenyReason::NOT_PROVISIONED);
    if (!provisioning.enabled)
        return deny(request, RegistrationDenyReason::DISABLED);
    if ((request.options & 0x0EU) != 0U ||
        ((request.options & 0x01U) != 0U && !m_policy.allowCMSScan))
        return deny(request, RegistrationDenyReason::UNSUPPORTED_OPTIONS);

    // determine the IP address to use for the registration
    uint32_t ipAddress = provisioning.staticIPAddress;
    bool dynamic = false;
    if (ipAddress != 0U) {
        if (request.ipAddress != 0U && request.ipAddress != ipAddress)
            return deny(request, RegistrationDenyReason::ADDRESS_CONFLICT);
    } else {
        if (!provisioning.allowDynamicAddress)
            return deny(request, RegistrationDenyReason::ADDRESS_UNAVAILABLE);
        ipAddress = request.ipAddress;
        if (ipAddress == 0U && m_policy.allocateDynamicAddress)
            ipAddress = m_policy.allocateDynamicAddress(request.llId, request.ipAddress);
        if (ipAddress == 0U)
            return deny(request, RegistrationDenyReason::ADDRESS_UNAVAILABLE);
        dynamic = true;
    }

    // create or update the IP binding for the registration
    IPBinding binding;
    binding.link.llId = request.llId;
    binding.ipAddress = ipAddress;
    binding.accessMode = DataAccessMode::CONVENTIONAL;
    binding.convergenceMode = DataConvergenceMode::SCEP;
    binding.origin = dynamic ? DataBindingOrigin::CONVENTIONAL_REGISTRATION :
        DataBindingOrigin::PROVISIONED;
    binding.authorized = true;
    if (!m_bindings.upsert(binding))
        return deny(request, RegistrationDenyReason::ADDRESS_CONFLICT);

    // update the registration state
    RegistrationState& state = m_registrations[request.llId];
    bool changed = state.ipAddress != ipAddress || state.options != request.options ||
        state.dynamic != dynamic;
    state.ipAddress = ipAddress;
    state.options = request.options & 0x01U;
    state.dynamic = dynamic;
    state.expiresAtMs = m_policy.registrationLifetimeMs == 0U ? 0U :
        nowMs + m_policy.registrationLifetimeMs;

    // prepare the result of the registration request
    ConventionalRegistrationResult result;
    result.decision = RegistrationDecision::ACCEPT;
    result.denyReason = RegistrationDenyReason::NONE;
    result.response.type = PDURegType::ACCEPT;
    result.response.options = state.options;
    result.response.llId = request.llId;
    result.response.ipAddress = ipAddress;
    result.bindingChanged = changed;

    return result;
}

/* Installs a static registration for the specified logical link identifier. */

bool ConventionalDataService::installStatic(uint32_t llId, uint32_t ipAddress,
    bool authorized)
{
    // create or update the IP binding for the static registration
    IPBinding binding;
    binding.link.llId = llId;
    binding.ipAddress = ipAddress;
    binding.accessMode = DataAccessMode::CONVENTIONAL;
    binding.convergenceMode = DataConvergenceMode::SCEP;
    binding.origin = DataBindingOrigin::PROVISIONED;
    binding.authorized = authorized;
    if (!m_bindings.upsert(binding))
        return false;

    // update the registration state for the static registration
    RegistrationState& state = m_registrations[llId];
    state.ipAddress = ipAddress;
    state.dynamic = false;
    state.expiresAtMs = 0U;

    return true;
}

/* Expires registrations that have passed their expiration time. */

void ConventionalDataService::expire(uint64_t nowMs)
{
    // iterate through the registrations and remove any that have expired
    for (auto it = m_registrations.begin(); it != m_registrations.end();) {
        if (it->second.expiresAtMs != 0U && nowMs >= it->second.expiresAtMs) {
            m_bindings.erase(it->first);
            m_links.erase(it->first);
            it = m_registrations.erase(it);
        } else {
            ++it;
        }
    }
}

/* Checks if the specified logical link identifier is registered. */

bool ConventionalDataService::isRegistered(uint32_t llId) const
{
    return m_registrations.find(llId) != m_registrations.end();
}

/* Converts a registration deny reason to a string representation. */

std::string ConventionalDataService::denyReasonToString(RegistrationDenyReason reason)
{
    switch (reason) {
        case RegistrationDenyReason::MALFORMED: return "MALFORMED";
        case RegistrationDenyReason::IDENTITY_MISMATCH: return "IDENTITY_MISMATCH";
        case RegistrationDenyReason::NOT_PROVISIONED: return "NOT_PROVISIONED";
        case RegistrationDenyReason::DISABLED: return "DISABLED";
        case RegistrationDenyReason::UNSUPPORTED_OPTIONS: return "UNSUPPORTED_OPTIONS";
        case RegistrationDenyReason::ADDRESS_UNAVAILABLE: return "ADDRESS_UNAVAILABLE";
        case RegistrationDenyReason::ADDRESS_CONFLICT: return "ADDRESS_CONFLICT";
        default: return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Denies a registration request. */

ConventionalRegistrationResult ConventionalDataService::deny(
    const ConventionalRegistration& request, RegistrationDenyReason reason) const
{
    // prepare the result of the denial operation
    ConventionalRegistrationResult result;
    result.decision = RegistrationDecision::DENY;
    result.denyReason = reason;
    result.response.type = PDURegType::DENY;
    result.response.llId = request.llId;

    return result;
}
