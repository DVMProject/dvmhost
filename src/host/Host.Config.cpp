// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*  Copyright (C) 2017-2025 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "common/network/udp/Socket.h"
#include "modem/port/ModemNullPort.h"
#include "modem/port/UARTPort.h"
#include "modem/port/PseudoPTYPort.h"
#include "modem/port/UDPPort.h"
#include "modem/port/specialized/V24UDPPort.h"
#include "Host.h"
#include "HostMain.h"

using namespace network;
using namespace modem;
using namespace lookups;

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Reads basic configuration parameters from the YAML configuration file. */

bool Host::readParams()
{
    yaml::Node modemConf = m_conf["system"]["modem"];

    yaml::Node modemProtocol = modemConf["protocol"];
    std::string portType = modemProtocol["type"].as<std::string>("null");

    yaml::Node udpProtocol = modemProtocol["udp"];
    std::string udpMode = udpProtocol["mode"].as<std::string>("master");

    bool udpMasterMode = false;
    std::transform(portType.begin(), portType.end(), portType.begin(), ::tolower);
    if ((portType == UART_PORT || portType == PTY_PORT) && g_remoteModemMode) {
        udpMasterMode = true;
    }

    yaml::Node protocolConf = m_conf["protocols"];
    m_dmrEnabled = protocolConf["dmr"]["enable"].as<bool>(false);
    m_p25Enabled = protocolConf["p25"]["enable"].as<bool>(false);
    m_nxdnEnabled = protocolConf["nxdn"]["enable"].as<bool>(false);

    yaml::Node systemConf = m_conf["system"];
    m_duplex = systemConf["duplex"].as<bool>(true);
    bool simplexSameFreq = systemConf["simplexSameFrequency"].as<bool>(false);

    m_timeout = systemConf["timeout"].as<uint32_t>(120U);
    m_rfModeHang = systemConf["rfModeHang"].as<uint32_t>(10U);
    m_rfTalkgroupHang = systemConf["rfTalkgroupHang"].as<uint32_t>(10U);
    m_netModeHang = systemConf["netModeHang"].as<uint32_t>(3U);
    if (!systemConf["modeHang"].isNone()) {
        m_rfModeHang = m_netModeHang = systemConf["modeHang"].as<uint32_t>();
    }

    m_activeTickDelay = (uint8_t)systemConf["activeTickDelay"].as<uint32_t>(5U);
    if (m_activeTickDelay < 1U)
        m_activeTickDelay = 1U;
    m_idleTickDelay = (uint8_t)systemConf["idleTickDelay"].as<uint32_t>(5U);
    if (m_idleTickDelay < 1U)
        m_idleTickDelay = 1U;

    m_identity = systemConf["identity"].as<std::string>();
    m_fixedMode = systemConf["fixedMode"].as<bool>(false);

    if (m_identity.length() > 8) {
        std::string identity = m_identity;
        m_identity = identity.substr(0, 8);

        ::LogWarning(LOG_HOST, "System Identity \"%s\" is too long; truncating to 8 characters, \"%s\".", identity.c_str(), m_identity.c_str());
    }

    int8_t lto = (int8_t)systemConf["localTimeOffset"].as<int32_t>(0);

    m_disableWatchdogOverflow = systemConf["disableWatchdogOverflow"].as<bool>(false);

    LogInfo("General Parameters");
    if (!udpMasterMode) {
        LogInfo("    DMR: %s", m_dmrEnabled ? "enabled" : "disabled");
        LogInfo("    P25: %s", m_p25Enabled ? "enabled" : "disabled");
        LogInfo("    NXDN: %s", m_nxdnEnabled ? "enabled" : "disabled");
        LogInfo("    Duplex: %s", m_duplex ? "yes" : "no");
        if (!m_duplex) {
            LogInfo("    Simplex Same Frequency: %s", simplexSameFreq ? "yes" : "no");
        }
        LogInfo("    Active Tick Delay: %ums", m_activeTickDelay);
        LogInfo("    Idle Tick Delay: %ums", m_idleTickDelay);
        LogInfo("    Timeout: %us", m_timeout);
        LogInfo("    RF Mode Hang: %us", m_rfModeHang);
        LogInfo("    RF Talkgroup Hang: %us", m_rfTalkgroupHang);
        LogInfo("    Net Mode Hang: %us", m_netModeHang);
        LogInfo("    Identity: %s", m_identity.c_str());
        LogInfo("    Fixed Mode: %s", m_fixedMode ? "yes" : "no");
        LogInfo("    Local Time Offset: %dh", lto);
        if (m_disableWatchdogOverflow) {
            LogInfo("    Disable Watchdog Overflow Check: yes");
        }

        yaml::Node systemInfo = systemConf["info"];
        m_latitude = systemInfo["latitude"].as<float>(0.0F);
        m_longitude = systemInfo["longitude"].as<float>(0.0F);
        m_height = systemInfo["height"].as<int>(0);
        m_power = systemInfo["power"].as<uint32_t>(0U);
        m_location = systemInfo["location"].as<std::string>();

        LogInfo("System Info Parameters");
        LogInfo("    Latitude: %fdeg N", m_latitude);
        LogInfo("    Longitude: %fdeg E", m_longitude);
        LogInfo("    Height: %um", m_height);
        LogInfo("    Power: %uW", m_power);
        LogInfo("    Location: \"%s\"", m_location.c_str());

        // try to load bandplan identity table
        std::string idenLookupFile = systemConf["iden_table"]["file"].as<std::string>();
        uint32_t idenReloadTime = systemConf["iden_table"]["time"].as<uint32_t>(0U);

        if (idenLookupFile.length() <= 0U) {
            ::LogError(LOG_HOST, "No bandplan identity table? This must be defined!");
            return false;
        }

        LogInfo("Iden Table Lookups");
        LogInfo("    File: %s", idenLookupFile.length() > 0U ? idenLookupFile.c_str() : "None");
        if (idenReloadTime > 0U)
            LogInfo("    Reload: %u mins", idenReloadTime);

        m_idenTable = new IdenTableLookup(idenLookupFile, idenReloadTime);
        m_idenTable->read();

        /*
        ** Channel Configuration
        */
        yaml::Node rfssConfig = systemConf["config"];
        m_channelLookup = new ChannelLookup();
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

        if (entry.txOffsetMhz() == 0U) {
            ::LogError(LOG_HOST, "Channel Id %u has an invalid Tx offset.", m_channelId);
            return false;
        }

        uint32_t calcSpace = (uint32_t)(entry.chSpaceKhz() / 0.125);
        float calcTxOffset = entry.txOffsetMhz() * 1000000.0;

        m_txFrequency = (uint32_t)((entry.baseFrequency() + ((calcSpace * 125) * m_channelNo)));
        m_rxFrequency = (uint32_t)(m_txFrequency + (int32_t)calcTxOffset);

        if (calcTxOffset < 0.0f && m_rxFrequency < entry.baseFrequency()) {
            ::LogWarning(LOG_HOST, "Channel Id %u Channel No $%04X has an invalid frequency. Rx Frequency (%u) is less then the base frequency (%u), this may result in incorrect trunking behavior.", m_channelId, m_channelNo,
                m_rxFrequency, entry.baseFrequency());
        }

        if (!m_duplex && simplexSameFreq) {
            m_rxFrequency = m_txFrequency;
        }

        /*
        ** Control Channel
        */
        {
            yaml::Node controlCh = rfssConfig["controlCh"];

            std::string restApiAddress = controlCh["restAddress"].as<std::string>("");
            uint16_t restApiPort = (uint16_t)controlCh["restPort"].as<uint32_t>(REST_API_DEFAULT_PORT);
            std::string restApiPassword = controlCh["restPassword"].as<std::string>();
            bool restSsl = controlCh["restSsl"].as<bool>(false);
            m_presenceTime = controlCh["presence"].as<uint32_t>(120U);

            VoiceChData data = VoiceChData(m_channelId, m_channelNo, restApiAddress, restApiPort, restApiPassword, restSsl);
            m_controlChData = data;

            if (!m_controlChData.address().empty() && m_controlChData.port() > 0) {
                ::LogInfoEx(LOG_HOST, "Control Channel REST API Address %s:%u SSL %u", m_controlChData.address().c_str(), m_controlChData.port(), restSsl);
            } else {
                ::LogInfoEx(LOG_HOST, "No Control Channel REST API Configured, CC notify disabled");
            }
        }

        /*
        ** Voice Channels
        */
        yaml::Node& voiceChList = rfssConfig["voiceChNo"];

        if (voiceChList.size() == 0U) {
            ::LogError(LOG_HOST, "No voice channel list defined!");
            return false;
        }

        for (size_t i = 0; i < voiceChList.size(); i++) {
            yaml::Node& channel = voiceChList[i];

            uint8_t chId = (uint8_t)channel["channelId"].as<uint32_t>(255U);

            // special case default handling for if the channelId field is missing from the
            // configuration
            if (chId == 255U) {
                chId = m_channelId;
            }

            if (chId > 15U) { // clamp to 15
                chId = 15U;
            }

            uint32_t chNo = (uint32_t)::strtoul(channel["channelNo"].as<std::string>("1").c_str(), NULL, 16);
            if (chNo == 0U) { // clamp to 1
                chNo = 1U;
            }
            if (chNo > 4095U) { // clamp to 4095
                chNo = 4095U;
            }

            std::string restApiAddress = channel["restAddress"].as<std::string>("0.0.0.0");
            uint16_t restApiPort = (uint16_t)channel["restPort"].as<uint32_t>(REST_API_DEFAULT_PORT);
            std::string restApiPassword = channel["restPassword"].as<std::string>();
            bool restSsl = channel["restSsl"].as<bool>(false);

            ::LogInfoEx(LOG_HOST, "Voice Channel Id %u Channel No $%04X REST API Address %s:%u SSL %u", chId, chNo, restApiAddress.c_str(), restApiPort, restSsl);

            VoiceChData data = VoiceChData(chId, chNo, restApiAddress, restApiPort, restApiPassword, restSsl);
            m_channelLookup->setRFChData(chNo, data);
            m_channelLookup->addRFCh(chNo);
        }

        std::string strVoiceChNo = "";
        std::vector<uint32_t> voiceChNo = m_channelLookup->rfChTable();
        for (auto it = voiceChNo.begin(); it != voiceChNo.end(); ++it) {
            uint32_t chNo = ::atoi(std::to_string(*it).c_str());
            ::lookups::VoiceChData voiceChData = m_channelLookup->getRFChData(chNo);

            char hexStr[29];

            ::sprintf(hexStr, "$%01X.%01X (%u.%u)", voiceChData.chId(), chNo, voiceChData.chId(), chNo);

            strVoiceChNo.append(std::string(hexStr));
            strVoiceChNo.append(",");
        }
        strVoiceChNo.erase(strVoiceChNo.find_last_of(","));

        /*
        ** Site Parameters
        */
        m_siteId = (uint8_t)::strtoul(rfssConfig["siteId"].as<std::string>("1").c_str(), NULL, 16);
        m_siteId = p25::P25Utils::siteId(m_siteId);

        m_dmrColorCode = rfssConfig["colorCode"].as<uint32_t>(2U);
        m_dmrColorCode = dmr::DMRUtils::colorCode(m_dmrColorCode);

        m_dmrNetId = (uint32_t)::strtoul(rfssConfig["dmrNetId"].as<std::string>("1").c_str(), NULL, 16);
        m_dmrNetId = dmr::DMRUtils::netId(m_dmrNetId, dmr::defines::SiteModel::SM_SMALL);

        m_p25NAC = (uint32_t)::strtoul(rfssConfig["nac"].as<std::string>("F7E").c_str(), NULL, 16);
        m_p25NAC = p25::P25Utils::nac(m_p25NAC);

        uint32_t p25TxNAC = (uint32_t)::strtoul(rfssConfig["txNAC"].as<std::string>("293").c_str(), NULL, 16);
        if (p25TxNAC == m_p25NAC) {
            LogWarning(LOG_HOST, "Only use txNAC when split NAC operations are needed. nac and txNAC should not be the same!");
        }

        m_p25NetId = (uint32_t)::strtoul(rfssConfig["netId"].as<std::string>("BB800").c_str(), NULL, 16);
        m_p25NetId = p25::P25Utils::netId(m_p25NetId);
        if (m_p25NetId == 0xBEE00) {
            ::fatal("error 4\n");
        }

        m_sysId = (uint32_t)::strtoul(rfssConfig["sysId"].as<std::string>("001").c_str(), NULL, 16);
        m_sysId = p25::P25Utils::sysId(m_sysId);

        m_p25RfssId = (uint8_t)::strtoul(rfssConfig["rfssId"].as<std::string>("1").c_str(), NULL, 16);
        m_p25RfssId = p25::P25Utils::rfssId(m_p25RfssId);

        m_nxdnRAN = rfssConfig["ran"].as<uint32_t>(1U);

        m_authoritative = rfssConfig["authoritative"].as<bool>(true);

        LogInfo("System Config Parameters");
        LogInfo("    Authoritative: %s", m_authoritative ? "yes" : "no");
        if (m_authoritative) {
            m_supervisor = rfssConfig["supervisor"].as<bool>(false);
            LogInfo("    Supervisor: %s", m_supervisor ? "yes" : "no");
        }
        LogInfo("    RX Frequency: %uHz", m_rxFrequency);
        LogInfo("    TX Frequency: %uHz", m_txFrequency);
        LogInfo("    Base Frequency: %uHz", entry.baseFrequency());
        LogInfo("    TX Offset: %fMHz", entry.txOffsetMhz());
        LogInfo("    Bandwidth: %fKHz", entry.chBandwidthKhz());
        LogInfo("    Channel Spacing: %fKHz", entry.chSpaceKhz());
        LogInfo("    Channel Id: %u", m_channelId);
        LogInfo("    Channel No.: $%04X (%u)", m_channelNo, m_channelNo);
        LogInfo("    Voice Channel No(s).: %s", strVoiceChNo.c_str());
        LogInfo("    Site Id: $%02X", m_siteId);
        LogInfo("    System Id: $%03X", m_sysId);
        LogInfo("    DMR Color Code: %u", m_dmrColorCode);
        LogInfo("    DMR Network Id: $%05X", m_dmrNetId);
        LogInfo("    P25 NAC: $%03X", m_p25NAC);

        if (p25TxNAC != p25::defines::NAC_DIGITAL_SQ && p25TxNAC != m_p25NAC) {
            LogInfo("    P25 Tx NAC: $%03X", p25TxNAC);
        }

        LogInfo("    P25 Network Id: $%05X", m_p25NetId);
        LogInfo("    P25 RFSS Id: $%02X", m_p25RfssId);
        LogInfo("    NXDN RAN: %u", m_nxdnRAN);

        if (!m_authoritative) {
            m_supervisor = false;
            LogWarning(LOG_HOST, "Host is non-authoritative! This requires REST API to handle permit TG for VCs and grant TG for CCs!");
        }
    }
    else {
        LogInfo("    Modem Remote Control: yes");
    }

    return true;
}

/* Initializes the modem DSP. */

bool Host::createModem()
{
    yaml::Node protocolConf = m_conf["protocols"];

    yaml::Node dmrProtocol = protocolConf["dmr"];
    uint32_t dmrQueueSize = dmrProtocol["queueSize"].as<uint32_t>(24U);

    // clamp queue size to no less than 24 and no greater the 100
    if (dmrQueueSize < 24U) {
        LogWarning(LOG_HOST, "DMR queue size must be greater then 24 frames, defaulting to 24 frames!");
        dmrQueueSize = 24U;
    }
    if (dmrQueueSize > 100U) {
        LogWarning(LOG_HOST, "DMR queue size must be less then 100 frames, defaulting to 100 frames!");
        dmrQueueSize = 100U;
    }
    if (dmrQueueSize > 60U) {
        LogWarning(LOG_HOST, "DMR queue size is excessive, >60 frames!");
    }

    m_dmrQueueSizeBytes = dmrQueueSize * (dmr::defines::DMR_FRAME_LENGTH_BYTES * 5U);

    yaml::Node p25Protocol = protocolConf["p25"];
    uint32_t p25QueueSize = p25Protocol["queueSize"].as<uint16_t>(12U);

    // clamp queue size to no less than 12 and no greater the 100 frames
    if (p25QueueSize < 12U) {
        LogWarning(LOG_HOST, "P25 queue size must be greater then 12 frames, defaulting to 12 frames!");
        p25QueueSize = 12U;
    }
    if (p25QueueSize > 50U) {
        LogWarning(LOG_HOST, "P25 queue size must be less then 50 frames, defaulting to 50 frames!");
        p25QueueSize = 50U;
    }
    if (p25QueueSize > 30U) {
        LogWarning(LOG_HOST, "P25 queue size is excessive, >30 frames!");
    }

    m_p25QueueSizeBytes = p25QueueSize * p25::defines::P25_LDU_FRAME_LENGTH_BYTES;

    yaml::Node nxdnProtocol = protocolConf["nxdn"];
    uint32_t nxdnQueueSize = nxdnProtocol["queueSize"].as<uint32_t>(31U);

    // clamp queue size to no less than 31 and no greater the 50 frames
    if (nxdnQueueSize < 31U) {
        LogWarning(LOG_HOST, "NXDN queue size must be greater then 31 frames, defaulting to 31 frames!");
        nxdnQueueSize = 31U;
    }
    if (nxdnQueueSize > 50U) {
        LogWarning(LOG_HOST, "NXDN queue size must be less then 50 frames, defaulting to 50 frames!");
        nxdnQueueSize = 50U;
    }

    m_nxdnQueueSizeBytes = nxdnQueueSize * nxdn::defines::NXDN_FRAME_LENGTH_BYTES;

    yaml::Node modemConf = m_conf["system"]["modem"];

    yaml::Node modemProtocol = modemConf["protocol"];
    std::string portType = modemProtocol["type"].as<std::string>("null");
    std::string modemMode = modemProtocol["mode"].as<std::string>("air");
    yaml::Node uartProtocol = modemProtocol["uart"];
    std::string uartPort = uartProtocol["port"].as<std::string>();
    uint32_t uartSpeed = uartProtocol["speed"].as<uint32_t>(115200);

    bool rxInvert = modemConf["rxInvert"].as<bool>(false);
    bool txInvert = modemConf["txInvert"].as<bool>(false);
    bool pttInvert = modemConf["pttInvert"].as<bool>(false);
    bool dcBlocker = modemConf["dcBlocker"].as<bool>(true);
    bool cosLockout = modemConf["cosLockout"].as<bool>(false);
    uint8_t fdmaPreamble = (uint8_t)modemConf["fdmaPreamble"].as<uint32_t>(80U);
    uint8_t dmrRxDelay = (uint8_t)modemConf["dmrRxDelay"].as<uint32_t>(7U);
    uint8_t p25CorrCount = (uint8_t)modemConf["p25CorrCount"].as<uint32_t>(4U);
    int rxDCOffset = modemConf["rxDCOffset"].as<int>(0);
    int txDCOffset = modemConf["txDCOffset"].as<int>(0);

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
    uint8_t rfPower = (uint8_t)hotspotParams["rfPower"].as<uint32_t>(100U);

    yaml::Node repeaterParams = modemConf["repeater"];

    int dmrSymLevel3Adj = repeaterParams["dmrSymLvl3Adj"].as<int>(0);
    int dmrSymLevel1Adj = repeaterParams["dmrSymLvl1Adj"].as<int>(0);
    int p25SymLevel3Adj = repeaterParams["p25SymLvl3Adj"].as<int>(0);
    int p25SymLevel1Adj = repeaterParams["p25SymLvl1Adj"].as<int>(0);
    int nxdnSymLevel3Adj = repeaterParams["nxdnSymLvl3Adj"].as<int>(0);
    int nxdnSymLevel1Adj = repeaterParams["nxdnSymLvl1Adj"].as<int>(0);

    yaml::Node softpotParams = modemConf["softpot"];

    uint8_t rxCoarse = (uint8_t)softpotParams["rxCoarse"].as<uint32_t>(127U);
    uint8_t rxFine = (uint8_t)softpotParams["rxFine"].as<uint32_t>(127U);
    uint8_t txCoarse = (uint8_t)softpotParams["txCoarse"].as<uint32_t>(127U);
    uint8_t txFine = (uint8_t)softpotParams["txFine"].as<uint32_t>(127U);
    uint8_t rssiCoarse = (uint8_t)softpotParams["rssiCoarse"].as<uint32_t>(127U);
    uint8_t rssiFine = (uint8_t)softpotParams["rssiFine"].as<uint32_t>(127U);

    uint16_t dmrFifoLength = (uint16_t)modemConf["dmrFifoLength"].as<uint32_t>(DMR_TX_BUFFER_LEN);
    uint16_t p25FifoLength = (uint16_t)modemConf["p25FifoLength"].as<uint32_t>(P25_TX_BUFFER_LEN);
    uint16_t nxdnFifoLength = (uint16_t)modemConf["nxdnFifoLength"].as<uint32_t>(NXDN_TX_BUFFER_LEN);

    yaml::Node dfsiParams = modemConf["dfsi"];

    bool rtrt = dfsiParams["rtrt"].as<bool>(true);
    bool diu = dfsiParams["diu"].as<bool>(true);
    uint16_t jitter = dfsiParams["jitter"].as<uint16_t>(200U);
    uint16_t dfsiCallTimeout = dfsiParams["callTimeout"].as<uint16_t>(200U);
    bool useFSCForUDP = dfsiParams["fsc"].as<bool>(false);
    uint32_t fscHeartbeat = dfsiParams["fscHeartbeat"].as<uint32_t>(5U);
    bool fscInitiator = dfsiParams["initiator"].as<bool>(false);
    bool dfsiTIAMode = dfsiParams["dfsiTIAMode"].as<bool>(false);

    // clamp fifo sizes
    if (dmrFifoLength < DMR_TX_BUFFER_LEN) {
        LogWarning(LOG_HOST, "DMR FIFO size must be greater then %u bytes, defaulting to %u bytes!", DMR_TX_BUFFER_LEN, DMR_TX_BUFFER_LEN);
        dmrFifoLength = DMR_TX_BUFFER_LEN;
    }

    if (p25FifoLength < 442U/*P25_TX_BUFFER_LEN*/) {
        LogWarning(LOG_HOST, "P25 FIFO size must be greater then %u bytes, defaulting to %u bytes!", 442U/*P25_TX_BUFFER_LEN*/, 442U/*P25_TX_BUFFER_LEN*/);
        p25FifoLength = 442U/*P25_TX_BUFFER_LEN*/;
    }

    if (nxdnFifoLength < NXDN_TX_BUFFER_LEN) {
        LogWarning(LOG_HOST, "NXDN FIFO size must be greater then %u frames, defaulting to %u frames!", NXDN_TX_BUFFER_LEN, NXDN_TX_BUFFER_LEN);
        nxdnFifoLength = NXDN_TX_BUFFER_LEN;
    }

    float rxLevel = modemConf["rxLevel"].as<float>(50.0F);
    float cwIdTXLevel = modemConf["cwIdTxLevel"].as<float>(50.0F);
    float dmrTXLevel = modemConf["dmrTxLevel"].as<float>(50.0F);
    float p25TXLevel = modemConf["p25TxLevel"].as<float>(50.0F);
    float nxdnTXLevel = modemConf["nxdnTxLevel"].as<float>(50.0F);
    if (!modemConf["txLevel"].isNone()) {
        cwIdTXLevel = dmrTXLevel = p25TXLevel = nxdnTXLevel = modemConf["txLevel"].as<float>(50.0F);
    }
    bool disableOFlowReset = modemConf["disableOFlowReset"].as<bool>(false);
    bool ignoreModemConfigArea = modemConf["ignoreModemConfigArea"].as<bool>(false);
    bool dumpModemStatus = modemConf["dumpModemStatus"].as<bool>(false);
    bool trace = modemConf["trace"].as<bool>(false);
    bool debug = modemConf["debug"].as<bool>(false);

    // if modem debug is being forced from the commandline -- enable modem debug
    if (g_modemDebug)
        debug = true;

    if (rfPower == 0U) { // clamp to 1
        rfPower = 1U;
    }
    if (rfPower > 100U) { // clamp to 100
        rfPower = 100U;
    }

    LogInfo("Modem Parameters");
    LogInfo("    Port Type: %s", portType.c_str());
    LogInfo("    Interface Mode: %s", modemMode.c_str());

    port::IModemPort* modemPort = nullptr;
    std::transform(portType.begin(), portType.end(), portType.begin(), ::tolower);
    if (portType == NULL_PORT) {
        modemPort = new port::ModemNullPort();
    }
    else if (portType == UART_PORT || portType == PTY_PORT) {
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

        if (portType == PTY_PORT) {
            modemPort = new port::UARTPort(uartPort, serialSpeed, false, false);
            LogInfo("    PTY Port: %s", uartPort.c_str());
            LogInfo("    PTY Speed: %u", uartSpeed);
        }
        else {
            if (modemMode == MODEM_MODE_DFSI) {
                modemPort = new port::UARTPort(uartPort, serialSpeed, false, true);
                LogInfo("    RTS/DTR boot flags enabled");
            } else {
                modemPort = new port::UARTPort(uartPort, serialSpeed, true, false);
            }
            LogInfo("    UART Port: %s", uartPort.c_str());
            LogInfo("    UART Speed: %u", uartSpeed);
        }
    }
    else {
        LogError(LOG_HOST, "Invalid protocol port type, %s!", portType.c_str());
        return false;
    }

    std::transform(modemMode.begin(), modemMode.end(), modemMode.begin(), ::tolower);
    if (modemMode == MODEM_MODE_DFSI) {
        m_isModemDFSI = true;
        LogInfo("    DFSI RT/RT: %s", rtrt ? "yes" : "no");
        LogInfo("    DFSI DIU Flag: %s", diu ? "yes" : "no");
        LogInfo("    DFSI Jitter Size: %u ms", jitter);
        if (g_remoteModemMode) {
            LogInfo("    DFSI Use FSC: %s", useFSCForUDP ? "yes" : "no");
            LogInfo("    DFSI FSC Heartbeat: %us", fscHeartbeat);
            LogInfo("    DFSI FSC Initiator: %s", fscInitiator ? "yes" : "no");
            LogInfo("    DFSI FSC TIA Frames: %s", dfsiTIAMode ? "yes" : "no");
        }
    }

    if (g_remoteModemMode) {
        if (portType == UART_PORT || portType == PTY_PORT) {
            m_modemRemotePort = new port::UDPPort(g_remoteAddress, g_remotePort);
            m_modemRemote = true;
            ignoreModemConfigArea = true;
        }
        else {
            delete modemPort;
            if (modemMode == MODEM_MODE_DFSI) {
                yaml::Node networkConf = m_conf["network"];
                uint32_t id = networkConf["id"].as<uint32_t>(1000U);
                if (useFSCForUDP) {
                    modemPort = new port::specialized::V24UDPPort(id, g_remoteAddress, g_remotePort + 1U, g_remotePort, true, fscInitiator, debug);
                    ((modem::port::specialized::V24UDPPort*)modemPort)->setHeartbeatInterval(fscHeartbeat);
               } else {
                    modemPort = new port::specialized::V24UDPPort(id, g_remoteAddress, g_remotePort, 0U, false, false, debug);
                }
                m_udpDFSIRemotePort = modemPort;
            } else {
                modemPort = new port::UDPPort(g_remoteAddress, g_remotePort);
            }
            m_modemRemote = false;
        }

        LogInfo("    UDP Mode: %s", m_modemRemote ? "master" : "peer");
        LogInfo("    UDP Address: %s", g_remoteAddress.c_str());
        LogInfo("    UDP Port: %u", g_remotePort);
    }

    if (!m_modemRemote) {
        LogInfo("    RX Invert: %s", rxInvert ? "yes" : "no");
        LogInfo("    TX Invert: %s", txInvert ? "yes" : "no");
        LogInfo("    PTT Invert: %s", pttInvert ? "yes" : "no");
        LogInfo("    DC Blocker: %s", dcBlocker ? "yes" : "no");
        LogInfo("    COS Lockout: %s", cosLockout ? "yes" : "no");
        LogInfo("    FDMA Preambles: %u (%.1fms)", fdmaPreamble, float(fdmaPreamble) * 0.2222F);
        LogInfo("    DMR RX Delay: %u (%.1fms)", dmrRxDelay, float(dmrRxDelay) * 0.0416666F);
        LogInfo("    P25 Corr. Count: %u (%.1fms)", p25CorrCount, float(p25CorrCount) * 0.667F);
        LogInfo("    RX DC Offset: %d", rxDCOffset);
        LogInfo("    TX DC Offset: %d", txDCOffset);
        LogInfo("    RX Tuning Offset: %dhz", rxTuning);
        LogInfo("    TX Tuning Offset: %dhz", txTuning);
        LogInfo("    RX Effective Frequency: %uhz", m_rxFrequency + rxTuning);
        LogInfo("    TX Effective Frequency: %uhz", m_txFrequency + txTuning);
        LogInfo("    RX Coarse: %u, Fine: %u", rxCoarse, rxFine);
        LogInfo("    TX Coarse: %u, Fine: %u", txCoarse, txFine);
        LogInfo("    RSSI Coarse: %u, Fine: %u", rssiCoarse, rssiFine);
        LogInfo("    RF Power Level: %u", rfPower);
        LogInfo("    RX Level: %.1f%%", rxLevel);
        LogInfo("    CW Id TX Level: %.1f%%", cwIdTXLevel);
        LogInfo("    DMR TX Level: %.1f%%", dmrTXLevel);
        LogInfo("    P25 TX Level: %.1f%%", p25TXLevel);
        LogInfo("    NXDN TX Level: %.1f%%", nxdnTXLevel);
        LogInfo("    Disable Overflow Reset: %s", disableOFlowReset ? "yes" : "no");
        LogInfo("    DMR Queue Size: %u (%u bytes)", dmrQueueSize, m_dmrQueueSizeBytes);
        LogInfo("    P25 Queue Size: %u (%u bytes)", p25QueueSize, m_p25QueueSizeBytes);
        LogInfo("    NXDN Queue Size: %u (%u bytes)", nxdnQueueSize, m_nxdnQueueSizeBytes);
        LogInfo("    DMR FIFO Size: %u bytes", dmrFifoLength);
        LogInfo("    P25 FIFO Size: %u bytes", p25FifoLength);
        LogInfo("    NXDN FIFO Size: %u bytes", nxdnFifoLength);

        if (ignoreModemConfigArea) {
            LogInfo("    Ignore Modem Configuration Area: yes");
        }

        if (dumpModemStatus) {
            LogInfo("    Dump Modem Status: yes");
        }
    }

    if (debug) {
        LogInfo("    Debug: yes");
    }

    if (m_isModemDFSI) {
        m_modem = new ModemV24(modemPort, m_duplex, m_p25QueueSizeBytes, m_p25QueueSizeBytes, rtrt, diu, jitter,
            dumpModemStatus, trace, debug);
        ((ModemV24*)m_modem)->setCallTimeout(dfsiCallTimeout);
        ((ModemV24*)m_modem)->setTIAFormat(dfsiTIAMode);
    } else {
        m_modem = new Modem(modemPort, m_duplex, rxInvert, txInvert, pttInvert, dcBlocker, cosLockout, fdmaPreamble, dmrRxDelay, p25CorrCount,
            m_dmrQueueSizeBytes, m_p25QueueSizeBytes, m_nxdnQueueSizeBytes, disableOFlowReset, ignoreModemConfigArea, dumpModemStatus, trace, debug);
    }
    if (!m_modemRemote) {
        m_modem->setModeParams(m_dmrEnabled, m_p25Enabled, m_nxdnEnabled);
        m_modem->setLevels(rxLevel, cwIdTXLevel, dmrTXLevel, p25TXLevel, nxdnTXLevel);
        m_modem->setSymbolAdjust(dmrSymLevel3Adj, dmrSymLevel1Adj, p25SymLevel3Adj, p25SymLevel1Adj, nxdnSymLevel3Adj, nxdnSymLevel1Adj);
        m_modem->setDCOffsetParams(txDCOffset, rxDCOffset);
        m_modem->setRFParams(m_rxFrequency, m_txFrequency, rxTuning, txTuning, rfPower, dmrDiscBWAdj, p25DiscBWAdj, nxdnDiscBWAdj, dmrPostBWAdj,
            p25PostBWAdj, nxdnPostBWAdj, adfGainMode, afcEnable, afcKI, afcKP, afcRange);
        m_modem->setSoftPot(rxCoarse, rxFine, txCoarse, txFine, rssiCoarse, rssiFine);
        m_modem->setDMRColorCode(m_dmrColorCode);
        if (m_p25NAC == p25::defines::NAC_REUSE_RX_NAC)
            m_modem->setP25NAC(p25::defines::NAC_DIGITAL_SQ);
        else
            m_modem->setP25NAC(m_p25NAC);
    }

    if (m_modemRemote) {
        m_modem->setOpenHandler(MODEM_OC_PORT_HANDLER_BIND(Host::rmtPortModemOpen, this));
        m_modem->setCloseHandler(MODEM_OC_PORT_HANDLER_BIND(Host::rmtPortModemClose, this));
        m_modem->setResponseHandler(MODEM_RESP_HANDLER_BIND(Host::rmtPortModemHandler, this));
    }

    if (useFSCForUDP) {
        modem::port::specialized::V24UDPPort* udpPort = dynamic_cast<modem::port::specialized::V24UDPPort*>(m_udpDFSIRemotePort);
        udpPort->openFSC();
    }

    bool ret = m_modem->open();
    if (!ret) {
        delete m_modem;
        m_modem = nullptr;
        return false;
    }

    m_modem->setFifoLength(dmrFifoLength, p25FifoLength, nxdnFifoLength);

    // are we on a protocol version older then 3?
    if (m_modem->getVersion() < 3U) {
        if (m_nxdnEnabled) {
            ::LogError(LOG_HOST, "NXDN is not supported on legacy firmware.");
            return false;
        }
    }

    return true;
}

/* Initializes network connectivity. */

bool Host::createNetwork()
{
    yaml::Node networkConf = m_conf["network"];
    bool netEnable = networkConf["enable"].as<bool>(false);
    bool restApiEnable = networkConf["restEnable"].as<bool>(false);

    // dump out if both networking and REST API are disabled
    if (!netEnable && !restApiEnable) {
        return true;
    }

    std::string address = networkConf["address"].as<std::string>();
    uint16_t port = (uint16_t)networkConf["port"].as<uint32_t>(TRAFFIC_DEFAULT_PORT);
    uint16_t local = (uint16_t)networkConf["local"].as<uint32_t>(0U);
    std::string restApiAddress = networkConf["restAddress"].as<std::string>("127.0.0.1");
    uint16_t restApiPort = (uint16_t)networkConf["restPort"].as<uint32_t>(REST_API_DEFAULT_PORT);
    std::string restApiPassword = networkConf["restPassword"].as<std::string>();
    bool restApiEnableSSL = networkConf["restSsl"].as<bool>(false);
    std::string restApiSSLCert = networkConf["restSslCertificate"].as<std::string>("web.crt");
    std::string restApiSSLKey = networkConf["restSslKey"].as<std::string>("web.key");
    bool restApiDebug = networkConf["restDebug"].as<bool>(false);
    uint32_t id = networkConf["id"].as<uint32_t>(1000U);
    uint32_t jitter = networkConf["talkgroupHang"].as<uint32_t>(360U);
    std::string password = networkConf["password"].as<std::string>();
    bool slot1 = networkConf["slot1"].as<bool>(true);
    bool slot2 = networkConf["slot2"].as<bool>(true);
    bool allowActivityTransfer = networkConf["allowActivityTransfer"].as<bool>(false);
    bool allowDiagnosticTransfer = networkConf["allowDiagnosticTransfer"].as<bool>(false);
    bool allowStatusTransfer = networkConf["allowStatusTransfer"].as<bool>(true);
    bool updateLookup = networkConf["updateLookups"].as<bool>(false);
    bool saveLookup = networkConf["saveLookups"].as<bool>(false);
    bool debug = networkConf["debug"].as<bool>(false);

    m_allowStatusTransfer = allowStatusTransfer;

    bool encrypted = networkConf["encrypted"].as<bool>(false);
    std::string key = networkConf["presharedKey"].as<std::string>();
    uint8_t presharedKey[AES_WRAPPED_PCKT_KEY_LEN];
    if (!key.empty()) {
        if (key.size() == 32) {
            // bryanb: shhhhhhh....dirty nasty hacks
            key = key.append(key); // since the key is 32 characters (16 hex pairs), double it on itself for 64 characters (32 hex pairs)
            LogWarning(LOG_HOST, "Half-length network preshared encryption key detected, doubling key on itself.");
        }

        if (key.size() == 64) {
            if ((key.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos)) {
                const char* keyPtr = key.c_str();
                ::memset(presharedKey, 0x00U, AES_WRAPPED_PCKT_KEY_LEN);

                for (uint8_t i = 0; i < AES_WRAPPED_PCKT_KEY_LEN; i++) {
                    char t[4] = {keyPtr[0], keyPtr[1], 0};
                    presharedKey[i] = (uint8_t)::strtoul(t, NULL, 16);
                    keyPtr += 2 * sizeof(char);
                }
            }
            else {
                LogWarning(LOG_HOST, "Invalid characters in the network preshared encryption key. Encryption disabled.");
                encrypted = false;
            }
        }
        else {
            LogWarning(LOG_HOST, "Invalid  network preshared encryption key length, key should be 32 hex pairs, or 64 characters. Encryption disabled.");
            encrypted = false;
        }
    }

    if (id > 999999999U) {
        ::LogError(LOG_HOST, "Network Peer ID cannot be greater then 999999999.");
        return false;
    }

    if (restApiPassword.length() > 64) {
        std::string password = restApiPassword;
        restApiPassword = password.substr(0, 64);

        ::LogWarning(LOG_HOST, "REST API password is too long; truncating to the first 64 characters.");
    }

    if (restApiPassword.empty() && restApiEnable) {
        ::LogWarning(LOG_HOST, "REST API password not provided; REST API disabled.");
        restApiEnable = false;
    }

    if (restApiSSLCert.empty() && restApiEnableSSL) {
        ::LogWarning(LOG_HOST, "REST API SSL certificate not provided; REST API SSL disabled.");
        restApiEnableSSL = false;
    }

    if (restApiSSLKey.empty() && restApiEnableSSL) {
        ::LogWarning(LOG_HOST, "REST API SSL certificate private key not provided; REST API SSL disabled.");
        restApiEnableSSL = false;
    }

    yaml::Node protocolConf = m_conf["protocols"];
    bool dmrCtrlChannel = protocolConf["dmr"]["control"]["dedicated"].as<bool>(false);
    bool p25CtrlChannel = protocolConf["p25"]["control"]["dedicated"].as<bool>(false);
    bool nxdnCtrlChannel = protocolConf["nxdn"]["control"]["dedicated"].as<bool>(false);

    IdenTable entry = m_idenTable->find(m_channelId);

    LogInfo("Network Parameters");
    LogInfo("    Enabled: %s", netEnable ? "yes" : "no");
    if (netEnable) {
        LogInfo("    Peer ID: %u", id);
        LogInfo("    Address: %s", address.c_str());
        LogInfo("    Port: %u", port);
        if (local > 0U)
            LogInfo("    Local: %u", local);
        else
            LogInfo("    Local: random");
        LogInfo("    DMR Jitter: %ums", jitter);
        LogInfo("    Slot 1: %s", slot1 ? "enabled" : "disabled");
        LogInfo("    Slot 2: %s", slot2 ? "enabled" : "disabled");
        LogInfo("    Allow Activity Log Transfer: %s", allowActivityTransfer ? "yes" : "no");
        LogInfo("    Allow Diagnostic Log Transfer: %s", allowDiagnosticTransfer ? "yes" : "no");
        LogInfo("    Allow Status Transfer: %s", m_allowStatusTransfer ? "yes" : "no");
        LogInfo("    Update Lookups: %s", updateLookup ? "yes" : "no");
        LogInfo("    Save Network Lookups: %s", saveLookup ? "yes" : "no");

        LogInfo("    Encrypted: %s", encrypted ? "yes" : "no");

        if (debug) {
            LogInfo("    Debug: yes");
        }
    }
    LogInfo("    REST API Enabled: %s", restApiEnable ? "yes" : "no");
    if (restApiEnable) {
        LogInfo("    REST API Address: %s", restApiAddress.c_str());
        LogInfo("    REST API Port: %u", restApiPort);

        LogInfo("    REST API SSL Enabled: %s", restApiEnableSSL ? "yes" : "no");
        LogInfo("    REST API SSL Certificate: %s", restApiSSLCert.c_str());
        LogInfo("    REST API SSL Private Key: %s", restApiSSLKey.c_str());

        if (restApiDebug) {
            LogInfo("    REST API Debug: yes");
        }
    }

    // initialize networking
    if (netEnable) {
        m_network = new Network(address, port, local, id, password, m_duplex, debug, m_dmrEnabled, m_p25Enabled, m_nxdnEnabled, slot1, slot2, 
            allowActivityTransfer, allowDiagnosticTransfer, updateLookup, saveLookup);

        m_network->setLookups(m_ridLookup, m_tidLookup);
        m_network->setMetadata(m_identity, m_rxFrequency, m_txFrequency, entry.txOffsetMhz(), entry.chBandwidthKhz(), m_channelId, m_channelNo,
            m_power, m_latitude, m_longitude, m_height, m_location);

        if (restApiEnable) {
            m_network->setRESTAPIData(restApiPassword, restApiPort);
        }

        if (!dmrCtrlChannel && !p25CtrlChannel && !nxdnCtrlChannel) {
            if (m_controlChData.address().empty() && m_controlChData.port() == 0) {
                m_network->setConventional(true);
            }
        }

        if (encrypted) {
            m_network->setPresharedKey(presharedKey);
        }

        m_network->enable(true);
        bool ret = m_network->open();
        if (!ret) {
            delete m_network;
            m_network = nullptr;
            LogError(LOG_HOST, "failed to initialize traffic networking!");
            return false;
        }

        ::LogSetNetwork(m_network);
    }

    // initialize network remote command
    if (restApiEnable) {
        m_restAddress = restApiAddress;
        m_restPort = restApiPort;
        m_RESTAPI = new RESTAPI(restApiAddress, restApiPort, restApiPassword, restApiSSLKey, restApiSSLCert, restApiEnableSSL, this, restApiDebug);
        m_RESTAPI->setLookups(m_ridLookup, m_tidLookup);
        bool ret = m_RESTAPI->open();
        if (!ret) {
            delete m_RESTAPI;
            m_RESTAPI = nullptr;
            LogError(LOG_HOST, "failed to initialize REST API networking! REST API will be unavailable!");
            // REST API failing isn't fatal -- we'll allow this to return normally
        }
    }
    else {
        m_restAddress = "0.0.0.0";
        m_restPort = REST_API_DEFAULT_PORT;
        m_RESTAPI = nullptr;
    }

    return true;
}
