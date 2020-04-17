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
#include "host/calibrate/HostCal.h"
#include "dmr/DMRDefines.h"
#include "p25/P25Defines.h"
#include "p25/data/DataHeader.h"
#include "p25/lc/LC.h"
#include "p25/P25Utils.h"
#include "HostMain.h"
#include "Log.h"
#include "Utils.h"

using namespace modem;

#include <cstdio>

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define DMR_CAL_STR         "[Tx] DMR 1200 Hz Tone Mode (2.75Khz Deviation)"
#define P25_CAL_STR         "[Tx] P25 1200 Hz Tone Mode (2.83Khz Deviation)"
#define LF_CAL_STR          "[Tx] DMR Low Frequency Mode (80 Hz square wave)"
#define DMR_CAL_1K_STR      "[Tx] DMR BS 1031 Hz Test Pattern (TS2 CC1 ID1 TG9)"
#define DMR_DMO_CAL_1K_STR  "[Tx] DMR MS 1031 Hz Test Pattern (TS2 CC1 ID1 TG9)"
#define P25_CAL_1K_STR      "[Tx] P25 1011 Hz Test Pattern (NAC293 ID1 TG1)"
#define DMR_FEC_STR         "[Rx] DMR MS FEC BER Test Mode"
#define DMR_FEC_1K_STR      "[Rx] DMR MS 1031 Hz Test Pattern (CC1 ID1 TG9)"
#define P25_FEC_STR         "[Rx] P25 FEC BER Test Mode"
#define P25_FEC_1K_STR      "[Rx] P25 1011 Hz Test Pattern (NAC293 ID1 TG1)"
#define RSSI_CAL_STR        "RSSI Calibration Mode"

#define DMR_SYM_LA_TST_STR  "[Tx] DMR Symbol Test (Level A [+3])"
#define P25_SYM_LA_TST_STR  "[Tx] P25 Symbol Test (Level A [+3])"
#define DMR_SYM_LB_TST_STR  "[Tx] DMR Symbol Test (Level B [+1])"
#define P25_SYM_LB_TST_STR  "[Tx] P25 Symbol Test (Level B [+1])"
#define DMR_SYM_LC_TST_STR  "[Tx] DMR Symbol Test (Level C [-1])"
#define P25_SYM_LC_TST_STR  "[Tx] P25 Symbol Test (Level C [-1])"
#define DMR_SYM_LD_TST_STR  "[Tx] DMR Symbol Test (Level D [-3])"
#define P25_SYM_LD_TST_STR  "[Tx] P25 Symbol Test (Level D [-3])"

// Voice LC MS Header, CC: 1, srcID: 1, dstID: TG9
const uint8_t VH_DMO1K[] = {
        0x00U, 0x20U, 0x08U, 0x08U, 0x02U, 0x38U, 0x15U, 0x00U, 0x2CU, 0xA0U, 0x14U,
        0x60U, 0x84U, 0x6DU, 0x5DU, 0x7FU, 0x77U, 0xFDU, 0x75U, 0x7EU, 0x30U, 0x30U,
        0x01U, 0x10U, 0x01U, 0x40U, 0x03U, 0xC0U, 0x13U, 0xC1U, 0x1EU, 0x80U, 0x6FU };

// Voice Term MS with LC, CC: 1, srcID: 1, dstID: TG9
const uint8_t VT_DMO1K[] = {
        0x00U, 0x4FU, 0x08U, 0xDCU, 0x02U, 0x88U, 0x15U, 0x78U, 0x2CU, 0xD0U, 0x14U,
        0xC0U, 0x84U, 0xADU, 0x5DU, 0x7FU, 0x77U, 0xFDU, 0x75U, 0x79U, 0x65U, 0x24U,
        0x02U, 0x28U, 0x06U, 0x20U, 0x0FU, 0x80U, 0x1BU, 0xC1U, 0x07U, 0x80U, 0x5CU };

// Voice coding data + FEC, 1031 Hz Test Pattern
const uint8_t VOICE_1K[] = {
        0xCEU, 0xA8U, 0xFEU, 0x83U, 0xACU, 0xC4U, 0x58U, 0x20U, 0x0AU, 0xCEU, 0xA8U,
        0xFEU, 0x83U, 0xA0U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x0CU, 0xC4U, 0x58U,
        0x20U, 0x0AU, 0xCEU, 0xA8U, 0xFEU, 0x83U, 0xACU, 0xC4U, 0x58U, 0x20U, 0x0AU };

// Recommended 1011 Hz test pattern for P25 Phase 1 (ANSI/TIA-102.CAAA)
// NAC: 0x293, srcID: 1, dstID: TG1
unsigned char LDU1_1K[] = {
    0x55, 0x75, 0xF5, 0xFF, 0x77, 0xFF, 0x29, 0x35, 0x54, 0x7B, 0xCB, 0x19, 0x4D, 0x0D, 0xCE, 0x24, 0xA1, 0x24,
    0x0D, 0x43, 0x3C, 0x0B, 0xE1, 0xB9, 0x18, 0x44, 0xFC, 0xC1, 0x62, 0x96, 0x27, 0x60, 0xE4, 0xE2, 0x4A, 0x10,
    0x90, 0xD4, 0x33, 0xC0, 0xBE, 0x1B, 0x91, 0x84, 0x4C, 0xFC, 0x16, 0x29, 0x62, 0x76, 0x0E, 0xC0, 0x00, 0x00,
    0x00, 0x00, 0x03, 0x89, 0x28, 0x49, 0x0D, 0x43, 0x3C, 0x02, 0xF8, 0x6E, 0x46, 0x11, 0x3F, 0xC1, 0x62, 0x94,
    0x89, 0xD8, 0x39, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x38, 0x24, 0xA1, 0x24, 0x35, 0x0C, 0xF0, 0x2F, 0x86, 0xE4,
    0x18, 0x44, 0xFF, 0x05, 0x8A, 0x58, 0x9D, 0x83, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x70, 0xE2, 0x4A, 0x12, 0x40,
    0xD4, 0x33, 0xC0, 0xBE, 0x1B, 0x91, 0x84, 0x4F, 0xF0, 0x16, 0x29, 0x62, 0x76, 0x0E, 0x6D, 0xE5, 0xD5, 0x48,
    0xAD, 0xE3, 0x89, 0x28, 0x49, 0x0D, 0x43, 0x3C, 0x08, 0xF8, 0x6E, 0x46, 0x11, 0x3F, 0xC1, 0x62, 0x96, 0x24,
    0xD8, 0x3B, 0xA1, 0x41, 0xC2, 0xD2, 0xBA, 0x38, 0x90, 0xA1, 0x24, 0x35, 0x0C, 0xF0, 0x2F, 0x86, 0xE4, 0x60,
    0x44, 0xFF, 0x05, 0x8A, 0x58, 0x9D, 0x83, 0x94, 0xC8, 0xFB, 0x02, 0x35, 0xA4, 0xE2, 0x4A, 0x12, 0x43, 0x50,
    0x33, 0xC0, 0xBE, 0x1B, 0x91, 0x84, 0x4F, 0xF0, 0x58, 0x29, 0x62, 0x76, 0x0E, 0xC0, 0x00, 0x00, 0x00, 0x0C,
    0x89, 0x28, 0x49, 0x0D, 0x43, 0x3C, 0x0B, 0xE1, 0xB8, 0x46, 0x11, 0x3F, 0xC1, 0x62, 0x96, 0x27, 0x60, 0xE4 };

unsigned char LDU2_1K[] = {
    0x55, 0x75, 0xF5, 0xFF, 0x77, 0xFF, 0x29, 0x3A, 0xB8, 0xA4, 0xEF, 0xB0, 0x9A, 0x8A, 0xCE, 0x24, 0xA1, 0x24,
    0x0D, 0x43, 0x3C, 0x0B, 0xE1, 0xB9, 0x18, 0x44, 0xFC, 0xC1, 0x62, 0x96, 0x27, 0x60, 0xEC, 0xE2, 0x4A, 0x10,
    0x90, 0xD4, 0x33, 0xC0, 0xBE, 0x1B, 0x91, 0x84, 0x4C, 0xFC, 0x16, 0x29, 0x62, 0x76, 0x0E, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x03, 0x89, 0x28, 0x49, 0x0D, 0x43, 0x3C, 0x02, 0xF8, 0x6E, 0x46, 0x11, 0x3F, 0xC1, 0x62, 0x94,
    0x89, 0xD8, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x24, 0xA1, 0x24, 0x35, 0x0C, 0xF0, 0x2F, 0x86, 0xE4,
    0x18, 0x44, 0xFF, 0x05, 0x8A, 0x58, 0x9D, 0x83, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE2, 0x4A, 0x12, 0x40,
    0xD4, 0x33, 0xC0, 0xBE, 0x1B, 0x91, 0x84, 0x4F, 0xF0, 0x16, 0x29, 0x62, 0x76, 0x0E, 0xE0, 0xE0, 0x00, 0x00,
    0x00, 0x03, 0x89, 0x28, 0x49, 0x0D, 0x43, 0x3C, 0x08, 0xF8, 0x6E, 0x46, 0x11, 0x3F, 0xC1, 0x62, 0x96, 0x24,
    0xD8, 0x39, 0xAE, 0x8B, 0x48, 0xB6, 0x49, 0x38, 0x90, 0xA1, 0x24, 0x35, 0x0C, 0xF0, 0x2F, 0x86, 0xE4, 0x60,
    0x44, 0xFF, 0x05, 0x8A, 0x58, 0x9D, 0x83, 0xB9, 0xA8, 0xF4, 0xF1, 0xFD, 0x60, 0xE2, 0x4A, 0x12, 0x43, 0x50,
    0x33, 0xC0, 0xBE, 0x1B, 0x91, 0x84, 0x4F, 0xF0, 0x58, 0x29, 0x62, 0x76, 0x0E, 0x40, 0x00, 0x00, 0x00, 0x0C,
    0x89, 0x28, 0x49, 0x0D, 0x43, 0x3C, 0x0B, 0xE1, 0xB8, 0x46, 0x11, 0x3F, 0xC1, 0x62, 0x96, 0x27, 0x60, 0xEC };

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the HostCal class.
/// </summary>
/// <param name="confFile">Full-path to the configuration file.</param>
HostCal::HostCal(const std::string& confFile) :
    m_confFile(confFile),
    m_conf(),
    m_port(),
    m_serial(),
    m_console(),
    m_fec(),
    m_transmit(false),
    m_duplex(true),
    m_txInvert(false),
    m_rxInvert(false),
    m_pttInvert(false),
    m_dcBlocker(true),
    m_txLevel(50.0F),
    m_rxLevel(50.0F),
    m_dmrEnabled(false),
    m_dmrRx1K(false),
    m_p25Enabled(false),
    m_p25Rx1K(false),
    m_txDCOffset(0),
    m_rxDCOffset(0),
    m_txDelay(1U),
    m_dmrDelay(7U),
    m_debug(false),
    m_mode(STATE_DMR_CAL),
    m_modeStr(DMR_CAL_STR),
    m_berFrames(0U),
    m_berBits(0U),
    m_berErrs(0U),
    m_berUndecodableLC(0U),
    m_berUncorrectable(0U),
    m_timeout(300U),
    m_timer(0U)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the HostCal class.
/// </summary>
HostCal::~HostCal()
{
    /* stub */
}

/// <summary>
/// Executes the calibration processing loop.
/// </summary>
/// <returns>Zero if successful, otherwise error occurred.</returns>
int HostCal::run()
{
    bool ret = yaml::Parse(m_conf, m_confFile.c_str());
    if (!ret) {
        ::fatal("cannot read the configuration file, %s\n", m_confFile.c_str());
    }

    yaml::Node modemConf = m_conf["system"]["modem"];
    m_port = modemConf["port"].as<std::string>();
    m_serial = CSerialController(m_port, SERIAL_115200);

    // initialize system logging
    ret = ::LogInitialise("", "", 0U, 2U);
    if (!ret) {
        ::fprintf(stderr, "unable to open the log file\n");
        return 1;
    }

    getHostVersion();
    ::LogInfo(">> Modem Calibration");

    if (m_port == NULL_MODEM) {
        ::LogError(LOG_HOST, "Calibration mode is unsupported with the null modem!");
        return 2;
    }

    // open serial connection to modem DSP and initialize
    ret = m_serial.open();
    if (!ret) {
        ::LogError(LOG_CAL, "Failed to open serial device");
        return 1;
    }

    ret = initModem();
    if (!ret) {
        ::LogError(LOG_CAL, "Modem is unresponsive");
        m_serial.close();
        return 1;
    }

    // open terminal console
    ret = m_console.open();
    if (!ret) {
        m_serial.close();
        return 1;
    }

    displayHelp();
    
    m_rxInvert = modemConf["rxInvert"].as<bool>(false);
    m_txInvert = modemConf["txInvert"].as<bool>(false);
    m_pttInvert = modemConf["pttInvert"].as<bool>(false);
    m_dcBlocker = modemConf["dcBlocker"].as<bool>(true);

    m_rxDCOffset = modemConf["rxDCOffset"].as<int>(0);
    m_txDCOffset = modemConf["txDCOffset"].as<int>(0);
    m_rxLevel = modemConf["rxLevel"].as<float>(50.0F);
    m_txLevel = modemConf["txLevel"].as<float>(50.0F);

    m_txDelay = modemConf["txDelay"].as<uint32_t>(1U);
    m_dmrDelay = modemConf["dmrDelay"].as<uint32_t>(7U);

    writeConfig();

    printStatus();

    bool end = false;
    while (!end) {
        int c = m_console.getChar();
        switch (c) {
            /** Level Adjustment Commands */
            case 'I':
                {
                    m_txInvert = !m_txInvert;
                    LogMessage(LOG_CAL, " - TX Invert: %s", m_txInvert ? "On" : "Off");
                    writeConfig();
                }
                break;
            case 'i':
                {
                    m_rxInvert = !m_rxInvert;
                    LogMessage(LOG_CAL, " - RX Invert: %s", m_rxInvert ? "On" : "Off");
                    writeConfig();
                }
                break;
            case 'p':
                {
                    m_pttInvert = !m_pttInvert;
                    LogMessage(LOG_CAL, " - PTT Invert: %s", m_pttInvert ? "On" : "Off");
                    writeConfig();
                }
                break;
            case 'd':
                {
                    m_dcBlocker = !m_dcBlocker;
                    LogMessage(LOG_CAL, " - DC Blocker: %s", m_dcBlocker ? "On" : "Off");
                    writeConfig();
                }
                break;
            case 'R':
                setRXLevel(1);
                break;
            case 'r':
                setRXLevel(-1);
                break;
            case 'T':
                setTXLevel(1);
                break;
            case 't':
                setTXLevel(-1);
                break;
            case 'c':
                setRXDCOffset(-1);
                break;
            case 'C':
                setRXDCOffset(1);
                break;
            case 'o':
                setTXDCOffset(-1);
                break;
            case 'O':
                setTXDCOffset(1);
                break;

            case '1':
                {
                    m_duplex = true;
                    m_dmrEnabled = false;
                    m_dmrRx1K = false;
                    m_p25Enabled = false;
                    m_p25Rx1K = false;
                    m_debug = false;

                    if (m_mode == STATE_DMR_CAL) {
                        m_modeStr = DMR_SYM_LA_TST_STR;
                        
                        LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                        writeConfig(STATE_DMR_LEVELA);
                    }
                    else if (m_mode == STATE_P25_CAL) {
                        m_modeStr = P25_SYM_LA_TST_STR;

                        LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                        writeConfig(STATE_P25_LEVELA);
                    }
                }
                break;
            case '2':
                {
                    m_duplex = true;
                    m_dmrEnabled = false;
                    m_dmrRx1K = false;
                    m_p25Enabled = false;
                    m_p25Rx1K = false;
                    m_debug = false;

                    if (m_mode == STATE_DMR_CAL) {
                        m_modeStr = DMR_SYM_LB_TST_STR;

                        LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                        writeConfig(STATE_DMR_LEVELB);
                    }
                    else if (m_mode == STATE_P25_CAL) {
                        m_modeStr = P25_SYM_LB_TST_STR;

                        LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                        writeConfig(STATE_P25_LEVELB);
                    }
                }
                break;
            case '3':
                {
                    m_duplex = true;
                    m_dmrEnabled = false;
                    m_dmrRx1K = false;
                    m_p25Enabled = false;
                    m_p25Rx1K = false;
                    m_debug = false;

                    if (m_mode == STATE_DMR_CAL) {
                        m_modeStr = DMR_SYM_LC_TST_STR;

                        LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                        writeConfig(STATE_DMR_LEVELC);
                    }
                    else if (m_mode == STATE_P25_CAL) {
                        m_modeStr = P25_SYM_LC_TST_STR;

                        LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                        writeConfig(STATE_P25_LEVELC);
                    }
                }
                break;
            case '4':
                {
                    m_duplex = true;
                    m_dmrEnabled = false;
                    m_dmrRx1K = false;
                    m_p25Enabled = false;
                    m_p25Rx1K = false;
                    m_debug = false;
                
                    if (m_mode == STATE_DMR_CAL) {
                        m_modeStr = DMR_SYM_LD_TST_STR;

                        LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                        writeConfig(STATE_DMR_LEVELD);
                    }
                    else if (m_mode == STATE_P25_CAL) {
                        m_modeStr = P25_SYM_LD_TST_STR;

                        LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                        writeConfig(STATE_P25_LEVELD);
                    }
                }
                break;

            /** Mode Commands */
            case 'Z':
                {
                    m_mode = STATE_DMR_CAL;
                    m_modeStr = DMR_CAL_STR;
                    m_duplex = true;
                    m_dmrEnabled = false;
                    m_dmrRx1K = false;
                    m_p25Enabled = false;
                    m_p25Rx1K = false;
                    m_debug = false;

                    LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                    writeConfig();
                }
                break;
            case 'z':
                {
                    m_mode = STATE_P25_CAL;
                    m_modeStr = P25_CAL_STR;
                    m_duplex = true;
                    m_dmrEnabled = false;
                    m_dmrRx1K = false;
                    m_p25Enabled = false;
                    m_p25Rx1K = false;
                    m_debug = false;

                    LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                    writeConfig();
                }
                break;
            case 'l':
                {
                    m_mode = STATE_LF_CAL;
                    m_modeStr = LF_CAL_STR;
                    m_duplex = true;
                    m_dmrEnabled = false;
                    m_dmrRx1K = false;
                    m_p25Enabled = false;
                    m_p25Rx1K = false;
                    m_debug = false;

                    LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                    writeConfig();
                }
                break;
            case 'M':
                {
                    m_mode = STATE_DMR_CAL_1K;
                    m_modeStr = DMR_CAL_1K_STR;
                    m_duplex = true;
                    m_dmrEnabled = false;
                    m_dmrRx1K = false;
                    m_p25Enabled = false;
                    m_p25Rx1K = false;
                    m_debug = false;

                    LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                    writeConfig();
                }
                break;
            case 'm':
                {
                    m_mode = STATE_DMR_DMO_CAL_1K;
                    m_modeStr = DMR_DMO_CAL_1K_STR;
                    m_duplex = true;
                    m_dmrEnabled = false;
                    m_dmrRx1K = false;
                    m_p25Enabled = false;
                    m_p25Rx1K = false;
                    m_debug = false;

                    LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                    writeConfig();
                }
                break;
            case 'P':
                {
                    m_mode = STATE_P25_CAL_1K;
                    m_modeStr = P25_CAL_1K_STR;
                    m_duplex = true;
                    m_dmrEnabled = false;
                    m_dmrRx1K = false;
                    m_p25Enabled = false;
                    m_p25Rx1K = false;
                    m_debug = false;

                    LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                    writeConfig();
                }
                break;
            case 'B':
            case 'J':
                {
                    m_mode = STATE_DMR;
                    if (c == 'J') {
                        m_modeStr = DMR_FEC_1K_STR;
                        m_dmrRx1K = true;
                    }
                    else {
                        m_modeStr = DMR_FEC_STR;
                        m_dmrRx1K = false;
                    }
                    m_duplex = false;
                    m_dmrEnabled = true;
                    m_p25Enabled = false;
                    m_debug = true;

                    LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                    writeConfig();
                }
                break;
            case 'b':
            case 'j':
                {
                    m_mode = STATE_P25;
                    if (c == 'j') {
                        m_modeStr = P25_FEC_1K_STR;
                        m_p25Rx1K = true;
                    }
                    else {
                        m_modeStr = P25_FEC_STR;
                        m_p25Rx1K = false;
                    }
                    m_duplex = false;
                    m_dmrEnabled = false;
                    m_p25Enabled = true;
                    m_debug = true;

                    LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                    writeConfig();
                }
                break;
            case 'x':
                {
                    m_mode = STATE_RSSI_CAL;
                    m_modeStr = RSSI_CAL_STR;
                    m_duplex = true;
                    m_dmrEnabled = false;
                    m_dmrRx1K = false;
                    m_p25Enabled = false;
                    m_p25Rx1K = false;
                    m_debug = false;

                    LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                    writeConfig();
                }
                break;

            /** General Commands */
            case ' ':
                setTransmit();
                break;
            case '`':
                printStatus();
                break;
            case 'V':
                getHostVersion();
                break;
            case 'v':
                getFirmwareVersion();
                break;
            case 'H':
            case 'h':
                displayHelp();
                break;
            case 'S':
            case 's':
                {
                    yaml::Serialize(m_conf, m_confFile.c_str(), yaml::SerializeConfig(4, 64, false, false));
                    LogMessage(LOG_CAL, " - Saved configuration to %s", m_confFile.c_str());
                }
                break;
            case 'Q':
            case 'q':
                end = true;
                break;

            case -1:
                break;
            default:
                LogError(LOG_CAL, "Unknown command - %c (H/h for help)", c);
                break;
        }

        uint8_t buffer[200U];
        readModem(buffer, 200U);

        timerClock();
        sleep(5U);
    }

    if (m_transmit)
        setTransmit();

    m_serial.close();
    m_console.close();
    return 0;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Helper to print the calibration help to the console.
/// </summary>
void HostCal::displayHelp()
{
    LogMessage(LOG_CAL, "General Commands:");
    LogMessage(LOG_CAL, "    <space>  Toggle transmit");
    LogMessage(LOG_CAL, "    `        Display current settings and operation mode");
    LogMessage(LOG_CAL, "    V        Display version of host");
    LogMessage(LOG_CAL, "    v        Display version of firmware");
    LogMessage(LOG_CAL, "    H/h      Display help");
    LogMessage(LOG_CAL, "    S/s      Save calibration settings to configuration file");
    LogMessage(LOG_CAL, "    Q/q      Quit");
    LogMessage(LOG_CAL, "Level Adjustment Commands:");
    LogMessage(LOG_CAL, "    I        Toggle transmit inversion");
    LogMessage(LOG_CAL, "    i        Toggle receive inversion");
    LogMessage(LOG_CAL, "    p        Toggle PTT inversion");
    LogMessage(LOG_CAL, "    d        Toggle DC blocker");
    LogMessage(LOG_CAL, "    R/r      Increase/Decrease receive level");
    LogMessage(LOG_CAL, "    T/t      Increase/Decrease transmit level");
    LogMessage(LOG_CAL, "    C/c      Increase/Decrease RX DC offset level");
    LogMessage(LOG_CAL, "    O/o      Increase/Decrease TX DC offset level");
    LogMessage(LOG_CAL, "Mode Commands:");
    LogMessage(LOG_CAL, "    Z        %s", DMR_CAL_STR);
    LogMessage(LOG_CAL, "    z        %s", P25_CAL_STR);
    LogMessage(LOG_CAL, "    l        %s", LF_CAL_STR);
    LogMessage(LOG_CAL, "    M        %s", DMR_CAL_1K_STR);
    LogMessage(LOG_CAL, "    m        %s", DMR_DMO_CAL_1K_STR);
    LogMessage(LOG_CAL, "    P        %s", P25_CAL_1K_STR);
    LogMessage(LOG_CAL, "    B        %s", DMR_FEC_STR);
    LogMessage(LOG_CAL, "    J        %s", DMR_FEC_1K_STR);
    LogMessage(LOG_CAL, "    b        %s", P25_FEC_STR);
    LogMessage(LOG_CAL, "    j        %s", P25_FEC_1K_STR);
    LogMessage(LOG_CAL, "    x        %s", RSSI_CAL_STR);
}

/// <summary>
/// Helper to change the Rx level.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setTXLevel(int incr)
{
    if (incr > 0 && m_txLevel < 100.0F) {
        m_txLevel += 0.25F;

        // clamp values
        if (m_txLevel > 100.0F)
            m_txLevel = 100.0F;

        LogMessage(LOG_CAL, " - TX Level: %.1f%%", m_txLevel);
        return writeConfig();
    }

    if (incr < 0 && m_txLevel > 0.0F) {
        m_txLevel -= 0.25F;

        // clamp values
        if (m_txLevel < 0.0F)
            m_txLevel = 0.0F;

        LogMessage(LOG_CAL, " - TX Level: %.1f%%", m_txLevel);
        return writeConfig();
    }

    return true;
}

/// <summary>
/// Helper to change the Rx level.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setRXLevel(int incr)
{
    if (incr > 0 && m_rxLevel < 100.0F) {
        m_rxLevel += 0.25F;

        // clamp values
        if (m_rxLevel > 100.0F)
            m_rxLevel = 100.0F;

        LogMessage(LOG_CAL, " - RX Level: %.1f%%", m_rxLevel);
        return writeConfig();
    }

    if (incr < 0 && m_rxLevel > 0.0F) {
        m_rxLevel -= 0.25F;

        // clamp values
        if (m_rxLevel < 0.0F)
            m_rxLevel = 0.0F;

        LogMessage(LOG_CAL, " - RX Level: %.1f%%", m_rxLevel);
        return writeConfig();
    }

    return true;
}

/// <summary>
/// Helper to change the Tx DC offset.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setTXDCOffset(int incr)
{
    if (incr > 0 && m_txDCOffset < 127) {
        m_txDCOffset++;
        LogMessage(LOG_CAL, " - TX DC Offset: %d", m_txDCOffset);
        return writeConfig();
    }

    if (incr < 0 && m_txDCOffset > -127) {
        m_txDCOffset--;
        LogMessage(LOG_CAL, " - TX DC Offset: %d", m_txDCOffset);
        return writeConfig();
    }

    return true;
}

/// <summary>
/// Helper to change the Rx DC offset.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setRXDCOffset(int incr)
{
    if (incr > 0 && m_rxDCOffset < 127) {
        m_rxDCOffset++;
        LogMessage(LOG_CAL, " - RX DC Offset: %d", m_rxDCOffset);
        return writeConfig();
    }

    if (incr < 0 && m_rxDCOffset > -127) {
        m_rxDCOffset--;
        LogMessage(LOG_CAL, " - RX DC Offset: %d", m_rxDCOffset);
        return writeConfig();
    }

    return true;
}

/// <summary>
/// Helper to toggle modem transmit mode.
/// </summary>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setTransmit()
{
    if (m_dmrEnabled || m_p25Enabled) {
        LogError(LOG_CAL, "No transmit allowed in a BER Test mode");
        return false;
    }

    m_transmit = !m_transmit;

    uint8_t buffer[50U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_CAL_DATA;
    buffer[3U] = m_transmit ? 0x01U : 0x00U;

    int ret = m_serial.write(buffer, 4U);
    if (ret <= 0)
        return false;

    sleep(25U);

    if (m_transmit)
        LogMessage(LOG_CAL, " - Modem start transmitting");
    else
        LogMessage(LOG_CAL, " - Modem stop transmitting");

    ret = readModem(buffer, 50U);
    if (ret <= 0)
        return false;

    if (buffer[2U] == CMD_NAK) {
        LogError(LOG_CAL, "Got a NAK from the modem");
        return false;
    }

    if (buffer[2U] != CMD_ACK) {
        Utils::dump("Invalid response", buffer, ret);
        return false;
    }

    return true;
}

/// <summary>
/// Initializes the modem DSP.
/// </summary>
/// <returns>True, if modem DSP is initialized, otherwise false.</returns>
bool HostCal::initModem()
{
    LogMessage(LOG_CAL, " - Initializing modem");
    sleep(2000U);

    if (!getFirmwareVersion())
        return false;

    bool ret = writeConfig();
    if (!ret) {
        LogMessage(LOG_CAL, " - Modem unresponsive, retrying...");
        sleep(2500U);
        ret = writeConfig();
        if (!ret) {
            LogError(LOG_CAL, "Modem unresponsive to configuration set after 2 attempts, calibration may fail.");
        }
    }

    LogMessage(LOG_CAL, " - Modem Ready");
    return true;
}

/// <summary>
/// Read data frames from the modem DSP.
/// </summary>
/// <param name="buffer"></param>
/// <param name="length"></param>
/// <returns>Zero if no data was read, otherwise returns length of data read.</returns>
int HostCal::readModem(uint8_t *buffer, uint32_t length)
{
    int n = m_serial.read(buffer + 0U, 1U);
    if (n <= 0)
        return n;

    if (buffer[0U] != DVM_FRAME_START)
        return 0;

    n = 0;
    for (uint32_t i = 0U; i < 20U && n == 0; i++) {
        n = m_serial.read(buffer + 1U, 1U);
        if (n < 0)
            return n;
        if (n == 0)
            sleep(10U);
    }

    if (n == 0)
        return -1;

    uint32_t len = buffer[1U];

    uint32_t offset = 2U;
    for (uint32_t i = 0U; i < 20U && offset < len; i++) {
        n = m_serial.read(buffer + offset, len - offset);
        if (n < 0)
            return n;
        if (n == 0)
            sleep(10U);
        if (n > 0)
            offset += n;
    }

    if (len > 0) {
        switch (buffer[2U]) {
            case CMD_CAL_DATA:
                {
                    bool inverted = (buffer[3U] == 0x80U);
                    short high = buffer[4U] << 8 | buffer[5U];
                    short low = buffer[6U] << 8 | buffer[7U];
                    short diff = high - low;
                    short centre = (high + low) / 2;
                    LogMessage(LOG_CAL, "Levels: inverted: %s, max: %d, min: %d, diff: %d, centre: %d", inverted ? "yes" : "no", high, low, diff, centre);
                }
                break;
            case CMD_RSSI_DATA:
                {
                    unsigned short max = buffer[3U] << 8 | buffer[4U];
                    unsigned short min = buffer[5U] << 8 | buffer[6U];
                    unsigned short ave = buffer[7U] << 8 | buffer[8U];
                    LogMessage(LOG_CAL, "RSSI: max: %u, min: %u, ave: %u", max, min, ave);
                }
                break;

            case CMD_DMR_DATA1:
            case CMD_DMR_DATA2:
                processDMRBER(buffer + 4U, buffer[3]);
                break;

            case CMD_DMR_LOST1:
            case CMD_DMR_LOST2:
                {
                    LogMessage(LOG_CAL, "DMR Transmission lost, total frames: %d, bits: %d, uncorrectable frames: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berUndecodableLC, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));

                    if (m_dmrEnabled) {
                        m_berBits = 0U;
                        m_berErrs = 0U;
                        m_berFrames = 0U;
                        m_berUndecodableLC = 0U;
                        m_berUncorrectable = 0U;
                    }
                }
                break;

            case CMD_P25_DATA:
                processP25BER(buffer + 3U);
                break;

            case CMD_P25_LOST:
                {
                    LogMessage(LOG_CAL, "P25 Transmission lost, total frames: %d, bits: %d, uncorrectable frames: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berUndecodableLC, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));

                    if (m_p25Enabled) {
                        m_berBits = 0U;
                        m_berErrs = 0U;
                        m_berFrames = 0U;
                        m_berUndecodableLC = 0U;
                        m_berUncorrectable = 0U;
                    }
                }
                break;

            // These should not be received, but don't complain if we do
            case CMD_GET_STATUS:
            case CMD_GET_VERSION:
            case CMD_ACK:
                break;

            case CMD_NAK:
                LogWarning(LOG_MODEM, "NAK, command = 0x%02X, reason = %u", buffer[3U], buffer[4U]);
                break;

            case CMD_DEBUG1:
            case CMD_DEBUG2:
            case CMD_DEBUG3:
            case CMD_DEBUG4:
            case CMD_DEBUG5:
                printDebug(buffer, len);
                break;

            default:
                LogWarning(LOG_MODEM, "Unknown message, type = %02X", buffer[2U]);
                Utils::dump("Buffer dump", buffer, len);
                break;
        }
    }

    return len;
}

/// <summary>
/// Process DMR Rx BER.
/// </summary>
/// <param name="buffer">Buffer containing DMR audio frames</param>
/// <param name="seq">DMR Audio Sequence</param>
void HostCal::processDMRBER(const uint8_t* buffer, uint8_t seq)
{
    if (seq == 65U) {
        timerStart();

        LogMessage(LOG_CAL, "DMR voice header received");

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
        return;
    }
    else if (seq == 66U) {
        if (m_berFrames != 0U) {
            LogMessage(LOG_CAL, "DMR voice end received, total frames: %d, total bits: %d, uncorrectable frames: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));
        }

        timerStop();

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
        return;
    }

    timerStart();

    uint32_t errs = m_fec.measureDMRBER(buffer);

    float ber = float(errs) / 1.41F;
    if (ber < 10.0F)
        LogMessage(LOG_CAL, "DMR audio seq. %d, FEC BER %% (errs): %.3f%% (%u/141)", seq & 0x0FU, ber, errs);
    else {
        LogWarning(LOG_CAL, "uncorrectable DMR audio seq. %d", seq & 0x0FU);
        m_berUncorrectable++;
    }

    m_berBits += 141U;
    m_berErrs += errs;
    m_berFrames++;
}

/// <summary>
/// Process DMR Tx 1011hz BER.
/// </summary>
/// <param name="buffer">Buffer containing DMR audio frames</param>
/// <param name="seq">DMR Audio Sequence</param>
void HostCal::processDMR1KBER(const uint8_t* buffer, uint8_t seq)
{
    uint32_t errs = 0U;

    if (seq == 65U) {
        timerStart();

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;

        for (uint32_t i = 0U; i < 33U; i++)
            errs += countErrs(buffer[i], VH_DMO1K[i]);

        m_berErrs += errs;
        m_berBits += 264;
        m_berFrames++;
        LogMessage(LOG_CAL, "DMR voice header received, 1031 Test Pattern BER %% (errs): %.3f%% (%u/264)", float(errs) / 2.64F, errs);
    }
    else if (seq == 66U) {
        for (uint32_t i = 0U; i < 33U; i++)
            errs += countErrs(buffer[i], VT_DMO1K[i]);

        m_berErrs += errs;
        m_berBits += 264;
        m_berFrames++;

        if (m_berFrames != 0U) {
            LogMessage(LOG_CAL, "DMR voice end received, total frames: %d, total bits: %d, uncorrectable frames: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));
        }

        timerStop();

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
        return;
    }

    timerStart();

    uint8_t dmrSeq = seq & 0x0FU;
    if (dmrSeq > 5U)
        dmrSeq = 5U;
    
    errs = 0U;
    for (uint32_t i = 0U; i < 33U; i++)
        errs += countErrs(buffer[i], VOICE_1K[i]);

    float ber = float(errs) / 2.64F;

    m_berErrs += errs;
    m_berBits += 264;
    m_berFrames++;

    if (ber < 10.0F)
        LogMessage(LOG_CAL, "DMR audio seq. %d, 1031 Test Pattern BER %% (errs): %.3f%% (%u/264)", seq & 0x0FU, ber, errs);
    else {
        LogWarning(LOG_CAL, "uncorrectable DMR audio seq. %d", seq & 0x0FU);
        m_berUncorrectable++;
    }
}

/// <summary>
/// Process P25 Rx BER.
/// </summary>
/// <param name="buffer">Buffer containing P25 audio frames</param>
void HostCal::processP25BER(const uint8_t* buffer)
{
    using namespace p25;

    uint8_t nid[P25_NID_LENGTH_BYTES];
    P25Utils::decode(buffer + 1U, nid, 48U, 114U);
    uint8_t duid = nid[1U] & 0x0FU;

    uint32_t errs = 0U;
    uint8_t imbe[18U];
    lc::LC lc = lc::LC();
    data::DataHeader dataHeader = data::DataHeader();

    if (duid == P25_DUID_HDU) {
        timerStart();

        bool ret = lc.decodeHDU(buffer + 1U);
        if (!ret) {
            LogWarning(LOG_CAL, "P25_DUID_HDU (Header), undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_RF, "P25_DUID_HDU (Header), dstId = %u, algo = %X, kid = %X", lc.getDstId(), lc.getAlgId(), lc.getKId());
        }
        
        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
    }
    else if (duid == P25_DUID_TDU) {
        if (m_berFrames != 0U) {
            LogMessage(LOG_CAL, "P25_DUID_TDU (Terminator Data Unit), total frames: %d, bits: %d, uncorrectable frames: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berUndecodableLC, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));
        }

        timerStop();

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
        return;
    }
    else if (duid == P25_DUID_LDU1) {
        timerStart();

        bool ret = lc.decodeLDU1(buffer + 1U);
        if (!ret) {
            LogWarning(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1), undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1) LC, mfId = $%02X, lco = $%02X, emerg = %u, encrypt = %u, prio = %u, group = %u, srcId = %u, dstId = %u", 
                lc.getMFId(), lc.getLCO(), lc.getEmergency(), lc.getEncrypted(), lc.getPriority(), lc.getGroup(), lc.getSrcId(), lc.getDstId());
        }

        P25Utils::decode(buffer + 1U, imbe, 114U, 262U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 262U, 410U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 452U, 600U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 640U, 788U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 830U, 978U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1020U, 1168U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1208U, 1356U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1398U, 1546U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1578U, 1726U);
        errs += m_fec.measureP25BER(imbe);

        float ber = float(errs) / 12.33F;
        if (ber < 10.0F)
            LogMessage(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1), audio FEC BER (errs): %.3f%% (%u/1233)", ber, errs);
        else {
            LogWarning(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1), uncorrectable audio");
            m_berUncorrectable++;
        }

        m_berBits += 1233U;
        m_berErrs += errs;
        m_berFrames++;
    }
    else if (duid == P25_DUID_LDU2) {
        timerStart();

        bool ret = lc.decodeLDU2(buffer + 1U);
        if (!ret) {
            LogWarning(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2), undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2) LC, mfId = $%02X, algo = %X, kid = %X", 
                lc.getMFId(), lc.getAlgId(), lc.getKId());
        }

        P25Utils::decode(buffer + 1U, imbe, 114U, 262U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 262U, 410U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 452U, 600U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 640U, 788U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 830U, 978U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1020U, 1168U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1208U, 1356U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1398U, 1546U);
        errs += m_fec.measureP25BER(imbe);

        P25Utils::decode(buffer + 1U, imbe, 1578U, 1726U);
        errs += m_fec.measureP25BER(imbe);

        float ber = float(errs) / 12.33F;
        if (ber < 10.0F)
            LogMessage(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2), audio FEC BER (errs): %.3f%% (%u/1233)", ber, errs);
        else {
            LogWarning(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2), uncorrectable audio");
            m_berUncorrectable++;
        }

        m_berBits += 1233U;
        m_berErrs += errs;
        m_berFrames++;
    }
    else if (duid == P25_DUID_PDU) {
        timerStop();

        // note: for the calibrator we will only process the PDU header -- and not the PDU data
        uint8_t pduBuffer[P25_LDU_FRAME_LENGTH_BYTES];
        uint32_t bits = P25Utils::decode(buffer + 1U, pduBuffer, 0, P25_LDU_FRAME_LENGTH_BITS);

        uint8_t* rfPDU = new uint8_t[P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U];
        ::memset(rfPDU, 0x00U, P25_MAX_PDU_COUNT * P25_LDU_FRAME_LENGTH_BYTES + 2U);
        uint32_t rfPDUBits = 0U;

        for (uint32_t i = 0U; i < bits; i++, rfPDUBits++) {
            bool b = READ_BIT(buffer, i);
            WRITE_BIT(rfPDU, rfPDUBits, b);
        }

        bool ret = dataHeader.decode(rfPDU + P25_SYNC_LENGTH_BYTES + P25_NID_LENGTH_BYTES);
        if (!ret) {
            LogWarning(LOG_RF, "P25_DUID_PDU (Packet Data Unit), unfixable RF 1/2 rate header data");
            Utils::dump(1U, "Unfixable PDU Data", rfPDU + P25_SYNC_LENGTH_BYTES + P25_NID_LENGTH_BYTES, P25_PDU_HEADER_LENGTH_BYTES);
        }
        else {
            LogMessage(LOG_CAL, "P25_DUID_PDU (Packet Data Unit), fmt = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padCount = %u, n = %u, seqNo = %u",
                dataHeader.getFormat(), dataHeader.getSAP(), dataHeader.getFullMessage(), dataHeader.getBlocksToFollow(), dataHeader.getPadCount(),
                dataHeader.getN(), dataHeader.getSeqNo());
        }

        delete[] rfPDU;
    }
    else if (duid == P25_DUID_TSDU) {
        timerStop();

        lc::TSBK tsbk = lc::TSBK();
        bool ret = tsbk.decode(buffer + 1U);
        if (!ret) {
            LogWarning(LOG_CAL, "P25_DUID_TSDU (Trunking System Data Unit), undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, "P25_DUID_TSDU (Trunking System Data Unit), mfId = $%02X, lco = $%02X, srcId = %u, dstId = %u, service = %u, status = %u, message = %u, extFunc = %u, netId = %u, sysId = %u",
                tsbk.getMFId(), tsbk.getLCO(), tsbk.getSrcId(), tsbk.getDstId(), tsbk.getService(), tsbk.getStatus(), tsbk.getMessage(), tsbk.getExtendedFunction(),
                tsbk.getNetId(), tsbk.getSysId());
        }
    }
}

/// <summary>
/// Process P25 Tx 1011hz BER.
/// </summary>
/// <param name="buffer">Buffer containing P25 audio frames</param>
void HostCal::processP251KBER(const uint8_t* buffer)
{
    using namespace p25;

    uint8_t nid[P25_NID_LENGTH_BYTES];
    P25Utils::decode(buffer + 1U, nid, 48U, 114U);
    uint8_t duid = nid[1U] & 0x0FU;

    uint32_t errs = 0U;
    lc::LC lc = lc::LC();

    if (duid == P25_DUID_HDU) {
        timerStart();

        bool ret = lc.decodeHDU(buffer + 1U);
        if (!ret) {
            LogWarning(LOG_CAL, "P25_DUID_HDU (Header), undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_RF, "P25_DUID_HDU (Header), dstId = %u, algo = %X, kid = %X", lc.getDstId(), lc.getAlgId(), lc.getKId());
        }

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
    }
    else if (duid == P25_DUID_TDU) {
        if (m_berFrames != 0U) {
            LogMessage(LOG_CAL, "P25_DUID_TDU (Terminator Data Unit), total frames: %d, bits: %d, uncorrectable frames: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berUndecodableLC, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));
        }

        timerStop();

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
        return;
    }
    else if (duid == P25_DUID_LDU1) {
        timerStart();

        bool ret = lc.decodeLDU1(buffer + 1U);
        if (!ret) {
            LogWarning(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1), undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1) LC, mfId = $%02X, lco = $%02X, emerg = %u, encrypt = %u, prio = %u, group = %u, srcId = %u, dstId = %u",
                lc.getMFId(), lc.getLCO(), lc.getEmergency(), lc.getEncrypted(), lc.getPriority(), lc.getGroup(), lc.getSrcId(), lc.getDstId());
        }

        for (uint32_t i = 0U; i < 216U; i++)
            errs += countErrs(buffer[i + 1U], LDU1_1K[i]);

        float ber = float(errs) / 12.33F;
        if (ber < 10.0F)
            LogMessage(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1), 1011 Test Pattern BER (errs): %.3f%% (%u/1233)", ber, errs);
        else {
            LogWarning(LOG_CAL, "P25_DUID_LDU1 (Logical Data Unit 1), uncorrectable audio");
            m_berUncorrectable++;
        }

        m_berBits += 1233U;
        m_berErrs += errs;
        m_berFrames++;
    }
    else if (duid == P25_DUID_LDU2) {
        timerStart();

        bool ret = lc.decodeLDU2(buffer + 1U);
        if (!ret) {
            LogWarning(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2), undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2) LC, mfId = $%02X, algo = %X, kid = %X",
                lc.getMFId(), lc.getAlgId(), lc.getKId());
        }

        for (uint32_t i = 0U; i < 216U; i++)
            errs += countErrs(buffer[i + 1U], LDU2_1K[i]);

        float ber = float(errs) / 12.33F;
        if (ber < 10.0F)
            LogMessage(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2), 1011 Test Pattern BER (errs): %.3f%% (%u/1233)", ber, errs);
        else {
            LogWarning(LOG_CAL, "P25_DUID_LDU2 (Logical Data Unit 2), uncorrectable audio");
            m_berUncorrectable++;
        }

        m_berBits += 1233U;
        m_berErrs += errs;
        m_berFrames++;
    }
}

/// <summary>
/// Retrieve the modem DSP version.
/// </summary>
/// <returns>True, if firmware version was recieved, otherwise false.</returns>
bool HostCal::getFirmwareVersion()
{
    uint8_t buffer[150U];

    int ret = 0;
    for (uint32_t i = 0U; i < 5U && ret <= 0; i++) {
        buffer[0U] = DVM_FRAME_START;
        buffer[1U] = 3U;
        buffer[2U] = CMD_GET_VERSION;

        ret = m_serial.write(buffer, 3U);
        if (ret <= 0)
            return false;

        sleep(100U);

        ret = readModem(buffer, 50U);
        if (ret < 0)
            return false;
        if (ret == 0)
            sleep(1000U);
    }

    if (ret <= 0) {
        LogError(LOG_CAL, "Unable to read the firmware version after 6 attempts");
        return false;
    }

    if (buffer[2U] != CMD_GET_VERSION) {
        Utils::dump("Invalid response", buffer, ret);
        return false;
    }

    LogMessage(LOG_CAL, MODEM_VERSION_STR, buffer[1U] - 4, buffer + 4U, buffer[3U]);
    return true;
}

/// <summary>
/// Write configuration to the modem DSP.
/// </summary>
/// <returns>True, if configuration is written, otherwise false.</returns>
bool HostCal::writeConfig()
{
    return writeConfig(m_mode);
}

/// <summary>
/// Write configuration to the modem DSP.
/// </summary>
/// <param name="modeOverride"></param>
/// <returns>True, if configuration is written, otherwise false.</returns>
bool HostCal::writeConfig(uint8_t modeOverride)
{
    uint8_t buffer[50U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 16U;
    buffer[2U] = CMD_SET_CONFIG;

    buffer[3U] = 0x00U;
    m_conf["system"]["modem"]["rxInvert"] = __BOOL_STR(m_rxInvert);
    if (m_rxInvert)
        buffer[3U] |= 0x01U;
    m_conf["system"]["modem"]["txInvert"] = __BOOL_STR(m_txInvert);
    if (m_txInvert)
        buffer[3U] |= 0x02U;
    m_conf["system"]["modem"]["pttInvert"] = __BOOL_STR(m_pttInvert);
    if (m_pttInvert)
        buffer[3U] |= 0x04U;
    if (m_debug)
        buffer[3U] |= 0x10U;
    if (!m_duplex)
        buffer[3U] |= 0x80U;

    buffer[4U] = 0x00U;
    m_conf["system"]["modem"]["dcBlocker"] = __BOOL_STR(m_dcBlocker);
    if (m_dcBlocker)
        buffer[4U] |= 0x01U;

    if (m_dmrEnabled)
        buffer[4U] |= 0x02U;
    if (m_p25Enabled)
        buffer[4U] |= 0x08U;

    buffer[5U] = m_txDelay;

    buffer[6U] = modeOverride;

    m_conf["system"]["modem"]["rxLevel"] = __FLOAT_STR(m_rxLevel);
    buffer[7U] = (uint8_t)(m_rxLevel * 2.55F + 0.5F);

    m_conf["system"]["modem"]["txLevel"] = __FLOAT_STR(m_txLevel);
    buffer[8U] = (uint8_t)(m_txLevel * 2.55F + 0.5F);

    buffer[9U] = 1U;

    buffer[10U] = m_dmrDelay;
    buffer[11U] = 128U;

    buffer[13U] = (uint8_t)(m_txLevel * 2.55F + 0.5F);
    buffer[15U] = (uint8_t)(m_txLevel * 2.55F + 0.5F);

    m_conf["system"]["modem"]["txDCOffset"] = __INT_STR(m_txDCOffset);
    buffer[16U] = (uint8_t)(m_txDCOffset + 128);
    m_conf["system"]["modem"]["rxDCOffset"] = __INT_STR(m_rxDCOffset);
    buffer[17U] = (uint8_t)(m_rxDCOffset + 128);

    int ret = m_serial.write(buffer, 16U);
    if (ret <= 0)
        return false;

    sleep(10U);

    ret = readModem(buffer, 50U);
    if (ret <= 0)
        return false;

    if (buffer[2U] == CMD_NAK) {
        LogError(LOG_CAL, "Got a NAK from the modem");
        return false;
    }

    if (buffer[2U] != CMD_ACK) {
        Utils::dump("Invalid response", buffer, ret);
        return false;
    }

    return true;
}

/// <summary>
///
/// </summary>
/// <param name="ms"></param>
void HostCal::sleep(uint32_t ms)
{
#if defined(_WIN32) || defined(_WIN64)
    ::Sleep(ms);
#else
    ::usleep(ms * 1000);
#endif
}

/// <summary>
///
/// </summary>
void HostCal::timerClock()
{
    if (m_timer > 0U && m_timeout > 0U) {
        m_timer += 1U;

        if (m_timer >= m_timeout) {
            LogMessage(LOG_CAL, "Transmission lost, total frames: %d, bits: %d, uncorrectable frames: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berUndecodableLC, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));

            m_berBits = 0U;
            m_berErrs = 0U;
            m_berFrames = 0U;
            m_berUndecodableLC = 0U;
            m_berUncorrectable = 0U;

            timerStop();
        }
    }
}

/// <summary>
///
/// </summary>
void HostCal::timerStart()
{
    if (m_timeout > 0U)
        m_timer = 1U;
}

/// <summary>
///
/// </summary>
void HostCal::timerStop()
{
    m_timer = 0U;
}

/// <summary>
/// Prints the current status of the calibration.
/// </summary>
void HostCal::printStatus()
{
    LogMessage(LOG_CAL, " - PTT Invert: %s, RX Invert: %s, TX Invert: %s, DC Blocker: %s, RX Level: %.1f%%, TX Level: %.1f%%, TX DC Offset: %d, RX DC Offset: %d",
        m_pttInvert ? "yes" : "no", m_rxInvert ? "yes" : "no", m_txInvert ? "yes" : "no", m_dcBlocker ? "yes" : "no",
        m_rxLevel, m_txLevel, m_txDCOffset, m_rxDCOffset);
    LogMessage(LOG_CAL, " - TX Delay: %u (%ums), DMR Delay: %u (%.1fms)", m_txDelay, (m_txDelay * 10), m_dmrDelay, float(m_dmrDelay) * 0.0416666F);
    LogMessage(LOG_CAL, " - Operating Mode: %s", m_modeStr.c_str());

    uint8_t buffer[50U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_GET_STATUS;

    int ret = m_serial.write(buffer, 4U);
    if (ret <= 0)
        return;

    sleep(25U);

    ret = readModem(buffer, 50U);
    if (ret <= 0)
        return;

    if (buffer[2U] == CMD_NAK) {
        LogError(LOG_CAL, "Got a NAK from the modem");
        return;
    }

    if (buffer[2U] != CMD_GET_STATUS) {
        Utils::dump("Invalid response", buffer, ret);
        return;
    }

    uint8_t modemState = buffer[4U];
    bool tx = (buffer[5U] & 0x01U) == 0x01U;

    bool adcOverflow = (buffer[5U] & 0x02U) == 0x02U;
    bool rxOverflow = (buffer[5U] & 0x04U) == 0x04U;
    bool txOverflow = (buffer[5U] & 0x08U) == 0x08U;
    bool dacOverflow = (buffer[5U] & 0x20U) == 0x20U;

    LogMessage(LOG_CAL, " - Diagnostic Values [Modem State: %u, Transmitting: %d, ADC Overflow: %d, Rx Overflow: %d, Tx Overflow: %d, DAC Overflow: %d]",
        modemState, tx, adcOverflow, rxOverflow, txOverflow, dacOverflow);
}

/// <summary>
///
/// </summary>
/// <param name="buffer"></param>
/// <param name="length"></param>
void HostCal::printDebug(const uint8_t* buffer, uint32_t length)
{
    if (buffer[2U] == CMD_DEBUG1) {
        LogMessage(LOG_MODEM, "M: %.*s", length - 3U, buffer + 3U);
    }
    else if (buffer[2U] == CMD_DEBUG2) {
        short val1 = (buffer[length - 2U] << 8) | buffer[length - 1U];
        LogMessage(LOG_MODEM, "M: %.*s %d", length - 5U, buffer + 3U, val1);
    }
    else if (buffer[2U] == CMD_DEBUG3) {
        short val1 = (buffer[length - 4U] << 8) | buffer[length - 3U];
        short val2 = (buffer[length - 2U] << 8) | buffer[length - 1U];
        LogMessage(LOG_MODEM, "M: %.*s %d %d", length - 7U, buffer + 3U, val1, val2);
    }
    else if (buffer[2U] == CMD_DEBUG4) {
        short val1 = (buffer[length - 6U] << 8) | buffer[length - 5U];
        short val2 = (buffer[length - 4U] << 8) | buffer[length - 3U];
        short val3 = (buffer[length - 2U] << 8) | buffer[length - 1U];
        LogMessage(LOG_MODEM, "M: %.*s %d %d %d", length - 9U, buffer + 3U, val1, val2, val3);
    }
    else if (buffer[2U] == CMD_DEBUG5) {
        short val1 = (buffer[length - 8U] << 8) | buffer[length - 7U];
        short val2 = (buffer[length - 6U] << 8) | buffer[length - 5U];
        short val3 = (buffer[length - 4U] << 8) | buffer[length - 3U];
        short val4 = (buffer[length - 2U] << 8) | buffer[length - 1U];
        LogMessage(LOG_MODEM, "M: %.*s %d %d %d %d", length - 11U, buffer + 3U, val1, val2, val3, val4);
    }
}

/// <summary>
///
/// </summary>
/// <param name="a"></param>
/// <param name="b"></param>
/// <returns></returns>
unsigned char HostCal::countErrs(unsigned char a, unsigned char b)
{
    int cnt = 0;
    unsigned char tmp = a ^ b;
    while (tmp) {
        if (tmp % 2 == 1)
            cnt++;
        tmp /= 2;
    }
    return cnt;
}
