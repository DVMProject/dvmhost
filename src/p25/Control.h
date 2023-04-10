/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2016,2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2023 by Bryan Biedenkapp N2PLL
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
#if !defined(__P25_CONTROL_H__)
#define __P25_CONTROL_H__

#include "Defines.h"
#include "p25/NID.h"
#include "p25/SiteData.h"
#include "p25/packet/Data.h"
#include "p25/packet/Voice.h"
#include "p25/packet/Trunk.h"
#include "network/BaseNetwork.h"
#include "lookups/RSSIInterpolator.h"
#include "lookups/IdenTableLookup.h"
#include "lookups/RadioIdLookup.h"
#include "lookups/TalkgroupIdLookup.h"
#include "p25/lookups/P25AffiliationLookup.h"
#include "modem/Modem.h"
#include "RingBuffer.h"
#include "Timer.h"
#include "yaml/Yaml.h"

#include <cstdio>
#include <vector>
#include <unordered_map>

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    namespace packet { class HOST_SW_API Voice; }
    namespace dfsi { namespace packet { class HOST_SW_API DFSIVoice; } }
    namespace packet { class HOST_SW_API Data; }
    namespace packet { class HOST_SW_API Trunk; }
    namespace dfsi { namespace packet { class HOST_SW_API DFSITrunk; } }
    namespace lookups { class HOST_SW_API P25AffiliationLookup; }

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements core logic for handling P25.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Control {
    public:
        /// <summary>Initializes a new instance of the Control class.</summary>
        Control(bool authoritative, uint32_t nac, uint32_t callHang, uint32_t queueSize, modem::Modem* modem, network::BaseNetwork* network,
            uint32_t timeout, uint32_t tgHang, bool duplex, ::lookups::RadioIdLookup* ridLookup,
            ::lookups::TalkgroupIdLookup* tidLookup, ::lookups::IdenTableLookup* idenTable, ::lookups::RSSIInterpolator* rssiMapper,
            bool dumpPDUData, bool repeatPDU, bool dumpTSBKData, bool debug, bool verbose);
        /// <summary>Finalizes a instance of the Control class.</summary>
        ~Control();

        /// <summary>Resets the data states for the RF interface.</summary>
        void reset();

        /// <summary>Helper to set P25 configuration options.</summary>
        void setOptions(yaml::Node& conf, bool supervisor, const std::string cwCallsign, const std::vector<uint32_t> voiceChNo,
            const std::unordered_map<uint32_t, ::lookups::VoiceChData> voiceChData, uint32_t pSuperGroup, uint32_t netId,
            uint32_t sysId, uint8_t rfssId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, bool printOptions);

        /// <summary>Gets a flag indicating whether the P25 control channel is running.</summary>
        bool getCCRunning() { return m_ccRunning; }
        /// <summary>Sets a flag indicating whether the P25 control channel is running.</summary>
        void setCCRunning(bool ccRunning) { m_ccPrevRunning = m_ccRunning; m_ccRunning = ccRunning; }
        /// <summary>Gets a flag indicating whether the P25 control channel is running.</summary>
        bool getCCHalted() { return m_ccHalted; }
        /// <summary>Sets a flag indicating whether the P25 control channel is halted.</summary>
        void setCCHalted(bool ccHalted) { m_ccHalted = ccHalted; }

        /// <summary>Process a data frame from the RF interface.</summary>
        bool processFrame(uint8_t* data, uint32_t len);
        /// <summary>Get the frame data length for the next frame in the data ring buffer.</summary>
        uint32_t peekFrameLength();
        /// <summary>Get frame data from data ring buffer.</summary>
        uint32_t getFrame(uint8_t* data);

        /// <summary>Helper to write P25 adjacent site information to the network.</summary>
        void writeAdjSSNetwork();

        /// <summary>Helper to write end of voice call frame data.</summary>
        bool writeRF_VoiceEnd();

        /// <summary>Updates the processor by the passed number of milliseconds.</summary>
        void clock(uint32_t ms);

        /// <summary>Sets a flag indicating whether P25 has supervisory functions and can send permit TG to voice channels.</summary>
        void setSupervisor(bool supervisor) { m_supervisor = supervisor; }
        /// <summary>Permits a TGID on a non-authoritative host.</summary>
        void permittedTG(uint32_t dstId);

        /// <summary>Gets instance of the NID class.</summary>
        NID nid() { return m_nid; }
        /// <summary>Gets instance of the Trunk class.</summary>
        packet::Trunk* trunk() { return m_trunk; }
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

    private:
        friend class packet::Voice;
        friend class dfsi::packet::DFSIVoice;
        packet::Voice* m_voice;
        friend class packet::Data;
        packet::Data* m_data;
        friend class packet::Trunk;
        friend class dfsi::packet::DFSITrunk;
        packet::Trunk* m_trunk;
        friend class lookups::P25AffiliationLookup;

        bool m_authoritative;
        bool m_supervisor;

        uint32_t m_nac;
        uint32_t m_txNAC;
        uint32_t m_timeout;

        modem::Modem* m_modem;
        network::BaseNetwork* m_network;

        bool m_inhibitIllegal;
        bool m_legacyGroupGrnt;
        bool m_legacyGroupReg;

        bool m_duplex;
        bool m_control;
        bool m_dedicatedControl;
        bool m_voiceOnControl;
        bool m_ackTSBKRequests;
        bool m_disableNetworkHDU;

        ::lookups::IdenTableLookup* m_idenTable;
        ::lookups::RadioIdLookup* m_ridLookup;
        ::lookups::TalkgroupIdLookup* m_tidLookup;
        lookups::P25AffiliationLookup m_affiliations;

        ::lookups::IdenTable m_idenEntry;

        RingBuffer<uint8_t> m_queue;

        RPT_RF_STATE m_rfState;
        uint32_t m_rfLastDstId;
        RPT_NET_STATE m_netState;
        uint32_t m_netLastDstId;

        uint32_t m_permittedDstId;

        bool m_tailOnIdle;
        bool m_ccRunning;
        bool m_ccPrevRunning;
        bool m_ccHalted;

        Timer m_rfTimeout;
        Timer m_rfTGHang;
        Timer m_netTimeout;
        Timer m_networkWatchdog;

        Timer m_ccPacketInterval;

        uint32_t m_hangCount;
        uint32_t m_tduPreambleCount;

        uint8_t m_ccFrameCnt;
        uint8_t m_ccSeq;

        NID m_nid;

        SiteData m_siteData;

        ::lookups::RSSIInterpolator* m_rssiMapper;
        uint8_t m_rssi;
        uint8_t m_maxRSSI;
        uint8_t m_minRSSI;
        uint32_t m_aveRSSI;
        uint32_t m_rssiCount;

        bool m_verbose;
        bool m_debug;

        /// <summary>Add data frame to the data ring buffer.</summary>
        void addFrame(const uint8_t* data, uint32_t length, bool net = false);

#if ENABLE_DFSI_SUPPORT
        /// <summary>Process a DFSI data frame from the RF interface.</summary>
        bool processDFSI(uint8_t* data, uint32_t len);
#endif

        /// <summary>Process a data frames from the network.</summary>
        void processNetwork();

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
    };
} // namespace p25

#endif // __P25_CONTROL_H__
