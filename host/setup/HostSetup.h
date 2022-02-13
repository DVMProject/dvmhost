/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2021 by Bryan Biedenkapp N2PLL
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
#if !defined(__HOST_SETUP_H__)
#define __HOST_SETUP_H__

#include "Defines.h"
#include "host/calibrate/Console.h"
#include "host/Host.h"
#include "lookups/IdenTableLookup.h"
#include "yaml/Yaml.h"

#include <string>

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements an interactive session to setup the DVM.
// ---------------------------------------------------------------------------

class HOST_SW_API HostSetup {
public:
    /// <summary>Initializes a new instance of the HostSetup class.</summary>
    HostSetup(const std::string& confFile);
    /// <summary>Finalizes a instance of the HostSetup class.</summary>
    ~HostSetup();

    /// <summary>Executes the processing loop.</summary>
    int run();

private:
    const std::string& m_confFile;
    yaml::Node m_conf;

    Console m_console;

    bool m_duplex;

    uint32_t m_rxFrequency;
    uint32_t m_txFrequency;
    uint8_t m_channelId;
    uint32_t m_channelNo;

    lookups::IdenTableLookup* m_idenTable;

    /// <summary>Helper to print the help to the console.</summary>
    void displayHelp();

    /// <summary>Helper to calculate the Rx/Tx frequencies.</summary>
    bool calculateRxTxFreq();

    /// <summary>Helper to sleep the thread.</summary>
    void sleep(uint32_t ms);

    /// <summary>Prints the current status.</summary>
    void printStatus();
};

#endif // __HOST_SETUP_H__
