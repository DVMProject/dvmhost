// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2002-2004,2007-2009,2011-2013,2015-2017,2020,2021 Jonathan Naylor, G4KLX
 *  Copyright (C) 1999-2001 Thomas Sailor, HB9JNX
 *  Copyright (C) 2020-2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/Log.h"
#include "modem/port/UARTPort.h"

using namespace modem::port;

#include <cstring>
#include <cassert>

#if defined(_WIN32)
#include <setupapi.h>
#include <winioctl.h>
#else
#include <sys/types.h>

#include <sys/ioctl.h>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#endif // defined(_WIN32)

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the UARTPort class. */

UARTPort::UARTPort(const std::string& device, SERIAL_SPEED speed, bool assertRTS) :
    m_isOpen(false),
    m_device(device),
    m_speed(speed),
    m_assertRTS(assertRTS),
#if defined(_WIN32)
    m_fd(INVALID_HANDLE_VALUE)
#else
    m_fd(-1)
#endif // defined(_WIN32)
{
    assert(!device.empty());
}

/* Finalizes a instance of the UARTPort class. */

UARTPort::~UARTPort() = default;

/* Opens a connection to the serial port. */

bool UARTPort::open()
{
    if (m_isOpen)
        return true;
#if defined(_WIN32)
    assert(m_fd == INVALID_HANDLE_VALUE);

    DWORD errCode;

    std::string baseName = m_device.substr(4U); // Convert "\\.\COM10" to "COM10"

    m_fd = ::CreateFileA(m_device.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_fd == INVALID_HANDLE_VALUE) {
        ::LogError(LOG_HOST, "Cannot open device - %s, err=%04lx", m_device.c_str(), ::GetLastError());
        return false;
    }

    DCB dcb;
    if (::GetCommState(m_fd, &dcb) == 0) {
        ::LogError(LOG_HOST, "Cannot get the attributes for %s, err=%04lx", m_device.c_str(), ::GetLastError());
        ::ClearCommError(m_fd, &errCode, NULL);
        ::CloseHandle(m_fd);
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

    if (::SetCommState(m_fd, &dcb) == 0) {
        ::LogError(LOG_HOST, "Cannot set the attributes for %s, err=%04lx", m_device.c_str(), ::GetLastError());
        ::ClearCommError(m_fd, &errCode, NULL);
        ::CloseHandle(m_fd);
        return false;
    }

    COMMTIMEOUTS timeouts;
    if (!::GetCommTimeouts(m_fd, &timeouts)) {
        ::LogError(LOG_HOST, "Cannot get the timeouts for %s, err=%04lx", m_device.c_str(), ::GetLastError());
        ::ClearCommError(m_fd, &errCode, NULL);
        ::CloseHandle(m_fd);
        return false;
    }

    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0UL;
    timeouts.ReadTotalTimeoutConstant = 0UL;

    if (!::SetCommTimeouts(m_fd, &timeouts)) {
        ::LogError(LOG_HOST, "Cannot set the timeouts for %s, err=%04lx", m_device.c_str(), ::GetLastError());
        ::ClearCommError(m_fd, &errCode, NULL);
        ::CloseHandle(m_fd);
        return false;
    }

    if (::EscapeCommFunction(m_fd, CLRDTR) == 0) {
        ::LogError(LOG_HOST, "Cannot clear DTR for %s, err=%04lx", m_device.c_str(), ::GetLastError());
        ::ClearCommError(m_fd, &errCode, NULL);
        ::CloseHandle(m_fd);
        return false;
    }

    if (::EscapeCommFunction(m_fd, m_assertRTS ? SETRTS : CLRRTS) == 0) {
        ::LogError(LOG_HOST, "Cannot set/clear RTS for %s, err=%04lx", m_device.c_str(), ::GetLastError());
        ::ClearCommError(m_fd, &errCode, NULL);
        ::CloseHandle(m_fd);
        return false;
    }

    ::ClearCommError(m_fd, &errCode, NULL);

    return true;
#else
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
#endif // defined(_WIN32)
}

/* Reads data from the serial port. */

int UARTPort::read(uint8_t* buffer, uint32_t length)
{
    assert(buffer != nullptr);
#if defined(_WIN32)
    assert(m_fd != INVALID_HANDLE_VALUE);

    if (length == 0U)
        return 0;

    uint32_t offset = 0U;

    while (offset < length) {
        int ret = readNonblock(buffer + offset, length - offset);
        if (ret < 0) {
            return ret;
        } else if (ret == 0) {
            if (offset == 0U)
                return 0;
        } else {
            offset += ret;
        }
    }
#else
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
#endif // defined(_WIN32)
    return length;
}

/* Writes data to the serial port. */

int UARTPort::write(const uint8_t* buffer, uint32_t length)
{
    assert(buffer != nullptr);
#if defined(_WIN32)
    if (m_isOpen && m_fd == INVALID_HANDLE_VALUE)
        return 0;

    assert(m_fd != INVALID_HANDLE_VALUE);

    if (length == 0U)
        return 0;

    uint32_t ptr = 0U;

    while (ptr < length) {
        DWORD bytes = 0UL;
        BOOL ret = ::WriteFile(m_fd, buffer + ptr, length - ptr, &bytes, NULL);
        if (!ret) {
            ::LogError(LOG_HOST, "Error from WriteFile for %s: %04lx", m_device.c_str(), ::GetLastError());
            return -1;
        }

        ptr += bytes;
    }
#else
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
                ::LogError(LOG_HOST, "Error returned from write(), errno=%d (%s)", errno, strerror(errno));
                return -1;
            }
        }

        if (n > 0)
            ptr += n;
    }
#endif // defined(_WIN32)
    return length;
}

/* Closes the connection to the serial port. */

void UARTPort::close()
{
#if defined(_WIN32)
    if (!m_isOpen && m_fd == INVALID_HANDLE_VALUE)
        return;

    assert(m_fd != INVALID_HANDLE_VALUE);

    ::CloseHandle(m_fd);
    m_fd = INVALID_HANDLE_VALUE;
#else
    if (!m_isOpen && m_fd == -1)
        return;

    assert(m_fd != -1);

    ::close(m_fd);
    m_fd = -1;
#endif // defined(_WIN32)
    m_isOpen = false;
}

#if defined(__APPLE__)
/* Helper on Apple to set serial port to non-blocking. */

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

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the UARTPort class. */

UARTPort::UARTPort(SERIAL_SPEED speed, bool assertRTS) :
    m_isOpen(false),
    m_speed(speed),
    m_assertRTS(assertRTS),
#if defined(_WIN32)
    m_fd(INVALID_HANDLE_VALUE)
#else
    m_fd(-1)
#endif // defined(_WIN32)
{
    /* stub */
}

#if defined(_WIN32)
/* Helper on Windows to read from serial port non-blocking. */

int UARTPort::readNonblock(unsigned char* buffer, unsigned int length)
{
    assert(m_fd != INVALID_HANDLE_VALUE);
    assert(buffer != NULL);

    if (length == 0U)
        return 0;

    DWORD errors;
    COMSTAT status;
    if (::ClearCommError(m_fd, &errors, &status) == 0) {
        ::LogError(LOG_HOST, "Error from ClearCommError for %s, err=%04lx", m_device.c_str(), ::GetLastError());
        return -1;
    }

    if (status.cbInQue == 0UL)
        return 0;

    DWORD readLength = status.cbInQue;
    if (length < readLength)
        readLength = length;

    DWORD bytes = 0UL;
    BOOL ret = ::ReadFile(m_fd, buffer, readLength, &bytes, NULL);
    if (!ret) {
        ::LogError(LOG_HOST, "Error from ReadFile for %s: %04lx", m_device.c_str(), ::GetLastError());
        return -1;
    }

    return int(bytes);
}
#endif // defined(_WIN32)

/* Checks it the serial port can be written to. */

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

/* Sets the termios setings on the serial port. */

bool UARTPort::setTermios()
{
#if !defined(_WIN32)
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
#endif // !defined(_WIN32)

    m_isOpen = true;
    return true;
}
