// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/data/IPConvergenceService.h"
#include "common/p25/P25Defines.h"

using namespace p25::data;
using namespace p25::defines;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the SCEPService class. */

SCEPService::SCEPService(const DataBindingRegistry& bindings, const SCEPPolicy& policy) :
    m_bindings(bindings),
    m_policy(policy)
{
    /* stub */
}

/* Decodes an IP datagram from the provided data buffer. */

IPDecodeResult SCEPService::decode(const uint8_t* data, uint32_t length) const
{
    IPDecodeResult result;
    if (!IPv4Packet::parse(data, length, result.packet))
        return result;

    result.result = result.packet.totalLength > m_policy.mtu ?
        ConvergenceResult::MTU_EXCEEDED : ConvergenceResult::OK;

    return result;
}

/* Encodes an IP datagram into the provided data buffer. */

IPEncodeResult SCEPService::encode(const uint8_t* data, uint32_t length) const
{
    IPEncodeResult result;
    IPDecodeResult decoded = decode(data, length);
    result.result = decoded.result;
    if (decoded.result != ConvergenceResult::OK)
        return result;

    result.length = decoded.packet.totalLength;
    bool broadcast = isBroadcast(decoded.packet.destinationAddress);
    bool multicast = isMulticast(decoded.packet.destinationAddress);
    if ((broadcast && !m_policy.allowBroadcast) || (multicast && !m_policy.allowMulticast)) {
        result.result = ConvergenceResult::POLICY_REJECTED;
        return result;
    }

    if (broadcast || multicast) {
        result.llId = WUID_ALL;
        result.delivery = m_policy.broadcastDelivery;
        return result;
    }

    result.llId = routeDownlink(decoded.packet.destinationAddress);
    if (result.llId == 0U) {
        result.result = ConvergenceResult::NO_BINDING;
        return result;
    }

    result.delivery = m_policy.unicastDelivery;

    return result;
}

/* Authorizes an uplink transmission for the specified logical link ID and IP packet. */

ConvergenceResult SCEPService::authorizeUplink(uint32_t llId, const IPv4Packet& packet) const
{
    // check if the packet exceeds the maximum transmission unit (MTU) defined in the policy
    if (packet.totalLength > m_policy.mtu)
        return ConvergenceResult::MTU_EXCEEDED;

    // check if the packet violates the broadcast or multicast policy
    if ((isBroadcast(packet.destinationAddress) && !m_policy.allowBroadcast) ||
        (isMulticast(packet.destinationAddress) && !m_policy.allowMulticast))
        return ConvergenceResult::POLICY_REJECTED;

    // check if the logical link ID corresponds to an authorized SCEP binding
    const IPBinding* binding = m_bindings.findByLLId(llId);
    if (binding == nullptr || !binding->authorized ||
        binding->convergenceMode != DataConvergenceMode::SCEP)
        return ConvergenceResult::NO_BINDING;

    // check if the source IP address of the packet matches the authorized binding
    if (binding->ipAddress != packet.sourceAddress)
        return ConvergenceResult::SOURCE_MISMATCH;

    return ConvergenceResult::OK;
}

/* Routes a downlink transmission to the specified destination address. */

uint32_t SCEPService::routeDownlink(uint32_t destinationAddress) const
{
    const IPBinding* binding = m_bindings.findByIPAddress(destinationAddress);
    if (binding == nullptr || !binding->authorized ||
        binding->convergenceMode != DataConvergenceMode::SCEP)
        return 0U;

    return binding->link.llId;
}

/* Determines if the specified address is a broadcast address. */

bool SCEPService::isBroadcast(uint32_t address)
{
    return address == 0xFFFFFFFFU;
}

/* Determines if the specified address is a multicast address. */

bool SCEPService::isMulticast(uint32_t address)
{
    return (address & 0xF0000000U) == 0xE0000000U;
}
