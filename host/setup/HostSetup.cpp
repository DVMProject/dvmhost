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
#include "dmr/DMRUtils.h"
#include "p25/P25Utils.h"
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
    yaml::Node modemConfig = systemConf["modem"];
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

    yaml::Node cwId = systemConf["cwId"];

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
        case 'M':
        {
            modemConfig = m_conf["system"]["modem"];
            m_conf["system"]["modem"]["protocol"]["type"] = std::string("uart"); // configuring modem, always sets type to UART

            yaml::Node uartConfig = modemConfig["protocol"]["uart"];

            std::string modemPort = uartConfig["port"].as<std::string>("/dev/ttyUSB0");
            uint32_t portSpeed = uartConfig["speed"].as<uint32_t>(115200);

            char value[21] = { '\0' };
            ::fprintf(stdout, "> Modem UART Port [%s] ? ", modemPort.c_str());
            ::fflush(stdout);

            m_console.getLine(value, 21, 0);
            if (value[0] != '\0') {
                modemPort = std::string(value);
                m_conf["system"]["modem"]["protocol"]["uart"]["port"] = modemPort;
            }

            ::fprintf(stdout, "> Port Speed [%u] ? ", portSpeed);
            ::fflush(stdout);

            m_console.getLine(value, 7, 0);
            if (value[0] != '\0') {
                sscanf(value, "%u", &portSpeed);
                m_conf["system"]["modem"]["protocol"]["uart"]["speed"] = __INT_STR(portSpeed);
            }

            printStatus();
        }
        break;

        case 'I':
        {
            std::string identity = m_conf["system"]["identity"].as<std::string>();

            char value[9] = { '\0' };
            ::fprintf(stdout, "> Identity [%s] ? ", identity.c_str());
            ::fflush(stdout);

            m_console.getLine(value, 9, 0);
            identity = std::string(value);
            if (identity.length() > 0) {
                m_conf["system"]["identity"] = identity;
            }

            printStatus();
        }
        break;

        case 'C':
        {
            cwId = m_conf["system"]["cwId"];
            bool enabled = cwId["enable"].as<bool>(false);
            uint32_t cwTime = cwId["time"].as<uint32_t>(10U);
            std::string callsign = cwId["callsign"].as<std::string>();

            char value[9] = { '\0' };
            ::fprintf(stdout, "> Callsign [%s] ? ", callsign.c_str());
            ::fflush(stdout);

            m_console.getLine(value, 9, 0);
            callsign = std::string(value);
            if (callsign.length() > 0) {
                m_conf["system"]["cwId"]["callsign"] = callsign;
            }

            ::fprintf(stdout, "> CW Enabled [%u] (Y/N) ? ", enabled);
            ::fflush(stdout);

            m_console.getLine(value, 2, 0);
            if (toupper(value[0]) == 'Y' || toupper(value[0]) == 'N') {
                enabled = value[0] == 'Y' ? true : false;
            }

            m_conf["system"]["cwId"]["enable"] = __BOOL_STR(enabled);

            ::fprintf(stdout, "> CW Interval [%u] (minutes) ? ", cwTime);
            ::fflush(stdout);

            m_console.getLine(value, 4, 0);
            uint32_t time = cwTime;
            sscanf(value, "%u", &time);
            if (value > 0) {
                m_conf["system"]["cwId"]["time"] = __INT_STR(time);
            }

            printStatus();
        }
        break;

        case 'N':
        {
            rfssConfig = m_conf["system"]["config"];
            uint32_t siteId = (uint8_t)::strtoul(rfssConfig["siteId"].as<std::string>("1").c_str(), NULL, 16);
            uint32_t dmrNetId = (uint32_t)::strtoul(rfssConfig["dmrNetId"].as<std::string>("1").c_str(), NULL, 16);
            uint32_t p25NetId = (uint32_t)::strtoul(rfssConfig["netId"].as<std::string>("BB800").c_str(), NULL, 16);
            uint32_t p25SysId = (uint32_t)::strtoul(rfssConfig["sysId"].as<std::string>("001").c_str(), NULL, 16);
            uint32_t p25RfssId = (uint8_t)::strtoul(rfssConfig["rfssId"].as<std::string>("1").c_str(), NULL, 16);

            char value[6] = { '\0' };
            ::fprintf(stdout, "> Site ID [$%02X] ? ", siteId);
            ::fflush(stdout);

            m_console.getLine(value, 3, 0);
            if (value[0] != '\0') {
                siteId = (uint32_t)::strtoul(std::string(value).c_str(), NULL, 16);
                siteId = p25::P25Utils::siteId(siteId);

                m_conf["system"]["config"]["siteId"] = __INT_HEX_STR(siteId);
            }

            ::fprintf(stdout, "> DMR Network ID [$%05X] ? ", dmrNetId);
            ::fflush(stdout);

            m_console.getLine(value, 6, 0);
            if (value[0] != '\0') {
                dmrNetId = (uint32_t)::strtoul(std::string(value).c_str(), NULL, 16);
                dmrNetId = dmr::DMRUtils::netId(dmrNetId, dmr::SITE_MODEL_TINY);

                m_conf["system"]["config"]["dmrNetId"] = __INT_HEX_STR(dmrNetId);
            }

            ::fprintf(stdout, "> P25 Network ID [$%05X] ? ", p25NetId);
            ::fflush(stdout);

            m_console.getLine(value, 6, 0);
            if (value[0] != '\0') {
                p25NetId = (uint32_t)::strtoul(std::string(value).c_str(), NULL, 16);
                p25NetId = p25::P25Utils::netId(p25NetId);

                m_conf["system"]["config"]["netId"] = __INT_HEX_STR(p25NetId);
            }

            ::fprintf(stdout, "> P25 System ID [$%03X] ? ", p25SysId);
            ::fflush(stdout);

            m_console.getLine(value, 4, 0);
            if (value[0] != '\0') {
                p25SysId = (uint32_t)::strtoul(std::string(value).c_str(), NULL, 16);
                p25SysId = p25::P25Utils::sysId(p25SysId);

                m_conf["system"]["config"]["sysId"] = __INT_HEX_STR(p25SysId);
            }

            ::fprintf(stdout, "> P25 RFSS ID [$%02X] ? ", p25RfssId);
            ::fflush(stdout);

            m_console.getLine(value, 3, 0);
            if (value[0] != '\0') {
                p25RfssId = (uint8_t)::strtoul(std::string(value).c_str(), NULL, 16);
                p25RfssId = p25::P25Utils::rfssId(p25RfssId);

                m_conf["system"]["config"]["rfssId"] = __INT_HEX_STR(p25RfssId);
            }

            printStatus();
        }
        break;
        
        case 'a':
        {
            rfssConfig = m_conf["system"]["config"];
            uint32_t dmrColorCode = rfssConfig["colorCode"].as<uint32_t>(2U);
            uint32_t p25NAC = (uint32_t)::strtoul(rfssConfig["nac"].as<std::string>("293").c_str(), NULL, 16);

            char value[6] = { '\0' };
            ::fprintf(stdout, "> DMR Color Code [%u] ? ", dmrColorCode);
            ::fflush(stdout);

            m_console.getLine(value, 2, 0);
            if (value[0] != '\0') {
                sscanf(value, "%u", &dmrColorCode);
                dmrColorCode = dmr::DMRUtils::colorCode(dmrColorCode);

                m_conf["system"]["config"]["colorCode"] = __INT_STR(dmrColorCode);
            }

            ::fprintf(stdout, "> P25 NAC [$%03X] ? ", p25NAC);
            ::fflush(stdout);

            m_console.getLine(value, 4, 0);
            if (value[0] != '\0') {
                p25NAC = (uint32_t)::strtoul(std::string(value).c_str(), NULL, 16);
                p25NAC = p25::P25Utils::nac(p25NAC);

                m_conf["system"]["config"]["nac"] = __INT_HEX_STR(p25NAC);
            }

            printStatus();
        }
        break;

        case 'i':
        {
            rfssConfig = m_conf["system"]["config"];
            m_channelId = (uint8_t)rfssConfig["channelId"].as<uint32_t>(0U);

            char value[3] = { '\0' };
            ::fprintf(stdout, "> Channel ID [%u] ? ", m_channelId);
            ::fflush(stdout);

            m_console.getLine(value, 3, 0);
            if (value[0] != '\0') {
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

                m_conf["system"]["config"]["channelId"] = __INT_STR(m_channelId);
            }

            printStatus();
        }
        break;

        case 'c':
        {
            rfssConfig = m_conf["system"]["config"];
            m_channelNo = (uint32_t)::strtoul(rfssConfig["channelNo"].as<std::string>("1").c_str(), NULL, 16);

            char value[5] = { '\0' };
            ::fprintf(stdout, "> Channel No [%u] ? ", m_channelNo);
            ::fflush(stdout);

            m_console.getLine(value, 5, 0);
            if (value[0] != '\0') {
                uint8_t prevChannelNo = m_channelNo;
                sscanf(value, "%u", &m_channelNo);

                if (m_channelNo < 0 || m_channelNo > 4096) {
                    ::LogError(LOG_SETUP, "Channel No %u is invalid.", m_channelNo);
                    m_channelNo = prevChannelNo;
                }

                m_conf["system"]["config"]["channelNo"] = __INT_HEX_STR(m_channelNo);
            }

            printStatus();
        }
        break;

        case 'f':
        {
            rfssConfig = m_conf["system"]["config"];
            m_channelNo = (uint32_t)::strtoul(rfssConfig["channelNo"].as<std::string>("1").c_str(), NULL, 16);

            char value[10] = { '\0' };
            ::fprintf(stdout, "> Tx Frequency [%uHz] (Hz) ? ", m_txFrequency);
            ::fflush(stdout);

            m_console.getLine(value, 10, 0);
            if (value[0] != '\0') {
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
                m_rxFrequency = m_txFrequency + (uint32_t)(entry.txOffsetMhz() * 1000000);

                float spaceHz = entry.chSpaceKhz() * 1000;

                uint32_t rootFreq = m_txFrequency - entry.baseFrequency();
                uint8_t prevChannelNo = m_channelNo;
                m_channelNo = (uint32_t)(rootFreq / spaceHz);

                if (m_channelNo < 0 || m_channelNo > 4096) {
                    ::LogError(LOG_SETUP, "Channel No %u is invalid.", m_channelNo);
                    m_channelNo = prevChannelNo;
                    m_txFrequency = prevTxFrequency;
                    m_rxFrequency = prevRxFrequency;
                    break;
                }

                m_conf["system"]["config"]["channelNo"] = __INT_HEX_STR(m_channelNo);
            }

            printStatus();
        }
        break;

        case '!':
        {
            modemConfig = m_conf["system"]["modem"];
            m_conf["system"]["modem"]["protocol"]["type"] = std::string("null"); // configuring modem, always sets type to UART
            printStatus();
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
    LogMessage(LOG_SETUP, "    !        Set \"null\" modem (disables modem communication)");
    LogMessage(LOG_SETUP, "    S/s      Save settings to configuration file");
    LogMessage(LOG_SETUP, "    Q/q      Quit");
    LogMessage(LOG_SETUP, "Setup Commands:");
    LogMessage(LOG_SETUP, "    M        Set modem port and speed");
    LogMessage(LOG_SETUP, "    I        Set identity (logical name)");
    LogMessage(LOG_SETUP, "    C        Set callsign and CW configuration");
    LogMessage(LOG_SETUP, "    N        Set site and network configuration");
    LogMessage(LOG_SETUP, "    a        Set NAC and Color Code");
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
    yaml::Node systemConf = m_conf["system"];
    {
        yaml::Node modemConfig = m_conf["system"]["modem"];
        std::string type = modemConfig["protocol"]["type"].as<std::string>();

        yaml::Node uartConfig = modemConfig["protocol"]["uart"];
        std::string modemPort = uartConfig["port"].as<std::string>();
        uint32_t portSpeed = uartConfig["speed"].as<uint32_t>(115200U);

        LogMessage(LOG_SETUP, " - Port Type: %s, Modem Port: %s, Port Speed: %u", type.c_str(), modemPort.c_str(), portSpeed);
    }

    {
        std::string identity = systemConf["identity"].as<std::string>();

        IdenTable entry = m_idenTable->find(m_channelId);
        if (entry.baseFrequency() == 0U) {
            ::LogError(LOG_HOST, "Channel Id %u has an invalid base frequency.", m_channelId);
        }

        calculateRxTxFreq();

        LogMessage(LOG_SETUP, " - Identity: %s", identity.c_str());
        LogMessage(LOG_SETUP, " - Channel Id: %u, Channel No: %u", m_channelId, m_channelNo);
        LogMessage(LOG_SETUP, " - Base Freq: %uHz, TX Offset: %fMHz, Bandwidth: %fKHz, Channel Spacing: %fKHz", entry.baseFrequency(), entry.txOffsetMhz(), entry.chBandwidthKhz(), entry.chSpaceKhz());
        LogMessage(LOG_SETUP, " - Rx Freq: %uHz, Tx Freq: %uHz", m_rxFrequency, m_txFrequency);
    }

    {
        yaml::Node cwId = systemConf["cwId"];
        bool enabled = cwId["enable"].as<bool>(false);
        uint32_t cwTime = cwId["time"].as<uint32_t>(10U);
        std::string callsign = cwId["callsign"].as<std::string>();

        LogMessage(LOG_SETUP, " - Callsign: %s, CW Interval: %u mins, CW Enabled: %u", callsign.c_str(), cwTime, enabled);
    }

    {
        yaml::Node rfssConfig = systemConf["config"];
        uint32_t dmrColorCode = rfssConfig["colorCode"].as<uint32_t>(2U);
        uint32_t p25NAC = (uint32_t)::strtoul(rfssConfig["nac"].as<std::string>("293").c_str(), NULL, 16);

        LogMessage(LOG_SETUP, " - DMR Color Code: %u, P25 NAC: $%03X", dmrColorCode, p25NAC);
    }

    {
        yaml::Node rfssConfig = systemConf["config"];
        uint32_t siteId = (uint8_t)::strtoul(rfssConfig["siteId"].as<std::string>("1").c_str(), NULL, 16);
        uint32_t dmrNetId = (uint32_t)::strtoul(rfssConfig["dmrNetId"].as<std::string>("1").c_str(), NULL, 16);
        uint32_t p25NetId = (uint32_t)::strtoul(rfssConfig["netId"].as<std::string>("BB800").c_str(), NULL, 16);
        uint32_t p25SysId = (uint32_t)::strtoul(rfssConfig["sysId"].as<std::string>("001").c_str(), NULL, 16);
        uint32_t p25RfssId = (uint8_t)::strtoul(rfssConfig["rfssId"].as<std::string>("1").c_str(), NULL, 16);

        LogMessage(LOG_SETUP, " - Site Id: $%02X, DMR Network Id: $%05X, P25 Network Id: $%05X, P25 System Id: $%03X, P25 RFSS Id: $%02X", siteId, dmrNetId, p25NetId, p25SysId, p25RfssId);
    }
}
