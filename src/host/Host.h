// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file Host.h
 * @ingroup host
 * @file Host.cpp
 * @ingroup host
 * @file Host.Config.cpp
 * @ingroup host
 * @file Host.DMR.cpp
 * @ingroup host
 * @file Host.P25.cpp
 * @ingroup host
 * @file Host.NXDN.cpp
 * @ingroup host
 */
#if !defined(__HOST_H__)
#define __HOST_H__

#include "Defines.h"
#include "common/Timer.h"
#include "common/lookups/AffiliationLookup.h"
#include "common/lookups/ChannelLookup.h"
#include "common/lookups/IdenTableLookup.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/network/json/json.h"
#include "common/yaml/Yaml.h"
#include "dmr/Control.h"
#include "p25/Control.h"
#include "nxdn/Control.h"
#include "network/Network.h"
#include "network/RESTAPI.h"
#include "modem/Modem.h"

#include <string>
#include <unordered_map>
#include <functional>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

class HOST_SW_API RESTAPI;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the core host service logic.
 * @ingroup host
 */
class HOST_SW_API Host {
public:
    /**
     * @brief Initializes a new instance of the Host class.
     * @param confFile Full-path to the configuration file.
     */
    Host(const std::string& confFile);
    /**
     * @brief Finalizes a instance of the Host class.
     */
    ~Host();

    /**
     * @brief Executes the main modem host processing loop.
     * @returns int Zero if successful, otherwise error occurred.
     */
    int run();

    /**
     * @brief Gets the RF channel lookup class.
     * @returns lookups::ChannelLookup* RF channel lookup.
     */
    lookups::ChannelLookup* rfCh() const { return m_channelLookup; }

private:
    const std::string& m_confFile;
    yaml::Node m_conf;

    modem::Modem* m_modem;
    bool m_modemRemote;
    network::Network* m_network;

    modem::port::IModemPort* m_modemRemotePort;

    uint8_t m_state;

    bool m_isTxCW;

    Timer m_modeTimer;
    Timer m_dmrTXTimer;
    Timer m_cwIdTimer;

    bool m_dmrEnabled;
    bool m_p25Enabled;
    bool m_nxdnEnabled;

    bool m_duplex;
    bool m_fixedMode;

    uint32_t m_timeout;
    uint32_t m_rfModeHang;
    uint32_t m_rfTalkgroupHang;
    uint32_t m_netModeHang;

    uint32_t m_lastDstId;
    uint32_t m_lastSrcId;

    bool m_allowStatusTransfer;

    std::string m_identity;
    std::string m_cwCallsign;
    uint32_t m_cwIdTime;

    float m_latitude;
    float m_longitude;
    int m_height;
    uint32_t m_power;
    std::string m_location;

    uint32_t m_rxFrequency;
    uint32_t m_txFrequency;
    uint8_t m_channelId;
    uint32_t m_channelNo;

    lookups::ChannelLookup* m_channelLookup;
    std::unordered_map<uint32_t, uint32_t> m_voiceChPeerId;
    lookups::VoiceChData m_controlChData;

    lookups::IdenTableLookup* m_idenTable;
    lookups::RadioIdLookup* m_ridLookup;
    lookups::TalkgroupRulesLookup* m_tidLookup;

    bool m_dmrBeacons;
    bool m_dmrTSCCData;
    bool m_dmrCtrlChannel;
    bool m_p25CCData;
    bool m_p25CtrlChannel;
    bool m_p25CtrlBroadcast;
    bool m_nxdnCCData;
    bool m_nxdnCtrlChannel;
    bool m_nxdnCtrlBroadcast;

    uint32_t m_presenceTime;

    uint8_t m_siteId;
    uint32_t m_sysId;
    uint32_t m_dmrNetId;
    uint32_t m_dmrColorCode;
    uint32_t m_p25NAC;
    uint32_t m_p25NetId;
    uint8_t m_p25RfssId;
    uint32_t m_nxdnRAN;

    uint32_t m_dmrQueueSizeBytes;
    uint32_t m_p25QueueSizeBytes;
    uint32_t m_nxdnQueueSizeBytes;

    bool m_authoritative;
    bool m_supervisor;

    Timer m_dmrBeaconDurationTimer;
    Timer m_dmrDedicatedTxTestTimer;
    Timer m_p25BcastDurationTimer;
    Timer m_p25DedicatedTxTestTimer;
    Timer m_nxdnBcastDurationTimer;
    Timer m_nxdnDedicatedTxTestTimer;
    
    Timer m_dmrTx1WatchdogTimer;
    uint32_t m_dmrTx1LoopMS;
    Timer m_dmrTx2WatchdogTimer;
    uint32_t m_dmrTx2LoopMS;
    Timer m_p25TxWatchdogTimer;
    uint32_t m_p25TxLoopMS;
    Timer m_nxdnTxWatchdogTimer;
    uint32_t m_nxdnTxLoopMS;
    uint8_t m_mainLoopStage;
    uint32_t m_mainLoopMS;
    Timer m_mainLoopWatchdogTimer;
    uint32_t m_adjSiteLoopMS;
    Timer m_adjSiteLoopWatchdogTimer;

    uint8_t m_activeTickDelay;
    uint8_t m_idleTickDelay;

    friend class RESTAPI;
    std::string m_restAddress;
    uint16_t m_restPort;
    RESTAPI *m_RESTAPI;

    /**
     * @brief Helper to generate the status of the host in JSON format.
     * @returns json::object Host status as a JSON object.
     */
    json::object getStatus();

    /**
     * @brief Modem port open callback.
     * @param modem Instance of the Modem class.
     * @returns bool True, if the modem is opened, otherwise false.
     */
    bool rmtPortModemOpen(modem::Modem* modem);
    /**
     * @brief Modem port close callback.
     * @param modem Instance of the Modem class.
     * @returns bool True, if the modem is closed, otherwise false.
     */
    bool rmtPortModemClose(modem::Modem* modem);
    /**
     * @brief Modem clock callback.
     * @param modem Instance of the Modem class.
     * @param ms 
     * @param rspType Modem message response type.
     * @param rspDblLen Flag indicating whether or not this message is a double length message.
     * @param[in] buffer Buffer containing modem message.
     * @param len Length of buffer.
     * @returns bool True, if the modem response was handled, otherwise false.
     */
    bool rmtPortModemHandler(modem::Modem* modem, uint32_t ms, modem::RESP_TYPE_DVM rspType, bool rspDblLen, const uint8_t* buffer, uint16_t len);

    /**
     * @brief Helper to set the host/modem running state.
     * @param state Host running state.
     */
    void setState(uint8_t state);

    /**
     * @brief Helper to create the state lock file.
     * @param state 
     */
    void createLockFile(const char* state) const;
    /**
     * @brief Helper to remove the state lock file.
     */
    void removeLockFile() const;

    // Configuration (Host.Config.cpp)
    /**
     * @brief Reads basic configuration parameters from the INI.
     * @returns bool True, if configuration was read successfully, otherwise false.
     */
    bool readParams();
    /**
     * @brief Initializes the modem DSP.
     * @returns bool True, if the modem was initialized, otherwise false.
     */
    bool createModem();
    /**
     * @brief Initializes network connectivity.
     * @returns bool True, if network connectivity was initialized, otherwise false.
     */
    bool createNetwork();

    // Digital Mobile Radio (Host.DMR.cpp)
    /**
     * @brief Helper to interrupt a running DMR beacon.
     * @param control Instance of the dmr::Control class.
     */
    void interruptDMRBeacon(dmr::Control* control);

    /**
     * @brief Helper to read DMR slot 1 frames from modem.
     * @param control Instance of the dmr::Control class.
     * @param afterReadCallback Function callback after reading frames.
     */
    void readFramesDMR1(dmr::Control* control, std::function<void()>&& afterReadCallback);
    /**
     * @brief Helper to write DMR slot 1 frames to modem.
     * @param control Instance of the dmr::Control class.
     * @param afterWriteCallback Function callback writing reading frames.
     */
    void writeFramesDMR1(dmr::Control* control, std::function<void()>&& afterWriteCallback);
    /**
     * @brief Helper to read DMR slot 2 frames from modem.
     * @param control Instance of the dmr::Control class.
     * @param afterReadCallback Function callback after reading frames.
     */
    void readFramesDMR2(dmr::Control* control, std::function<void()>&& afterReadCallback);
    /**
     * @brief Helper to write DMR slot 2 frames to modem.
     * @param control Instance of the dmr::Control class.
     * @param afterWriteCallback Function callback writing reading frames.
     */
    void writeFramesDMR2(dmr::Control* control, std::function<void()>&& afterWriteCallback);

    // Project 25 (Host.P25.cpp)
    /**
     * @brief Helper to interrupt a running P25 control channel.
     * @param control Instance of the p25::Control class.
     */
    void interruptP25Control(p25::Control* control);

    /**
     * @brief Helper to read P25 frames from modem.
     * @param control Instance of the p25::Control class.
     * @param afterReadCallback Function callback after reading frames.
     */
    void readFramesP25(p25::Control* control, std::function<void()>&& afterReadCallback);
    /**
     * @brief Helper to write P25 frames to modem.
     * @param control Instance of the p25::Control class.
     * @param afterWriteCallback Function callback writing reading frames.
     */
    void writeFramesP25(p25::Control* control, std::function<void()>&& afterWriteCallback);

    // Next Generation Digital Narrowband (Host.NXDN.cpp)
    /**
     * @brief Helper to interrupt a running NXDN control channel.
     * @param control Instance of the nxdn::Control class.
     */
    void interruptNXDNControl(nxdn::Control* control);

    /**
     * @brief Helper to read NXDN frames from modem.
     * @param control Instance of the nxdn::Control class.
     * @param afterReadCallback Function callback after reading frames.
     */
    void readFramesNXDN(nxdn::Control* control, std::function<void()>&& afterReadCallback);
    /**
     * @brief Helper to write NXDN frames to modem.
     * @param control Instance of the nxdn::Control class.
     * @param afterWriteCallback Function callback writing reading frames.
     */
    void writeFramesNXDN(nxdn::Control* control, std::function<void()>&& afterWriteCallback);
};

#endif // __HOST_H__
