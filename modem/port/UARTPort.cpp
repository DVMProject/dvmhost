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
*   Copyright (C) 2002-2004,2007-2011,2013,2014-2017,2019,2020,2021 by Jonathan Naylor G4KLX
*   Copyright (C) 1999-2001 by Thomas Sailor HB9JNX
*   Copyright (C) 2020-2021 by Bryan Biedenkapp N2PLL
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
#include "Defines.h"
#include "modem/port/UARTPort.h"
#include "Log.h"

using namespace modem::port;

#include <cstring>
#include <cassert>

#include <sys/types.h>

#if defined(_WIN32) || defined(_WIN64)
#include <setupapi.h>
#include <winioctl.h>
#else
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#endif

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

#if defined(_WIN32) || defined(_WIN64)
/// <summary>
/// Initializes a new instance of the UARTPort class.
/// </summary>
/// <param name="device">Serial port device.</param>
/// <param name="speed">Serial port speed.</param>
/// <param name="assertRTS"></param>
UARTPort::UARTPort(const std::string& device, SERIAL_SPEED speed, bool assertRTS) :
    m_isOpen(false),
    m_device(device),
    m_speed(speed),
    m_assertRTS(assertRTS),
    m_handle(INVALID_HANDLE_VALUE)
{
    assert(!device.empty());
}

/// <summary>
/// Finalizes a instance of the UARTPort class.
/// </summary>
UARTPort::~UARTPort()
{
    /* stub */
}

/// <summary>
/// Opens a connection to the serial port.
/// </summary>
/// <returns>True, if connection is opened, otherwise false.</returns>
bool UARTPort::open()
{
    if (m_isOpen)
        return true;

    assert(m_handle == INVALID_HANDLE_VALUE);

    DWORD errCode;

    // Convert "\\.\COM10" to "COM10"
    std::string baseName = m_device.substr(4U);

    m_handle = ::CreateFileA(m_device.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_handle == INVALID_HANDLE_VALUE) {
        ::LogError(LOG_HOST, "Cannot open device - %s, err=%04lx", m_device.c_str(), ::GetLastError());
        return false;
    }

    DCB dcb;
    if (::GetCommState(m_handle, &dcb) == 0) {
        ::LogError(LOG_HOST, "Cannot get the attributes for %s, err=%04lx", m_device.c_str(), ::GetLastError());
        ::ClearCommError(m_handle, &errCode, NULL);
        ::CloseHandle(m_handle);
        return false;
    }

    dcb.BaudRate = DWORD(m_speed);
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.fParity = FALSE;
    dcb.StopBits = ONESTOPBIT;
    dcb.fInX = FALSE;
    dcb.fOutX = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;

    if (::SetCommState(m_handle, &dcb) == 0) {
        ::LogError(LOG_HOST, "Cannot set the attributes for %s, err=%04lx", m_device.c_str(), ::GetLastError());
        ::ClearCommError(m_handle, &errCode, NULL);
        ::CloseHandle(m_handle);
        return false;
    }

    COMMTIMEOUTS timeouts;
    if (!::GetCommTimeouts(m_handle, &timeouts)) {
        ::LogError(LOG_HOST, "Cannot get the timeouts for %s, err=%04lx", m_device.c_str(), ::GetLastError());
        ::ClearCommError(m_handle, &errCode, NULL);
        ::CloseHandle(m_handle);
        return false;
    }

    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0UL;
    timeouts.ReadTotalTimeoutConstant = 0UL;

    if (!::SetCommTimeouts(m_handle, &timeouts)) {
        ::LogError(LOG_HOST, "Cannot set the timeouts for %s, err=%04lx", m_device.c_str(), ::GetLastError());
        ::ClearCommError(m_handle, &errCode, NULL);
        ::CloseHandle(m_handle);
        return false;
    }

    if (::EscapeCommFunction(m_handle, CLRDTR) == 0) {
        ::LogError(LOG_HOST, "Cannot clear DTR for %s, err=%04lx", m_device.c_str(), ::GetLastError());
        ::ClearCommError(m_handle, &errCode, NULL);
        ::CloseHandle(m_handle);
        return false;
    }

    if (::EscapeCommFunction(m_handle, m_assertRTS ? SETRTS : CLRRTS) == 0) {
        ::LogError(LOG_HOST, "Cannot set/clear RTS for %s, err=%04lx", m_device.c_str(), ::GetLastError());
        ::ClearCommError(m_handle, &errCode, NULL);
        ::CloseHandle(m_handle);
        return false;
    }

    ::ClearCommError(m_handle, &errCode, NULL);

    m_isOpen = true;
    return true;
}

/// <summary>
/// Reads data from the serial port.
/// </summary>
/// <param name="buffer">Buffer to read data from the serial port to.</param>
/// <param name="length">Length of data to read from the serial port.</param>
/// <returns>Actual length of data read from serial port.</returns>
int UARTPort::read(uint8_t* buffer, uint32_t length)
{
    assert(m_handle != INVALID_HANDLE_VALUE);
    assert(buffer != NULL);

    uint32_t ptr = 0U;

    while (ptr < length) {
        int ret = readNonblock(buffer + ptr, length - ptr);
        if (ret < 0) {
            return ret;
        }
        else if (ret == 0) {
            if (ptr == 0U)
                return 0;
        }
        else {
            ptr += ret;
        }
    }

    return int(length);
}

/// <summary>
/// Writes data to the serial port.
/// </summary>
/// <param name="buffer">Buffer containing data to write to serial port.</param>
/// <param name="length">Length of data to write to serial port.</param>
/// <returns>Actual length of data written to the serial port.</returns>
int UARTPort::write(const uint8_t* buffer, uint32_t length)
{
    assert(buffer != NULL);

    if (m_isOpen && m_handle == INVALID_HANDLE_VALUE)
        return 0;

    assert(m_handle != INVALID_HANDLE_VALUE);

    if (length == 0U)
        return 0;

    uint32_t ptr = 0U;

    while (ptr < length) {
        DWORD bytes = 0UL;
        BOOL ret = ::WriteFile(m_handle, buffer + ptr, length - ptr, &bytes, NULL);
        if (!ret) {
            ::LogError(LOG_HOST, "Error from WriteFile for %s: %04lx", m_device.c_str(), ::GetLastError());
            return -1;
        }

        ptr += bytes;
    }

    return int(length);
}

/// <summary>
/// Closes the connection to the serial port.
/// </summary>
void UARTPort::close()
{
    if (!m_isOpen && m_handle == INVALID_HANDLE_VALUE)
        return;

    assert(m_handle != INVALID_HANDLE_VALUE);

    ::CloseHandle(m_handle);
    m_handle = INVALID_HANDLE_VALUE;
    m_isOpen = false;
}

#else

/// <summary>
/// Initializes a new instance of the UARTPort class.
/// </summary>
/// <param name="device">Serial port device.</param>
/// <param name="speed">Serial port speed.</param>
/// <param name="assertRTS"></param>
UARTPort::UARTPort(const std::string& device, SERIAL_SPEED speed, bool assertRTS) : 
    m_isOpen(false),
    m_device(device),
    m_speed(speed),
    m_assertRTS(assertRTS),
    m_fd(-1)
{
    assert(!device.empty());
}

/// <summary>
/// Finalizes a instance of the UARTPort class.
/// </summary>
UARTPort::~UARTPort()
{
}

/// <summary>
/// Opens a connection to the serial port.
/// </summary>
/// <returns>True, if connection is opened, otherwise false.</returns>
bool UARTPort::open()
{
    if (m_isOpen)
        return true;

    assert(m_fd == -1);

#if defined(__APPLE__)
    m_fd = ::open(m_device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK); /*open in block mode under OSX*/
#else
    m_fd = ::open(m_device.c_str(), O_RDWR | O_NOCTTY | O_NDELAY, 0);
#endif
    if (m_fd < 0) {
        ::LogError(LOG_HOST, "Cannot open device - %s", m_device.c_str());
        return false;
    }

    if (::isatty(m_fd) == 0) {
        LogError(LOG_HOST, "%s is not a TTY device", m_device.c_str());
        ::close(m_fd);
        return false;
    }

    return setTermios();
}

/// <summary>
/// Reads data from the serial port.
/// </summary>
/// <param name="buffer">Buffer to read data from the serial port to.</param>
/// <param name="length">Length of data to read from the serial port.</param>
/// <returns>Actual length of data read from serial port.</returns>
int UARTPort::read(uint8_t* buffer, uint32_t length)
{
    assert(buffer != NULL);
    assert(m_fd != -1);

    if (length == 0U)
        return 0;

    uint32_t offset = 0U;

    while (offset < length) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(m_fd, &fds);

        int n;
        if (offset == 0U) {
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 0;

            n = ::select(m_fd + 1, &fds, NULL, NULL, &tv);
            if (n == 0)
                return 0;
        }
        else {
            n = ::select(m_fd + 1, &fds, NULL, NULL, NULL);
        }

        if (n < 0) {
            ::LogError(LOG_HOST, "Error from select(), errno=%d", errno);
            return -1;
        }

        if (n > 0) {
            ssize_t len = ::read(m_fd, buffer + offset, length - offset);
            if (len < 0) {
                if (errno != EAGAIN) {
                    ::LogError(LOG_HOST, "Error from read(), errno=%d", errno);
                    return -1;
                }
            }

            if (len > 0)
                offset += len;
        }
    }

    return length;
}

/// <summary>
/// Writes data to the serial port.
/// </summary>
/// <param name="buffer">Buffer containing data to write to serial port.</param>
/// <param name="length">Length of data to write to serial port.</param>
/// <returns>Actual length of data written to the serial port.</returns>
int UARTPort::write(const uint8_t* buffer, uint32_t length)
{
    assert(buffer != NULL);

    if (m_isOpen && m_fd == -1)
        return 0;

    assert(m_fd != -1);

    if (length == 0U)
        return 0;

    uint32_t ptr = 0U;
    while (ptr < length) {
        ssize_t n = 0U;
        if (canWrite())
            n = ::write(m_fd, buffer + ptr, length - ptr);
        if (n < 0) {
            if (errno != EAGAIN) {
                ::LogError(LOG_HOST, "Error returned from write(), errno=%d", errno);
                return -1;
            }
        }

        if (n > 0)
            ptr += n;
    }

    return length;
}

/// <summary>
/// Closes the connection to the serial port.
/// </summary>
void UARTPort::close()
{
    if (!m_isOpen && m_fd == -1)
        return;

    assert(m_fd != -1);

    ::close(m_fd);
    m_fd = -1;
    m_isOpen = false;
}

#if defined(__APPLE__)
/// <summary>
///
/// </summary>
/// <returns></returns>
int UARTPort::setNonblock(bool nonblock)
{
    int flag = ::fcntl(m_fd, F_GETFL, 0);

    if (nonblock)
        flag |= O_NONBLOCK;
    else
        flag &= ~O_NONBLOCK;

    return ::fcntl(m_fd, F_SETFL, flag);
}
#endif
#endif

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
UARTPort::UARTPort(SERIAL_SPEED speed, bool assertRTS) :
    m_isOpen(false),
    m_speed(speed),
    m_assertRTS(assertRTS),
#if defined(_WIN32) || defined(_WIN64)
    m_handle(INVALID_HANDLE_VALUE)
#else
    m_fd(-1)
#endif
{
    /* stub */
}

#if defined(_WIN32) || defined(_WIN64)
/// <summary>
///
/// </summary>
/// <param name="buffer"></param>
/// <param name="length"></param>
/// <returns></returns>
int UARTPort::readNonblock(uint8_t* buffer, uint32_t length)
{
    assert(m_handle != INVALID_HANDLE_VALUE);
    assert(buffer != NULL);

    if (length == 0U)
        return 0;

    DWORD errors;
    COMSTAT status;
    if (::ClearCommError(m_handle, &errors, &status) == 0) {
        ::LogError(LOG_HOST, "Error from ClearCommError for %s, err=%04lx", m_device.c_str(), ::GetLastError());
        return -1;
    }

    if (status.cbInQue == 0UL)
        return 0;

    DWORD readLength = status.cbInQue;
    if (length < readLength)
        readLength = length;

    DWORD bytes = 0UL;
    BOOL ret = ::ReadFile(m_handle, buffer, readLength, &bytes, NULL);
    if (!ret) {
        ::LogError(LOG_HOST, "Error from ReadFile for %s: %04lx", m_device.c_str(), ::GetLastError());
        return -1;
    }

    return int(bytes);
}

#else

/// <summary>
///
/// </summary>
/// <returns></returns>
bool UARTPort::canWrite() 
{
#if defined(__APPLE__)
    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(m_fd, &wset);

    struct timeval timeo;
    timeo.tv_sec = 0;
    timeo.tv_usec = 0;

    int rc = ::select(m_fd + 1, NULL, &wset, NULL, &timeo);
    if (rc > 0 && FD_ISSET(m_fd, &wset))
        return true;

    return false;
#else
    return true;
#endif
}

/// <summary>
///
/// </summary>
/// <returns></returns>
bool UARTPort::setTermios()
{
    termios termios;
    if (::tcgetattr(m_fd, &termios) < 0) {
        ::LogError(LOG_HOST, "Cannot get the attributes for %s", m_device.c_str());
        ::close(m_fd);
        return false;
    }

    termios.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK);
    termios.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL);
    termios.c_iflag &= ~(IXON | IXOFF | IXANY);
    termios.c_oflag &= ~(OPOST);
    termios.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CRTSCTS);
    termios.c_cflag |= (CS8 | CLOCAL | CREAD);
    termios.c_lflag &= ~(ISIG | ICANON | IEXTEN);
    termios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
#if defined(__APPLE__)
    termios.c_cc[VMIN] = 1;
    termios.c_cc[VTIME] = 1;
#else
    termios.c_cc[VMIN] = 0;
    termios.c_cc[VTIME] = 10;
#endif

    switch (m_speed) {
    case SERIAL_1200:
        ::cfsetospeed(&termios, B1200);
        ::cfsetispeed(&termios, B1200);
        break;
    case SERIAL_2400:
        ::cfsetospeed(&termios, B2400);
        ::cfsetispeed(&termios, B2400);
        break;
    case SERIAL_4800:
        ::cfsetospeed(&termios, B4800);
        ::cfsetispeed(&termios, B4800);
        break;
    case SERIAL_9600:
        ::cfsetospeed(&termios, B9600);
        ::cfsetispeed(&termios, B9600);
        break;
    case SERIAL_19200:
        ::cfsetospeed(&termios, B19200);
        ::cfsetispeed(&termios, B19200);
        break;
    case SERIAL_38400:
        ::cfsetospeed(&termios, B38400);
        ::cfsetispeed(&termios, B38400);
        break;
    case SERIAL_115200:
        ::cfsetospeed(&termios, B115200);
        ::cfsetispeed(&termios, B115200);
        break;
    case SERIAL_230400:
        ::cfsetospeed(&termios, B230400);
        ::cfsetispeed(&termios, B230400);
        break;
    case SERIAL_460800:
        ::cfsetospeed(&termios, B460800);
        ::cfsetispeed(&termios, B460800);
        break;
    default:
        ::LogError(LOG_HOST, "Unsupported serial port speed - %u", m_speed);
        ::close(m_fd);
        return false;
    }

    if (::tcsetattr(m_fd, TCSANOW, &termios) < 0) {
        ::LogError(LOG_HOST, "Cannot set the attributes for %s", m_device.c_str());
        ::close(m_fd);
        return false;
    }

    if (m_assertRTS) {
        uint32_t y;
        if (::ioctl(m_fd, TIOCMGET, &y) < 0) {
            ::LogError(LOG_HOST, "Cannot get the control attributes for %s", m_device.c_str());
            ::close(m_fd);
            return false;
        }

        y |= TIOCM_RTS;

        if (::ioctl(m_fd, TIOCMSET, &y) < 0) {
            ::LogError(LOG_HOST, "Cannot set the control attributes for %s", m_device.c_str());
            ::close(m_fd);
            return false;
        }
    }

#if defined(__APPLE__)
    setNonblock(false);
#endif

    m_isOpen = true;
    return true;
}
#endif
