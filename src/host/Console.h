// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *
 */
/**
 * @file Console.h
 * @ingroup host
 * @file Console.cpp
 * @ingroup host
 */
#if !defined(__CONSOLE_H__)
#define __CONSOLE_H__

#include "Defines.h"

#include <termios.h>

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Implements cross-platform handling of the terminal console. This is
 *  mainly used for the calibration mode.
 * @ingroup host
 */
class HOST_SW_API Console {
public:
    /**
     * @brief Initializes a new instance of the Console class.
     */
    Console();
    /**
     * @brief Finalizes a instance of the Console class.
     */
    ~Console();

    /**
     * @brief Opens the terminal console.
     * @returns bool True, if the console was opened, otherwise false.
     */
    bool open();

    /**
     * @brief Closes the terminal console.
     */
    void close();

    /**
     * @brief Retrieves a character input on the keyboard.
     * @returns int Character input on the keyboard.
     */
    int getChar();

    /**
     * @brief Retrieves an array of characters input on the keyboard.
     * @param line 
     * @param max 
     * @param mask 
     * @returns int 
     */
    int getLine(char line[], int max, char mask);

private:
    termios m_termios;
};

#endif // __CONSOLE_H__
