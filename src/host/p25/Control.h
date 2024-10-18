// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup host_p25 Project 25
 * @brief Implementation for the TIA-102 Project 25 standard.
 * @ingroup host
 *
 * @defgroup host_p25_tsbk Trunk Signalling Block
 * @brief Implementation for the data handling for the Project 25 standard (TSDU/TSBK)
 * @ingroup host_p25
 * 
 * @file Control.h
 * @ingroup host_p25
 * @file Control.cpp
 * @ingroup host_p25
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
#include <mutex>

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
    // ---------------------------------------------------------------------------

    /**
     * @brief This class implements core logic for handling P25.
     * @ingroup host_p25
     */
    class HOST_SW_API Control {
    public:
        /**
         * @brief Initializes a new instance of the Control class.
         * @param authoritative Flag indicating whether or not the DVM is grant authoritative.
         * @param nac P25 Network Access Code.
         * @param callHang Amount of hangtime for a P25 call.
         * @param queueSize Modem frame buffer queue size (bytes).
         * @param modem Instance of the Modem class.
         * @param network Instance of the BaseNetwork class.
         * @param timeout Transmit timeout.
         * @param tgHang Amount of time to hang on the last talkgroup mode from RF.
         * @param duplex Flag indicating full-duplex operation.
         * @param chLookup Instance of the ChannelLookup class.
         * @param ridLookup Instance of the RadioIdLookup class.
         * @param tidLookup Instance of the TalkgroupRulesLookup class.
         * @param idenTable Instance of the IdenTableLookup class.
         * @param rssi Instance of the RSSIInterpolator class.
         * @param dumpPDUData Flag indicating whether PDU data is dumped to the log.
         * @param repeatPDU Flag indicating whether incoming PDUs will be repeated automatically.
         * @param dumpTSBKData Flag indicating whether TSBK data is dumped to the log.
         * @param debug Flag indicating whether P25 debug is enabled.
         * @param verbose Flag indicating whether P25 verbose logging is enabled.
         */
        Control(bool authoritative, uint32_t nac, uint32_t callHang, uint32_t queueSize, modem::Modem* modem, network::Network* network,
            uint32_t timeout, uint32_t tgHang, bool duplex, ::lookups::ChannelLookup* chLookup, ::lookups::RadioIdLookup* ridLookup,
            ::lookups::TalkgroupRulesLookup* tidLookup, ::lookups::IdenTableLookup* idenTable, ::lookups::RSSIInterpolator* rssiMapper,
            bool dumpPDUData, bool repeatPDU, bool dumpTSBKData, bool debug, bool verbose);
        /**
         * @brief Finalizes a instance of the Control class.
         */
        ~Control();

        /**
         * @brief Resets the data states for the RF interface.
         */
        void reset();

        /**
         * @brief Helper to set P25 configuration options.
         * @param conf Instance of the yaml::Node class.
         * @param supervisor Flag indicating whether the DMR has supervisory functions.
         * @param cwCallsign CW callsign of this host.
         * @param controlChData Control Channel data.
         * @param pSuperGroup 
         * @param netId P25 Network ID.
         * @param sysId P25 System ID.
         * @param rfssId P25 RFSS ID.
         * @param siteId P25 Site ID.
         * @param channelId Channel ID.
         * @param channelNo Channel Number.
         * @param printOptions Flag indicating whether or not options should be printed to log.
         */
        void setOptions(yaml::Node& conf, bool supervisor, const std::string cwCallsign, const ::lookups::VoiceChData controlChData,
            uint32_t netId, uint32_t sysId, uint8_t rfssId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, bool printOptions);

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
        /** @} */

        /**
         * @brief Helper to write end of voice call frame data.
         * @returns bool True, if end of voice call was written, otherwise false.
         */
        bool writeRF_VoiceEnd();

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
         * @param dataPermit Flag indicating this permit is for data.
         */
        void permittedTG(uint32_t dstId, bool dataPermit = false);
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
         * @brief Gets instance of the NID class.
         * @returns NID Instance of the NID class.
         */
        NID nid() { return m_nid; }
        /**
         * @brief Gets instance of the ControlSignaling class.
         * @returns ControlSignaling* Instance of the ControlSignaling class.
         */
        packet::ControlSignaling* control() { return m_control; }
        /**
         * @brief Gets instance of the P25AffiliationLookup class.
         * @returns P25AffiliationLookup Instance of the P25AffiliationLookup class.
         */
        lookups::P25AffiliationLookup affiliations() { return m_affiliations; }

        /**
         * @brief Flag indicating whether the processor or is busy or not.
         * @returns bool True, if processor is busy, otherwise false.
         */
        bool isBusy() const;

        /**
         * @brief Flag indicating whether debug is enabled or not.
         * @returns bool True, if debugging is enabled, otherwise false.
         */
        bool getDebug() const { return m_debug; };
        /**
         * @brief Flag indicating whether verbosity is enabled or not.
         * @returns bool True, if verbose logging is enabled, otherwise false.
         */
        bool getVerbose() const { return m_verbose; };
        /**
         * @brief Helper to change the debug and verbose state.
         * @param debug Flag indicating whether debug is enabled or not.
         * @param verbose Flag indicating whether verbosity is enabled or not.
         */
        void setDebugVerbose(bool debug, bool verbose);

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
        bool m_isModemDFSI;
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
        bool m_sndcpSupport;
        bool m_ignoreAffiliationCheck;

        ::lookups::IdenTableLookup* m_idenTable;
        ::lookups::RadioIdLookup* m_ridLookup;
        ::lookups::TalkgroupRulesLookup* m_tidLookup;
        lookups::P25AffiliationLookup m_affiliations;
        ::lookups::VoiceChData m_controlChData;

        ::lookups::IdenTable m_idenEntry;

        RingBuffer<uint8_t> m_txImmQueue;
        RingBuffer<uint8_t> m_txQueue;
        static std::mutex m_queueLock;

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

        /**
         * @brief Add data frame to the data ring buffer.
         * @param data Frame data to add to Tx queue.
         * @param length Length of data to add.
         * @param net Flag indicating whether the data came from the network or not
         * @param imm Flag indicating whether or not the data is priority and is added to the immediate queue.
         */
        void addFrame(const uint8_t* data, uint32_t length, bool net = false, bool imm = false);

        /**
         * @brief Process a data frames from the network.
         */
        void processNetwork();
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
         * @brief Helper to write control channel frame data.
         * @returns bool True, if control data is written, otherwise false.
         */
        bool writeRF_ControlData();
        /**
         * @brief Helper to write end of control channel frame data.
         * @returns bool True, if end of control channel is written, otherwise false.
         */
        bool writeRF_ControlEnd();

        /**
         * @brief Helper to write data nulls.
         */
        void writeRF_Nulls();
        /**
         * @brief Helper to write TDU preamble packet burst.
         * @param preambleCount Number of preamble TDUs to transmit.
         * @param force Force transmitting preamble TDUs.
         */
        void writeRF_Preamble(uint32_t preambleCount = 0, bool force = false);
        /**
         * @brief Helper to write a P25 TDU packet.
         * @param noNetwork Flag indicating this TDU shouldn't be written to the network.
         * @param imm Flag indicating the TDU should be written to the immediate queue.
         */
        void writeRF_TDU(bool noNetwork, bool imm = false);

        /**
         * @brief Helper to setup and generate LLA AM1 parameters.
         */
        void generateLLA_AM1_Parameters();
    };
} // namespace p25

#endif // __P25_CONTROL_H__
