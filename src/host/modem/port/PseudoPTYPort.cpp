// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2020,2021 Jonathan Naylor, G4KLX
*
*/
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

/// <summary>
/// Initializes a new instance of the PseudoPTYPort class.
/// </summary>
/// <param name="device">Serial port device.</param>
/// <param name="speed">Serial port speed.</param>
/// <param name="assertRTS"></param>
PseudoPTYPort::PseudoPTYPort(const std::string& symlink, SERIAL_SPEED speed, bool assertRTS) : UARTPort(speed, assertRTS),
    m_symlink(symlink)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the PseudoPTYPort class.
/// </summary>
PseudoPTYPort::~PseudoPTYPort() = default;

/// <summary>
/// Opens a connection to the serial port.
/// </summary>
/// <returns>True, if connection is opened, otherwise false.</returns>
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

/// <summary>
/// Closes the connection to the serial port.
/// </summary>
void PseudoPTYPort::close()
{
    UARTPort::close();
    ::unlink(m_symlink.c_str());
}
