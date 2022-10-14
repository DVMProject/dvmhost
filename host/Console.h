/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMCal project. (https://github.com/g4klx/MMDVMCal)
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
#if !defined(__CONSOLE_H__)
#define __CONSOLE_H__

#include "Defines.h"

#if !defined(_WIN32) && !defined(_WIN64)
#include <termios.h>
#endif

// ---------------------------------------------------------------------------
//  Class Declaration
//      Implements cross-platform handling of the terminal console. This is
//      mainly used for the calibration mode.
// ---------------------------------------------------------------------------

class HOST_SW_API Console {
public:
    /// <summary>Initializes a new instance of the Console class.</summary>
    Console();
    /// <summary>Finalizes a instance of the Console class.</summary>
    ~Console();

    /// <summary>Opens the terminal console.</summary>
    bool open();

    /// <summary>Retrieves a character input on the keyboard.</summary>
    int getChar();

    /// <summary>Retrieves an array of characters input on the keyboard.</summary>
    int getLine(char line[], int max, char mask);

    /// <summary>Closes the terminal console.</summary>
    void close();

private:
#if !defined(_WIN32) && !defined(_WIN64)
    termios m_termios;
#endif
};

#endif // __CONSOLE_H__
