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
#include "host/setup/HostSetup.h"
#include "HostMain.h"
#include "Log.h"
#include "Utils.h"

using namespace lookups;

#include <cstdio>
#include <algorithm>

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the HostSetup class.
/// </summary>
/// <param name="confFile">Full-path to the configuration file.</param>
HostSetup::HostSetup(const std::string& confFile) :
    m_confFile(confFile),
    m_conf(),
    m_console(),
    m_duplex(true),
    m_identity("ABCD123"),
    m_callsign("ABCD123"),
    m_rxFrequency(0U),
    m_txFrequency(0U),
    m_channelId(0U),
    m_channelNo(0U),
    m_idenTable(NULL)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the HostSetup class.
/// </summary>
HostSetup::~HostSetup()
{
    /* stub */
}

/// <summary>
/// Executes the processing loop.
/// </summary>
/// <returns>Zero if successful, otherwise error occurred.</returns>
int HostSetup::run()
{
    bool ret = yaml::Parse(m_conf, m_confFile.c_str());
    if (!ret) {
        ::fatal("cannot read the configuration file, %s\n", m_confFile.c_str());
    }

    // initialize system logging
    ret = ::LogInitialise("", "", 0U, 1U);
    if (!ret) {
        ::fprintf(stderr, "unable to open the log file\n");
        return 1;
    }

    getHostVersion();
    ::LogInfo(">> Modem Setup");

    yaml::Node systemConf = m_conf["system"];
    m_duplex = systemConf["duplex"].as<bool>(true);

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

    m_identity = systemConf["identity"].as<std::string>();
    ::LogInfo("    Identity: %s", m_identity.c_str());

    yaml::Node cwId = systemConf["cwId"];
    uint32_t time = systemConf["cwId"].as<uint32_t>(10U);
    m_callsign = systemConf["cwId"].as<std::string>();

    LogInfo("CW Id Parameters");
    LogInfo("    Time: %u mins", time);
    LogInfo("    Callsign: %s", m_callsign.c_str());

    m_callsign = cwId["callsign"].as<std::string>();

    yaml::Node rfssConfig = systemConf["config"];
    m_channelId = (uint8_t)rfssConfig["channelId"].as<uint32_t>(0U);
    if (m_channelId > 15U) { // clamp to 15
        m_channelId = 15U;
    }

    m_channelNo = (uint32_t)::strtoul(rfssConfig["channelNo"].as<std::string>("1").c_str(), NULL, 16);
    if (m_channelNo == 0U) { // clamp to 1
        m_channelNo = 1U;
    }
    if (m_channelNo > 4095U) { // clamp to 4095
        m_channelNo = 4095U;
    }

    if (!calculateRxTxFreq()) {
        return false;
    }

    IdenTable entry = m_idenTable->find(m_channelId);
    if (entry.baseFrequency() == 0U) {
        ::LogError(LOG_HOST, "Channel Id %u has an invalid base frequency.", m_channelId);
        return false;
    }

    LogInfo("System Config Parameters");
    LogInfo("    RX Frequency: %uHz", m_rxFrequency);
    LogInfo("    TX Frequency: %uHz", m_txFrequency);
    LogInfo("    Base Frequency: %uHz", entry.baseFrequency());
    LogInfo("    TX Offset: %fMHz", entry.txOffsetMhz());

    // open terminal console
    ret = m_console.open();
    if (!ret) {
        return 1;
    }

    displayHelp();

    printStatus();

    bool end = false;
    while (!end) {
        int c = m_console.getChar();
        switch (c) {

            /** Setup Commands */
        case 'I':
        {
            char value[9] = { '\0' };
            ::fprintf(stdout, "> Identity ? ");
            ::fflush(stdout);

            m_console.getLine(value, 9, 0);
            m_identity = std::string(value);
            writeConfig();
        }
        break;

        case 'i':
        {
            char value[3] = { '\0' };
            ::fprintf(stdout, "> Channel ID ? ");
            ::fflush(stdout);

            m_console.getLine(value, 3, 0);

            uint8_t prevChannelId = m_channelId;

            // bryanb: appease the compiler...
            uint32_t channelId = m_channelId;
            sscanf(value, "%u", &channelId);

            m_channelId = (uint8_t)channelId;

            IdenTable entry = m_idenTable->find(m_channelId);
            if (entry.baseFrequency() == 0U) {
                ::LogError(LOG_SETUP, "Channel Id %u has an invalid base frequency.", m_channelId);
                m_channelId = prevChannelId;
            }

            writeConfig();
        }
        break;

        case 'c':
        {
            char value[5] = { '\0' };
            ::fprintf(stdout, "> Channel No ? ");
            ::fflush(stdout);

            m_console.getLine(value, 5, 0);

            uint8_t prevChannelNo = m_channelNo;
            sscanf(value, "%u", &m_channelNo);

            if (m_channelNo < 0 || m_channelNo > 4096) {
                ::LogError(LOG_SETUP, "Channel No %u is invalid.", m_channelNo);
                m_channelNo = prevChannelNo;
            }

            writeConfig();
        }
        break;

        case 'f':
        {
            char value[10] = { '\0' };
            ::fprintf(stdout, "> Tx Frequency (Hz) ? ");
            ::fflush(stdout);

            m_console.getLine(value, 10, 0);

            uint32_t txFrequency = m_txFrequency;
            sscanf(value, "%u", &txFrequency);

            IdenTable entry = m_idenTable->find(m_channelId);
            if (txFrequency < entry.baseFrequency()) {
                ::LogError(LOG_SETUP, "Tx Frequency %uHz is out of band range for base frequency %uHz. Tx Frequency must be greater then base frequency!", txFrequency, entry.baseFrequency());
                break;
            }

            if (txFrequency > entry.baseFrequency() + 25500000) {
                ::LogError(LOG_SETUP, "Tx Frequency %uHz is out of band range for base frequency %uHz. Tx Frequency must be no more then 25.5 Mhz higher then base frequency!", txFrequency, entry.baseFrequency());
                break;
            }

            uint32_t prevTxFrequency = m_txFrequency;
            m_txFrequency = txFrequency;
            uint32_t prevRxFrequency = m_rxFrequency;
            m_rxFrequency = m_txFrequency + (entry.txOffsetMhz() * 1000000);

            float spaceHz = entry.chSpaceKhz() * 1000;

            uint32_t rootFreq = m_txFrequency - entry.baseFrequency();
            uint8_t prevChannelNo = m_channelNo;
            m_channelNo = rootFreq / spaceHz;

            if (m_channelNo < 0 || m_channelNo > 4096) {
                ::LogError(LOG_SETUP, "Channel No %u is invalid.", m_channelNo);
                m_channelNo = prevChannelNo;
                m_txFrequency = prevTxFrequency;
                m_rxFrequency = prevRxFrequency;
                break;
            }

            writeConfig();
        }
        break;

        /** General Commands */
        case '`':
            printStatus();
            break;
        case 'V':
            getHostVersion();
            break;
        case 'H':
        case 'h':
            displayHelp();
            break;
        case 'S':
        case 's':
        {
            yaml::Serialize(m_conf, m_confFile.c_str(), yaml::SerializeConfig(4, 64, false, false));
            LogMessage(LOG_SETUP, " - Saved configuration to %s", m_confFile.c_str());
        }
        break;
        case 'Q':
        case 'q':
            end = true;
            break;

        case 13:
        case 10:
        case -1:
            break;
        default:
            LogError(LOG_SETUP, "Unknown command - %c (H/h for help)", c);
            break;
        }

        sleep(5U);
    }

    m_console.close();
    return 0;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Helper to print the help to the console.
/// </summary>
void HostSetup::displayHelp()
{
    LogMessage(LOG_SETUP, "General Commands:");
    LogMessage(LOG_SETUP, "    `        Display current settings");
    LogMessage(LOG_SETUP, "    V        Display version of host");
    LogMessage(LOG_SETUP, "    H/h      Display help");
    LogMessage(LOG_SETUP, "    S/s      Save settings to configuration file");
    LogMessage(LOG_SETUP, "    Q/q      Quit");
    LogMessage(LOG_SETUP, "Setup Commands:");
    LogMessage(LOG_SETUP, "    I        Set identity (logical name)");
    LogMessage(LOG_SETUP, "    i        Set logical channel ID");
    LogMessage(LOG_SETUP, "    c        Set logical channel number (by channel number)");
    LogMessage(LOG_SETUP, "    f        Set logical channel number (by Tx frequency)");
}

/// <summary>
/// Helper to calculate the Rx/Tx frequencies.
/// </summary>
bool HostSetup::calculateRxTxFreq()
{
    IdenTable entry = m_idenTable->find(m_channelId);
    if (entry.baseFrequency() == 0U) {
        ::LogError(LOG_HOST, "Channel Id %u has an invalid base frequency.", m_channelId);
        return false;
    }

    if (m_channelNo == 0U) { // clamp to 1
        m_channelNo = 1U;
    }
    if (m_channelNo > 4095U) { // clamp to 4095
        m_channelNo = 4095U;
    }

    if (m_duplex) {
        if (entry.txOffsetMhz() == 0U) {
            ::LogError(LOG_HOST, "Channel Id %u has an invalid Tx offset.", m_channelId);
            return false;
        }

        uint32_t calcSpace = (uint32_t)(entry.chSpaceKhz() / 0.125);
        float calcTxOffset = entry.txOffsetMhz() * 1000000;

        m_rxFrequency = (uint32_t)((entry.baseFrequency() + ((calcSpace * 125) * m_channelNo)) + calcTxOffset);
        m_txFrequency = (uint32_t)((entry.baseFrequency() + ((calcSpace * 125) * m_channelNo)));
    }
    else {
        uint32_t calcSpace = (uint32_t)(entry.chSpaceKhz() / 0.125);

        m_rxFrequency = (uint32_t)((entry.baseFrequency() + ((calcSpace * 125) * m_channelNo)));
        m_txFrequency = m_rxFrequency;
    }

    return true;
}

/// <summary>
/// Write configuration file.
/// </summary>
/// <returns>True, if configuration is written, otherwise false.</returns>
bool HostSetup::writeConfig()
{
    m_conf["system"]["identity"] = m_identity;

    m_conf["system"]["config"]["channelId"] = __INT_STR(m_channelId);
    m_conf["system"]["config"]["channelNo"] = __INT_STR(m_channelNo);

    printStatus();
    return true;
}

/// <summary>
/// Helper to sleep the thread.
/// </summary>
/// <param name="ms">Milliseconds to sleep.</param>
void HostSetup::sleep(uint32_t ms)
{
#if defined(_WIN32) || defined(_WIN64)
    ::Sleep(ms);
#else
    ::usleep(ms * 1000);
#endif
}

/// <summary>
/// Prints the current status of the calibration.
/// </summary>
void HostSetup::printStatus()
{
    IdenTable entry = m_idenTable->find(m_channelId);
    if (entry.baseFrequency() == 0U) {
        ::LogError(LOG_HOST, "Channel Id %u has an invalid base frequency.", m_channelId);
    }

    calculateRxTxFreq();

    LogMessage(LOG_SETUP, " - Channel ID: %u, Channel No: %u", m_channelId, m_channelNo);
    LogMessage(LOG_SETUP, " - Base Freq: %uHz, TX Offset: %fMHz, Bandwidth: %fKHz, Channel Spacing: %fKHz", entry.baseFrequency(), entry.txOffsetMhz(), entry.chBandwidthKhz(), entry.chSpaceKhz());
    LogMessage(LOG_SETUP, " - Rx Freq: %uHz, Tx Freq: %uHz, Identity: %s, Callsign: %s", m_rxFrequency, m_txFrequency, m_identity.c_str(), m_callsign.c_str());
}
