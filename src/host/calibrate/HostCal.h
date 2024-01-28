// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @derivedfrom MMDVMCal (https://github.com/g4klx/MMDVMCal)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
*   Copyright (C) 2017,2018 Andy Uribe, CA6JAU
*   Copyright (C) 2021-2023 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__HOST_CAL_H__)
#define __HOST_CAL_H__

#include "Defines.h"
#include "host/setup/HostSetup.h"
#include "host/Console.h"

#include <string>

#if !defined(CATCH2_TEST_COMPILATION)

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the interactive calibration mode.
// ---------------------------------------------------------------------------

class HOST_SW_API HostCal : public HostSetup {
public:
    /// <summary>Initializes a new instance of the HostCal class.</summary>
    HostCal(const std::string& confFile);
    /// <summary>Finalizes a instance of the HostCal class.</summary>
    ~HostCal();

    /// <summary>Executes the calibration processing loop.</summary>
    int run(int argc, char **argv);

private:
    Console m_console;

    /// <summary>Helper to print the calibration help to the console.</summary>
    void displayHelp();

    /// <summary>Helper to change the Tx level.</summary>
    bool setTXLevel(int incr);
    /// <summary>Helper to change the Rx level.</summary>
    bool setRXLevel(int incr);
    /// <summary>Helper to change the Tx DC offset.</summary>
    bool setTXDCOffset(int incr);
    /// <summary>Helper to change the Rx DC offset.</summary>
    bool setRXDCOffset(int incr);
    /// <summary>Helper to change the Tx coarse level.</summary>
    bool setTXCoarseLevel(int incr);
    /// <summary>Helper to change the Rx coarse level.</summary>
    bool setRXCoarseLevel(int incr);
    /// <summary>Helper to change the Rx fine level.</summary>
    bool setRXFineLevel(int incr);
    /// <summary>Helper to change the RSSI level.</summary>
    bool setRSSICoarseLevel(int incr);

    /// <summary>Prints the current status of the calibration.</summary>
    void printStatus();
};

#endif // !defined(CATCH2_TEST_COMPILATION)

#endif // __HOST_CAL_H__
