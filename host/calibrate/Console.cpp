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
*   Copyright (C) 2015,2016 by Jonathan Naylor G4KLX
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
#include "host/calibrate/Console.h"

#include <cstdio>

#if defined(_WIN32) || defined(_WIN64)
#include <conio.h>
#else
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#endif

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
#if defined(_WIN32) || defined(_WIN64)
/// <summary>
/// Initializes a new instance of the Console class.
/// </summary>
Console::Console()
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the Console class.
/// </summary>
Console::~Console()
{
    /* stub */
}

/// <summary>
/// Opens the terminal console.
/// </summary>
bool Console::open()
{
    return true;
}

/// <summary>
/// Retrieves a character input on the keyboard.
/// </summary>
/// <returns></returns>
int Console::getChar()
{
    if (::_kbhit() == 0)
        return -1;

    return ::_getch();
}

/// <summary>
/// Closes the terminal console.
/// </summary>
void Console::close()
{
    /* stub */
}
#else
/// <summary>
/// Initializes a new instance of the Console class.
/// </summary>
Console::Console() :
    m_termios()
{
    ::memset(&m_termios, 0x00U, sizeof(termios));
}

/// <summary>
/// Finalizes a instance of the Console class.
/// </summary>
Console::~Console()
{
    /* stub */
}

/// <summary>
/// Opens the terminal console.
/// </summary>
bool Console::open()
{
    termios tios;

    int n = ::tcgetattr(STDIN_FILENO, &tios);
    if (n != 0) {
        ::fprintf(stderr, "tcgetattr: returned %d\r\n", n);
        return -1;
    }

    m_termios = tios;

    ::cfmakeraw(&tios);

    n = ::tcsetattr(STDIN_FILENO, TCSANOW, &tios);
    if (n != 0) {
        ::fprintf(stderr, "tcsetattr: returned %d\r\n", n);
        return -1;
    }

    return true;
}

/// <summary>
/// Retrieves a character input on the keyboard.
/// </summary>
/// <returns></returns>
int Console::getChar()
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    int n = ::select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    if (n <= 0) {
        if (n < 0)
            ::fprintf(stderr, "select: returned %d\r\n", n);
        return -1;
    }

    char c;
    n = ::read(STDIN_FILENO, &c, 1);
    if (n <= 0) {
        if (n < 0)
            ::fprintf(stderr, "read: returned %d\r\n", n);
        return -1;
    }

    return c;
}

/// <summary>
/// Closes the terminal console.
/// </summary>
void Console::close()
{
    int n = ::tcsetattr(STDIN_FILENO, TCSANOW, &m_termios);
    if (n != 0)
        ::fprintf(stderr, "tcsetattr: returned %d\r\n", n);
}
#endif

/// <summary>
/// Retrieves an array of characters input on the keyboard.
/// </summary>
/// <param name="line"></param>
/// <param name="max"></param>
/// <param name="mask"></param>
/// <returns></returns>
int Console::getLine(char line[], int max, char mask)
{
    int nch = 0;
    int c;
    bool skipNext = false;
    max = max - 1; /* leave room for '\0' */

    while ((c = getChar()) != '\n') {
        if (c != -1) {
            if (c == 10 || c == 13)
                break;

            // skip "double-byte" control characters
            if (c == 224) {
                skipNext = true;
                continue;
            }

            if (skipNext) {
                skipNext = false;
                continue;
            }

            // has characters and backspace character?
            if (nch > 0 && (c == 127 || c == 8)) {
                // handle backspace
                ::fputc(0x8, stdout);
                ::fputc(' ', stdout);
                ::fputc(0x8, stdout);
                ::fflush(stdout);

                line[--nch] = 0;
            }
            else {
                // skip control characters
                if (iscntrl(c))
                    continue;

                if (nch < max) {
                    // valid mask character?
                    if (' ' - 1 < mask && mask < 127)
                        ::fputc(mask, stdout);
                    else
                        ::fputc(c, stdout);
                    ::fflush(stdout);

                    line[nch++] = c;
                }
            }
        }
    }

    if (c == EOF && nch == 0)
        return EOF;

    ::fputc('\r', stdout);
    ::fputc('\n', stdout);
    ::fflush(stdout);

    line[nch] = '\0';
    return nch;
}
