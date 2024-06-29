// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2021 Jonathan Naylor, G4KLX
 *  Copyright (C) 2021 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/Log.h"
#include "modem/port/UDPPort.h"

using namespace modem::port;
using namespace network;

#include <cstring>
#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t BUFFER_LENGTH = 2000U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the UDPPort class. */

UDPPort::UDPPort(const std::string& address, uint16_t modemPort) :
    m_socket(modemPort),
    m_addr(),
    m_addrLen(0U),
    m_buffer(2000U, "UDP Port Ring Buffer")
{
    assert(!address.empty());
    assert(modemPort > 0U);

    if (udp::Socket::lookup(address, modemPort, m_addr, m_addrLen) != 0)
        m_addrLen = 0U;

    if (m_addrLen > 0U) {
        std::string addrStr = udp::Socket::address(m_addr);
        LogWarning(LOG_HOST, "SECURITY: Remote modem expects IP address; %s for remote modem control", addrStr.c_str());
    }
}

/* Finalizes a instance of the UDPPort class. */

UDPPort::~UDPPort() = default;

/* Opens a connection to the port. */

bool UDPPort::open()
{
    if (m_addrLen == 0U) {
        LogError(LOG_NET, "Unable to resolve the address of the modem");
        return false;
    }

    return m_socket.open(m_addr);
}

/* Reads data from the port. */

int UDPPort::read(uint8_t* buffer, uint32_t length)
{
    assert(buffer != nullptr);
    assert(length > 0U);

    uint8_t data[BUFFER_LENGTH];
    ::memset(data, 0x00U, BUFFER_LENGTH);

    sockaddr_storage addr;
    uint32_t addrLen;
    int ret = m_socket.read(data, BUFFER_LENGTH, addr, addrLen);

    // An error occurred on the socket
    if (ret < 0)
        return ret;

    // Add new data to the ring buffer
    if (ret > 0) {
        if (udp::Socket::match(addr, m_addr)) {
            m_buffer.addData(data, ret);
        }
        else {
            std::string addrStr = udp::Socket::address(addr);
            LogWarning(LOG_HOST, "SECURITY: Remote modem mode encountered invalid IP address; %s", addrStr.c_str());
        }
    }

    // Get required data from the ring buffer
    uint32_t avail = m_buffer.dataSize();
    if (avail < length)
        length = avail;

    if (length > 0U)
        m_buffer.get(buffer, length);

    return int(length);
}

/* Writes data to the port. */

int UDPPort::write(const uint8_t* buffer, uint32_t length)
{
    assert(buffer != nullptr);
    assert(length > 0U);

    return m_socket.write(buffer, length, m_addr, m_addrLen) ? int(length) : -1;
}

/* Closes the connection to the port. */

void UDPPort::close()
{
    m_socket.close();
}
