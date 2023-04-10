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
*   Copyright (C) 2017-2022 by Bryan Biedenkapp N2PLL
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
#include "modem/port/UARTPort.h"
#include "p25/P25Defines.h"
#include "p25/data/DataHeader.h"
#include "p25/lc/LC.h"
#include "p25/lc/tsbk/TSBKFactory.h"
#include "p25/NID.h"
#include "p25/Sync.h"
#include "p25/P25Utils.h"
#include "nxdn/NXDNDefines.h"
#include "nxdn/channel/LICH.h"
#include "nxdn/NXDNUtils.h"
#include "edac/CRC.h"
#include "HostMain.h"
#include "Log.h"
#include "StopWatch.h"
#include "Utils.h"

using namespace modem;
using namespace lookups;

#include <cstdio>
#include <algorithm>

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define DMR_CAL_STR         "[Tx] DMR 1200 Hz Tone Mode (2.75Khz Deviation)"
#define P25_CAL_STR         "[Tx] P25 1200 Hz Tone Mode (2.83Khz Deviation)"
#define DMR_LF_CAL_STR      "[Tx] DMR Low Frequency Mode (80 Hz square wave)"
#define DMR_CAL_1K_STR      "[Tx] DMR BS 1031 Hz Test Pattern (TS2 CC1 ID1 TG9)"
#define DMR_DMO_CAL_1K_STR  "[Tx] DMR MS 1031 Hz Test Pattern (TS2 CC1 ID1 TG9)"
#define P25_CAL_1K_STR      "[Tx] P25 1011 Hz Test Pattern (NAC293 ID1 TG1)"
#define P25_TDU_TEST_STR    "[Tx] P25 TDU Data Test Stream"
#define NXDN_CAL_1K_STR     "[Tx] NXDN 1031 Hz Test Pattern (RAN1 ID1 TG1)"
#define DMR_FEC_STR         "[Rx] DMR MS FEC BER Test Mode"
#define DMR_FEC_1K_STR      "[Rx] DMR MS 1031 Hz Test Pattern (CC1 ID1 TG9)"
#define P25_FEC_STR         "[Rx] P25 FEC BER Test Mode"
#define P25_FEC_1K_STR      "[Rx] P25 1011 Hz Test Pattern (NAC293 ID1 TG1)"
#define NXDN_FEC_STR        "[Rx] NXDN FEC BER Test Mode"
#define RSSI_CAL_STR        "RSSI Calibration Mode"

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
const uint8_t LDU1_1K[] = {
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

const uint8_t LDU2_1K[] = {
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
    m_modem(nullptr),
    m_queue(4096U, "Frame Buffer"),
    m_console(),
    m_fec(),
    m_transmit(false),
    m_duplex(true),
    m_dmrEnabled(false),
    m_dmrRx1K(false),
    m_p25Enabled(false),
    m_p25Rx1K(false),
    m_p25TduTest(false),
    m_nxdnEnabled(false),
    m_isHotspot(false),
    m_debug(false),
    m_mode(STATE_DMR_CAL_1K),
    m_modeStr(DMR_CAL_1K_STR),
    m_rxFrequency(0U),
    m_rxAdjustedFreq(0U),
    m_txFrequency(0U),
    m_txAdjustedFreq(0U),
    m_channelId(0U),
    m_channelNo(0U),
    m_idenTable(nullptr),
    m_berFrames(0U),
    m_berBits(0U),
    m_berErrs(0U),
    m_berUndecodableLC(0U),
    m_berUncorrectable(0U),
    m_timeout(300U),
    m_timer(0U),
    m_updateConfigFromModem(false),
    m_hasFetchedStatus(false)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the HostCal class.
/// </summary>
HostCal::~HostCal()
{
    delete m_modem;
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

    // initialize system logging
    ret = ::LogInitialise("", "", 0U, 1U, true);
    if (!ret) {
        ::fprintf(stderr, "unable to open the log file\n");
        return 1;
    }

    getHostVersion();
    ::LogInfo(">> Modem Calibration");

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

    std::string identity = systemConf["identity"].as<std::string>();
    ::LogInfo("    Identity: %s", identity.c_str());

    yaml::Node rfssConfig = systemConf["config"];
    m_channelId = (uint8_t)rfssConfig["channelId"].as<uint32_t>(0U);
    if (m_channelId > 15U) { // clamp to 15
        m_channelId = 15U;
    }

    IdenTable entry = m_idenTable->find(m_channelId);
    if (entry.baseFrequency() == 0U) {
        ::LogError(LOG_HOST, "Channel Id %u has an invalid base frequency.", m_channelId);
        return false;
    }

    m_channelNo = (uint32_t)::strtoul(rfssConfig["channelNo"].as<std::string>("1").c_str(), NULL, 16);
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

    LogInfo("System Config Parameters");
    LogInfo("    RX Frequency: %uHz", m_rxFrequency);
    LogInfo("    TX Frequency: %uHz", m_txFrequency);
    LogInfo("    Base Frequency: %uHz", entry.baseFrequency());
    LogInfo("    TX Offset: %fMHz", entry.txOffsetMhz());

    yaml::Node modemConf = systemConf["modem"];

    bool rxInvert = modemConf["rxInvert"].as<bool>(false);
    bool txInvert = modemConf["txInvert"].as<bool>(false);
    bool pttInvert = modemConf["pttInvert"].as<bool>(false);
    bool dcBlocker = modemConf["dcBlocker"].as<bool>(true);

    int rxDCOffset = modemConf["rxDCOffset"].as<int>(0);
    int txDCOffset = modemConf["txDCOffset"].as<int>(0);
    float rxLevel = modemConf["rxLevel"].as<float>(50.0F);
    float txLevel = modemConf["txLevel"].as<float>(50.0F);

    yaml::Node hotspotParams = modemConf["hotspot"];

    int dmrDiscBWAdj = hotspotParams["dmrDiscBWAdj"].as<int>(0);
    int p25DiscBWAdj = hotspotParams["p25DiscBWAdj"].as<int>(0);
    int nxdnDiscBWAdj = hotspotParams["nxdnDiscBWAdj"].as<int>(0);
    int dmrPostBWAdj = hotspotParams["dmrPostBWAdj"].as<int>(0);
    int p25PostBWAdj = hotspotParams["p25PostBWAdj"].as<int>(0);
    int nxdnPostBWAdj = hotspotParams["nxdnPostBWAdj"].as<int>(0);

    ADF_GAIN_MODE adfGainMode = (ADF_GAIN_MODE)hotspotParams["adfGainMode"].as<uint32_t>(0U);

    bool afcEnable = hotspotParams["afcEnable"].as<bool>(false);
    uint8_t afcKI = (uint8_t)hotspotParams["afcKI"].as<uint32_t>(11U);
    uint8_t afcKP = (uint8_t)hotspotParams["afcKP"].as<uint32_t>(4U);
    uint8_t afcRange = (uint8_t)hotspotParams["afcRange"].as<uint32_t>(1U);

    int rxTuning = hotspotParams["rxTuning"].as<int>(0);
    int txTuning = hotspotParams["txTuning"].as<int>(0);

    // apply the frequency tuning offsets
    m_rxAdjustedFreq = m_rxFrequency + rxTuning;
    m_txAdjustedFreq = m_txFrequency + txTuning;

    yaml::Node repeaterParams = modemConf["repeater"];

    int dmrSymLevel3Adj = repeaterParams["dmrSymLvl3Adj"].as<int>(0);
    int dmrSymLevel1Adj = repeaterParams["dmrSymLvl1Adj"].as<int>(0);
    int p25SymLevel3Adj = repeaterParams["p25SymLvl3Adj"].as<int>(0);
    int p25SymLevel1Adj = repeaterParams["p25SymLvl1Adj"].as<int>(0);
    int nxdnSymLevel3Adj = repeaterParams["nxdnSymLvl3Adj"].as<int>(0);
    int nxdnSymLevel1Adj = repeaterParams["nxdnSymLvl1Adj"].as<int>(0);

    yaml::Node softpotParams = modemConf["softpot"];

    uint8_t rxCoarsePot = (uint8_t)softpotParams["rxCoarse"].as<uint32_t>(127U);
    uint8_t rxFinePot = (uint8_t)softpotParams["rxFine"].as<uint32_t>(127U);
    uint8_t txCoarsePot = (uint8_t)softpotParams["txCoarse"].as<uint32_t>(127U);
    uint8_t txFinePot = (uint8_t)softpotParams["txFine"].as<uint32_t>(127U);
    uint8_t rssiCoarsePot = (uint8_t)softpotParams["rssiCoarse"].as<uint32_t>(127U);
    uint8_t rssiFinePot = (uint8_t)softpotParams["rssiFine"].as<uint32_t>(127U);

    uint8_t fdmaPreamble = (uint8_t)modemConf["fdmaPreamble"].as<uint32_t>(80U);
    uint8_t dmrRxDelay = (uint8_t)modemConf["dmrRxDelay"].as<uint32_t>(7U);
    uint8_t p25CorrCount = (uint8_t)modemConf["p25CorrCount"].as<uint32_t>(5U);

    bool ignoreModemConfigArea = modemConf["ignoreModemConfigArea"].as<bool>(false);

    yaml::Node modemProtocol = modemConf["protocol"];
    std::string portType = modemProtocol["type"].as<std::string>("null");

    yaml::Node uartProtocol = modemProtocol["uart"];
    std::string uartPort = uartProtocol["port"].as<std::string>();
    uint32_t uartSpeed = uartProtocol["speed"].as<uint32_t>(115200);

    port::IModemPort* modemPort = nullptr;
    std::transform(portType.begin(), portType.end(), portType.begin(), ::tolower);
    if (portType == NULL_PORT) {
        ::LogError(LOG_HOST, "Calibration mode is unsupported with the null modem!");
        return 2;
    }
    else if (portType == UART_PORT) {
        port::SERIAL_SPEED serialSpeed = port::SERIAL_115200;
        switch (uartSpeed) {
        case 1200:
            serialSpeed = port::SERIAL_1200;
            break;
        case 2400:
            serialSpeed = port::SERIAL_2400;
            break;
        case 4800:
            serialSpeed = port::SERIAL_4800;
            break;
        case 9600:
            serialSpeed = port::SERIAL_9600;
            break;
        case 19200:
            serialSpeed = port::SERIAL_19200;
            break;
        case 38400:
            serialSpeed = port::SERIAL_38400;
            break;
        case 76800:
            serialSpeed = port::SERIAL_76800;
            break;
        case 230400:
            serialSpeed = port::SERIAL_230400;
            break;
        case 460800:
            serialSpeed = port::SERIAL_460800;
            break;
        default:
            LogWarning(LOG_HOST, "Unsupported serial speed %u, defaulting to %u", uartSpeed, port::SERIAL_115200);
            uartSpeed = 115200;
        case 115200:
            break;
        }

        modemPort = new port::UARTPort(uartPort, serialSpeed, true);
        LogInfo("Modem Parameters");
        LogInfo("    UART Port: %s", uartPort.c_str());
        LogInfo("    UART Speed: %u", uartSpeed);
        LogInfo("    RX Invert: %s", rxInvert ? "yes" : "no");
        LogInfo("    TX Invert: %s", txInvert ? "yes" : "no");
        LogInfo("    PTT Invert: %s", pttInvert ? "yes" : "no");
        LogInfo("    DC Blocker: %s", dcBlocker ? "yes" : "no");
        LogInfo("    FDMA Preambles: %u (%.1fms)", fdmaPreamble, float(fdmaPreamble) * 0.2083F);
        LogInfo("    DMR RX Delay: %u (%.1fms)", dmrRxDelay, float(dmrRxDelay) * 0.0416666F);
        LogInfo("    P25 Corr. Count: %u (%.1fms)", p25CorrCount, float(p25CorrCount) * 0.667F);
        LogInfo("    RX DC Offset: %d", rxDCOffset);
        LogInfo("    TX DC Offset: %d", txDCOffset);
        LogInfo("    RX Tuning Offset: %dhz", rxTuning);
        LogInfo("    TX Tuning Offset: %dhz", txTuning);
        LogInfo("    RX Effective Frequency: %uhz", m_rxAdjustedFreq);
        LogInfo("    TX Effective Frequency: %uhz", m_txAdjustedFreq);
        LogInfo("    RX Coarse: %u, Fine: %u", rxCoarsePot, rxFinePot);
        LogInfo("    TX Coarse: %u, Fine: %u", txCoarsePot, txFinePot);
        LogInfo("    RSSI Coarse: %u, Fine: %u", rssiCoarsePot, rssiFinePot);
        LogInfo("    RX Level: %.1f%%", rxLevel);
        LogInfo("    TX Level: %.1f%%", txLevel);

        if (ignoreModemConfigArea) {
            LogInfo("    Ignore Modem Configuration Area: yes");
        }
    }
    else if (g_remoteModemMode) {
        ::LogError(LOG_HOST, "Calibration mode is unsupported with a remote modem!");
        return 2;
    }

    if (modemPort == nullptr) {
        ::LogError(LOG_HOST, "Invalid modem port type, %s!", portType.c_str());
        return 2;
    }

    p25::lc::TSBK::setVerbose(true);
    p25::lc::TSBK::setWarnCRC(true);
    p25::lc::tsbk::TSBKFactory::setWarnCRC(true);

    m_modem = new Modem(modemPort, false, rxInvert, txInvert, pttInvert, dcBlocker, false, fdmaPreamble, dmrRxDelay, p25CorrCount, 3960U, 2592U, 1488U, false, ignoreModemConfigArea, false, false, false);
    m_modem->setLevels(rxLevel, txLevel, txLevel, txLevel, txLevel);
    m_modem->setSymbolAdjust(dmrSymLevel3Adj, dmrSymLevel1Adj, p25SymLevel3Adj, p25SymLevel1Adj, nxdnSymLevel3Adj, nxdnSymLevel1Adj);
    m_modem->setDCOffsetParams(txDCOffset, rxDCOffset);
    m_modem->setRFParams(m_rxFrequency, m_txFrequency, rxTuning, txTuning, 100U, dmrDiscBWAdj, p25DiscBWAdj, nxdnDiscBWAdj, dmrPostBWAdj, p25PostBWAdj, nxdnPostBWAdj, adfGainMode,
        afcEnable, afcKI, afcKP, afcRange);
    m_modem->setSoftPot(rxCoarsePot, rxFinePot, txCoarsePot, txFinePot, rssiCoarsePot, rssiFinePot);

    m_modem->setOpenHandler(MODEM_OC_PORT_HANDLER_BIND(HostCal::portModemOpen, this));
    m_modem->setCloseHandler(MODEM_OC_PORT_HANDLER_BIND(HostCal::portModemClose, this));
    m_modem->setResponseHandler(MODEM_RESP_HANDLER_BIND(HostCal::portModemHandler, this));

    // open modem and initialize
    ret = m_modem->open();
    if (!ret) {
        ::LogError(LOG_CAL, "Failed to open modem");
        return 1;
    }

    // open terminal console
    ret = m_console.open();
    if (!ret) {
        return 1;
    }

    readFlash();

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

        case 'X':
        {
            char value[5] = { '\0' };
            ::fprintf(stdout, "> FDMA Preambles [%u] ? ", m_modem->m_fdmaPreamble);
            ::fflush(stdout);

            m_console.getLine(value, 5, 0);
            if (value[0] != '\0') {
                // bryanb: appease the compiler...
                uint32_t fdmaPreamble = m_modem->m_fdmaPreamble;
                sscanf(value, "%u", &fdmaPreamble);

                m_modem->m_fdmaPreamble = (uint8_t)fdmaPreamble;

                writeConfig();
            }
        }
        break;

        case 'W':
        {
            char value[5] = { '\0' };
            ::fprintf(stdout, "> DMR Rx Delay [%u] ? ", m_modem->m_dmrRxDelay);
            ::fflush(stdout);

            m_console.getLine(value, 5, 0);
            if (value[0] != '\0') {
                // bryanb: appease the compiler...
                uint32_t dmrRxDelay = m_modem->m_dmrRxDelay;
                sscanf(value, "%u", &dmrRxDelay);

                m_modem->m_dmrRxDelay = (uint8_t)dmrRxDelay;

                writeConfig();
            }
        }
        break;

        case 'w':
        {
            if (!m_isHotspot) {
                char value[5] = { '\0' };
                ::fprintf(stdout, "> P25 Correlation Count [%u] ? ", m_modem->m_p25CorrCount);
                ::fflush(stdout);

                m_console.getLine(value, 5, 0);
                if (value[0] != '\0') {
                    // bryanb: appease the compiler...
                    uint32_t p25CorrCount = m_modem->m_p25CorrCount;
                    sscanf(value, "%u", &p25CorrCount);

                    m_modem->m_p25CorrCount = (uint8_t)p25CorrCount;

                    writeConfig();
                }
            }
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
        case '-':
            if (!m_isHotspot)
                setDMRSymLevel3Adj(-1);
            break;
        case '=':
            if (!m_isHotspot)
                setDMRSymLevel3Adj(1);
            break;
        case '_':
            if (!m_isHotspot)
                setDMRSymLevel1Adj(-1);
            break;
        case '+':
            if (!m_isHotspot)
                setDMRSymLevel1Adj(1);
            break;

        case '[':
            if (!m_isHotspot)
                setP25SymLevel3Adj(-1);
            break;
        case ']':
            if (!m_isHotspot)
                setP25SymLevel3Adj(1);
            break;
        case '{':
            if (!m_isHotspot)
                setP25SymLevel1Adj(-1);
            break;
        case '}':
            if (!m_isHotspot)
                setP25SymLevel1Adj(1);
            break;

        case ';':
            if (!m_isHotspot && m_modem->getVersion() >= 3U)
                setNXDNSymLevel3Adj(-1);
            else {
                LogWarning(LOG_CAL, "NXDN is not supported on your firmware!", NXDN_FEC_STR);
            }
            break;
        case '\'':
            if (!m_isHotspot && m_modem->getVersion() >= 3U)
                setNXDNSymLevel3Adj(1);
            else {
                LogWarning(LOG_CAL, "NXDN is not supported on your firmware!", NXDN_FEC_STR);
            }
            break;
        case ':':
            if (!m_isHotspot && m_modem->getVersion() >= 3U)
                setNXDNSymLevel1Adj(-1);
            else {
                LogWarning(LOG_CAL, "NXDN is not supported on your firmware!", NXDN_FEC_STR);
            }
            break;
        case '"':
            if (!m_isHotspot && m_modem->getVersion() >= 3U)
                setNXDNSymLevel1Adj(1);
            else {
                LogWarning(LOG_CAL, "NXDN is not supported on your firmware!", NXDN_FEC_STR);
            }
            break;

        case '1':
        {
            if (m_isHotspot) {
                char value[5] = { '\0' };
                ::fprintf(stdout, "> DMR Discriminator BW Offset [%d] ? ", m_modem->m_dmrDiscBWAdj);
                ::fflush(stdout);

                m_console.getLine(value, 5, 0);
                if (value[0] != '\0') {
                    int bwAdj = m_modem->m_dmrDiscBWAdj;
                    sscanf(value, "%d", &bwAdj);

                    m_modem->m_dmrDiscBWAdj = bwAdj;

                    writeRFParams();
                }
            }
        }
        break;

        case '2':
        {
            if (m_isHotspot) {
                char value[5] = { '\0' };
                ::fprintf(stdout, "> P25 Discriminator BW Offset [%d] ? ", m_modem->m_p25DiscBWAdj);
                ::fflush(stdout);

                m_console.getLine(value, 5, 0);
                if (value[0] != '\0') {
                    int bwAdj = m_modem->m_p25DiscBWAdj;
                    sscanf(value, "%d", &bwAdj);

                    m_modem->m_p25DiscBWAdj = bwAdj;

                    writeRFParams();
                }
            }
        }
        break;

        case '3':
        {
            if (m_isHotspot && m_modem->getVersion() >= 3U) {
                char value[5] = { '\0' };
                ::fprintf(stdout, "> NXDN Discriminator BW Offset [%d] ? ", m_modem->m_nxdnDiscBWAdj);
                ::fflush(stdout);

                m_console.getLine(value, 5, 0);
                if (value[0] != '\0') {
                    int bwAdj = m_modem->m_nxdnDiscBWAdj;
                    sscanf(value, "%d", &bwAdj);

                    m_modem->m_nxdnDiscBWAdj = bwAdj;

                    writeRFParams();
                }
            }
            else {
                LogWarning(LOG_CAL, "NXDN is not supported on your firmware!", NXDN_FEC_STR);
            }
        }
        break;

        case '4':
        {
            if (m_isHotspot) {
                char value[5] = { '\0' };
                ::fprintf(stdout, "> DMR Post Demodulation BW Offset [%d] ? ", m_modem->m_dmrPostBWAdj);
                ::fflush(stdout);

                m_console.getLine(value, 5, 0);
                if (value[0] != '\0') {
                    int bwAdj = m_modem->m_dmrPostBWAdj;
                    sscanf(value, "%d", &bwAdj);

                    m_modem->m_dmrPostBWAdj = bwAdj;

                    writeRFParams();
                }
            }
        }
        break;

        case '5':
        {
            if (m_isHotspot) {
                char value[5] = { '\0' };
                ::fprintf(stdout, "> P25 Post Demodulation BW Offset [%d] ? ", m_modem->m_p25PostBWAdj);
                ::fflush(stdout);

                m_console.getLine(value, 5, 0);
                if (value[0] != '\0') {
                    int bwAdj = m_modem->m_p25PostBWAdj;
                    sscanf(value, "%d", &bwAdj);

                    m_modem->m_p25PostBWAdj = bwAdj;

                    writeRFParams();
                }
            }
        }
        break;

        case '6':
        {
            if (m_isHotspot && m_modem->getVersion() >= 3U) {
                char value[5] = { '\0' };
                ::fprintf(stdout, "> NXDN Post Demodulation BW Offset [%d] ? ", m_modem->m_nxdnPostBWAdj);
                ::fflush(stdout);

                m_console.getLine(value, 5, 0);
                if (value[0] != '\0') {
                    int bwAdj = m_modem->m_nxdnPostBWAdj;
                    sscanf(value, "%d", &bwAdj);

                    m_modem->m_nxdnPostBWAdj = bwAdj;

                    writeRFParams();
                }
            }
            else {
                LogWarning(LOG_CAL, "NXDN is not supported on your firmware!", NXDN_FEC_STR);
            }
        }
        break;

        case '7':
        {
            if (m_isHotspot) {
                char value[2] = { '\0' };
                ::fprintf(stdout, "> ADF7021 Gain Mode (0 - Auto, 1 - Auto High Lin., 2 - Low, 3 - High) [%u] ? ", m_modem->m_adfGainMode);
                ::fflush(stdout);

                m_console.getLine(value, 2, 0);
                if (value[0] != '\0') {
                    uint32_t gainMode = (uint32_t)m_modem->m_adfGainMode;
                    sscanf(value, "%u", &gainMode);

                    if (gainMode >= 0U && gainMode < 4U)
                    {
                        m_modem->m_adfGainMode = (ADF_GAIN_MODE)gainMode;
                        writeRFParams();
                    }
                    else
                    {
                        m_modem->m_adfGainMode = ADF_GAIN_AUTO;
                        writeRFParams();
                    }
                }
            }
        }
        break;

        case '8':
        {
            if (m_isHotspot && m_modem->getVersion() >= 3U) {
                char value[5] = { '\0' };
                ::fprintf(stdout, "> ADF7021 AFC Enabled [%u] (Y/N) ? ", m_modem->m_afcEnable);
                ::fflush(stdout);

                m_console.getLine(value, 2, 0);
                if (toupper(value[0]) == 'Y' || toupper(value[0]) == 'N') {
                    m_modem->m_afcEnable = value[0] == 'Y' ? true : false;
                }

                ::fprintf(stdout, "> AFC Range [%u] ? ", m_modem->m_afcRange);
                ::fflush(stdout);

                m_console.getLine(value, 4, 0);
                if (value[0] != '\0') {
                    uint32_t afcRange = m_modem->m_afcRange;
                    sscanf(value, "%u", &afcRange);

                    if (afcRange >= 0U && afcRange < 256U)
                        m_modem->m_afcRange = (uint8_t)afcRange;
                    else
                        m_modem->m_afcRange = 4U;
                }

                ::fprintf(stdout, "> AFC KI Parameter [%u] ? ", m_modem->m_afcKI);
                ::fflush(stdout);

                m_console.getLine(value, 3, 0);
                if (value[0] != '\0') {
                    uint32_t afcKI = m_modem->m_afcKI;
                    sscanf(value, "%u", &afcKI);

                    if (afcKI >= 0U && afcKI < 16U)
                        m_modem->m_afcKI = (uint8_t)afcKI;
                    else
                        m_modem->m_afcKI = 11U;
                }

                ::fprintf(stdout, "> AFC KP Parameter [%u] ? ", m_modem->m_afcKP);
                ::fflush(stdout);

                m_console.getLine(value, 2, 0);
                if (value[0] != '\0') {
                    uint32_t afcKP = m_modem->m_afcKP;
                    sscanf(value, "%u", &afcKP);

                    if (afcKP >= 0U && afcKP < 8U)
                        m_modem->m_afcKP = (uint8_t)afcKP;
                    else
                        m_modem->m_afcKP = 1U;
                }

                writeRFParams();
            }
            else {
                LogWarning(LOG_CAL, "ADF7021 AFC alignment is not supported on your firmware!");
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
        case 'l':
        {
            m_mode = STATE_P25;
            m_modeStr = P25_TDU_TEST_STR;
            m_duplex = true;
            m_dmrEnabled = false;
            m_p25Enabled = true;
            m_p25TduTest = true;
            m_nxdnEnabled = false;

            m_queue.clear();

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
        case '0':
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
            if (c == '0') {
                m_duplex = true;
            } else {
                m_duplex = false;
            }
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
            m_duplex = false;
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
                m_duplex = false;
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
            getHostVersion();
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
        {
            m_mode = STATE_IDLE;
            writeConfig();
            if (m_isHotspot) {
                writeRFParams();
            }
            writeSymbolAdjust();
            yaml::Serialize(m_conf, m_confFile.c_str(), yaml::SerializeConfig(4, 64, false, false));
            LogMessage(LOG_CAL, " - Saved configuration to %s", m_confFile.c_str());
            if (writeFlash()) {
                LogMessage(LOG_CAL, " - Wrote configuration area on modem");
            }
        }
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

        if (m_p25TduTest && m_queue.hasSpace(p25::P25_TDU_FRAME_LENGTH_BYTES + 2U)) {
            uint8_t data[p25::P25_TDU_FRAME_LENGTH_BYTES + 2U];
            ::memset(data + 2U, 0x00U, p25::P25_TDU_FRAME_LENGTH_BYTES);

            // Generate Sync
            p25::Sync::addP25Sync(data + 2U);

            // Generate NID
            std::unique_ptr<p25::NID> nid = new_unique(p25::NID, 1U);
            nid->encode(data + 2U, p25::P25_DUID_TDU);

            // Add busy bits
            p25::P25Utils::addBusyBits(data + 2U, p25::P25_TDU_FRAME_LENGTH_BITS, true, true);

            data[0U] = modem::TAG_EOT;
            data[1U] = 0x00U;

            addFrame(data, p25::P25_TDU_FRAME_LENGTH_BYTES + 2U, p25::P25_LDU_FRAME_LENGTH_BYTES);
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

    m_modem->close();
    m_console.close();
    return 0;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <param name="modem"></param>
bool HostCal::portModemOpen(Modem* modem)
{
    sleep(2000U);

    bool ret = writeRFParams();
    if (!ret) {
        ret = writeRFParams();
        if (!ret) {
            LogError(LOG_MODEM, "Modem unresponsive to RF parameters set after 2 attempts. Stopping.");
            m_modem->close();
            return false;
        }
    }

    ret = writeConfig();
    if (!ret) {
        ret = writeConfig();
        if (!ret) {
            LogError(LOG_MODEM, "Modem unresponsive to configuration set after 2 attempts. Stopping.");
            m_modem->close();
            return false;
        }
    }

    LogMessage(LOG_MODEM, "Modem Ready [Calibration Mode]");

    // handled modem open
    return true;
}

/// <summary>
///
/// </summary>
/// <param name="modem"></param>
bool HostCal::portModemClose(Modem* modem)
{
    // handled modem close
    return true;
}

/// <summary>
///
/// </summary>
/// <param name="modem"></param>
/// <param name="ms"></param>
/// <param name="rspType"></param>
/// <param name="rspDblLen"></param>
/// <param name="data"></param>
/// <param name="len"></param>
/// <returns></returns>
bool HostCal::portModemHandler(Modem* modem, uint32_t ms, RESP_TYPE_DVM rspType, bool rspDblLen, const uint8_t* buffer, uint16_t len)
{
    switch (m_mode) {
        case STATE_P25:
        {
            if (m_transmit) {
                uint8_t dataLen = 0U;
                m_queue.peek(&dataLen, 1U);

                if (!m_queue.isEmpty() && m_modem->m_p25Space >= dataLen) {
                    uint8_t data[p25::P25_LDU_FRAME_LENGTH_BYTES + 2U];

                    dataLen = 0U;
                    m_queue.getData(&dataLen, 1U);
                    m_queue.getData(data, dataLen);

                    uint8_t buffer[250U];

                    buffer[0U] = DVM_FRAME_START;
                    buffer[1U] = dataLen + 2U;
                    buffer[2U] = CMD_P25_DATA;

                    ::memcpy(buffer + 3U, data + 1U, dataLen - 1U);

                    uint8_t len = dataLen + 2U;

                    int ret = m_modem->write(buffer, len);
                    if (ret != int(len))
                        LogError(LOG_MODEM, "Error writing P25 data");

                    m_modem->m_p25Space -= dataLen;
                }
            }
        }
        break;

        default:
            break;
    }

    if (rspType == RTM_OK && len > 0) {
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
            uint16_t max = buffer[3U] << 8 | buffer[4U];
            uint16_t min = buffer[5U] << 8 | buffer[6U];
            uint16_t ave = buffer[7U] << 8 | buffer[8U];
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

        case CMD_NXDN_DATA:
            processNXDNBER(buffer + 3U);
            break;

        case CMD_NXDN_LOST:
        {
            LogMessage(LOG_CAL, "NXDN Transmission lost, total frames: %d, bits: %d, uncorrectable frames: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berUndecodableLC, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));

            if (m_nxdnEnabled) {
                m_berBits = 0U;
                m_berErrs = 0U;
                m_berFrames = 0U;
                m_berUndecodableLC = 0U;
                m_berUncorrectable = 0U;
            }
        }
        break;

        case CMD_GET_STATUS:
        {
            m_isHotspot = (buffer[3U] & 0x01U) == 0x01U;

            uint8_t modemState = buffer[4U];

            bool tx = (buffer[5U] & 0x01U) == 0x01U;

            bool adcOverflow = (buffer[5U] & 0x02U) == 0x02U;
            bool rxOverflow = (buffer[5U] & 0x04U) == 0x04U;
            bool txOverflow = (buffer[5U] & 0x08U) == 0x08U;
            bool dacOverflow = (buffer[5U] & 0x20U) == 0x20U;

            // spaces from the modem are returned in "logical" frame count, not raw byte size
            m_modem->m_dmrSpace1 = buffer[7U] * (dmr::DMR_FRAME_LENGTH_BYTES + 2U);
            m_modem->m_dmrSpace2 = buffer[8U] * (dmr::DMR_FRAME_LENGTH_BYTES + 2U);
            m_modem->m_p25Space = buffer[10U] * (p25::P25_LDU_FRAME_LENGTH_BYTES);
            m_modem->m_nxdnSpace = buffer[11U] * (nxdn::NXDN_FRAME_LENGTH_BYTES);

            if (m_hasFetchedStatus && m_requestedStatus) {
                LogMessage(LOG_CAL, " - Diagnostic Values [Modem State: %u, Transmitting: %d, ADC Overflow: %d, Rx Overflow: %d, Tx Overflow: %d, DAC Overflow: %d, HS: %u]",
                    modemState, tx, adcOverflow, rxOverflow, txOverflow, dacOverflow, m_isHotspot);
            }

            m_hasFetchedStatus = true;
            m_requestedStatus = false;
        }
        break;

        case CMD_FLSH_READ:
        {
            uint8_t len = buffer[1U];
            if (m_debug) {
                Utils::dump(1U, "Modem Flash Contents", buffer + 3U, len - 3U);
            }
            if (len == 249U) {
                bool ret = edac::CRC::checkCCITT162(buffer + 3U, DVM_CONF_AREA_LEN);
                if (!ret) {
                    LogWarning(LOG_CAL, "HostCal::portModemHandler(), clearing modem configuration area; first setup?");
                    eraseFlash();
                }
                else {
                    bool isErased = (buffer[DVM_CONF_AREA_LEN] & 0x80U) == 0x80U;
                    uint8_t confAreaVersion = buffer[DVM_CONF_AREA_LEN] & 0x7FU;

                    if (!isErased) {
                        if (confAreaVersion != DVM_CONF_AREA_VER) {
                            LogError(LOG_MODEM, "HostCal::portModemHandler(), invalid version for configuration area, %02X != %02X", DVM_CONF_AREA_VER, confAreaVersion);
                        }
                        else {
                            processFlashConfig(buffer);

                            // reset update config flag if its set
                            if (m_updateConfigFromModem) {
                                m_updateConfigFromModem = false;
                            }
                        }
                    }
                    else {
                        LogWarning(LOG_MODEM, "HostCal::portModemHandler(), modem configuration area was erased and does not contain active configuration!");
                    }
                }
            }
            else {
                LogWarning(LOG_MODEM, "Incorrect length for configuration area! Ignoring.");
            }
        }
        break;

        case CMD_GET_VERSION:
        case CMD_ACK:
            break;

        case CMD_NAK:
            LogWarning(LOG_CAL, "NAK, command = 0x%02X, reason = %u", buffer[3U], buffer[4U]);
            break;

        case CMD_DEBUG1:
        case CMD_DEBUG2:
        case CMD_DEBUG3:
        case CMD_DEBUG4:
        case CMD_DEBUG5:
        case CMD_DEBUG_DUMP:
            m_modem->printDebug(buffer, len);
            break;

        default:
            LogWarning(LOG_CAL, "Unknown message, type = %02X", buffer[2U]);
            Utils::dump("Buffer dump", buffer, len);
            break;
        }
    }

    // handled modem response
    return true;
}

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
    LogMessage(LOG_CAL, "Mode Commands:");
    LogMessage(LOG_CAL, "    Z        %s", DMR_CAL_STR);
    LogMessage(LOG_CAL, "    z        %s", P25_CAL_STR);
    LogMessage(LOG_CAL, "    L        %s", DMR_LF_CAL_STR);
    LogMessage(LOG_CAL, "    M        %s", DMR_CAL_1K_STR);
    LogMessage(LOG_CAL, "    m        %s", DMR_DMO_CAL_1K_STR);
    LogMessage(LOG_CAL, "    P        %s", P25_CAL_1K_STR);
    LogMessage(LOG_CAL, "    l        %s", P25_TDU_TEST_STR);
    LogMessage(LOG_CAL, "    N        %s", NXDN_CAL_1K_STR);
    LogMessage(LOG_CAL, "    B        %s", DMR_FEC_STR);
    LogMessage(LOG_CAL, "    J        %s", DMR_FEC_1K_STR);
    LogMessage(LOG_CAL, "    b        %s", P25_FEC_STR);
    LogMessage(LOG_CAL, "    j        %s", P25_FEC_1K_STR);
    LogMessage(LOG_CAL, "    n        %s", NXDN_FEC_STR);
    LogMessage(LOG_CAL, "    x        %s", RSSI_CAL_STR);
    LogMessage(LOG_CAL, "Engineering Commands:");
    if (!m_isHotspot) {
        LogMessage(LOG_CAL, "    -/=      Inc/Dec DMR +/- 3 Symbol Level");
        LogMessage(LOG_CAL, "    _/+      Inc/Dec DMR +/- 1 Symbol Level");
        LogMessage(LOG_CAL, "    [/]      Inc/Dec P25 +/- 3 Symbol Level");
        LogMessage(LOG_CAL, "    {/}      Inc/Dec P25 +/- 1 Symbol Level");
        LogMessage(LOG_CAL, "    ;/'      Inc/Dec NXDN +/- 3 Symbol Level");
        LogMessage(LOG_CAL, "    :/\"      Inc/Dec NXDN +/- 1 Symbol Level");
    }
    else {
        LogMessage(LOG_CAL, "    1        Set DMR Disc. Bandwidth Offset");
        LogMessage(LOG_CAL, "    2        Set P25 Disc. Bandwidth Offset");
        LogMessage(LOG_CAL, "    3        Set NXDN Disc. Bandwidth Offset");
        LogMessage(LOG_CAL, "    4        Set DMR Post Demod Bandwidth Offset");
        LogMessage(LOG_CAL, "    5        Set P25 Post Demod Bandwidth Offset");
        LogMessage(LOG_CAL, "    6        Set NXDN Post Demod Bandwidth Offset");
        LogMessage(LOG_CAL, "    7        Set ADF7021 Rx Auto. Gain Mode");
        LogMessage(LOG_CAL, "    8        Set ADF7021 AFC Settings");
    }
    if (!m_modem->m_flashDisabled) {
        LogMessage(LOG_CAL, "    E        Erase modem configuration area");
        LogMessage(LOG_CAL, "    e        Read modem configuration area");
    }
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
/// Helper to change the DMR Symbol Level 3 adjust.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setDMRSymLevel3Adj(int incr)
{
    if (incr > 0 && m_modem->m_dmrSymLevel3Adj < 127) {
        m_modem->m_dmrSymLevel3Adj++;
        LogMessage(LOG_CAL, " - DMR Symbol Level +/- 3 Adjust: %d", m_modem->m_dmrSymLevel3Adj);
        return writeSymbolAdjust();
    }

    if (incr < 0 && m_modem->m_dmrSymLevel3Adj > -127) {
        m_modem->m_dmrSymLevel3Adj--;
        LogMessage(LOG_CAL, " - DMR Symbol Level +/- 3 Adjust: %d", m_modem->m_dmrSymLevel3Adj);
        return writeSymbolAdjust();
    }

    return true;
}

/// <summary>
/// Helper to change the DMR Symbol Level 1 adjust.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setDMRSymLevel1Adj(int incr)
{
    if (incr > 0 && m_modem->m_dmrSymLevel1Adj < 127) {
        m_modem->m_dmrSymLevel1Adj++;
        LogMessage(LOG_CAL, " - DMR Symbol Level +/- 1 Adjust: %d", m_modem->m_dmrSymLevel1Adj);
        return writeSymbolAdjust();
    }

    if (incr < 0 && m_modem->m_dmrSymLevel1Adj > -127) {
        m_modem->m_dmrSymLevel1Adj--;
        LogMessage(LOG_CAL, " - DMR Symbol Level +/- 1 Adjust: %d", m_modem->m_dmrSymLevel1Adj);
        return writeSymbolAdjust();
    }

    return true;
}

/// <summary>
/// Helper to change the P25 Symbol Level 3 adjust.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setP25SymLevel3Adj(int incr)
{
    if (incr > 0 && m_modem->m_p25SymLevel3Adj < 127) {
        m_modem->m_p25SymLevel3Adj++;
        LogMessage(LOG_CAL, " - P25 Symbol Level +/- 3 Adjust: %d", m_modem->m_p25SymLevel3Adj);
        return writeSymbolAdjust();
    }

    if (incr < 0 && m_modem->m_p25SymLevel3Adj > -127) {
        m_modem->m_p25SymLevel3Adj--;
        LogMessage(LOG_CAL, " - P25 Symbol Level +/- 3 Adjust: %d", m_modem->m_p25SymLevel3Adj);
        return writeSymbolAdjust();
    }

    return true;
}

/// <summary>
/// Helper to change the P25 Symbol Level 1 adjust.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setP25SymLevel1Adj(int incr)
{
    if (incr > 0 && m_modem->m_p25SymLevel1Adj < 127) {
        m_modem->m_p25SymLevel1Adj++;
        LogMessage(LOG_CAL, " - P25 Symbol Level +/- 1 Adjust: %d", m_modem->m_p25SymLevel1Adj);
        return writeSymbolAdjust();
    }

    if (incr < 0 && m_modem->m_p25SymLevel1Adj > -127) {
        m_modem->m_p25SymLevel1Adj--;
        LogMessage(LOG_CAL, " - P25 Symbol Level +/- 1 Adjust: %d", m_modem->m_p25SymLevel1Adj);
        return writeSymbolAdjust();
    }

    return true;
}

/// <summary>
/// Helper to change the NXDN Symbol Level 3 adjust.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setNXDNSymLevel3Adj(int incr)
{
    if (incr > 0 && m_modem->m_nxdnSymLevel3Adj < 127) {
        m_modem->m_nxdnSymLevel3Adj++;
        LogMessage(LOG_CAL, " - NXDN Symbol Level +/- 3 Adjust: %d", m_modem->m_nxdnSymLevel3Adj);
        return writeSymbolAdjust();
    }

    if (incr < 0 && m_modem->m_nxdnSymLevel3Adj > -127) {
        m_modem->m_nxdnSymLevel3Adj--;
        LogMessage(LOG_CAL, " - NXDN Symbol Level +/- 3 Adjust: %d", m_modem->m_nxdnSymLevel3Adj);
        return writeSymbolAdjust();
    }

    return true;
}

/// <summary>
/// Helper to change the NXDN Symbol Level 1 adjust.
/// </summary>
/// <param name="incr">Amount to change.</param>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setNXDNSymLevel1Adj(int incr)
{
    if (incr > 0 && m_modem->m_nxdnSymLevel1Adj < 127) {
        m_modem->m_nxdnSymLevel1Adj++;
        LogMessage(LOG_CAL, " - NXDN Symbol Level +/- 1 Adjust: %d", m_modem->m_nxdnSymLevel1Adj);
        return writeSymbolAdjust();
    }

    if (incr < 0 && m_modem->m_nxdnSymLevel1Adj > -127) {
        m_modem->m_nxdnSymLevel1Adj--;
        LogMessage(LOG_CAL, " - NXDN Symbol Level +/- 1 Adjust: %d", m_modem->m_nxdnSymLevel1Adj);
        return writeSymbolAdjust();
    }

    return true;
}

/// <summary>
/// Helper to toggle modem transmit mode.
/// </summary>
/// <returns>True, if setting was applied, otherwise false.</returns>
bool HostCal::setTransmit()
{
    if (m_dmrEnabled || (m_p25Enabled && !m_p25TduTest) || m_nxdnEnabled) {
        LogError(LOG_CAL, "No transmit allowed in a BER Test mode");
        return false;
    }

    m_transmit = !m_transmit;

    if (m_p25Enabled && m_p25TduTest) {
        if (m_transmit)
            LogMessage(LOG_CAL, " - Modem start transmitting");
        else
            LogMessage(LOG_CAL, " - Modem stop transmitting");

        m_modem->clock(0U);
        return true;
    }

    uint8_t buffer[50U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_CAL_DATA;
    buffer[3U] = m_transmit ? 0x01U : 0x00U;

    int ret = m_modem->write(buffer, 4U);
    if (ret <= 0)
        return false;

    sleep(25U);

    if (m_transmit)
        LogMessage(LOG_CAL, " - Modem start transmitting");
    else
        LogMessage(LOG_CAL, " - Modem stop transmitting");

    m_modem->clock(0U);

    return true;
}

/// <summary>
/// Process DMR Rx BER.
/// </summary>
/// <param name="buffer">Buffer containing DMR data</param>
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
/// Process DMR Rx 1011hz BER.
/// </summary>
/// <param name="buffer">Buffer containing DMR data</param>
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
/// <param name="buffer">Buffer containing P25 data</param>
void HostCal::processP25BER(const uint8_t* buffer)
{
    using namespace p25;

    uint8_t sync[P25_SYNC_LENGTH_BYTES];
    ::memcpy(sync, buffer + 1U, P25_SYNC_LENGTH_BYTES);

    uint8_t syncErrs = 0U;
    for (uint8_t i = 0U; i < P25_SYNC_LENGTH_BYTES; i++)
        syncErrs += Utils::countBits8(sync[i] ^ P25_SYNC_BYTES[i]);

    LogMessage(LOG_CAL, "P25, sync word, errs = %u, sync word = %02X %02X %02X %02X %02X %02X", syncErrs,
        sync[0U], sync[1U], sync[2U], sync[3U], sync[4U], sync[5U]);

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
            LogWarning(LOG_CAL, P25_HDU_STR ", undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, P25_HDU_STR ", dstId = %u, algo = %X, kid = %X", lc.getDstId(), lc.getAlgId(), lc.getKId());
        }

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
    }
    else if (duid == P25_DUID_TDU) {
        if (m_berFrames != 0U) {
            LogMessage(LOG_CAL, P25_TDU_STR ", total frames: %d, bits: %d, uncorrectable frames: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berUndecodableLC, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));
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
            LogWarning(LOG_CAL, P25_LDU1_STR ", undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, P25_LDU1_STR " LC, mfId = $%02X, lco = $%02X, emerg = %u, encrypt = %u, prio = %u, group = %u, srcId = %u, dstId = %u",
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
            LogMessage(LOG_CAL, P25_LDU1_STR ", audio FEC BER (errs): %.3f%% (%u/1233)", ber, errs);
        else {
            LogWarning(LOG_CAL, P25_LDU1_STR ", uncorrectable audio");
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
            LogWarning(LOG_CAL, P25_LDU2_STR ", undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, P25_LDU2_STR " LC, mfId = $%02X, algo = %X, kid = %X",
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
            LogMessage(LOG_CAL, P25_LDU2_STR ", audio FEC BER (errs): %.3f%% (%u/1233)", ber, errs);
        else {
            LogWarning(LOG_CAL, P25_LDU2_STR ", uncorrectable audio");
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
            LogWarning(LOG_RF, P25_PDU_STR ", unfixable RF 1/2 rate header data");
            Utils::dump(1U, "Unfixable PDU Data", rfPDU + P25_SYNC_LENGTH_BYTES + P25_NID_LENGTH_BYTES, P25_PDU_HEADER_LENGTH_BYTES);
        }
        else {
            LogMessage(LOG_CAL, P25_PDU_STR ", ack = %u, outbound = %u, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padCount = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u",
                dataHeader.getAckNeeded(), dataHeader.getOutbound(), dataHeader.getFormat(), dataHeader.getMFId(), dataHeader.getSAP(), dataHeader.getFullMessage(),
                dataHeader.getBlocksToFollow(), dataHeader.getPadCount(), dataHeader.getNs(), dataHeader.getFSN(), dataHeader.getLastFragment(),
                dataHeader.getHeaderOffset());
        }

        delete[] rfPDU;
    }
    else if (duid == P25_DUID_TSDU) {
        timerStop();

        std::unique_ptr<lc::TSBK> tsbk = lc::tsbk::TSBKFactory::createTSBK(buffer + 1U);

        Utils::dump(1U, "Raw TSBK Dump", buffer + 1U, P25_TSDU_FRAME_LENGTH_BYTES);

        if (tsbk == nullptr) {
            LogWarning(LOG_CAL, P25_TSDU_STR ", undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, P25_TSDU_STR ", mfId = $%02X, lco = $%02X, srcId = %u, dstId = %u, service = %u, netId = %u, sysId = %u",
                tsbk->getMFId(), tsbk->getLCO(), tsbk->getSrcId(), tsbk->getDstId(), tsbk->getService(), tsbk->getNetId(), tsbk->getSysId());
        }
    }
}

/// <summary>
/// Process P25 Rx 1011hz BER.
/// </summary>
/// <param name="buffer">Buffer containing P25 data</param>
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
            LogWarning(LOG_CAL, P25_HDU_STR ", undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_RF, P25_HDU_STR ", dstId = %u, algo = %X, kid = %X", lc.getDstId(), lc.getAlgId(), lc.getKId());
        }

        m_berBits = 0U;
        m_berErrs = 0U;
        m_berFrames = 0U;
        m_berUndecodableLC = 0U;
        m_berUncorrectable = 0U;
    }
    else if (duid == P25_DUID_TDU) {
        if (m_berFrames != 0U) {
            LogMessage(LOG_CAL, P25_TDU_STR ", total frames: %d, bits: %d, uncorrectable frames: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", m_berFrames, m_berBits, m_berUncorrectable, m_berUndecodableLC, m_berErrs, float(m_berErrs * 100U) / float(m_berBits));
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
            LogWarning(LOG_CAL, P25_LDU1_STR ", undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, P25_LDU1_STR " LC, mfId = $%02X, lco = $%02X, emerg = %u, encrypt = %u, prio = %u, group = %u, srcId = %u, dstId = %u",
                lc.getMFId(), lc.getLCO(), lc.getEmergency(), lc.getEncrypted(), lc.getPriority(), lc.getGroup(), lc.getSrcId(), lc.getDstId());
        }

        for (uint32_t i = 0U; i < 216U; i++)
            errs += countErrs(buffer[i + 1U], LDU1_1K[i]);

        float ber = float(errs) / 12.33F;
        if (ber < 10.0F)
            LogMessage(LOG_CAL, P25_LDU1_STR ", 1011 Test Pattern BER (errs): %.3f%% (%u/1233)", ber, errs);
        else {
            LogWarning(LOG_CAL, P25_LDU1_STR ", uncorrectable audio");
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
            LogWarning(LOG_CAL, P25_LDU2_STR ", undecodable LC");
            m_berUndecodableLC++;
        }
        else {
            LogMessage(LOG_CAL, P25_LDU2_STR " LC, mfId = $%02X, algo = %X, kid = %X",
                lc.getMFId(), lc.getAlgId(), lc.getKId());
        }

        for (uint32_t i = 0U; i < 216U; i++)
            errs += countErrs(buffer[i + 1U], LDU2_1K[i]);

        float ber = float(errs) / 12.33F;
        if (ber < 10.0F)
            LogMessage(LOG_CAL, P25_LDU2_STR ", 1011 Test Pattern BER (errs): %.3f%% (%u/1233)", ber, errs);
        else {
            LogWarning(LOG_CAL, P25_LDU2_STR ", uncorrectable audio");
            m_berUncorrectable++;
        }

        m_berBits += 1233U;
        m_berErrs += errs;
        m_berFrames++;
    }
}

/// <summary>
/// Process NXDN Rx BER.
/// </summary>
/// <param name="buffer">Buffer containing NXDN data</param>
void HostCal::processNXDNBER(const uint8_t* buffer)
{
    using namespace nxdn;

    unsigned char data[NXDN_FRAME_LENGTH_BYTES];
    ::memcpy(data, buffer, NXDN_FRAME_LENGTH_BYTES);
    NXDNUtils::scrambler(data);

    channel::LICH lich;
    bool valid = lich.decode(data);

    if (valid) {
        uint8_t usc = lich.getFCT();
        uint8_t opt = lich.getOption();

        if (usc == NXDN_LICH_USC_SACCH_NS) {
            if (m_berFrames == 0U) {
                LogMessage(LOG_CAL, "NXDN VCALL (Voice Call), 1031 Test Pattern Start");

                timerStart();
                m_berErrs = 0U;
                m_berBits = 0U;
                m_berFrames = 0U;
                return;
            } else {
                float ber = float(m_berErrs * 100U) / float(m_berBits);
                LogMessage(LOG_CAL, "NXDN TX_REL (Transmission Release), 1031 Test Pattern BER, frames: %u, errs: %.3f%% (%u/%u)", m_berFrames, ber, m_berErrs, m_berBits);

                timerStop();
                m_berErrs = 0U;
                m_berBits = 0U;
                m_berFrames = 0U;
                return;
            }
        } else if (opt == NXDN_LICH_STEAL_NONE) {
            timerStart();

            uint32_t errors = 0U;

            errors += m_fec.regenerateNXDN(data + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 0U);
            errors += m_fec.regenerateNXDN(data + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 9U);
            errors += m_fec.regenerateNXDN(data + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 18U);
            errors += m_fec.regenerateNXDN(data + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 27U);

            m_berBits += 188U;
            m_berErrs += errors;
            m_berFrames++;

            float ber = float(errors) / 1.88F;
            LogMessage(LOG_CAL, "NXDN VCALL (Voice Call), 1031 Test Pattern BER, (errs): %.3f%% (%u/188)", ber, errors);
        }
    }
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
    if (m_isHotspot && m_transmit) {
        setTransmit();
        sleep(25U);
    }

    uint8_t buffer[25U];
    ::memset(buffer, 0x00U, 25U);
    uint8_t lengthToWrite = 17U;

    buffer[0U] = DVM_FRAME_START;
    buffer[2U] = CMD_SET_CONFIG;

    buffer[3U] = 0x00U;
    m_conf["system"]["modem"]["rxInvert"] = __BOOL_STR(m_modem->m_rxInvert);
    if (m_modem->m_rxInvert)
        buffer[3U] |= 0x01U;
    m_conf["system"]["modem"]["txInvert"] = __BOOL_STR(m_modem->m_txInvert);
    if (m_modem->m_txInvert)
        buffer[3U] |= 0x02U;
    m_conf["system"]["modem"]["pttInvert"] = __BOOL_STR(m_modem->m_pttInvert);
    if (m_modem->m_pttInvert)
        buffer[3U] |= 0x04U;
    if (m_debug)
        buffer[3U] |= 0x10U;
    if (!m_duplex)
        buffer[3U] |= 0x80U;

    buffer[4U] = 0x00U;
    m_conf["system"]["modem"]["dcBlocker"] = __BOOL_STR(m_modem->m_dcBlocker);
    if (m_modem->m_dcBlocker)
        buffer[4U] |= 0x01U;

    if (m_dmrEnabled)
        buffer[4U] |= 0x02U;
    if (m_p25Enabled)
        buffer[4U] |= 0x08U;

    if (m_modem->m_fdmaPreamble > MAX_FDMA_PREAMBLE) {
        LogWarning(LOG_P25, "oversized FDMA preamble count, reducing to maximum %u", MAX_FDMA_PREAMBLE);
        m_modem->m_fdmaPreamble = MAX_FDMA_PREAMBLE;
    }

    m_conf["system"]["modem"]["fdmaPreamble"] = __INT_STR(m_modem->m_fdmaPreamble);
    buffer[5U] = m_modem->m_fdmaPreamble;

    buffer[6U] = modeOverride;

    m_conf["system"]["modem"]["rxLevel"] = __FLOAT_STR(m_modem->m_rxLevel);
    buffer[7U] = (uint8_t)(m_modem->m_rxLevel * 2.55F + 0.5F);

    m_conf["system"]["modem"]["txLevel"] = __FLOAT_STR(m_modem->m_cwIdTXLevel);
    buffer[8U] = (uint8_t)(m_modem->m_cwIdTXLevel * 2.55F + 0.5F);

    buffer[9U] = 1U;

    m_conf["system"]["modem"]["dmrRxDelay"] = __INT_STR(m_modem->m_dmrRxDelay);
    buffer[10U] = m_modem->m_dmrRxDelay;

    uint32_t nac = 0xF7EU;
    buffer[11U] = (nac >> 4) & 0xFFU;
    buffer[12U] = (nac << 4) & 0xF0U;

    buffer[13U] = (uint8_t)(m_modem->m_cwIdTXLevel * 2.55F + 0.5F);
    buffer[15U] = (uint8_t)(m_modem->m_cwIdTXLevel * 2.55F + 0.5F);

    m_conf["system"]["modem"]["txDCOffset"] = __INT_STR(m_modem->m_txDCOffset);
    buffer[16U] = (uint8_t)(m_modem->m_txDCOffset + 128);
    m_conf["system"]["modem"]["rxDCOffset"] = __INT_STR(m_modem->m_rxDCOffset);
    buffer[17U] = (uint8_t)(m_modem->m_rxDCOffset + 128);

    m_conf["system"]["modem"]["p25CorrCount"] = __INT_STR(m_modem->m_p25CorrCount);
    buffer[14U] = (uint8_t)m_modem->m_p25CorrCount;

    // are we on a protocol version 3 firmware?
    if (m_modem->getVersion() >= 3U) {
        lengthToWrite = 24U;

        if (m_nxdnEnabled)
            buffer[4U] |= 0x10U;

        buffer[18U] = (uint8_t)(m_modem->m_cwIdTXLevel * 2.55F + 0.5F);

        buffer[19U] = m_modem->m_rxCoarsePot;
        buffer[20U] = m_modem->m_rxFinePot;
        buffer[21U] = m_modem->m_txCoarsePot;
        buffer[22U] = m_modem->m_txFinePot;
        buffer[23U] = m_modem->m_rssiCoarsePot;
        buffer[24U] = m_modem->m_rssiFinePot;
    }

    buffer[1U] = lengthToWrite;

    int ret = m_modem->write(buffer, lengthToWrite);
    if (ret != lengthToWrite)
        return false;

    sleep(10U);

    m_modem->clock(0U);
    return true;
}

/// <summary>
/// Write RF parameters to the air interface modem.
/// </summary>
/// <returns></returns>
bool HostCal::writeRFParams()
{
    uint8_t buffer[22U];
    ::memset(buffer, 0x00U, 22U);
    uint8_t lengthToWrite = 18U;

    buffer[0U] = DVM_FRAME_START;
    buffer[2U] = CMD_SET_RFPARAMS;

    buffer[3U] = 0x00U;

    buffer[4U] = (m_rxAdjustedFreq >> 0) & 0xFFU;
    buffer[5U] = (m_rxAdjustedFreq >> 8) & 0xFFU;
    buffer[6U] = (m_rxAdjustedFreq >> 16) & 0xFFU;
    buffer[7U] = (m_rxAdjustedFreq >> 24) & 0xFFU;

    buffer[8U] = (m_txAdjustedFreq >> 0) & 0xFFU;
    buffer[9U] = (m_txAdjustedFreq >> 8) & 0xFFU;
    buffer[10U] = (m_txAdjustedFreq >> 16) & 0xFFU;
    buffer[11U] = (m_txAdjustedFreq >> 24) & 0xFFU;

    buffer[12U] = (unsigned char)(100 * 2.55F + 0.5F); // cal sets power fixed to 100

    m_conf["system"]["modem"]["hotspot"]["dmrDiscBWAdj"] = __INT_STR(m_modem->m_dmrDiscBWAdj);
    buffer[13U] = (uint8_t)(m_modem->m_dmrDiscBWAdj + 128);
    m_conf["system"]["modem"]["hotspot"]["p25DiscBWAdj"] = __INT_STR(m_modem->m_p25DiscBWAdj);
    buffer[14U] = (uint8_t)(m_modem->m_p25DiscBWAdj + 128);
    m_conf["system"]["modem"]["hotspot"]["dmrPostBWAdj"] = __INT_STR(m_modem->m_dmrPostBWAdj);
    buffer[15U] = (uint8_t)(m_modem->m_dmrPostBWAdj + 128);
    m_conf["system"]["modem"]["hotspot"]["p25PostBWAdj"] = __INT_STR(m_modem->m_p25PostBWAdj);
    buffer[16U] = (uint8_t)(m_modem->m_p25PostBWAdj + 128);

    m_conf["system"]["modem"]["hotspot"]["adfGainMode"] = __INT_STR((int)m_modem->m_adfGainMode);
    buffer[17U] = (uint8_t)m_modem->m_adfGainMode;

    // are we on a protocol version 3 firmware?
    if (m_modem->getVersion() >= 3U) {
        lengthToWrite = 22U;

        m_conf["system"]["modem"]["hotspot"]["nxdnDiscBWAdj"] = __INT_STR(m_modem->m_nxdnDiscBWAdj);
        buffer[18U] = (uint8_t)(m_modem->m_nxdnDiscBWAdj + 128);
        m_conf["system"]["modem"]["hotspot"]["nxdnPostBWAdj"] = __INT_STR(m_modem->m_nxdnPostBWAdj);
        buffer[19U] = (uint8_t)(m_modem->m_nxdnPostBWAdj + 128);

        // support optional AFC parameters
        m_conf["system"]["modem"]["hotspot"]["afcEnable"] = __BOOL_STR(m_modem->m_afcEnable);
        m_conf["system"]["modem"]["hotspot"]["afcKI"] = __INT_STR(m_modem->m_afcKI);
        m_conf["system"]["modem"]["hotspot"]["afcKP"] = __INT_STR(m_modem->m_afcKP);
        buffer[20U] = (m_modem->m_afcEnable ? 0x80 : 0x00) +
            (m_modem->m_afcKP << 4) + (m_modem->m_afcKI);
        m_conf["system"]["modem"]["hotspot"]["afcRange"] = __INT_STR(m_modem->m_afcRange);
        buffer[21U] = m_modem->m_afcRange;
    }

    buffer[1U] = lengthToWrite;

    int ret = m_modem->write(buffer, lengthToWrite);
    if (ret <= 0)
        return false;

    sleep(10U);

    m_modem->clock(0U);
    return true;
}

/// <summary>
/// Write symbol level adjustments to the modem DSP.
/// </summary>
/// <returns>True, if level adjustments are written, otherwise false.</returns>
bool HostCal::writeSymbolAdjust()
{
    uint8_t buffer[20U];
    ::memset(buffer, 0x00U, 20U);
    uint8_t lengthToWrite = 7U;

    buffer[0U] = DVM_FRAME_START;
    buffer[2U] = CMD_SET_SYMLVLADJ;

    m_conf["system"]["modem"]["repeater"]["dmrSymLvl3Adj"] = __INT_STR(m_modem->m_dmrSymLevel3Adj);
    buffer[3U] = (uint8_t)(m_modem->m_dmrSymLevel3Adj + 128);
    m_conf["system"]["modem"]["repeater"]["dmrSymLvl1Adj"] = __INT_STR(m_modem->m_dmrSymLevel1Adj);
    buffer[4U] = (uint8_t)(m_modem->m_dmrSymLevel1Adj + 128);

    m_conf["system"]["modem"]["repeater"]["p25SymLvl3Adj"] = __INT_STR(m_modem->m_p25SymLevel3Adj);
    buffer[5U] = (uint8_t)(m_modem->m_p25SymLevel3Adj + 128);
    m_conf["system"]["modem"]["repeater"]["p25SymLvl1Adj"] = __INT_STR(m_modem->m_p25SymLevel1Adj);
    buffer[6U] = (uint8_t)(m_modem->m_p25SymLevel1Adj + 128);

    // are we on a protocol version 3 firmware?
    if (m_modem->getVersion() >= 3U) {
        lengthToWrite = 9U;

        m_conf["system"]["modem"]["repeater"]["nxdnSymLvl3Adj"] = __INT_STR(m_modem->m_nxdnSymLevel3Adj);
        buffer[7U] = (uint8_t)(m_modem->m_nxdnSymLevel3Adj + 128);
        m_conf["system"]["modem"]["repeater"]["nxdnSymLvl1Adj"] = __INT_STR(m_modem->m_nxdnSymLevel1Adj);
        buffer[8U] = (uint8_t)(m_modem->m_nxdnSymLevel1Adj + 128);
    }

    buffer[1U] = lengthToWrite;

    int ret = m_modem->write(buffer, lengthToWrite);
    if (ret <= 0)
        return false;

    sleep(10U);

    m_modem->clock(0U);
    return true;
}

/// <summary>
/// Helper to sleep the calibration thread.
/// </summary>
/// <param name="ms">Milliseconds to sleep.</param>
void HostCal::sleep(uint32_t ms)
{
#if defined(_WIN32) || defined(_WIN64)
    ::Sleep(ms);
#else
    ::usleep(ms * 1000);
#endif
}

/// <summary>
/// Read the configuration area on the air interface modem.
/// </summary>
bool HostCal::readFlash()
{
    if (m_modem->m_flashDisabled) {
        return false;
    }

    uint8_t buffer[3U];
    ::memset(buffer, 0x00U, 3U);

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 3U;
    buffer[2U] = CMD_FLSH_READ;

    int ret = m_modem->write(buffer, 3U);
    if (ret <= 0)
        return false;

    sleep(1000U);

    m_modem->clock(0U);
    return true;
}

/// <summary>
/// Process the configuration data from the air interface modem.
/// </summary>
/// <param name="buffer"></param>
void HostCal::processFlashConfig(const uint8_t *buffer)
{
    if (m_updateConfigFromModem) {
        LogMessage(LOG_CAL, " - Restoring local configuration from configuration area on modem");

        // general config
        m_modem->m_rxInvert = (buffer[3U] & 0x01U) == 0x01U;
        m_conf["system"]["modem"]["rxInvert"] = __BOOL_STR(m_modem->m_rxInvert);
        m_modem->m_txInvert = (buffer[3U] & 0x02U) == 0x02U;
        m_conf["system"]["modem"]["txInvert"] = __BOOL_STR(m_modem->m_txInvert);
        m_modem->m_pttInvert = (buffer[3U] & 0x04U) == 0x04U;
        m_conf["system"]["modem"]["pttInvert"] = __BOOL_STR(m_modem->m_pttInvert);

        m_modem->m_dcBlocker = (buffer[4U] & 0x01U) == 0x01U;
        m_conf["system"]["modem"]["dcBlocker"] = __BOOL_STR(m_modem->m_dcBlocker);

        m_modem->m_fdmaPreamble = buffer[5U];
        m_conf["system"]["modem"]["fdmaPreamble"] = __INT_STR(m_modem->m_fdmaPreamble);

        // levels
        m_modem->m_rxLevel = (float(buffer[7U]) - 0.5F) / 2.55F;
        m_conf["system"]["modem"]["rxLevel"] = __FLOAT_STR(m_modem->m_rxLevel);
        m_modem->m_cwIdTXLevel = (float(buffer[8U]) - 0.5F) / 2.55F;
        m_conf["system"]["modem"]["txLevel"] = __FLOAT_STR(m_modem->m_cwIdTXLevel);

        m_modem->m_dmrRxDelay = buffer[10U];
        m_conf["system"]["modem"]["dmrRxDelay"] = __INT_STR(m_modem->m_dmrRxDelay);

        m_modem->m_p25CorrCount = buffer[11U];
        m_conf["system"]["modem"]["p25CorrCount"] = __INT_STR(m_modem->m_p25CorrCount);

        m_modem->m_txDCOffset = int(buffer[16U]) - 128;
        m_conf["system"]["modem"]["txDCOffset"] = __INT_STR(m_modem->m_txDCOffset);
        m_modem->m_rxDCOffset = int(buffer[17U]) - 128;
        m_conf["system"]["modem"]["rxDCOffset"] = __INT_STR(m_modem->m_rxDCOffset);

        writeConfig();
        sleep(500);

        // symbol adjust
        m_modem->m_dmrSymLevel3Adj = int(buffer[35U]) - 128;
        m_conf["system"]["modem"]["repeater"]["dmrSymLvl3Adj"] = __INT_STR(m_modem->m_dmrSymLevel3Adj);
        m_modem->m_dmrSymLevel1Adj = int(buffer[36U]) - 128;
        m_conf["system"]["modem"]["repeater"]["dmrSymLvl1Adj"] = __INT_STR(m_modem->m_dmrSymLevel1Adj);

        m_modem->m_p25SymLevel3Adj = int(buffer[37U]) - 128;
        m_conf["system"]["modem"]["repeater"]["p25SymLvl3Adj"] = __INT_STR(m_modem->m_p25SymLevel3Adj);
        m_modem->m_p25SymLevel1Adj = int(buffer[38U]) - 128;
        m_conf["system"]["modem"]["repeater"]["p25SymLvl1Adj"] = __INT_STR(m_modem->m_p25SymLevel1Adj);

        // are we on a protocol version 3 firmware?
        if (m_modem->getVersion() >= 3U) {
            m_modem->m_nxdnSymLevel3Adj = int(buffer[41U]) - 128;
            m_conf["system"]["modem"]["repeater"]["nxdnSymLvl3Adj"] = __INT_STR(m_modem->m_nxdnSymLevel3Adj);
            m_modem->m_nxdnSymLevel1Adj = int(buffer[42U]) - 128;
            m_conf["system"]["modem"]["repeater"]["nxdnSymLvl1Adj"] = __INT_STR(m_modem->m_nxdnSymLevel1Adj);
        }

        writeSymbolAdjust();
        sleep(500);

        // RF parameters
        m_modem->m_dmrDiscBWAdj = int8_t(buffer[20U]) - 128;
        m_conf["system"]["modem"]["hotspot"]["dmrDiscBWAdj"] = __INT_STR(m_modem->m_dmrDiscBWAdj);
        m_modem->m_p25DiscBWAdj = int8_t(buffer[21U]) - 128;
        m_conf["system"]["modem"]["hotspot"]["p25DiscBWAdj"] = __INT_STR(m_modem->m_p25DiscBWAdj);
        m_modem->m_dmrPostBWAdj = int8_t(buffer[22U]) - 128;
        m_conf["system"]["modem"]["hotspot"]["dmrPostBWAdj"] = __INT_STR(m_modem->m_dmrPostBWAdj);
        m_modem->m_p25PostBWAdj = int8_t(buffer[23U]) - 128;
        m_conf["system"]["modem"]["hotspot"]["p25PostBWAdj"] = __INT_STR(m_modem->m_p25PostBWAdj);

        m_modem->m_adfGainMode = (ADF_GAIN_MODE)buffer[24U];
        m_conf["system"]["modem"]["hotspot"]["adfGainMode"] = __INT_STR((int)m_modem->m_adfGainMode);

        // are we on a protocol version 3 firmware?
        if (m_modem->getVersion() >= 3U) {
            m_modem->m_nxdnDiscBWAdj = int8_t(buffer[39U]) - 128;
            m_conf["system"]["modem"]["repeater"]["nxdnDiscBWAdj"] = __INT_STR(m_modem->m_nxdnDiscBWAdj);
            m_modem->m_nxdnPostBWAdj = int8_t(buffer[40U]) - 128;
            m_conf["system"]["modem"]["repeater"]["nxdnPostBWAdj"] = __INT_STR(m_modem->m_nxdnPostBWAdj);
        }

        m_modem->m_txTuning = int(buffer[25U]) - 128;
        m_conf["system"]["modem"]["hotspot"]["txTuning"] = __INT_STR(m_modem->m_txTuning);
        m_txAdjustedFreq = m_txFrequency + m_modem->m_txTuning;
        m_modem->m_rxTuning = int(buffer[26U]) - 128;
        m_conf["system"]["modem"]["hotspot"]["rxTuning"] = __INT_STR(m_modem->m_rxTuning);
        m_rxAdjustedFreq = m_rxFrequency + m_modem->m_rxTuning;

        // are we on a protocol version 3 firmware?
        if (m_modem->getVersion() >= 3U) {
            m_modem->m_rxCoarsePot = buffer[43U];
            m_conf["system"]["modem"]["softpot"]["rxCoarse"] = __INT_STR(m_modem->m_rxCoarsePot);
            m_modem->m_rxFinePot = buffer[44U];
            m_conf["system"]["modem"]["softpot"]["rxFine"] = __INT_STR(m_modem->m_rxFinePot);

            m_modem->m_txCoarsePot = buffer[45U];
            m_conf["system"]["modem"]["softpot"]["txCoarse"] = __INT_STR(m_modem->m_txCoarsePot);
            m_modem->m_txFinePot = buffer[46U];
            m_conf["system"]["modem"]["softpot"]["txFine"] = __INT_STR(m_modem->m_txFinePot);

            m_modem->m_rssiCoarsePot = buffer[47U];
            m_conf["system"]["modem"]["softpot"]["rssiCoarse"] = __INT_STR(m_modem->m_rssiCoarsePot);
            m_modem->m_rssiFinePot = buffer[48U];
            m_conf["system"]["modem"]["softpot"]["rssiFine"] = __INT_STR(m_modem->m_rssiFinePot);
        }

        writeRFParams();
        sleep(500);
    }
}

/// <summary>
/// Erase the configuration area on the air interface modem.
/// </summary>
bool HostCal::eraseFlash()
{
    if (m_modem->m_flashDisabled) {
        return false;
    }

    uint8_t buffer[249U];
    ::memset(buffer, 0x00U, 249U);

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 249U;
    buffer[2U] = CMD_FLSH_WRITE;

    // configuration version
    buffer[DVM_CONF_AREA_LEN] = DVM_CONF_AREA_VER & 0x7FU;
    buffer[DVM_CONF_AREA_LEN] |= 0x80U; // flag erased
    edac::CRC::addCCITT162(buffer + 3U, DVM_CONF_AREA_LEN);

    int ret = m_modem->write(buffer, 249U);
    if (ret <= 0)
        return false;

    sleep(1000U);

    m_updateConfigFromModem = false;
    LogMessage(LOG_CAL, " - Erased configuration area on modem");

    m_modem->clock(0U);
    return true;
}

/// <summary>
/// Write the configuration area on the air interface modem.
/// </summary>
bool HostCal::writeFlash()
{
    if (m_modem->m_flashDisabled) {
        return false;
    }

    uint8_t buffer[249U];
    ::memset(buffer, 0x00U, 249U);

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 249U;
    buffer[2U] = CMD_FLSH_WRITE;

    // general config
    buffer[3U] = 0x00U;
    if (m_modem->m_rxInvert)
        buffer[3U] |= 0x01U;
    if (m_modem->m_txInvert)
        buffer[3U] |= 0x02U;
    if (m_modem->m_pttInvert)
        buffer[3U] |= 0x04U;

    buffer[4U] = 0x00U;
    if (m_modem->m_dcBlocker)
        buffer[4U] |= 0x01U;

    buffer[5U] = m_modem->m_fdmaPreamble;

    buffer[7U] = (uint8_t)(m_modem->m_rxLevel * 2.55F + 0.5F);
    buffer[8U] = (uint8_t)(m_modem->m_cwIdTXLevel * 2.55F + 0.5F);

    buffer[10U] = m_modem->m_dmrRxDelay;
    buffer[11U] = (uint8_t)m_modem->m_p25CorrCount;

    buffer[13U] = (uint8_t)(m_modem->m_cwIdTXLevel * 2.55F + 0.5F);
    buffer[15U] = (uint8_t)(m_modem->m_cwIdTXLevel * 2.55F + 0.5F);

    buffer[16U] = (uint8_t)(m_modem->m_txDCOffset + 128);
    buffer[17U] = (uint8_t)(m_modem->m_rxDCOffset + 128);

    // RF parameters
    buffer[20U] = (uint8_t)(m_modem->m_dmrDiscBWAdj + 128);
    buffer[21U] = (uint8_t)(m_modem->m_p25DiscBWAdj + 128);
    buffer[22U] = (uint8_t)(m_modem->m_dmrPostBWAdj + 128);
    buffer[23U] = (uint8_t)(m_modem->m_p25PostBWAdj + 128);

    buffer[24U] = (uint8_t)m_modem->m_adfGainMode;

    uint32_t txTuning = (uint32_t)m_modem->m_txTuning;
    __SET_UINT32(txTuning, buffer, 25U);
    uint32_t rxTuning = (uint32_t)m_modem->m_rxTuning;
    __SET_UINT32(rxTuning, buffer, 29U);

    // symbol adjust
    buffer[35U] = (uint8_t)(m_modem->m_dmrSymLevel3Adj + 128);
    buffer[36U] = (uint8_t)(m_modem->m_dmrSymLevel1Adj + 128);

    buffer[37U] = (uint8_t)(m_modem->m_p25SymLevel3Adj + 128);
    buffer[38U] = (uint8_t)(m_modem->m_p25SymLevel1Adj + 128);

    // are we on a protocol version 3 firmware?
    if (m_modem->getVersion() >= 3U) {
        buffer[39U] = (uint8_t)(m_modem->m_nxdnDiscBWAdj + 128);
        buffer[40U] = (uint8_t)(m_modem->m_nxdnPostBWAdj + 128);

        buffer[41U] = (uint8_t)(m_modem->m_nxdnSymLevel3Adj + 128);
        buffer[42U] = (uint8_t)(m_modem->m_nxdnSymLevel1Adj + 128);
    }

    // are we on a protocol version 3 firmware?
    if (m_modem->getVersion() >= 3U) {
        buffer[43U] = m_modem->m_rxCoarsePot;
        buffer[44U] = m_modem->m_rxFinePot;

        buffer[45U] = m_modem->m_txCoarsePot;
        buffer[46U] = m_modem->m_txFinePot;

        buffer[47U] = m_modem->m_rssiCoarsePot;
        buffer[48U] = m_modem->m_rssiFinePot;
    }

    // software signature
    std::string software;
    software.append(__NET_NAME__ " " __VER__ " (built " __BUILD__ ")");
    for (uint8_t i = 0; i < software.length(); i++) {
        buffer[176U + i] = software[i];
    }

    // configuration version
    buffer[DVM_CONF_AREA_LEN] = DVM_CONF_AREA_VER;
    edac::CRC::addCCITT162(buffer + 3U, DVM_CONF_AREA_LEN);

#if DEBUG_MODEM_CAL
    Utils::dump(1U, "HostCal::writeFlash(), Written", buffer, 249U);
#endif

    int ret = m_modem->write(buffer, 249U);
    if (ret <= 0)
        return false;

    sleep(1000U);

    m_updateConfigFromModem = false;

    m_modem->clock(0U);
    return true;
}

/// <summary>
/// Helper to clock the calibration BER timer.
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
/// Helper to start the calibration BER timer.
/// </summary>
void HostCal::timerStart()
{
    if (m_timeout > 0U)
        m_timer = 1U;
}

/// <summary>
/// Helper to stop the calibration BER timer.
/// </summary>
void HostCal::timerStop()
{
    m_timer = 0U;
}

/// <summary>
/// Retrieve the current status from the air interface modem.
/// </summary>
void HostCal::getStatus()
{
    uint8_t buffer[50U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_GET_STATUS;

    int ret = m_modem->write(buffer, 4U);
    if (ret <= 0)
        return;

    sleep(25U);

    m_requestedStatus = true;
    m_modem->clock(0U);
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

/// <summary>
/// Add data frame to the data ring buffer.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="maxFrameSize"></param>
void HostCal::addFrame(const uint8_t* data, uint32_t length, uint32_t maxFrameSize)
{
    assert(data != nullptr);

    uint32_t space = m_queue.freeSpace();
    if (space < (length + 1U)) {
        uint32_t queueLen = m_queue.length();
        m_queue.resize(queueLen + maxFrameSize);
        LogError(LOG_CAL, "overflow in the frame queue while writing data; queue free is %u, needed %u; resized was %u is %u", space, length, queueLen, m_queue.length());
        return;
    }

    uint8_t len = length;
    m_queue.addData(&len, 1U);
    m_queue.addData(data, len);
}

/// <summary>
/// Counts the total number of bit errors between bytes.
/// </summary>
/// <param name="a"></param>
/// <param name="b"></param>
/// <returns></returns>
uint8_t HostCal::countErrs(uint8_t a, uint8_t b)
{
    int cnt = 0;
    uint8_t tmp = a ^ b;
    while (tmp) {
        if (tmp % 2 == 1)
            cnt++;
        tmp /= 2;
    }
    return cnt;
}
