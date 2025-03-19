// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file Slot.h
 * @ingroup host_dmr
 * @file Slot.cpp
 * @ingroup host_dmr
 */
#if !defined(__DMR_SLOT_H__)
#define __DMR_SLOT_H__

#include "Defines.h"
#include "common/dmr/SiteData.h"
#include "common/lookups/RSSIInterpolator.h"
#include "common/lookups/IdenTableLookup.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/network/Network.h"
#include "common/RingBuffer.h"
#include "common/StopWatch.h"
#include "common/Timer.h"
#include "dmr/Control.h"
#include "dmr/lookups/DMRAffiliationLookup.h"
#include "dmr/packet/ControlSignaling.h"
#include "dmr/packet/Data.h"
#include "dmr/packet/Voice.h"
#include "modem/Modem.h"

#include <vector>
#include <mutex>

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
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief This structure contains shortened data for adjacent sites.
     * @ingroup host_dmr
     */
    struct AdjSiteData 
    {
        public:
            /**
             * @brief Channel Number.
             */
            uint32_t channelNo;
            /**
             * @brief System Identity.
             */
            uint32_t systemIdentity;
            /**
             * @brief DMR require registration.
             */
            bool requireReg;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief This class implements core logic for handling DMR slots.
     * @ingroup host_dmr
     */
    class HOST_SW_API Slot {
    public:
        /**
         * @brief Initializes a new instance of the Slot class.
         * @param slotNo DMR slot number.
         * @param queueSize Modem frame buffer queue size (bytes).
         * @param timeout Transmit timeout.
         * @param tgHang Amount of time to hang on the last talkgroup mode from RF.
         * @param dumpDataPacket Flag indicating whether data packets are dumped to the log.
         * @param repeatDataPacket Flag indicating whether incoming data packets will be repeated automatically.
         * @param dumpCSBKData Flag indicating whether CSBK data is dumped to the log.
         * @param debug Flag indicating whether DMR debug is enabled.
         * @param verbose Flag indicating whether DMR verbose logging is enabled.
         */
        Slot(uint32_t slotNo, uint32_t timeout, uint32_t tgHang, uint32_t queueSize, bool dumpDataPacket, bool repeatDataPacket,
            bool dumpCSBKData, bool debug, bool verbose);
        /**
         * @brief Finalizes a instance of the Slot class.
         */
        ~Slot();

        /** @name CC Control */
        /**
         * @brief Gets a flag indicating whether the control channel is running.
         * @returns bool Flag indicating whether the control channel is running.
         */
        bool getCCRunning() const { return m_ccRunning; }
        /**
         * @brief Sets a flag indicating whether the control channel is running.
         * @param ccRunning Flag indicating whether the control channel is running.
         */
        void setCCRunning(bool ccRunning) { m_ccPrevRunning = m_ccRunning; m_ccRunning = ccRunning; }
        /**
         * @brief Gets a flag indicating whether the control channel is running.
         * @returns bool Flag indicating whether the control channel is running.
         */
        bool getCCHalted() const { return m_ccHalted; }
        /**
         * @brief Sets a flag indicating whether the control channel is halted.
         * @param ccHalted Flag indicating whether the control channel is halted.
         */
        void setCCHalted(bool ccHalted) { m_ccHalted = ccHalted; }
        /** @} */

        /** @name Frame Processing */
        /**
         * @brief Process a data frame from the RF interface.
         * @param data Buffer containing data frame.
         * @param len Length of data frame.
         * @returns bool True, if frame was successfully processed, otherwise false.
         */
        bool processFrame(uint8_t* data, uint32_t len);
        /**
         * @brief Get the frame data length for the next frame in the data ring buffer.
         * @returns uint32_t Length of frame data retrieved.
         */
        uint32_t peekFrameLength();
        /**
         * @brief Helper to determine whether or not the internal frame queue is full.
         * @returns bool True if frame queue is full, otherwise false.
         */
        bool isQueueFull();
        /**
         * @brief Get frame data from data ring buffer.
         * @param[out] data Buffer to store frame data.
         * @returns uint32_t Length of frame data retrieved.
         */
        uint32_t getFrame(uint8_t* data);

        /**
         * @brief Process a data frames from the network.
         * @param[in] data Instance of data::Data DMR data container class.
         */
        void processNetwork(const data::NetData& data);
        /** @} */

        /** @name In-Call Control */
        /**
         * @brief Helper to process an In-Call Control message.
         * @param command In-Call Control Command.
         * @param dstId Destination ID.
         */
        void processInCallCtrl(network::NET_ICC::ENUM command, uint32_t dstId);
        /** @} */

        /** @name Data Clocking */
        /**
         * @brief Updates the processor.
         */
        void clock();
        /**
         * @brief Updates the adj. site tables and affiliations.
         * @param ms Number of milliseconds.
         */
        void clockSiteData(uint32_t ms);
        /** @} */

        /** @name Supervisory Control */
        /**
         * @brief Sets a flag indicating whether control has supervisory functions and can send permit TG to voice channels.
         * @param supervisor Flag indicating whether control has supervisory functions.
         */
        void setSupervisor(bool supervisor) { m_supervisor = supervisor; }
        /**
         * @brief Permits a TGID on a non-authoritative host.
         * @param dstId Destination ID.
         */
        void permittedTG(uint32_t dstId);
        /**
         * @brief Grants a TGID on a non-authoritative host.
         * @param srcId Source Radio ID.
         * @param dstId Destination ID.
         * @param grp Flag indicating group grant.
         */
        void grantTG(uint32_t srcId, uint32_t dstId, bool grp);
        /**
         * @brief Releases a granted TG.
         * @param dstId Destination ID.
         */
        void releaseGrantTG(uint32_t dstId);
        /**
         * @brief Touches a granted TG to keep a channel grant alive.
         * @param dstId Destination ID.
         */
        void touchGrantTG(uint32_t dstId);
        /** @} */

        /**
         * @brief Gets instance of the ControlSignaling class.
         * @returns ControlSignaling* Instance of the ControlSignaling class.
         */
        packet::ControlSignaling* control() { return m_control; }

        /**
         * @brief Returns the current operating RF state of the NXDN controller.
         * @returns RPT_RF_STATE 
         */
        RPT_RF_STATE getRFState() const { return m_rfState; }
        /**
         * @brief Clears the current operating RF state back to idle (with no data reset!).
         */
        void clearRFReject();

        /**
         * @brief Helper to change the debug and verbose state.
         * @param debug Flag indicating whether debug is enabled or not.
         * @param verbose Flag indicating whether verbosity is enabled or not.
         */
        void setDebugVerbose(bool debug, bool verbose);

        /**
         * @brief Helper to enable and configure TSCC support for this slot.
         * @param enable Flag indicating TSCC support is enabled for this slot.
         * @param dedicated Flag indicating this slot is the dedicated TSCC.
         */
        void setTSCC(bool enable, bool dedicated);
        /**
         * @brief Helper to activate a TSCC payload slot.
         * @param dstId Destination ID.
         * @param srcId Source Radio ID.
         * @param group Flag indicating group grant.
         * @param voice Flag indicating if the payload traffic is voice.
         */
        void setTSCCActivated(uint32_t dstId, uint32_t srcId, bool group, bool voice);

        /**
         * @brief Sets a flag indicating whether the slot has will perform source ID checks before issuing a grant.
         * @param disableSourceIdGrant Flag indicating whether the slot has will perform source ID checks before issuing a grant.
         */
        void setDisableSourceIDGrantCheck(bool disableSourceIdGrant) { m_disableGrantSrcIdCheck = disableSourceIdGrant; }
        /**
         * @brief Sets a flag indicating whether the voice channels will notify the TSCC of traffic channel changes.
         * @param notifyCC Flag indicating whether the voice channels will notify the TSCC of traffic channel changes.
         */
        void setNotifyCC(bool notifyCC) { m_notifyCC = notifyCC; }

        /**
         * @brief Helper to set the voice error silence threshold.
         * @param threshold Voice error silence threshold.
         */
        void setSilenceThreshold(uint32_t threshold) { m_silenceThreshold = threshold; }
        /**
         * @brief Helper to set the frame loss threshold.
         * @param threshold Frame loss threashold.
         */
        void setFrameLossThreshold(uint32_t threshold) { m_frameLossThreshold = threshold; }

        /**
         * @brief Helper to get the last transmitted destination ID.
         * @returns uint32_t Last transmitted Destination ID.
         */
        uint32_t getLastDstId() const;
        /**
         * @brief Helper to get the last transmitted source ID.
         * @returns uint32_t Last transmitted source radio ID.
         */
        uint32_t getLastSrcId() const;

        /**
         * @brief Helper to initialize the slot processor.
         * @param dmr Instance of the Control class.
         * @param authoritative Flag indicating whether or not the DVM is grant authoritative.
         * @param colorCode DMR access color code.
         * @param siteData DMR site data.
         * @param embeddedLCOnly 
         * @param dumpTAData 
         * @param callHang Amount of hangtime for a DMR call.
         * @param modem Instance of the Modem class.
         * @param network Instance of the BaseNetwork class.
         * @param duplex Flag indicating full-duplex operation.
         * @param chLookup Instance of the ChannelLookup class.
         * @param ridLookup Instance of the RadioIdLookup class.
         * @param tidLookup Instance of the TalkgroupRulesLookup class.
         * @param idenTable Instance of the IdenTableLookup class.
         * @param rssi Instance of the RSSIInterpolator class.
         * @param jitter 
         * @param verbose Flag indicating whether DMR verbose logging is enabled.
         */
        static void init(Control* dmr, bool authoritative, uint32_t colorCode, SiteData siteData, bool embeddedLCOnly, bool dumpTAData, uint32_t callHang, modem::Modem* modem,
            network::Network* network, bool duplex, ::lookups::ChannelLookup* chLookup, ::lookups::RadioIdLookup* ridLookup, ::lookups::TalkgroupRulesLookup* tidLookup,
            ::lookups::IdenTableLookup* idenTable, ::lookups::RSSIInterpolator* rssiMapper, uint32_t jitter, bool verbose);
        /**
         * @brief Sets local configured site data.
         * @param controlChData ::lookups::VoiceChData instance representing the control channel.
         * @param netId DMR Network ID.
         * @param siteId DMR Site ID.
         * @param channelId Channel ID.
         * @param channelNo Channel Number.
         * @param requireReq Flag indicating this site requires registration.
         */
        static void setSiteData(::lookups::VoiceChData controlChData, uint32_t netId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, bool requireReq);
        /**
         * @brief Sets TSCC Aloha configuration.
         * @param nRandWait Random Access Wait Period
         * @param backOff 
         */
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
        std::mutex m_queueLock;

        RPT_RF_STATE m_rfState;
        uint32_t m_rfLastDstId;
        uint32_t m_rfLastSrcId;
        RPT_NET_STATE m_netState;
        uint32_t m_netLastDstId;
        uint32_t m_netLastSrcId;

        uint32_t m_permittedDstId;

        std::unique_ptr<lc::LC> m_rfLC;
        std::unique_ptr<lc::PrivacyLC> m_rfPrivacyLC;

        uint8_t m_rfSeqNo;

        std::unique_ptr<lc::LC> m_netLC;
        std::unique_ptr<lc::PrivacyLC> m_netPrivacyLC;

        Timer m_networkWatchdog;
        Timer m_rfTimeoutTimer;
        Timer m_rfTGHang;
        Timer m_rfLossWatchdog;
        Timer m_netTimeoutTimer;
        Timer m_netTGHang;
        Timer m_packetTimer;

        std::unordered_map<uint8_t, AdjSiteData> m_adjSiteTable;
        std::unordered_map<uint8_t, uint8_t> m_adjSiteUpdateCnt;

        Timer m_adjSiteUpdateTimer;
        uint32_t m_adjSiteUpdateInterval;
        Timer m_adjSiteUpdate;

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
        bool m_ignoreAffiliationCheck;
        bool m_disableNetworkGrant;
        bool m_convNetGrantDemand;

        uint32_t m_tsccPayloadDstId;
        uint32_t m_tsccPayloadSrcId;
        bool m_tsccPayloadGroup;
        bool m_tsccPayloadVoice;
        Timer m_tsccPayloadActRetry;
        uint8_t m_tsccAdjSSCnt;

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

        /**
         * @brief Short LC Activity Type
         */
        enum SLCO_ACT_TYPE {
            NONE,       //! None
            VOICE,      //! Voice
            DATA,       //! Data
            CSBK        //! CSBK
        };

        static defines::FLCO::E m_flco1;
        static uint8_t m_id1;
        static SLCO_ACT_TYPE m_actType1;

        static defines::FLCO::E m_flco2;
        static uint8_t m_id2;
        static SLCO_ACT_TYPE m_actType2;

        static bool m_verifyReg;

        static uint8_t m_alohaNRandWait;
        static uint8_t m_alohaBackOff;

        /**
         * @brief Add data frame to the data ring buffer.
         * @param data Frame data to add to Tx queue.
         * @param length Length of data to add.
         * @param net Flag indicating whether the data came from the network or not
         * @param imm Flag indicating whether or not the data is priority and is added to the immediate queue.
         */
        void addFrame(const uint8_t* data, bool net = false, bool imm = false);

        /**
         * @brief Helper to process loss of frame stream from modem.
         */
        void processFrameLoss();

        /**
         * @brief Helper to send a REST API request to the CC to release a channel grant at the end of a call.
         * @param dstId Destination ID.
         */
        void notifyCC_ReleaseGrant(uint32_t dstId);
        /**
         * @brief Helper to send a REST API request to the CC to "touch" a channel grant to refresh grant timers.
         * @param dstId Destination ID.
         */
        void notifyCC_TouchGrant(uint32_t dstId);

        /**
         * @brief Write data frame to the network.
         * @param[in] data Buffer containing frame data to write to the network.
         * @param dataType DMR Data Type for this frame.
         * @param control Control Byte.
         * @param errors Number of bit errors detected for this frame.
         * @param noSequence Flag indicating this frame carries no sequence number.
         */
        void writeNetwork(const uint8_t* data, defines::DataType::E dataType, uint8_t control, uint8_t errors = 0U, bool noSequence = false);
        /**
         * @brief Write data frame to the network.
         * @param[in] data Buffer containing frame data to write to the network.
         * @param dataType DMR Data Type for this frame.
         * @param flco Full-Link Control Opcode.
         * @param srcId Source Radio ID.
         * @param dstId Destination ID.
         * @param control Control Byte.
         * @param errors Number of bit errors detected for this frame.
         * @param noSequence Flag indicating this frame carries no sequence number.
         */
        void writeNetwork(const uint8_t* data, defines::DataType::E dataType, defines::FLCO::E flco, uint32_t srcId,
            uint32_t dstId, uint8_t control, uint8_t errors = 0U, bool noSequence = false);

        /**
         * @brief Helper to write RF end of frame data.
         * @param writeEnd Flag indicating end of transmission should be written to air.
         */
        void writeEndRF(bool writeEnd = false);
        /**
         * @brief Helper to write network end of frame data.
         * @param writeEnd Flag indicating end of transmission should be written to air.
         */
        void writeEndNet(bool writeEnd = false);

        /**
         * @brief Helper to write control channel packet data.
         * @param frameCnt Frame counter.
         * @param n 
         */
        void writeRF_ControlData(uint16_t frameCnt, uint8_t n);

        /**
         * @brief Clears the flag indicating whether the slot is a TSCC payload slot.
         */
        void clearTSCCActivated();

        /**
         * @brief Helper to set the DMR short LC.
         * @param slotNo DMR slot number.
         * @param id ID.
         * @param flco Full-Link Control Opcode.
         * @param actType Traffic activity type.
         */
        static void setShortLC(uint32_t slotNo, uint32_t id, defines::FLCO::E flco = defines::FLCO::GROUP, Slot::SLCO_ACT_TYPE actType = Slot::SLCO_ACT_TYPE::NONE);
        /**
         * @brief Helper to set the DMR short LC for TSCC.
         * @param siteData Instance of the dmr::SiteData class.
         * @param counter TSCC CSC counter.
         */
        static void setShortLC_TSCC(SiteData siteData, uint16_t counter);
        /**
         * @brief Helper to set the DMR short LC for payload.
         * @param siteData Instance of the dmr::SiteData class.
         * @param counter TSCC CSC counter.
         */
        static void setShortLC_Payload(SiteData siteData, uint16_t counter);
    };
} // namespace dmr

#endif // __DMR_SLOT_H__
