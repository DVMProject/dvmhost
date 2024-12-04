// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2020,2021 Jonathan Naylor, G4KLX
 *
 */
#if !defined(_WIN32)
#include "common/Log.h"
#include "modem/port/PseudoPTYPort.h"

#include <cstring>
#include <cassert>

using namespace modem::port;

#include <cerrno>
#include <unistd.h>
#if defined(__linux__)
	#include <pty.h>
#else
	#include <util.h>
#endif

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the PseudoPTYPort class. */

PseudoPTYPort::PseudoPTYPort(const std::string& symlink, SERIAL_SPEED speed, bool assertRTS) : UARTPort(speed, assertRTS, false),
    m_symlink(symlink)
{
    /* stub */
}

/* Finalizes a instance of the PseudoPTYPort class. */

PseudoPTYPort::~PseudoPTYPort() = default;

/* Opens a connection to the serial port. */

bool PseudoPTYPort::open()
{
    assert(m_fd == -1);

    int slavefd;
    char slave[300];
    int result = ::openpty(&m_fd, &slavefd, slave, NULL, NULL);
    if (result < 0) {
        ::LogError(LOG_HOST, "Cannot open the pseudo tty - errno : %d", errno);
        return false;
    }

    // remove any previous stale symlink
    ::unlink(m_symlink.c_str());

    int ret = ::symlink(slave, m_symlink.c_str());
    if (ret != 0) {
        ::LogError(LOG_HOST, "Cannot make symlink to %s with %s", slave, m_symlink.c_str());
        close();
        return false;
    }

    ::LogMessage(LOG_HOST, "Made symbolic link from %s to %s", slave, m_symlink.c_str());
    m_device = std::string(::ttyname(m_fd));
    return setTermios();
}

/* Closes the connection to the serial port. */

void PseudoPTYPort::close()
{
    UARTPort::close();
    ::unlink(m_symlink.c_str());
}
#endif // !defined(_WIN32)
