// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017,2018 Andy Uribe, CA6JAU
 *  Copyright (C) 2021-2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file HostCal.h
 * @ingroup setup
 * @file HostCal.cpp
 * @ingroup setup
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
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the interactive calibration mode.
 * @ingroup setup
 */
class HOST_SW_API HostCal : public HostSetup {
public:
    /**
     * @brief Initializes a new instance of the HostCal class.
     * @param confFile Full-path to the configuration file.
     */
    HostCal(const std::string& confFile);
    /**
     * @brief Finalizes a instance of the HostCal class.
     */
    ~HostCal();

    /**
     * @brief Executes the calibration processing loop.
     * @param argc main() argc. 
     * @param argv main() argv.
     * @returns int Exit code.
     */
    int run(int argc, char **argv);

private:
    Console m_console;

    /**
     * @brief Helper to print the calibration help to the console.
     */
    void displayHelp();

    /**
     * @brief Helper to change the Tx level.
     * @param incr Amount to increment.
     * @returns bool True, if level incremented, otherwise false.
     */
    bool setTXLevel(int incr);
    /**
     * @brief Helper to change the Rx level.
     * @param incr Amount to increment.
     * @returns bool True, if level incremented, otherwise false.
     */
    bool setRXLevel(int incr);
    /**
     * @brief Helper to change the Tx DC offset.
     * @param incr Amount to change.
     * @returns bool True, if offset changed, otherwise false.
     */
    bool setTXDCOffset(int incr);
    /**
     * @brief Helper to change the Rx DC offset.
     * @param incr Amount to change.
     * @returns bool True, if offset changed, otherwise false.
     */
    bool setRXDCOffset(int incr);
    /**
     * @brief Helper to change the Tx coarse level.
     * @param incr Amount to change.
     * @returns bool True, if offset changed, otherwise false.
     */
    bool setTXCoarseLevel(int incr);
    /**
     * @brief Helper to change the Rx coarse level.
     * @param incr Amount to change.
     * @returns bool True, if offset changed, otherwise false.
     */
    bool setRXCoarseLevel(int incr);
    /**
     * @brief Helper to change the Rx fine level.
     * @param incr Amount to change.
     * @returns bool True, if offset changed, otherwise false.
     */
    bool setRXFineLevel(int incr);
    /**
     * @brief Helper to change the RSSI level.
     * @param incr Amount to change.
     * @returns bool True, if offset changed, otherwise false.
     */
    bool setRSSICoarseLevel(int incr);

    /**
     * @brief Prints the current status of the calibration.
     */
    void printStatus();
};

#endif // !defined(CATCH2_TEST_COMPILATION)

#endif // __HOST_CAL_H__
