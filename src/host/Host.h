// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
*
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
//      This class implements the core host service logic.
// ---------------------------------------------------------------------------

class HOST_SW_API Host {
public:
    /// <summary>Initializes a new instance of the Host class.</summary>
    Host(const std::string& confFile);
    /// <summary>Finalizes a instance of the Host class.</summary>
    ~Host();

    /// <summary>Executes the main modem host processing loop.</summary>
    int run();

    /// <summary>Gets the RF channel lookup class.</summary>
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

    /// <summary>Modem port open callback.</summary>
    bool rmtPortModemOpen(modem::Modem* modem);
    /// <summary>Modem port close callback.</summary>
    bool rmtPortModemClose(modem::Modem* modem);
    /// <summary>Modem clock callback.</summary>
    bool rmtPortModemHandler(modem::Modem* modem, uint32_t ms, modem::RESP_TYPE_DVM rspType, bool rspDblLen, const uint8_t* buffer, uint16_t len);

    /// <summary>Helper to set the host/modem running state.</summary>
    void setState(uint8_t state);

    /// <summary>Helper to create the state lock file.</summary>
    void createLockFile(const char* state) const;
    /// <summary>Helper to remove the state lock file.</summary>
    void removeLockFile() const;

    /** (Host.Config.cpp) */
    /// <summary>Reads basic configuration parameters from the INI.</summary>
    bool readParams();
    /// <summary>Initializes the modem DSP.</summary>
    bool createModem();
    /// <summary>Initializes network connectivity.</summary>
    bool createNetwork();

    /** Digital Mobile Radio (Host.DMR.cpp) */
    /// <summary>Helper to interrupt a running DMR beacon.</summary>
    void interruptDMRBeacon(dmr::Control* control);

    /// <summary>Helper to read DMR slot 1 frames from modem.</summary>
    void readFramesDMR1(dmr::Control* control, std::function<void()>&& afterReadCallback);
    /// <summary>Helper to write DMR slot 1 frames to modem.</summary>
    void writeFramesDMR1(dmr::Control* control, std::function<void()>&& afterWriteCallback);
    /// <summary>Helper to read DMR slot 2 frames from modem.</summary>
    void readFramesDMR2(dmr::Control* control, std::function<void()>&& afterReadCallback);
    /// <summary>Helper to write DMR slot 2 frames to modem.</summary>
    void writeFramesDMR2(dmr::Control* control, std::function<void()>&& afterWriteCallback);

    /** Project 25 (Host.P25.cpp) */
    /// <summary>Helper to interrupt a running P25 control channel.</summary>
    void interruptP25Control(p25::Control* control);

    /// <summary>Helper to read P25 frames from modem.</summary>
    void readFramesP25(p25::Control* control, std::function<void()>&& afterReadCallback);
    /// <summary>Helper to write P25 frames to modem.</summary>
    void writeFramesP25(p25::Control* control, std::function<void()>&& afterWriteCallback);

    /** Next Generation Digital Narrowband (Host.NXDN.cpp) */
    /// <summary>Helper to interrupt a running NXDN control channel.</summary>
    void interruptNXDNControl(nxdn::Control* control);

    /// <summary>Helper to read NXDN frames from modem.</summary>
    void readFramesNXDN(nxdn::Control* control, std::function<void()>&& afterReadCallback);
    /// <summary>Helper to write NXDN frames to modem.</summary>
    void writeFramesNXDN(nxdn::Control* control, std::function<void()>&& afterWriteCallback);
};

#endif // __HOST_H__
