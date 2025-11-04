// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Lorenzo L. Romero, K2LLR
 */
/**
 * @file CtsCorController.cpp
 * @ingroup bridge
 */

#include "Defines.h"
#include "CtsCorController.h"

#if !defined(_WIN32)
#include <errno.h>
#endif

CtsCorController::CtsCorController(const std::string& port)
    : m_port(port), m_isOpen(false), m_ownsFd(true)
#if defined(_WIN32)
    , m_fd(INVALID_HANDLE_VALUE)
#else
    , m_fd(-1)
#endif // defined(_WIN32)
{
}

CtsCorController::~CtsCorController()
{
    close();
}

bool CtsCorController::open(int reuseFd)
{
    if (m_isOpen)
        return true;

#if defined(_WIN32)
    std::string deviceName = m_port;
    if (deviceName.find("\\\\.\\") == std::string::npos) {
        deviceName = "\\\\." + m_port;
    }

    m_fd = ::CreateFileA(deviceName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_fd == INVALID_HANDLE_VALUE) {
        ::LogError(LOG_HOST, "Cannot open CTS COR device - %s, err=%04lx", m_port.c_str(), ::GetLastError());
        return false;
    }

    DCB dcb;
    if (::GetCommState(m_fd, &dcb) == 0) {
        ::LogError(LOG_HOST, "Cannot get the attributes for %s, err=%04lx", m_port.c_str(), ::GetLastError());
        ::CloseHandle(m_fd);
        m_fd = INVALID_HANDLE_VALUE;
        return false;
    }

    dcb.BaudRate = 9600;
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
        ::LogError(LOG_HOST, "Cannot set the attributes for %s, err=%04lx", m_port.c_str(), ::GetLastError());
        ::CloseHandle(m_fd);
        m_fd = INVALID_HANDLE_VALUE;
        return false;
    }

#else
    // If reusing an existing file descriptor from RTS PTT, don't open a new one
    if (reuseFd >= 0) {
        m_fd = reuseFd;
        m_ownsFd = false; // Only COR can close file descriptor
        ::LogInfo(LOG_HOST, "CTS COR Controller reusing file descriptor from RTS PTT on %s", m_port.c_str());
        m_isOpen = true;
        return true;
    }
    
    m_ownsFd = true; // COR owns the file descriptor

    // Open port if not available
    m_fd = ::open(m_port.c_str(), O_RDONLY | O_NOCTTY | O_NDELAY, 0);
    if (m_fd < 0) {
        // Try rw if ro fails
        m_fd = ::open(m_port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY, 0);
        if (m_fd < 0) {
            ::LogError(LOG_HOST, "Cannot open CTS COR device - %s", m_port.c_str());
            return false;
        }
    }

    if (::isatty(m_fd) == 0) {
        ::LogError(LOG_HOST, "%s is not a TTY device", m_port.c_str());
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    // Save current RTS state before configuring termios
    int savedModemState = 0;
    if (::ioctl(m_fd, TIOCMGET, &savedModemState) < 0) {
        ::LogError(LOG_HOST, "Cannot get the control attributes for %s", m_port.c_str());
        ::close(m_fd);
        m_fd = -1;
        return false;
    }
    bool savedRtsState = (savedModemState & TIOCM_RTS) != 0;

    if (!setTermios()) {
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    // Restore RTS to its original state
    int currentModemState = 0;
    if (::ioctl(m_fd, TIOCMGET, &currentModemState) < 0) {
        ::LogError(LOG_HOST, "Cannot get the control attributes for %s after termios", m_port.c_str());
        ::close(m_fd);
        m_fd = -1;
        return false;
    }
    bool currentRtsState = (currentModemState & TIOCM_RTS) != 0;
    if (currentRtsState != savedRtsState) {
        // Restore RTS to original state
        if (savedRtsState) {
            currentModemState |= TIOCM_RTS;
        } else {
            currentModemState &= ~TIOCM_RTS;
        }
        if (::ioctl(m_fd, TIOCMSET, &currentModemState) < 0) {
            ::LogError(LOG_HOST, "Cannot restore RTS state for %s", m_port.c_str());
            ::close(m_fd);
            m_fd = -1;
            return false;
        }
        ::LogDebug(LOG_HOST, "CTS COR: Restored RTS to %s on %s", savedRtsState ? "HIGH" : "LOW", m_port.c_str());
    }
#endif // defined(_WIN32)

    ::LogInfo(LOG_HOST, "CTS COR Controller opened on %s (RTS preserved)", m_port.c_str());
    m_isOpen = true;
    return true;
}

void CtsCorController::close()
{
    if (!m_isOpen)
        return;

#if defined(_WIN32)
    if (m_fd != INVALID_HANDLE_VALUE) {
        ::CloseHandle(m_fd);
        m_fd = INVALID_HANDLE_VALUE;
    }
#else
    // Only close the file descriptor if we opened it ourselves
    // If we're reusing a descriptor from RTS PTT, don't close it
    if (m_fd != -1 && m_ownsFd) {
        ::close(m_fd);
        m_fd = -1;
    } else if (m_fd != -1 && !m_ownsFd) {
        m_fd = -1;
    }
#endif // defined(_WIN32)

    m_isOpen = false;
    ::LogInfo(LOG_HOST, "CTS COR Controller closed");
}

bool CtsCorController::isCtsAsserted()
{
    if (!m_isOpen)
        return false;

#if defined(_WIN32)
    DWORD modemStat = 0;
    if (::GetCommModemStatus(m_fd, &modemStat) == 0) {
        ::LogError(LOG_HOST, "Cannot read modem status for %s, err=%04lx", m_port.c_str(), ::GetLastError());
        return false;
    }
    return (modemStat & MS_CTS_ON) != 0;
#else
    int modemState = 0;
    if (::ioctl(m_fd, TIOCMGET, &modemState) < 0) {
        ::LogError(LOG_HOST, "Cannot get the control attributes for %s", m_port.c_str());
        return false;
    }
    return (modemState & TIOCM_CTS) != 0;
#endif // defined(_WIN32)
}

bool CtsCorController::setTermios()
{
#if !defined(_WIN32)
    termios termios;
    if (::tcgetattr(m_fd, &termios) < 0) {
        ::LogError(LOG_HOST, "Cannot get the attributes for %s", m_port.c_str());
        return false;
    }

    termios.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK);
    termios.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL);
    termios.c_iflag &= ~(IXON | IXOFF | IXANY);
    termios.c_oflag &= ~(OPOST);
    // Important: Disable hardware flow control (CRTSCTS) to avoid affecting RTS
    // We only want to read CTS, not control RTS
    termios.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CRTSCTS);
    termios.c_cflag |= (CS8 | CLOCAL | CREAD);
    termios.c_lflag &= ~(ISIG | ICANON | IEXTEN);
    termios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    termios.c_cc[VMIN] = 0;
    termios.c_cc[VTIME] = 10;

    ::cfsetospeed(&termios, B9600);
    ::cfsetispeed(&termios, B9600);

    if (::tcsetattr(m_fd, TCSANOW, &termios) < 0) {
        ::LogError(LOG_HOST, "Cannot set the attributes for %s", m_port.c_str());
        return false;
    }
#endif // !defined(_WIN32)

    return true;
}


