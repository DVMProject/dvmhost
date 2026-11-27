// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Formatting and Style Example
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 DVMProject Authors
 *
 */
#include "common/Defines.h"
#include "examples/style/FormattingExample.h"

using namespace style_example;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the FormattingExample class. */

FormattingExample::FormattingExample(FormattingObserver* observer) :
    m_sessionId(0U),
    m_response(false),
    m_valid(false),
    m_observer(observer),
    m_state(STATE_IDLE),
    m_payload()
{
    m_payload.revision = 0U;
}

/* Finalizes an instance of the FormattingExample class. */

FormattingExample::~FormattingExample() = default;

/* Decodes an example frame and updates the current state. */

bool FormattingExample::decode(const uint8_t* data, uint32_t length)
{
    assert(data != nullptr);

    FormattingFrameInfo info;
    if (!decodeHeader(data, length, info)) {
        reset();
        return false;
    }

    const uint8_t* payload = data + EXAMPLE_HEADER_LENGTH_BYTES;
    if (!setPayload(payload, info.payloadLength)) {
        reset();
        return false;
    }

    m_sessionId = info.sessionId;
    m_response = info.response;
    m_valid = true;
    m_state = STATE_DECODED;
    return true;
}

/* Encodes the current state as an example frame. */

UInt8Array FormattingExample::encode(uint32_t* length) const
{
    assert(length != nullptr);

    *length = 0U;
    if (!m_valid || m_payload.data.size() > EXAMPLE_MAX_PAYLOAD_LENGTH_BYTES)
        return nullptr;

    const uint32_t frameLength = EXAMPLE_HEADER_LENGTH_BYTES + (uint32_t)m_payload.data.size();
    UInt8Array frame = std::make_unique<uint8_t[]>(frameLength);
    ::memset(frame.get(), 0x00U, frameLength);

    frame[0U] = EXAMPLE_PROTOCOL_VERSION;
    WRITE_BIT(frame.get(), 8U, m_response);
    SET_UINT16(m_sessionId, frame.get(), 2U);
    SET_UINT16((uint16_t)m_payload.data.size(), frame.get(), 4U);

    if (!m_payload.data.empty())
        ::memcpy(frame.get() + EXAMPLE_HEADER_LENGTH_BYTES, m_payload.data.data(), m_payload.data.size());

    *length = frameLength;
    return frame;
}

/* Replaces the current payload. */

bool FormattingExample::setPayload(const uint8_t* data, uint32_t length)
{
    if (length > EXAMPLE_MAX_PAYLOAD_LENGTH_BYTES || (length > 0U && data == nullptr))
        return false;

    if (length == 0U) {
        m_payload.data.clear();
        m_payload.revision++;
        m_state = STATE_CONFIGURED;
        return true;
    }

    DECLARE_UINT8_ARRAY(payload, length);
    ::memcpy(payload, data, length);
    m_payload.data.assign(payload, payload + length);
    m_payload.revision++;
    m_state = STATE_CONFIGURED;
    return true;
}

/* Clears all decoded and configured frame state. */

void FormattingExample::reset()
{
    m_sessionId = 0U;
    m_response = false;
    m_valid = false;
    m_state = STATE_IDLE;
    m_payload.data.clear();
    m_payload.revision = 0U;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Validates the fixed header and returns its decoded metadata. */

bool FormattingExample::decodeHeader(const uint8_t* data, uint32_t length, FormattingFrameInfo& info) const
{
    assert(data != nullptr);

    if (length < EXAMPLE_HEADER_LENGTH_BYTES || data[0U] != EXAMPLE_PROTOCOL_VERSION)
        return false;

    info.response = READ_BIT(data, 8U) != 0U;
    info.sessionId = GET_UINT16(data, 2U);
    info.payloadLength = GET_UINT16(data, 4U);

    if (info.payloadLength > EXAMPLE_MAX_PAYLOAD_LENGTH_BYTES)
        return false;

    return length == EXAMPLE_HEADER_LENGTH_BYTES + info.payloadLength;
}

/* Copies state from another FormattingExample instance. */

void FormattingExample::copy(const FormattingExample& data)
{
    m_sessionId = data.m_sessionId;
    m_response = data.m_response;
    m_valid = data.m_valid;
    m_observer = data.m_observer;
    m_state = data.m_state;
    m_payload = data.m_payload;
}
