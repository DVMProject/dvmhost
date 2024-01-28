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
*   Copyright (C) 2017-2023 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DMR_SLOT_H__)
#define __DMR_SLOT_H__

#include "Defines.h"
#include "common/dmr/SiteData.h"
#include "common/lookups/RSSIInterpolator.h"
#include "common/lookups/IdenTableLookup.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/RingBuffer.h"
#include "common/StopWatch.h"
#include "common/Timer.h"
#include "dmr/Control.h"
#include "dmr/lookups/DMRAffiliationLookup.h"
#include "dmr/packet/ControlSignaling.h"
#include "dmr/packet/Data.h"
#include "dmr/packet/Voice.h"
#include "modem/Modem.h"
#include "network/Network.h"

#include <vector>

namespace dmr
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    class HOST_SW_API Control;
    namespace packet { class HOST_SW_API Voice; }
    namespace packet { class HOST_SW_API Data; }
    namespace packet { class HOST_SW_API ControlSignaling; }

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements core logic for handling DMR slots.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Slot {
    public:
        /// <summary>Initializes a new instance of the Slot class.</summary>
        Slot(uint32_t slotNo, uint32_t timeout, uint32_t tgHang, uint32_t queueSize, bool dumpDataPacket, bool repeatDataPacket,
            bool dumpCSBKData, bool debug, bool verbose);
        /// <summary>Finalizes a instance of the Slot class.</summary>
        ~Slot();

        /// <summary>Gets a flag indicating whether the P25 control channel is running.</summary>
        bool getCCRunning() const { return m_ccRunning; }
        /// <summary>Sets a flag indicating whether the P25 control channel is running.</summary>
        void setCCRunning(bool ccRunning) { m_ccPrevRunning = m_ccRunning; m_ccRunning = ccRunning; }
        /// <summary>Gets a flag indicating whether the DMR control channel is running.</summary>
        bool getCCHalted() const { return m_ccHalted; }
        /// <summary>Sets a flag indicating whether the DMR control channel is halted.</summary>
        void setCCHalted(bool ccHalted) { m_ccHalted = ccHalted; }

        /// <summary>Process a data frame from the RF interface.</summary>
        bool processFrame(uint8_t* data, uint32_t len);
        /// <summary>Get data frame from data ring buffer.</summary>
        uint32_t getFrame(uint8_t* data);

        /// <summary>Process a data frames from the network.</summary>
        void processNetwork(const data::Data& data);

        /// <summary>Updates the slot processor.</summary>
        void clock();

        /// <summary>Permits a TGID on a non-authoritative host.</summary>
        void permittedTG(uint32_t dstId);
        /// <summary>Grants a TGID on a non-authoritative host.</summary>
        void grantTG(uint32_t srcId, uint32_t dstId, bool grp);
        /// <summary>Releases a granted TG.</summary>
        void releaseGrantTG(uint32_t dstId);
        /// <summary>Touches a granted TG to keep a channel grant alive.</summary>
        void touchGrantTG(uint32_t dstId);

        /// <summary>Gets instance of the ControlSignaling class.</summary>
        packet::ControlSignaling* control() { return m_control; }

        /// <summary>Helper to change the debug and verbose state.</summary>
        void setDebugVerbose(bool debug, bool verbose);

        /// <summary>Helper to enable and configure TSCC support for this slot.</summary>
        void setTSCC(bool enable, bool dedicated);
        /// <summary>Helper to activate a TSCC payload slot.</summary>
        void setTSCCActivated(uint32_t dstId, uint32_t srcId, bool group, bool voice);
        /// <summary>Sets a flag indicating whether the slot has supervisory functions and can send permit TG to voice channels.</summary>
        void setSupervisor(bool supervisor) { m_supervisor = supervisor; }
        /// <summary>Sets a flag indicating whether the slot has will perform source ID checks before issuing a grant.</summary>
        void setDisableSourceIDGrantCheck(bool disableSourceIdGrant) { m_disableGrantSrcIdCheck = disableSourceIdGrant; }
        /// <summary>Sets a flag indicating whether the voice channels will notify the TSCC of traffic channel changes.</summary>
        void setNotifyCC(bool notifyCC) { m_notifyCC = notifyCC; }
        /// <summary>Helper to set the voice error silence threshold.</summary>
        void setSilenceThreshold(uint32_t threshold) { m_silenceThreshold = threshold; }
        /// <summary>Helper to set the frame loss threshold.</summary>
        void setFrameLossThreshold(uint32_t threshold) { m_frameLossThreshold = threshold; }

        /// <summary>Helper to get the last transmitted destination ID.</summary>
        uint32_t getLastDstId() const;
        /// <summary>Helper to get the last transmitted source ID.</summary>
        uint32_t getLastSrcId() const;

        /// <summary>Helper to initialize the slot processor.</summary>
        static void init(Control* dmr, bool authoritative, uint32_t colorCode, SiteData siteData, bool embeddedLCOnly, bool dumpTAData, uint32_t callHang, modem::Modem* modem,
            network::Network* network, bool duplex, ::lookups::RadioIdLookup* ridLookup, ::lookups::TalkgroupRulesLookup* tidLookup,
            ::lookups::IdenTableLookup* idenTable, ::lookups::RSSIInterpolator* rssiMapper, uint32_t jitter, bool verbose);
        /// <summary>Sets local configured site data.</summary>
        static void setSiteData(const std::vector<uint32_t> voiceChNo, const std::unordered_map<uint32_t, ::lookups::VoiceChData> voiceChData,
            ::lookups::VoiceChData controlChData, uint32_t netId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, bool requireReq);
        /// <summary>Sets TSCC Aloha configuration.</summary>
        static void setAlohaConfig(uint8_t nRandWait, uint8_t backOff);

    private:
        friend class Control;
        friend class packet::Voice;
        packet::Voice* m_voice;
        friend class packet::Data;
        packet::Data* m_data;
        friend class packet::ControlSignaling;
        packet::ControlSignaling* m_control;

        uint32_t m_slotNo;

        RingBuffer<uint8_t> m_txImmQueue;
        RingBuffer<uint8_t> m_txQueue;

        RPT_RF_STATE m_rfState;
        uint32_t m_rfLastDstId;
        uint32_t m_rfLastSrcId;
        RPT_NET_STATE m_netState;
        uint32_t m_netLastDstId;
        uint32_t m_netLastSrcId;

        uint32_t m_permittedDstId;

        std::unique_ptr<lc::LC> m_rfLC;
        std::unique_ptr<lc::PrivacyLC> m_rfPrivacyLC;
        std::unique_ptr<data::DataHeader> m_rfDataHeader;

        uint8_t m_rfSeqNo;

        std::unique_ptr<lc::LC> m_netLC;
        std::unique_ptr<lc::PrivacyLC> m_netPrivacyLC;
        std::unique_ptr<data::DataHeader> m_netDataHeader;

        Timer m_networkWatchdog;
        Timer m_rfTimeoutTimer;
        Timer m_rfTGHang;
        Timer m_rfLossWatchdog;
        Timer m_netTimeoutTimer;
        Timer m_netTGHang;
        Timer m_packetTimer;

        Timer m_ccPacketInterval;

        StopWatch m_interval;
        StopWatch m_elapsed;

        uint32_t m_rfFrames;
        uint32_t m_netFrames;
        uint32_t m_netLost;
        uint32_t m_netMissed;

        uint32_t m_rfBits;
        uint32_t m_netBits;
        uint32_t m_rfErrs;
        uint32_t m_netErrs;

        bool m_rfTimeout;
        bool m_netTimeout;

        uint8_t m_rssi;
        uint8_t m_maxRSSI;
        uint8_t m_minRSSI;
        uint32_t m_aveRSSI;
        uint32_t m_rssiCount;

        uint32_t m_silenceThreshold;

        uint8_t m_frameLossCnt;
        uint8_t m_frameLossThreshold;

        uint8_t m_ccSeq;
        bool m_ccRunning;
        bool m_ccPrevRunning;
        bool m_ccHalted;

        bool m_enableTSCC;
        bool m_dedicatedTSCC;

        uint32_t m_tsccPayloadDstId;
        uint32_t m_tsccPayloadSrcId;
        bool m_tsccPayloadGroup;
        bool m_tsccPayloadVoice;
        Timer m_tsccPayloadActRetry;

        bool m_disableGrantSrcIdCheck;

        uint32_t m_lastLateEntry;

        bool m_supervisor;
        bool m_notifyCC;

        bool m_verbose;
        bool m_debug;

        static Control* m_dmr;

        static bool m_authoritative;

        static uint32_t m_colorCode;

        static SiteData m_siteData;
        static uint32_t m_channelNo;

        static bool m_embeddedLCOnly;
        static bool m_dumpTAData;

        static modem::Modem* m_modem;
        static network::Network* m_network;

        static bool m_duplex;

        static ::lookups::IdenTableLookup* m_idenTable;
        static ::lookups::RadioIdLookup* m_ridLookup;
        static ::lookups::TalkgroupRulesLookup* m_tidLookup;
        static lookups::DMRAffiliationLookup* m_affiliations;
        static ::lookups::VoiceChData m_controlChData;

        static ::lookups::IdenTable m_idenEntry;

        static uint32_t m_hangCount;

        static ::lookups::RSSIInterpolator* m_rssiMapper;

        static uint32_t m_jitterTime;
        static uint32_t m_jitterSlots;

        static uint8_t* m_idle;

        static uint8_t m_flco1;
        static uint8_t m_id1;
        static bool m_voice1;

        static uint8_t m_flco2;
        static uint8_t m_id2;
        static bool m_voice2;

        static bool m_verifyReg;

        static uint8_t m_alohaNRandWait;
        static uint8_t m_alohaBackOff;

        /// <summary>Add data frame to the data ring buffer.</summary>
        void addFrame(const uint8_t* data, bool net = false, bool imm = false);

        /// <summary>Helper to process loss of frame stream from modem.</summary>
        void processFrameLoss();

        /// <summary>Helper to send a REST API request to the CC to release a channel grant at the end of a call.</summary>
        void notifyCC_ReleaseGrant(uint32_t dstId);
        /// <summary>Helper to send a REST API request to the CC to "touch" a channel grant to refresh grant timers.</summary>
        void notifyCC_TouchGrant(uint32_t dstId);

        /// <summary>Write data frame to the network.</summary>
        void writeNetwork(const uint8_t* data, uint8_t dataType, uint8_t errors = 0U, bool noSequence = false);
        /// <summary>Write data frame to the network.</summary>
        void writeNetwork(const uint8_t* data, uint8_t dataType, uint8_t flco, uint32_t srcId,
            uint32_t dstId, uint8_t errors = 0U, bool noSequence = false);

        /// <summary>Helper to write RF end of frame data.</summary>
        void writeEndRF(bool writeEnd = false);
        /// <summary>Helper to write network end of frame data.</summary>
        void writeEndNet(bool writeEnd = false);

        /// <summary>Helper to write control channel packet data.</summary>
        void writeRF_ControlData(uint16_t frameCnt, uint8_t n);

        /// <summary>Clears the flag indicating whether the slot is a TSCC payload slot.</summary>
        void clearTSCCActivated();

        /// <summary>Helper to set the DMR short LC.</summary>
        static void setShortLC(uint32_t slotNo, uint32_t id, uint8_t flco = FLCO_GROUP, bool voice = true);
        /// <summary>Helper to set the DMR short LC for TSCC.</summary>
        static void setShortLC_TSCC(SiteData siteData, uint16_t counter);
        /// <summary>Helper to set the DMR short LC for payload.</summary>
        static void setShortLC_Payload(SiteData siteData, uint16_t counter);
    };
} // namespace dmr

#endif // __DMR_SLOT_H__
