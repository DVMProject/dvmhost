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
*   Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
*
*/
#if !defined(__CONSOLE_H__)
#define __CONSOLE_H__

#include "Defines.h"

#include <termios.h>

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
    termios m_termios;
};

#endif // __CONSOLE_H__
