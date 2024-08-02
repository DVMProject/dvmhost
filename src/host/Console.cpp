// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *
 */
#include "host/Console.h"

#include <cstdio>

#if defined(_WIN32)
#include <conio.h>
#else
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#endif // defined(_WIN32)

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the Console class. */

#if !defined(_WIN32)
Console::Console() :
    m_termios()
#else
Console::Console()
#endif // !defined(_WIN32)
{
#if !defined(_WIN32)
    ::memset(&m_termios, 0x00U, sizeof(termios));
#endif // !defined(_WIN32)
}

/* Finalizes a instance of the Console class. */

Console::~Console() = default;

/* Opens the terminal console. */

bool Console::open()
{
#if !defined(_WIN32)
    termios tios;

    int n = ::tcgetattr(STDIN_FILENO, &tios);
    if (n != 0) {
        ::fprintf(stderr, "tcgetattr: returned %d\r\n", n);
        return false;
    }

    m_termios = tios;

    ::cfmakeraw(&tios);

    n = ::tcsetattr(STDIN_FILENO, TCSANOW, &tios);
    if (n != 0) {
        ::fprintf(stderr, "tcsetattr: returned %d\r\n", n);
        return false;
    }
#endif // !defined(_WIN32)
    return true;
}

/* Closes the terminal console. */

void Console::close()
{
#if !defined(_WIN32)
    int n = ::tcsetattr(STDIN_FILENO, TCSANOW, &m_termios);
    if (n != 0)
        ::fprintf(stderr, "tcsetattr: returned %d\r\n", n);
#endif // !defined(_WIN32)
}

/* Retrieves a character input on the keyboard. */

int Console::getChar()
{
#if defined(_WIN32)
    if (::_kbhit() == 0)
        return -1;

    return ::_getch();
#else
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
#endif // defined(_WIN32)
}

/* Retrieves an array of characters input on the keyboard. */

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
