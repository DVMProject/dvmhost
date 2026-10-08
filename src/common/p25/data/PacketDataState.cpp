// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/data/PacketDataState.h"

using namespace p25::data;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Inserts or updates an IP binding in the registry. Returns true if the operation was successful. */

bool DataBindingRegistry::upsert(const IPBinding& binding)
{
    if (binding.link.llId == 0U || binding.ipAddress == 0U)
        return false;

    const IPBinding* owner = findByIPAddress(binding.ipAddress);
    if (owner != nullptr && owner->link.llId != binding.link.llId)
        return false;

    m_bindings[binding.link.llId] = binding;
    return true;
}

/* Erases an IP binding from the registry by its logical link ID. Returns true if the operation was successful. */

bool DataBindingRegistry::erase(uint32_t llId)
{
    return m_bindings.erase(llId) > 0U;
}

/* Clears all IP bindings from the registry. */

void DataBindingRegistry::clear()
{
    m_bindings.clear();
}

/* Finds an IP binding by its logical link ID. */

const IPBinding* DataBindingRegistry::findByLLId(uint32_t llId) const
{
    auto it = m_bindings.find(llId);
    return it == m_bindings.end() ? nullptr : &it->second;
}

/* Finds an IP binding by its IP address. */

const IPBinding* DataBindingRegistry::findByIPAddress(uint32_t ipAddress) const
{
    for (const auto& entry : m_bindings) {
        if (entry.second.ipAddress == ipAddress)
            return &entry.second;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Returns the next send sequence number for the specified logical link. */

uint8_t DataLinkManager::nextSendSequence(uint32_t llId, bool& synchronize)
{
    DataLinkState& state = m_states[llId];
    if (!state.sendInitialized) {
        state.sendInitialized = true;
        state.sendSequence = 0U;
        synchronize = true;
        return state.sendSequence;
    }

    state.sendSequence = (state.sendSequence + 1U) & 0x07U;
    synchronize = false;
    return state.sendSequence;
}

/* Accepts a received sequence number for the specified logical link. */

ReceiveSequenceResult DataLinkManager::acceptReceiveSequence(uint32_t llId, uint8_t sequence,
    bool synchronize, uint32_t packetFingerprint, uint8_t& expectedSequence)
{
    DataLinkState& state = m_states[llId];
    expectedSequence = state.receiveInitialized ? uint8_t((state.receiveSequence + 1U) & 0x07U) : 0U;

    if (sequence > 7U || (synchronize && sequence != 0U))
        return ReceiveSequenceResult::OUT_OF_SEQUENCE;

    if (!state.receiveInitialized || synchronize) {
        state.receiveInitialized = true;
        state.receiveSequence = sequence;
        state.receiveFingerprint = packetFingerprint;
        return ReceiveSequenceResult::ACCEPTED;
    }

    if (sequence == state.receiveSequence && packetFingerprint == state.receiveFingerprint)
        return ReceiveSequenceResult::DUPLICATE;

    // BAED permits gaps: record the newly received N(S), allowing the upper
    // layer to observe that intervening logical packets were lost
    state.receiveSequence = sequence;
    state.receiveFingerprint = packetFingerprint;
    return ReceiveSequenceResult::ACCEPTED;
}

/* Computes the fingerprint of the given data. */

uint32_t DataLinkManager::fingerprint(const uint8_t* data, uint32_t length)
{
    if (data == nullptr)
        return 0U;

    // stable FNV-1a fingerprint used only for duplicate detection -- the CAI
    // assembler has already validated CRCPACKET before this point
    uint32_t value = 2166136261U;
    for (uint32_t i = 0U; i < length; i++) {
        value ^= data[i];
        value *= 16777619U;
    }
    return value;
}

/* Sets the readiness state for the specified logical link. */

void DataLinkManager::setReady(uint32_t llId, bool ready)
{
    if (llId != 0U)
        m_states[llId].readyForNextPacket = ready;
}

/* Checks if the specified logical link is ready. */

bool DataLinkManager::isReady(uint32_t llId) const
{
    auto it = m_states.find(llId);
    return it == m_states.end() ? true : it->second.readyForNextPacket;
}

/* Checks if the specified logical link has an associated state. */

bool DataLinkManager::hasState(uint32_t llId) const
{
    return m_states.find(llId) != m_states.end();
}

/* Erases the state associated with the specified logical link. */

void DataLinkManager::erase(uint32_t llId)
{
    m_states.erase(llId);
}

/* Clears all logical link states. */

void DataLinkManager::clear()
{
    m_states.clear();
}
