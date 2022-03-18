/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
 *   Copyright (C) 2020,2021 by Jonathan Naylor G4KLX
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "modem/port/PseudoPTYPort.h"
#include "Log.h"

#include <cstring>
#include <cassert>
#include <cstdlib>

#if !defined(_WIN32) && !defined(_WIN64)

using namespace modem::port;

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
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
PseudoPTYPort::~PseudoPTYPort()
{
    /* stub */
}

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

#endif
