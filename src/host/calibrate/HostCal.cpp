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
#include "calibrate/HostCal.h"
#include "modem/Modem.h"
#include "HostMain.h"
#include "common/Log.h"

using namespace modem;
using namespace lookups;

#if !defined(CATCH2_TEST_COMPILATION)

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the HostCal class.
/// </summary>
/// <param name="confFile">Full-path to the configuration file.</param>
HostCal::HostCal(const std::string& confFile) : HostSetup(confFile),
    m_console()
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
/// <param name="argc"></param>
/// <param name="argv"></param>
/// <returns>Zero if successful, otherwise error occurred.</returns>
int HostCal::run(int argc, char **argv)
{
    bool ret = false;
    try {
        ret = yaml::Parse(m_conf, m_confFile.c_str());
        if (!ret) {
            ::fatal("cannot read the configuration file, %s\n", m_confFile.c_str());
        }
    }
    catch (yaml::OperationException const& e) {
        ::fatal("cannot read the configuration file - %s (%s)", m_confFile.c_str(), e.message());
    }

    // initialize system logging
    ret = ::LogInitialise("", "", 0U, 1U, true);
    if (!ret) {
        ::fprintf(stderr, "unable to open the log file\n");
        return 1;
    }

    ::LogInfo(__BANNER__ "\r\n" __PROG_NAME__ " " __VER__ " (built " __BUILD__ ")\r\n" \
        "Copyright (c) 2017-2024 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\r\n" \
        "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\r\n" \
        ">> Modem Calibration\r\n");

    yaml::Node systemConf = m_conf["system"];
    m_startupDuplex = m_duplex = systemConf["duplex"].as<bool>(true);

    // try to load bandplan identity table
    std::string idenLookupFile = systemConf["iden_table"]["file"].as<std::string>();
    uint32_t idenReloadTime = systemConf["iden_table"]["time"].as<uint32_t>(0U);

    if (idenLookupFile.length() <= 0U) {
        ::LogError(LOG_HOST, "No bandplan identity table? This must be defined!");
        return 1;
    }

    LogInfo("Iden Table Lookups");
    LogInfo("    File: %s", idenLookupFile.length() > 0U ? idenLookupFile.c_str() : "None");
    if (idenReloadTime > 0U)
        LogInfo("    Reload: %u mins", idenReloadTime);

    m_idenTable = new IdenTableLookup(idenLookupFile, idenReloadTime);
    m_idenTable->read();

    LogInfo("General Parameters");

    std::string identity = systemConf["identity"].as<std::string>();
    ::LogInfo("    Identity: %s", identity.c_str());

    yaml::Node rfssConfig = systemConf["config"];
    m_channelId = (uint8_t)rfssConfig["channelId"].as<uint32_t>(0U);
    if (m_channelId > 15U) { // clamp to 15
        m_channelId = 15U;
    }

    // if modem debug is being forced from the commandline -- enable modem debug
    if (g_modemDebug)
        m_debug = true;

    if (!calculateRxTxFreq()) {
        return false;
    }

    // initialize modem
    if (!createModem()) {
        return false;
    }

    // open modem and initialize
    ret = m_modem->open();
    if (!ret) {
        ::LogError(LOG_CAL, "Failed to open modem");
        return 1;
    }

    m_isConnected = true;

    // open terminal console
    ret = m_console.open();
    if (!ret) {
        return 1;
    }

    readFlash();

    writeFifoLength();
    writeConfig();
    writeRFParams();

    getStatus();
    uint8_t timeout = 0U;
    while (!m_hasFetchedStatus) {
        m_modem->clock(0U);

        timeout++;
        if (timeout >= 75U) {
            break;
        }

        sleep(5U);
    }

    if (!m_hasFetchedStatus) {
        ::LogError(LOG_CAL, "Failed to get status from modem");

        m_isConnected = false;
        m_modem->close();
        m_console.close();
        return 2;
    }

    m_modem->m_statusTimer.start();

    displayHelp();
    printStatus();

    StopWatch stopWatch;
    stopWatch.start();

    bool end = false;
    while (!end) {
        int c = m_console.getChar();
        switch (c) {
        /** Level Adjustment Commands */
        case 'I':
        {
            if (!m_isHotspot) {
                m_modem->m_txInvert = !m_modem->m_txInvert;
                LogMessage(LOG_CAL, " - TX Invert: %s", m_modem->m_txInvert ? "On" : "Off");
                writeConfig();
            }
        }
        break;
        case 'i':
        {
            if (!m_isHotspot) {
                m_modem->m_rxInvert = !m_modem->m_rxInvert;
                LogMessage(LOG_CAL, " - RX Invert: %s", m_modem->m_rxInvert ? "On" : "Off");
                writeConfig();
            }
        }
        break;
        case 'p':
        {
            if (!m_isHotspot) {
                m_modem->m_pttInvert = !m_modem->m_pttInvert;
                LogMessage(LOG_CAL, " - PTT Invert: %s", m_modem->m_pttInvert ? "On" : "Off");
                writeConfig();
            }
        }
        break;
        case 'd':
        {
            if (!m_isHotspot) {
                m_modem->m_dcBlocker = !m_modem->m_dcBlocker;
                LogMessage(LOG_CAL, " - DC Blocker: %s", m_modem->m_dcBlocker ? "On" : "Off");
                writeConfig();
            }
        }
        break;
        case 'D':
        {
            m_debug = !m_debug;
            LogMessage(LOG_CAL, " - Modem Debug: %s", m_debug ? "On" : "Off");
            writeConfig();
        }
        break;
        case 'R':
            if (!m_isHotspot) {
                setRXLevel(1);
            }
            break;
        case 'r':
            if (!m_isHotspot) {
                setRXLevel(-1);
            }
            break;
        case 'T':
            setTXLevel(1);
            break;
        case 't':
            setTXLevel(-1);
            break;
        case 'c':
            if (!m_isHotspot) {
                setRXDCOffset(-1);
            }
            break;
        case 'C':
            if (!m_isHotspot) {
                setRXDCOffset(1);
            }
            break;
        case 'o':
            if (!m_isHotspot) {
                setTXDCOffset(-1);
            }
            break;
        case 'O':
            if (!m_isHotspot) {
                setTXDCOffset(1);
            }
            break;

        case 'F':
        {
            char value[10] = { '\0' };
            ::fprintf(stdout, "> Rx Frequency Offset [%dHz] (Hz) ? ", m_modem->m_rxTuning);
            ::fflush(stdout);

            m_console.getLine(value, 10, 0);
            if (value[0] != '\0') {
                int rxTuning = m_modem->m_rxTuning;
                sscanf(value, "%d", &rxTuning);

                m_modem->m_rxTuning = rxTuning;
                m_conf["system"]["modem"]["hotspot"]["rxTuning"] = __INT_STR(m_modem->m_rxTuning);
                m_rxAdjustedFreq = m_rxFrequency + m_modem->m_rxTuning;

                writeRFParams();
                Thread::sleep(2);
                writeConfig();
            }
        }
        break;

        case 'f':
        {
            char value[10] = { '\0' };
            ::fprintf(stdout, "> Tx Frequency Offset [%dHz] (Hz) ? ", m_modem->m_txTuning);
            ::fflush(stdout);

            m_console.getLine(value, 10, 0);
            if (value[0] != '\0') {
                int txTuning = m_modem->m_txTuning;
                sscanf(value, "%d", &txTuning);

                m_modem->m_txTuning = txTuning;
                m_conf["system"]["modem"]["hotspot"]["txTuning"] = __INT_STR(m_modem->m_txTuning);
                m_txAdjustedFreq = m_txFrequency + m_modem->m_txTuning;

                writeRFParams();
                Thread::sleep(2);
                writeConfig();
            }
        }
        break;

        /** Engineering Commands */
        case 'E':
            eraseFlash();
            break;
        case 'e':
            readFlash();
            break;

        case '1':
        {
            if (!m_isHotspot) {
                if (m_modem->getVersion() >= 3U) {
                    setRXCoarseLevel(1);
                }
                else {
                    LogWarning(LOG_CAL, "Rx Coarse Level adjustment is not supported on your firmware!");
                }
            }
        }
        break;

        case '2':
        {
            if (!m_isHotspot) {
                if (m_modem->getVersion() >= 3U) {
                    setRXCoarseLevel(-1);
                }
                else {
                    LogWarning(LOG_CAL, "Rx Coarse Level adjustment is not supported on your firmware!");
                }
            }
        }
        break;

        case '3':
        {
            if (!m_isHotspot) {
                if (m_modem->getVersion() >= 3U) {
                    setRXFineLevel(1);
                }
                else {
                    LogWarning(LOG_CAL, "Rx Fine Level adjustment is not supported on your firmware!");
                }
            }
        }
        break;

        case '4':
        {
            if (!m_isHotspot) {
                if (m_modem->getVersion() >= 3U) {
                    setRXFineLevel(-1);
                }
                else {
                    LogWarning(LOG_CAL, "Rx Fine Level adjustment is not supported on your firmware!");
                }
            }
        }
        break;

        case '5':
        {
            if (!m_isHotspot) {
                if (m_modem->getVersion() >= 3U) {
                    setTXCoarseLevel(1);
                }
                else {
                    LogWarning(LOG_CAL, "Tx Coarse Level adjustment is not supported on your firmware!");
                }
            }
        }
        break;

        case '6':
        {
            if (!m_isHotspot) {
                if (m_modem->getVersion() >= 3U) {
                    setTXCoarseLevel(-1);
                }
                else {
                    LogWarning(LOG_CAL, "Tx Coarse Level adjustment is not supported on your firmware!");
                }
            }
        }
        break;

        case '9':
        {
            if (!m_isHotspot) {
                if (m_modem->getVersion() >= 3U) {
                    setRSSICoarseLevel(1);
                }
                else {
                    LogWarning(LOG_CAL, "RSSI Coarse Level adjustment is not supported on your firmware!");
                }
            }
        }
        break;

        case '0':
        {
            if (!m_isHotspot) {
                if (m_modem->getVersion() >= 3U) {
                    setRSSICoarseLevel(-1);
                }
                else {
                    LogWarning(LOG_CAL, "RSSI Coarse Level adjustment is not supported on your firmware!");
                }
            }
        }
        break;

        case ')':
        {
            m_duplex = !m_duplex;
            writeConfig();
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
            m_p25TduTest = false;
            m_nxdnEnabled = false;

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
            m_p25TduTest = false;
            m_nxdnEnabled = false;

            LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
            writeConfig();
        }
        break;
        case 'L':
        {
            m_mode = STATE_DMR_LF_CAL;
            m_modeStr = DMR_LF_CAL_STR;
            m_duplex = true;
            m_dmrEnabled = false;
            m_dmrRx1K = false;
            m_p25Enabled = false;
            m_p25Rx1K = false;
            m_p25TduTest = false;
            m_nxdnEnabled = false;

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
            m_p25TduTest = false;
            m_nxdnEnabled = false;

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
            m_p25TduTest = false;
            m_nxdnEnabled = false;

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
            m_p25TduTest = false;
            m_nxdnEnabled = false;

            LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
            writeConfig();
        }
        break;
        case 'N':
        {
            // are we on a protocol version 3 firmware?
            if (m_modem->getVersion() >= 3U) {
                m_mode = STATE_NXDN_CAL;
                m_modeStr = NXDN_CAL_1K_STR;
                m_duplex = true;
                m_dmrEnabled = false;
                m_dmrRx1K = false;
                m_p25Enabled = false;
                m_p25Rx1K = false;
                m_p25TduTest = false;
                m_nxdnEnabled = false;

                LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                writeConfig();
            }
            else {
                LogWarning(LOG_CAL, "%s test mode is not supported on your firmware!", NXDN_CAL_1K_STR);
            }
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

            m_duplex = m_startupDuplex;
            m_dmrEnabled = true;
            m_p25Enabled = false;
            m_nxdnEnabled = false;

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

            m_duplex = m_startupDuplex;
            m_dmrEnabled = false;
            m_p25Enabled = true;
            m_p25TduTest = false;
            m_nxdnEnabled = false;

            LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
            writeConfig();
        }
        break;
        case 'n':
        {
            // are we on a protocol version 3 firmware?
            if (m_modem->getVersion() >= 3U) {
                m_mode = STATE_NXDN;
                m_modeStr = NXDN_FEC_STR;

                m_duplex = m_startupDuplex;
                m_dmrEnabled = false;
                m_p25Enabled = false;
                m_p25Rx1K = false;
                m_p25TduTest = false;
                m_nxdnEnabled = true;

                LogMessage(LOG_CAL, " - %s", m_modeStr.c_str());
                writeConfig();
            }
            else {
                LogWarning(LOG_CAL, "%s test mode is not supported on your firmware!", NXDN_FEC_STR);
            }
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
            m_p25TduTest = false;

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
            ::LogInfo(__PROG_NAME__ " %s (built %s)", __VER__, __BUILD__);
            ::LogInfo("Copyright (c) 2017-2024 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.");
            ::LogInfo("Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others");
            break;
        case 'v':
            // bryanb: this actually polls the firmware -- should we do this or just
            // call m_modem->getVersion()?
            m_modem->getFirmwareVersion();
            break;
        case 'H':
        case 'h':
            displayHelp();
            break;
        case 'S':
        case 's':
            saveConfig();
            break;
        case 'U':
            m_updateConfigFromModem = true;
            readFlash();
            break;
        case 'Q':
        case 'q':
            m_mode = STATE_IDLE;
            writeConfig();
            end = true;
            break;

        case 13:
        case 10:
        case -1:
            break;
        default:
            LogError(LOG_CAL, "Unknown command - %c (H/h for help)", c);
            break;
        }

        // ------------------------------------------------------
        //  -- Modem Clocking                                 --
        // ------------------------------------------------------

        uint32_t ms = stopWatch.elapsed();
        stopWatch.start();

        m_modem->clock(ms);

        timerClock();

        if (ms < 2U)
            Thread::sleep(1U);
    }

    if (m_transmit)
        setTransmit();

    m_isConnected = false;
    m_modem->close();
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
    if (!m_modem->m_flashDisabled) {
        LogMessage(LOG_CAL, "    U        Read modem configuration area and reset local configuration");
    }
    LogMessage(LOG_CAL, "    )        Swap Duplex Flag (depending on mode this will enable/disable duplex)");
    LogMessage(LOG_CAL, "    Q/q      Quit");
    LogMessage(LOG_CAL, "Level Adjustment Commands:");
    if (!m_isHotspot) {
        LogMessage(LOG_CAL, "    I        Toggle transmit inversion");
        LogMessage(LOG_CAL, "    i        Toggle receive inversion");
        LogMessage(LOG_CAL, "    p        Toggle PTT inversion");
        LogMessage(LOG_CAL, "    d        Toggle DC blocker");
    }
    if (!m_isHotspot) {
        LogMessage(LOG_CAL, "    R/r      Increase/Decrease receive level");
        LogMessage(LOG_CAL, "    T/t      Increase/Decrease transmit level");
    } else {
        LogMessage(LOG_CAL, "    T/t      Increase/Decrease deviation level");
    }
    if (!m_isHotspot) {
        LogMessage(LOG_CAL, "    C/c      Increase/Decrease RX DC offset level");
        LogMessage(LOG_CAL, "    O/o      Increase/Decrease TX DC offset level");
    }
    LogMessage(LOG_CAL, "    X        Set FDMA Preambles");
    LogMessage(LOG_CAL, "    W        Set DMR Rx Delay");
    if (!m_isHotspot) {
        LogMessage(LOG_CAL, "    w        Set P25 Correlation Count");
    }
    if (m_isHotspot) {
        LogMessage(LOG_CAL, "    F        Set Rx Frequency Adjustment");
        LogMessage(LOG_CAL, "    f        Set Tx Frequency Adjustment");
    }
    if (!m_isHotspot) {
        LogMessage(LOG_CAL, "    1/2      Increase/Decrease receive coarse level");
        LogMessage(LOG_CAL, "    3/4      Increase/Decrease receive fine level");
        LogMessage(LOG_CAL, "    5/6      Increase/Decrease transmit coarse level");
        LogMessage(LOG_CAL, "    9/0      Increase/Decrease RSSI coarse level");
    }
    LogMessage(LOG_CAL, "Mode Commands:");
    LogMessage(LOG_CAL, "    Z        %s", DMR_CAL_STR);
    LogMessage(LOG_CAL, "    z        %s", P25_CAL_STR);
    LogMessage(LOG_CAL, "    L        %s", DMR_LF_CAL_STR);
    LogMessage(LOG_CAL, "    M        %s", DMR_CAL_1K_STR);
    LogMessage(LOG_CAL, "    m        %s", DMR_DMO_CAL_1K_STR);
    LogMessage(LOG_CAL, "    P        %s", P25_CAL_1K_STR);
    LogMessage(LOG_CAL, "    N        %s", NXDN_CAL_1K_STR);
    LogMessage(LOG_CAL, "    B        %s", DMR_FEC_STR);
    LogMessage(LOG_CAL, "    J        %s", DMR_FEC_1K_STR);
    LogMessage(LOG_CAL, "    b        %s", P25_FEC_STR);
    LogMessage(LOG_CAL, "    j        %s", P25_FEC_1K_STR);
    LogMessage(LOG_CAL, "    n        %s", NXDN_FEC_STR);
    LogMessage(LOG_CAL, "    x        %s", RSSI_CAL_STR);
}

/// <summary>
/// Helper to change the Rx level.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setTXLevel(int incr)
{
    if (incr > 0 && m_modem->m_cwIdTXLevel < 100.0F) {
        m_modem->m_cwIdTXLevel += 0.25F;

        // clamp values
        if (m_modem->m_cwIdTXLevel > 100.0F)
            m_modem->m_cwIdTXLevel = 100.0F;

        LogMessage(LOG_CAL, " - TX Level: %.1f%%", m_modem->m_cwIdTXLevel);
        return writeConfig();
    }

    if (incr < 0 && m_modem->m_cwIdTXLevel > 0.0F) {
        m_modem->m_cwIdTXLevel -= 0.25F;

        // clamp values
        if (m_modem->m_cwIdTXLevel < 0.0F)
            m_modem->m_cwIdTXLevel = 0.0F;

        LogMessage(LOG_CAL, " - TX Level: %.1f%%", m_modem->m_cwIdTXLevel);
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
    if (incr > 0 && m_modem->m_rxLevel < 100.0F) {
        m_modem->m_rxLevel += 0.25F;

        // clamp values
        if (m_modem->m_rxLevel > 100.0F)
            m_modem->m_rxLevel = 100.0F;

        LogMessage(LOG_CAL, " - RX Level: %.1f%%", m_modem->m_rxLevel);
        return writeConfig();
    }

    if (incr < 0 && m_modem->m_rxLevel > 0.0F) {
        m_modem->m_rxLevel -= 0.25F;

        // clamp values
        if (m_modem->m_rxLevel < 0.0F)
            m_modem->m_rxLevel = 0.0F;

        LogMessage(LOG_CAL, " - RX Level: %.1f%%", m_modem->m_rxLevel);
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
    if (incr > 0 && m_modem->m_txDCOffset < 127) {
        m_modem->m_txDCOffset++;
        LogMessage(LOG_CAL, " - TX DC Offset: %d", m_modem->m_txDCOffset);
        return writeConfig();
    }

    if (incr < 0 && m_modem->m_txDCOffset > -127) {
        m_modem->m_txDCOffset--;
        LogMessage(LOG_CAL, " - TX DC Offset: %d", m_modem->m_txDCOffset);
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
    if (incr > 0 && m_modem->m_rxDCOffset < 127) {
        m_modem->m_rxDCOffset++;
        LogMessage(LOG_CAL, " - RX DC Offset: %d", m_modem->m_rxDCOffset);
        return writeConfig();
    }

    if (incr < 0 && m_modem->m_rxDCOffset > -127) {
        m_modem->m_rxDCOffset--;
        LogMessage(LOG_CAL, " - RX DC Offset: %d", m_modem->m_rxDCOffset);
        return writeConfig();
    }

    return true;
}

/// <summary>
/// Helper to change the Rx coarse level.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setRXCoarseLevel(int incr)
{
    if (incr > 0 && m_modem->m_rxCoarsePot < 255U) {
        if (m_modem->m_rxCoarsePot != 255U) {
            m_modem->m_rxCoarsePot += 1U;
        }

        LogMessage(LOG_CAL, " - RX Coarse Level: %u", m_modem->m_rxCoarsePot);
        return writeConfig();
    }

    if (incr < 0 && m_modem->m_rxCoarsePot > 0U) {
        if (m_modem->m_rxCoarsePot != 0) {
            m_modem->m_rxCoarsePot -= 1U;
        }

        LogMessage(LOG_CAL, " - RX Coarse Level: %u", m_modem->m_rxCoarsePot);
        return writeConfig();
    }

    return true;
}

/// <summary>
/// Helper to change the Rx fine level.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setRXFineLevel(int incr)
{
    if (incr > 0 && m_modem->m_rxFinePot < 255U) {
        if (m_modem->m_rxFinePot != 255U) {
            m_modem->m_rxFinePot += 1U;
        }

        LogMessage(LOG_CAL, " - RX Fine Level: %u", m_modem->m_rxFinePot);
        return writeConfig();
    }

    if (incr < 0 && m_modem->m_rxFinePot > 0U) {
        if (m_modem->m_rxFinePot != 0) {
            m_modem->m_rxFinePot -= 1U;
        }

        LogMessage(LOG_CAL, " - RX Fine Level: %u", m_modem->m_rxFinePot);
        return writeConfig();
    }

    return true;
}

/// <summary>
/// Helper to change the Tx coarse level.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setTXCoarseLevel(int incr)
{
    if (incr > 0 && m_modem->m_txCoarsePot < 255U) {
        if (m_modem->m_txCoarsePot != 255U) {
            m_modem->m_txCoarsePot += 1U;
        }

        LogMessage(LOG_CAL, " - TX Coarse Level: %u", m_modem->m_txCoarsePot);
        return writeConfig();
    }

    if (incr < 0 && m_modem->m_txCoarsePot > 0U) {
        if (m_modem->m_txCoarsePot != 0) {
            m_modem->m_txCoarsePot -= 1U;
        }

        LogMessage(LOG_CAL, " - TX Coarse Level: %u", m_modem->m_txCoarsePot);
        return writeConfig();
    }

    return true;
}

/// <summary>
/// Helper to change the RSSI coarse level.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setRSSICoarseLevel(int incr)
{
    if (incr > 0 && m_modem->m_rssiCoarsePot < 255U) {
        if (m_modem->m_rssiCoarsePot != 255U) {
            m_modem->m_rssiCoarsePot += 1U;
        }

        LogMessage(LOG_CAL, " - RSSI Coarse Level: %u", m_modem->m_rssiCoarsePot);
        return writeConfig();
    }

    if (incr < 0 && m_modem->m_rssiCoarsePot > 0U) {
        if (m_modem->m_rssiCoarsePot != 0) {
            m_modem->m_rssiCoarsePot -= 1U;
        }

        LogMessage(LOG_CAL, " - RSSI Coarse Level: %u", m_modem->m_rssiCoarsePot);
        return writeConfig();
    }

    return true;
}

/// <summary>
/// Prints the current status of the calibration.
/// </summary>
/// <param name="status">Flag indicating that the current modem status should be fetched.</param>
void HostCal::printStatus()
{
    yaml::Node systemConf = m_conf["system"];
    {
        yaml::Node modemConfig = m_conf["system"]["modem"];
        std::string type = modemConfig["protocol"]["type"].as<std::string>();

        yaml::Node uartConfig = modemConfig["protocol"]["uart"];
        std::string modemPort = uartConfig["port"].as<std::string>();
        uint32_t portSpeed = uartConfig["speed"].as<uint32_t>(115200U);

        LogMessage(LOG_CAL, " - Operating Mode: %s, Port Type: %s, Modem Port: %s, Port Speed: %u, Proto Ver: %u", m_modeStr.c_str(), type.c_str(), modemPort.c_str(), portSpeed, m_modem->getVersion());
    }

    {
        if (!m_isHotspot) {
            LogMessage(LOG_CAL, " - PTT Invert: %s, RX Invert: %s, TX Invert: %s, DC Blocker: %s",
                m_modem->m_pttInvert ? "yes" : "no", m_modem->m_rxInvert ? "yes" : "no", m_modem->m_txInvert ? "yes" : "no", m_modem->m_dcBlocker ? "yes" : "no");
        }
        LogMessage(LOG_CAL, " - RX Level: %.1f%%, TX Level: %.1f%%, TX DC Offset: %d, RX DC Offset: %d",
            m_modem->m_rxLevel, m_modem->m_cwIdTXLevel, m_modem->m_txDCOffset, m_modem->m_rxDCOffset);
        if (!m_isHotspot) {
            LogMessage(LOG_CAL, " - RX Coarse Level: %u, RX Fine Level: %u, TX Coarse Level: %u, RSSI Coarse Level: %u",
                m_modem->m_rxCoarsePot, m_modem->m_rxFinePot, m_modem->m_txCoarsePot, m_modem->m_rssiCoarsePot);
            LogMessage(LOG_CAL, " - DMR Symbol +/- 3 Level Adj.: %d, DMR Symbol +/- 1 Level Adj.: %d, P25 Symbol +/- 3 Level Adj.: %d, P25 Symbol +/- 1 Level Adj.: %d",
                m_modem->m_dmrSymLevel3Adj, m_modem->m_dmrSymLevel1Adj, m_modem->m_p25SymLevel3Adj, m_modem->m_p25SymLevel1Adj);

            // are we on a protocol version 3 firmware?
            if (m_modem->getVersion() >= 3U) {
                LogMessage(LOG_CAL, " - NXDN Symbol +/- 3 Level Adj.: %d, NXDN Symbol +/- 1 Level Adj.: %d",
                    m_modem->m_nxdnSymLevel3Adj, m_modem->m_nxdnSymLevel1Adj);
            }
        }
        if (m_isHotspot) {
            LogMessage(LOG_CAL, " - DMR Disc. BW: %d, P25 Disc. BW: %d, DMR Post Demod BW: %d, P25 Post Demod BW: %d",
                m_modem->m_dmrDiscBWAdj, m_modem->m_p25DiscBWAdj, m_modem->m_dmrPostBWAdj, m_modem->m_p25PostBWAdj);

            // are we on a protocol version 3 firmware?
            if (m_modem->getVersion() >= 3U) {
                LogMessage(LOG_CAL, " - NXDN Disc. BW: %d, NXDN Post Demod BW: %d",
                    m_modem->m_nxdnDiscBWAdj, m_modem->m_nxdnPostBWAdj);

                LogMessage(LOG_CAL, " - AFC Enabled: %u, AFC KI: %u, AFC KP: %u, AFC Range: %u",
                    m_modem->m_afcEnable, m_modem->m_afcKI, m_modem->m_afcKP, m_modem->m_afcRange);
            }

            switch (m_modem->m_adfGainMode) {
                case ADF_GAIN_AUTO_LIN:
                    LogMessage(LOG_CAL, " - ADF7021 Gain Mode: Auto High Linearity");
                    break;
                case ADF_GAIN_LOW:
                    LogMessage(LOG_CAL, " - ADF7021 Gain Mode: Low");
                    break;
                case ADF_GAIN_HIGH:
                    LogMessage(LOG_CAL, " - ADF7021 Gain Mode: High");
                    break;
                case ADF_GAIN_AUTO:
                default:
                    LogMessage(LOG_CAL, " - ADF7021 Gain Mode: Auto");
                    break;
            }
        }
        LogMessage(LOG_CAL, " - FDMA Preambles: %u (%.1fms), DMR Rx Delay: %u (%.1fms), P25 Corr. Count: %u (%.1fms)", m_modem->m_fdmaPreamble, float(m_modem->m_fdmaPreamble) * 0.2222F, m_modem->m_dmrRxDelay, float(m_modem->m_dmrRxDelay) * 0.0416666F,
            m_modem->m_p25CorrCount, float(m_modem->m_p25CorrCount) * 0.667F);
        LogMessage(LOG_CAL, " - Rx Freq: %uHz, Tx Freq: %uHz, Rx Offset: %dHz, Tx Offset: %dHz", m_modem->m_rxFrequency, m_modem->m_txFrequency, m_modem->m_rxTuning, m_modem->m_txTuning);
        LogMessage(LOG_CAL, " - Rx Effective Freq: %uHz, Tx Effective Freq: %uHz", m_rxAdjustedFreq, m_txAdjustedFreq);
    }

    getStatus();
}

#endif // !defined(CATCH2_TEST_COMPILATION)
