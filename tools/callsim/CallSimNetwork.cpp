// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - FNE Call Simulator
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "CallSimNetwork.h"

using namespace network;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Clocks the shared FNE connection while excluding concurrent writers. */

void CallSimNetwork::clockConnection(uint32_t ms)
{
    std::lock_guard<std::mutex> lock(m_writeMutex);
    clock(ms);
}

/* Writes a DMR frame using call-specific RTP state. */

bool CallSimNetwork::writeDMRCall(const dmr::data::NetData& data, CallStreamState& stream, bool end)
{
    std::lock_guard<std::mutex> lock(m_writeMutex);
    if (stream.streamId == 0U)
        stream.streamId = createStreamId();

    uint32_t messageLength = 0U;
    UInt8Array message = createDMR_Message(messageLength, stream.streamId, data);
    if (message == nullptr)
        return false;

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, message.get(), messageLength,
        nextSequence(stream, end), stream.streamId);
}

/* Writes a P25 LDU1 using call-specific RTP state. */

bool CallSimNetwork::writeP25LDU1Call(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd,
    const uint8_t* data, CallStreamState& stream)
{
    std::lock_guard<std::mutex> lock(m_writeMutex);
    uint32_t messageLength = 0U;
    UInt8Array message = createP25_LDU1Message(messageLength, control, lsd, data, P25DEF::FrameType::DATA_UNIT, 0U);
    if (message == nullptr)
        return false;

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength,
        nextSequence(stream, false), stream.streamId);
}

/* Writes a P25 LDU2 using call-specific RTP state. */

bool CallSimNetwork::writeP25LDU2Call(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd,
    const uint8_t* data, CallStreamState& stream)
{
    std::lock_guard<std::mutex> lock(m_writeMutex);
    uint32_t messageLength = 0U;
    UInt8Array message = createP25_LDU2Message(messageLength, control, lsd, data, 0U);
    if (message == nullptr)
        return false;

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength,
        nextSequence(stream, false), stream.streamId);
}

/* Writes a terminating P25 TDU using call-specific RTP state. */

bool CallSimNetwork::writeP25TDUCall(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd,
    CallStreamState& stream)
{
    std::lock_guard<std::mutex> lock(m_writeMutex);
    uint32_t messageLength = 0U;
    UInt8Array message = createP25_TDUMessage(messageLength, control, lsd, 0U);
    if (message == nullptr)
        return false;

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength,
        nextSequence(stream, true), stream.streamId);
}

/* Writes an NXDN frame using call-specific RTP state. */

bool CallSimNetwork::writeNXDNCall(const nxdn::lc::RTCH& control, const uint8_t* data, uint32_t length,
    CallStreamState& stream, bool end)
{
    std::lock_guard<std::mutex> lock(m_writeMutex);
    uint32_t messageLength = 0U;
    UInt8Array message = createNXDN_Message(messageLength, control, data, length);
    if (message == nullptr)
        return false;

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN }, message.get(), messageLength,
        nextSequence(stream, end), stream.streamId);
}

/* Writes a group-call grant request while excluding concurrent frame writers. */

bool CallSimNetwork::writeGroupGrantRequest(uint8_t mode, uint32_t srcId, uint32_t dstId, uint8_t slot)
{
    std::lock_guard<std::mutex> lock(m_writeMutex);
    return writeGrantReq(mode, srcId, dstId, slot, false, 0U);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Allocates an RTP stream ID on first use and returns the next sequence. */

uint16_t CallSimNetwork::nextSequence(CallStreamState& stream, bool end)
{
    if (stream.streamId == 0U)
        stream.streamId = createStreamId();

    if (end)
        return RTP_END_OF_CALL_SEQ;

    return stream.sequence++;
}
