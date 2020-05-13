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
*   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2017,2018 by Andy Uribe CA6JAU
*   Copyright (C) 2017-2020 by Bryan Biedenkapp N2PLL
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
#if !defined(__HOST_CAL_H__)
#define __HOST_CAL_H__

#include "Defines.h"
#include "edac/AMBEFEC.h"
#include "modem/Modem.h"
#include "modem/SerialController.h"
#include "host/calibrate/Console.h"
#include "host/Host.h"
#include "yaml/Yaml.h"

#include <string>

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the interactive calibration mode.
// ---------------------------------------------------------------------------

class HOST_SW_API HostCal {
public:
    /// <summary>Initializes a new instance of the HostCal class.</summary>
    HostCal(const std::string& confFile);
    /// <summary>Finalizes a instance of the HostCal class.</summary>
    ~HostCal();

    /// <summary>Executes the calibration processing loop.</summary>
    int run();

private:
    const std::string& m_confFile;
    yaml::Node m_conf;

    std::string m_port;
    modem::CSerialController m_serial;

    Console m_console;
    edac::AMBEFEC m_fec;
    bool m_transmit;

    bool m_duplex;

    bool m_txInvert;
    bool m_rxInvert;
    bool m_pttInvert;

    bool m_dcBlocker;

    float m_txLevel;
    float m_rxLevel;

    bool m_dmrEnabled;
    bool m_dmrRx1K;
    bool m_p25Enabled;
    bool m_p25Rx1K;
    int m_txDCOffset;
    int m_rxDCOffset;

    uint8_t m_fdmaPreamble;
    uint8_t m_dmrRxDelay;

    bool m_debug;

    uint8_t m_mode;
    std::string m_modeStr;

    uint32_t m_berFrames;
    uint32_t m_berBits;
    uint32_t m_berErrs;
    uint32_t m_berUndecodableLC;
    uint32_t m_berUncorrectable;

    uint32_t m_timeout;
    uint32_t m_timer;

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
    /// <summary>Helper to toggle modem transmit mode.</summary>
    bool setTransmit();

    /// <summary>Initializes the modem DSP.</summary>
    bool initModem();
    /// <summary>Read data frames from the modem DSP.</summary>
    int readModem(uint8_t* buffer, uint32_t length);
    /// <summary>Process DMR Rx BER.</summary>
    void processDMRBER(const uint8_t* buffer, uint8_t seq);
    /// <summary>Process DMR Tx 1011hz BER.</summary>
    void processDMR1KBER(const uint8_t* buffer, uint8_t seq);
    /// <summary>Process P25 Rx BER.</summary>
    void processP25BER(const uint8_t* buffer);
    /// <summary>Process P25 Tx 1011hz BER.</summary>
    void processP251KBER(const uint8_t* buffer);
    /// <summary>Retrieve the modem DSP version.</summary>
    bool getFirmwareVersion();
    /// <summary>Write configuration to the modem DSP.</summary>
    bool writeConfig();
    /// <summary>Write configuration to the modem DSP.</summary>
    bool writeConfig(uint8_t modeOverride);
    /// <summary></summary>
    void sleep(uint32_t ms);

    /// <summary></summary>
    void timerClock();
    /// <summary></summary>
    void timerStart();
    /// <summary></summary>
    void timerStop();

    /// <summary>Prints the current status of the calibration.</summary>
    void printStatus();
    /// <summary></summary>
    void printDebug(const uint8_t* buffer, uint32_t length);

    /// <summary></summary>
    unsigned char countErrs(unsigned char a, unsigned char b);
};

#endif // __HOST_CAL_H__
