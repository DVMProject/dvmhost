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
*   Copyright (C) 2016,2017 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__P25_CONTROL_H__)
#define __P25_CONTROL_H__

#include "Defines.h"
#include "common/p25/NID.h"
#include "common/lookups/RSSIInterpolator.h"
#include "common/lookups/IdenTableLookup.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/p25/SiteData.h"
#include "common/RingBuffer.h"
#include "common/StopWatch.h"
#include "common/Timer.h"
#include "common/yaml/Yaml.h"
#include "p25/packet/Data.h"
#include "p25/packet/Voice.h"
#include "p25/packet/ControlSignaling.h"
#include "network/Network.h"
#include "p25/lookups/P25AffiliationLookup.h"
#include "modem/Modem.h"

#include <cstdio>
#include <vector>
#include <unordered_map>
#include <random>

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    namespace packet { class HOST_SW_API Voice; }
    namespace packet { class HOST_SW_API Data; }
    namespace packet { class HOST_SW_API Trunk; }
    namespace lookups { class HOST_SW_API P25AffiliationLookup; }

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements core logic for handling P25.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Control {
    public:
        /// <summary>Initializes a new instance of the Control class.</summary>
        Control(bool authoritative, uint32_t nac, uint32_t callHang, uint32_t queueSize, modem::Modem* modem, network::Network* network,
            uint32_t timeout, uint32_t tgHang, bool duplex, ::lookups::ChannelLookup* chLookup, ::lookups::RadioIdLookup* ridLookup,
            ::lookups::TalkgroupRulesLookup* tidLookup, ::lookups::IdenTableLookup* idenTable, ::lookups::RSSIInterpolator* rssiMapper,
            bool dumpPDUData, bool repeatPDU, bool dumpTSBKData, bool debug, bool verbose);
        /// <summary>Finalizes a instance of the Control class.</summary>
        ~Control();

        /// <summary>Resets the data states for the RF interface.</summary>
        void reset();

        /// <summary>Helper to set P25 configuration options.</summary>
        void setOptions(yaml::Node& conf, bool supervisor, const std::string cwCallsign, const ::lookups::VoiceChData controlChData,
            uint32_t netId, uint32_t sysId, uint8_t rfssId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, bool printOptions);

        /// <summary>Gets a flag indicating whether the P25 control channel is running.</summary>
        bool getCCRunning() const { return m_ccRunning; }
        /// <summary>Sets a flag indicating whether the P25 control channel is running.</summary>
        void setCCRunning(bool ccRunning) { m_ccPrevRunning = m_ccRunning; m_ccRunning = ccRunning; }
        /// <summary>Gets a flag indicating whether the P25 control channel is running.</summary>
        bool getCCHalted() const { return m_ccHalted; }
        /// <summary>Sets a flag indicating whether the P25 control channel is halted.</summary>
        void setCCHalted(bool ccHalted) { m_ccHalted = ccHalted; }

        /// <summary>Process a data frame from the RF interface.</summary>
        bool processFrame(uint8_t* data, uint32_t len);
        /// <summary>Get the frame data length for the next frame in the data ring buffer.</summary>
        uint32_t peekFrameLength();
        /// <summary>Get frame data from data ring buffer.</summary>
        uint32_t getFrame(uint8_t* data);

        /// <summary>Helper to write end of voice call frame data.</summary>
        bool writeRF_VoiceEnd();

        /// <summary>Updates the processor.</summary>
        void clock();
        /// <summary>Updates the adj. site tables and affiliations.</summary>
        void clockSiteData(uint32_t ms);

        /// <summary>Sets a flag indicating whether P25 has supervisory functions and can send permit TG to voice channels.</summary>
        void setSupervisor(bool supervisor) { m_supervisor = supervisor; }
        /// <summary>Permits a TGID on a non-authoritative host.</summary>
        void permittedTG(uint32_t dstId);
        /// <summary>Grants a TGID on a non-authoritative host.</summary>
        void grantTG(uint32_t srcId, uint32_t dstId, bool grp);
        /// <summary>Releases a granted TG.</summary>
        void releaseGrantTG(uint32_t dstId);
        /// <summary>Touches a granted TG to keep a channel grant alive.</summary>
        void touchGrantTG(uint32_t dstId);

        /// <summary>Gets instance of the NID class.</summary>
        NID nid() { return m_nid; }
        /// <summary>Gets instance of the ControlSignaling class.</summary>
        packet::ControlSignaling* control() { return m_control; }
        /// <summary>Gets instance of the P25AffiliationLookup class.</summary>
        lookups::P25AffiliationLookup affiliations() { return m_affiliations; }

        /// <summary>Flag indicating whether the processor or is busy or not.</summary>
        bool isBusy() const;

        /// <summary>Flag indicating whether P25 debug is enabled or not.</summary>
        bool getDebug() const { return m_debug; };
        /// <summary>Flag indicating whether P25 verbosity is enabled or not.</summary>
        bool getVerbose() const { return m_verbose; };
        /// <summary>Helper to change the debug and verbose state.</summary>
        void setDebugVerbose(bool debug, bool verbose);

        /// <summary>Helper to get the last transmitted destination ID.</summary>
        uint32_t getLastDstId() const;
        /// <summary>Helper to get the last transmitted source ID.</summary>
        uint32_t getLastSrcId() const;

    private:
        friend class packet::Voice;
        packet::Voice* m_voice;
        friend class packet::Data;
        packet::Data* m_data;
        friend class packet::ControlSignaling;
        packet::ControlSignaling* m_control;
        friend class lookups::P25AffiliationLookup;

        bool m_authoritative;
        bool m_supervisor;

        uint32_t m_nac;
        uint32_t m_txNAC;
        uint32_t m_timeout;

        modem::Modem* m_modem;
        network::Network* m_network;

        bool m_inhibitUnauth;
        bool m_legacyGroupGrnt;
        bool m_legacyGroupReg;

        bool m_duplex;
        bool m_enableControl;
        bool m_dedicatedControl;
        bool m_voiceOnControl;
        bool m_controlOnly;
        bool m_ackTSBKRequests;
        bool m_disableNetworkGrant;
        bool m_disableNetworkHDU;
        bool m_allowExplicitSourceId;
        bool m_convNetGrantDemand;

        ::lookups::IdenTableLookup* m_idenTable;
        ::lookups::RadioIdLookup* m_ridLookup;
        ::lookups::TalkgroupRulesLookup* m_tidLookup;
        lookups::P25AffiliationLookup m_affiliations;
        ::lookups::VoiceChData m_controlChData;

        ::lookups::IdenTable m_idenEntry;

        RingBuffer<uint8_t> m_txImmQueue;
        RingBuffer<uint8_t> m_txQueue;

        RPT_RF_STATE m_rfState;
        uint32_t m_rfLastDstId;
        uint32_t m_rfLastSrcId;
        RPT_NET_STATE m_netState;
        uint32_t m_netLastDstId;
        uint32_t m_netLastSrcId;

        uint32_t m_permittedDstId;

        bool m_tailOnIdle;
        bool m_ccRunning;
        bool m_ccPrevRunning;
        bool m_ccHalted;

        Timer m_rfTimeout;
        Timer m_rfTGHang;
        Timer m_rfLossWatchdog;
        Timer m_netTimeout;
        Timer m_netTGHang;
        Timer m_networkWatchdog;

        Timer m_adjSiteUpdate;

        Timer m_ccPacketInterval;

        StopWatch m_interval;

        uint32_t m_hangCount;
        uint32_t m_tduPreambleCount;

        uint8_t m_frameLossCnt;
        uint8_t m_frameLossThreshold;

        uint8_t m_ccFrameCnt;
        uint8_t m_ccSeq;

        std::mt19937 m_random;

        uint8_t* m_llaK;
        uint8_t* m_llaRS;
        uint8_t* m_llaCRS;
        uint8_t* m_llaKS;

        NID m_nid;

        SiteData m_siteData;

        ::lookups::RSSIInterpolator* m_rssiMapper;
        uint8_t m_rssi;
        uint8_t m_maxRSSI;
        uint8_t m_minRSSI;
        uint32_t m_aveRSSI;
        uint32_t m_rssiCount;

        bool m_notifyCC;

        bool m_verbose;
        bool m_debug;

        /// <summary>Add data frame to the data ring buffer.</summary>
        void addFrame(const uint8_t* data, uint32_t length, bool net = false, bool imm = false);

        /// <summary>Process a data frames from the network.</summary>
        void processNetwork();
        /// <summary>Helper to process loss of frame stream from modem.</summary>
        void processFrameLoss();

        /// <summary>Helper to send a REST API request to the CC to release a channel grant at the end of a call.</summary>
        void notifyCC_ReleaseGrant(uint32_t dstId);
        /// <summary>Helper to send a REST API request to the CC to "touch" a channel grant to refresh grant timers.</summary>
        void notifyCC_TouchGrant(uint32_t dstId);

        /// <summary>Helper to write control channel frame data.</summary>
        bool writeRF_ControlData();
        /// <summary>Helper to write end of control channel frame data.</summary>
        bool writeRF_ControlEnd();

        /// <summary>Helper to write data nulls.</summary>
        void writeRF_Nulls();
        /// <summary>Helper to write TDU preamble packet burst.</summary>
        void writeRF_Preamble(uint32_t preambleCount = 0, bool force = false);
        /// <summary>Helper to write a P25 TDU packet.</summary>
        void writeRF_TDU(bool noNetwork);

        /// <summary>Helper to setup and generate LLA AM1 parameters.</summary>
        void generateLLA_AM1_Parameters();
    };
} // namespace p25

#endif // __P25_CONTROL_H__
