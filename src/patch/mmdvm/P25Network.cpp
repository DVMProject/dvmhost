// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - TG Patch
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2009-2014,2016,2020,2021 by Jonathan Naylor G4KLX
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "patch/Defines.h"
#include "common/p25/P25Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "mmdvm/P25Network.h"

using namespace p25::defines;
using namespace network::udp;
using namespace mmdvm;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

/*
** bryanb: we purposely do handling this way, instead of using the DFSI classes, to ensure we maintain compat 
**  with upstream MMDVM, extra data handled from our DFSI could confuse the P25 gateway
*/

const uint8_t REC62[] = {
    0x62U, 0x02U, 0x02U, 0x0CU, 0x0BU, 0x12U, 0x64U, 0x00U, 0x00U, 0x80U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U
};

const uint8_t REC63[] = {
    0x63U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U
};

const uint8_t REC64[] = {
    0x64U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U
};

const uint8_t REC65[] = {
    0x65U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U
};

const uint8_t REC66[] = {
    0x66U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U
};

const uint8_t REC67[] = {
    0x67U, 0xF0U, 0x9DU, 0x6AU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U
};

const uint8_t REC68[] = {
    0x68U, 0x19U, 0xD4U, 0x26U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U
};

const uint8_t REC69[] = {
    0x69U, 0xE0U, 0xEBU, 0x7BU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U
};

const uint8_t REC6A[] = {
    0x6AU, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
};

const uint8_t REC6B[] = {
    0x6BU, 0x02U, 0x02U, 0x0CU, 0x0BU, 0x12U, 0x64U, 0x00U, 0x00U, 0x80U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U
};

const uint8_t REC6C[] = {
    0x6CU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U
};

const uint8_t REC6D[] = {
    0x6DU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U
};

const uint8_t REC6E[] = {
    0x6EU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U
};

const uint8_t REC6F[] = {
    0x6FU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U
};

const uint8_t REC70[] = {
    0x70U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U
};

const uint8_t REC71[] = {
    0x71U, 0xACU, 0xB8U, 0xA4U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U
};

const uint8_t REC72[] = {
    0x72U, 0x9BU, 0xDCU, 0x75U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U
};

const uint8_t REC73[] = {
    0x73U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
};

const uint8_t REC80[] = {
    0x80U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
};

const uint32_t BUFFER_LENGTH = 100U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the P25Network class. */

P25Network::P25Network(const std::string& gatewayAddress, uint16_t gatewayPort, uint16_t localPort, bool debug) :
    m_socket(localPort),
    m_addr(),
    m_addrLen(0U),
    m_debug(debug),
    m_buffer(1000U, "MMDVM P25 Network")
{
    if (Socket::lookup(gatewayAddress, gatewayPort, m_addr, m_addrLen) != 0)
        m_addrLen = 0U;
}

/* Finalizes a instance of the P25Network class. */

P25Network::~P25Network() = default;

/* Reads P25 raw frame data from the P25 ring buffer. */

uint32_t P25Network::read(uint8_t* data, uint32_t length)
{
    assert(data != nullptr);

    if (m_buffer.isEmpty())
        return 0U;

    uint8_t c = 0U;
    m_buffer.get(&c, 1U);

    if (c == 0U) {
        return 0U;
    }

    if (c > length) {
        return 0U;
    }

    m_buffer.get(data, c);
    return c;
}

/* Writes P25 LDU1 frame data to the network. */

bool P25Network::writeLDU1(const uint8_t* ldu1, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, bool end)
{
    assert(ldu1 != nullptr);

    uint8_t buffer[22U];

    // The '62' record
    ::memcpy(buffer, REC62, 22U);
    ::memcpy(buffer + 10U, ldu1 + 10U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $62 LDU1 Sent", buffer, 22U);

    bool ret = m_socket.write(buffer, 22U, m_addr, m_addrLen);
    if (!ret)
        return false;

    // The '63' record
    ::memcpy(buffer, REC63, 14U);
    ::memcpy(buffer + 1U, ldu1 + 26U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $63 LDU1 Sent", buffer, 14U);

    ret = m_socket.write(buffer, 14U, m_addr, m_addrLen);
    if (!ret)
        return false;

    // The '64' record
    ::memcpy(buffer, REC64, 17U);
    buffer[1U] = control.getLCO();
    buffer[2U] = control.getMFId();
    ::memcpy(buffer + 5U, ldu1 + 55U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $64 LDU1 Sent", buffer, 17U);

    ret = m_socket.write(buffer, 17U, m_addr, m_addrLen);
    if (!ret)
        return false;

    // The '65' record
    ::memcpy(buffer, REC65, 17U);
    uint32_t id = control.getDstId();
    buffer[1U] = (id >> 16) & 0xFFU;
    buffer[2U] = (id >> 8) & 0xFFU;
    buffer[3U] = (id >> 0) & 0xFFU;
    ::memcpy(buffer + 5U, ldu1 + 80U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $65 LDU1 Sent", buffer, 17U);

    ret = m_socket.write(buffer, 17U, m_addr, m_addrLen);
    if (!ret)
        return false;

    // The '66' record
    ::memcpy(buffer, REC66, 17U);
    id = control.getSrcId();
    buffer[1U] = (id >> 16) & 0xFFU;
    buffer[2U] = (id >> 8) & 0xFFU;
    buffer[3U] = (id >> 0) & 0xFFU;
    ::memcpy(buffer + 5U, ldu1 + 105U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $66 LDU1 Sent", buffer, 17U);

    ret = m_socket.write(buffer, 17U, m_addr, m_addrLen);
    if (!ret)
        return false;

    // The '67' record
    ::memcpy(buffer, REC67, 17U);
    ::memcpy(buffer + 5U, ldu1 + 130U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $67 LDU1 Sent", buffer, 17U);

    ret = m_socket.write(buffer, 17U, m_addr, m_addrLen);
    if (!ret)
        return false;

    // The '68' record
    ::memcpy(buffer, REC68, 17U);
    ::memcpy(buffer + 5U, ldu1 + 155U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $68 LDU1 Sent", buffer, 17U);

    ret = m_socket.write(buffer, 17U, m_addr, m_addrLen);
    if (!ret)
        return false;

    // The '69' record
    ::memcpy(buffer, REC69, 17U);
    ::memcpy(buffer + 5U, ldu1 + 180U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $69 LDU1 Sent", buffer, 17U);

    ret = m_socket.write(buffer, 17U, m_addr, m_addrLen);
    if (!ret)
        return false;

    // The '6A' record
    ::memcpy(buffer, REC6A, 16U);
    buffer[1U] = lsd.getLSD1();
    buffer[2U] = lsd.getLSD2();
    ::memcpy(buffer + 5U, ldu1 + 204U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $6A LDU1 Sent", buffer, 16U);

    ret = m_socket.write(buffer, 16U, m_addr, m_addrLen);
    if (!ret)
        return false;

    if (end) {
        if (m_debug)
            Utils::dump(1U, "MMDVM Network END Sent", REC80, 17U);

        ret = m_socket.write(REC80, 17U, m_addr, m_addrLen);
        if (!ret)
            return false;
    }

    return true;
}

/* Writes P25 LDU2 frame data to the network. */

bool P25Network::writeLDU2(const uint8_t* ldu2, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, bool end)
{
    assert(ldu2 != nullptr);

    uint8_t buffer[22U];

    // The '6B' record
    ::memcpy(buffer, REC6B, 22U);
    ::memcpy(buffer + 10U, ldu2 + 10U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $6B LDU2 Sent", buffer, 22U);

    bool ret = m_socket.write(buffer, 22U, m_addr, m_addrLen);
    if (!ret)
        return false;

    // The '6C' record
    ::memcpy(buffer, REC6C, 14U);
    ::memcpy(buffer + 1U, ldu2 + 26U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $6C LDU2 Sent", buffer, 14U);

    ret = m_socket.write(buffer, 14U, m_addr, m_addrLen);
    if (!ret)
        return false;

    uint8_t mi[MI_LENGTH_BYTES];
    control.getMI(mi);

    // The '6D' record
    ::memcpy(buffer, REC6D, 17U);
    buffer[1U] = mi[0U];
    buffer[2U] = mi[1U];
    buffer[3U] = mi[2U];
    ::memcpy(buffer + 5U, ldu2 + 55U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $6D LDU2 Sent", buffer, 17U);

    ret = m_socket.write(buffer, 17U, m_addr, m_addrLen);
    if (!ret)
        return false;

    // The '6E' record
    ::memcpy(buffer, REC6E, 17U);
    buffer[1U] = mi[3U];
    buffer[2U] = mi[4U];
    buffer[3U] = mi[5U];
    ::memcpy(buffer + 5U, ldu2 + 80U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $6E LDU2 Sent", buffer, 17U);

    ret = m_socket.write(buffer, 17U, m_addr, m_addrLen);
    if (!ret)
        return false;

    // The '6F' record
    ::memcpy(buffer, REC6F, 17U);
    buffer[1U] = mi[6U];
    buffer[2U] = mi[7U];
    buffer[3U] = mi[8U];
    ::memcpy(buffer + 5U, ldu2 + 105U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $6F LDU2 Sent", buffer, 17U);

    ret = m_socket.write(buffer, 17U, m_addr, m_addrLen);
    if (!ret)
        return false;

    // The '70' record
    ::memcpy(buffer, REC70, 17U);
    buffer[1U] = control.getAlgId();
    uint32_t id = control.getKId();
    buffer[2U] = (id >> 8) & 0xFFU;
    buffer[3U] = (id >> 0) & 0xFFU;
    ::memcpy(buffer + 5U, ldu2 + 130U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $70 LDU2 Sent", buffer, 17U);

    ret = m_socket.write(buffer, 17U, m_addr, m_addrLen);
    if (!ret)
        return false;

    // The '71' record
    ::memcpy(buffer, REC71, 17U);
    ::memcpy(buffer + 5U, ldu2 + 155U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $71 LDU2 Sent", buffer, 17U);

    ret = m_socket.write(buffer, 17U, m_addr, m_addrLen);
    if (!ret)
        return false;

    // The '72' record
    ::memcpy(buffer, REC72, 17U);
    ::memcpy(buffer + 5U, ldu2 + 180U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $72 LDU2 Sent", buffer, 17U);

    ret = m_socket.write(buffer, 17U, m_addr, m_addrLen);
    if (!ret)
        return false;

    // The '73' record
    ::memcpy(buffer, REC73, 16U);
    buffer[1U] = lsd.getLSD1();
    buffer[2U] = lsd.getLSD2();
    ::memcpy(buffer + 5U, ldu2 + 204U, RAW_IMBE_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "MMDVM Network $73 LDU2 Sent", buffer, 16U);

    ret = m_socket.write(buffer, 16U, m_addr, m_addrLen);
    if (!ret)
        return false;

    if (end) {
        if (m_debug)
            Utils::dump(1U, "MMDVM Network END Sent", REC80, 17U);

        ret = m_socket.write(REC80, 17U, m_addr, m_addrLen);
        if (!ret)
            return false;
    }

    return true;
}

/* Updates the timer by the passed number of milliseconds. */

void P25Network::clock(uint32_t ms)
{
    uint8_t buffer[BUFFER_LENGTH];

    sockaddr_storage address;
    uint32_t addrLen;
    int length = m_socket.read(buffer, BUFFER_LENGTH, address, addrLen);
    if (length <= 0)
        return;

    if (!Socket::match(m_addr, address)) {
        LogMessage(LOG_NET, "MMDVM, packet received from an invalid source");
        return;
    }

    if (m_debug)
        Utils::dump(1U, "MMDVM Network Data Received", buffer, length);

    uint8_t c = length;
    m_buffer.addData(&c, 1U);

    m_buffer.addData(buffer, length);
}

/* Helper to determine if we are connected to a MMDVM gateway. */

bool P25Network::isConnected() const
{
    return (m_addrLen != 0);
}

/* Opens connection to the network. */

bool P25Network::open()
{
    if (m_addrLen == 0U) {
        LogError(LOG_NET, "MMDVM, Unable to resolve the address of the P25 Gateway");
        return false;
    }

    LogMessage(LOG_NET, "MMDVM, Opening P25 network connection");

    return m_socket.open(m_addr);
}

/* Closes connection to the network. */

void P25Network::close()
{
    m_socket.close();

    LogMessage(LOG_NET, "MMDVM, Closing P25 network connection");
}
