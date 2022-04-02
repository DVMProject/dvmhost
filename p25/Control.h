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
#if !defined(__P25_CONTROL_H__)
#define __P25_CONTROL_H__

#include "Defines.h"
#include "p25/TrunkPacket.h"
#include "p25/DataPacket.h"
#include "p25/VoicePacket.h"
#include "p25/NID.h"
#include "p25/SiteData.h"
#include "network/BaseNetwork.h"
#include "network/RemoteControl.h"
#include "lookups/RSSIInterpolator.h"
#include "lookups/IdenTableLookup.h"
#include "lookups/RadioIdLookup.h"
#include "lookups/TalkgroupIdLookup.h"
#include "modem/Modem.h"
#include "RingBuffer.h"
#include "Timer.h"
#include "yaml/Yaml.h"

#include <cstdio>

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------
    class HOST_SW_API VoicePacket;
    namespace dfsi { class HOST_SW_API DFSIVoicePacket; }
    class HOST_SW_API DataPacket;
    class HOST_SW_API TrunkPacket;
    namespace dfsi { class HOST_SW_API DFSITrunkPacket; }

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements core logic for handling P25.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Control {
    public:
        /// <summary>Initializes a new instance of the Control class.</summary>
        Control(uint32_t nac, uint32_t callHang, uint32_t queueSize, modem::Modem* modem, network::BaseNetwork* network,
            uint32_t timeout, uint32_t tgHang, uint32_t ccBcstInterval, bool duplex, lookups::RadioIdLookup* ridLookup,
            lookups::TalkgroupIdLookup* tidLookup, lookups::IdenTableLookup* idenTable, lookups::RSSIInterpolator* rssiMapper,
            bool dumpPDUData, bool repeatPDU, bool dumpTSBKData, bool debug, bool verbose);
        /// <summary>Finalizes a instance of the Control class.</summary>
        ~Control();

        /// <summary>Resets the data states for the RF interface.</summary>
        void reset();

        /// <summary>Helper to set P25 configuration options.</summary>
        void setOptions(yaml::Node& conf, const std::string cwCallsign, const std::vector<uint32_t> voiceChNo,
            uint32_t pSuperGroup, uint32_t netId, uint32_t sysId, uint8_t rfssId, uint8_t siteId,
            uint8_t channelId, uint32_t channelNo, bool printOptions);
        /// <summary>Sets a flag indicating whether the P25 control channel is running.</summary>
        void setCCRunning(bool ccRunning);

        /// <summary>Process a data frame from the RF interface.</summary>
        bool processFrame(uint8_t* data, uint32_t len);
        /// <summary>Get frame data from data ring buffer.</summary>
        uint32_t getFrame(uint8_t* data);

        /// <summary>Helper to write P25 adjacent site information to the network.</summary>
        void writeAdjSSNetwork();

        /// <summary>Helper to write control channel frame data.</summary>
        bool writeControlRF();
        /// <summary>Helper to write end of control channel frame data.</summary>
        bool writeControlEndRF();
        /// <summary>Helper to write end of frame data.</summary>
        bool writeEndRF();

        /// <summary>Updates the processor by the passed number of milliseconds.</summary>
        void clock(uint32_t ms);

        /// <summary>Gets instance of the NID class.</summary>
        NID nid() { return m_nid; }
        /// <summary>Gets instance of the TrunkPacket class.</summary>
        TrunkPacket* trunk() { return m_trunk; }

        /// <summary>Helper to change the debug and verbose state.</summary>
        void setDebugVerbose(bool debug, bool verbose);

    private:
        friend class VoicePacket;
        friend class dfsi::DFSIVoicePacket;
        VoicePacket* m_voice;
        friend class DataPacket;
        DataPacket* m_data;
        friend class TrunkPacket;
        friend class dfsi::DFSITrunkPacket;
        TrunkPacket* m_trunk;

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

        lookups::IdenTableLookup* m_idenTable;
        lookups::RadioIdLookup* m_ridLookup;
        lookups::TalkgroupIdLookup* m_tidLookup;

        lookups::IdenTable m_idenEntry;

        RingBuffer<uint8_t> m_queue;

        RPT_RF_STATE m_rfState;
        uint32_t m_rfLastDstId;
        RPT_NET_STATE m_netState;
        uint32_t m_netLastDstId;

        bool m_tailOnIdle;
        bool m_ccOnIdle;
        bool m_ccRunning;
        uint32_t m_ccBcstInterval;

        Timer m_rfTimeout;
        Timer m_rfTGHang;
        Timer m_netTimeout;
        Timer m_networkWatchdog;

        uint32_t m_hangCount;
        uint32_t m_tduPreambleCount;
        
        uint8_t m_ccFrameCnt;
        uint8_t m_ccSeq;

        NID m_nid;

        SiteData m_siteData;

        lookups::RSSIInterpolator* m_rssiMapper;
        uint8_t m_rssi;
        uint8_t m_maxRSSI;
        uint8_t m_minRSSI;
        uint32_t m_aveRSSI;
        uint32_t m_rssiCount;

        bool m_verbose;
        bool m_debug;

        /// <summary>Write data processed from RF to the data ring buffer.</summary>
        void writeQueueRF(const uint8_t* data, uint32_t length);
        /// <summary>Write data processed from the network to the data ring buffer.</summary>
        void writeQueueNet(const uint8_t* data, uint32_t length);

#if ENABLE_DFSI_SUPPORT
        /// <summary>Process a DFSI data frame from the RF interface.</summary>
        bool processDFSI(uint8_t* data, uint32_t len);
#endif

        /// <summary>Process a data frames from the network.</summary>
        void processNetwork();

        /// <summary>Helper to write data nulls.</summary>
        void writeRF_Nulls();
        /// <summary>Helper to write TDU preamble packet burst.</summary>
        void writeRF_Preamble(uint32_t preambleCount = 0, bool force = false);
        /// <summary>Helper to write a P25 TDU packet.</summary>
        void writeRF_TDU(bool noNetwork);

        /// <summary>Helper to set the busy status bits on P25 frame data.</summary>
        void setBusyBits(uint8_t* data, uint32_t ssOffset, bool b1, bool b2);
        /// <summary>Helper to add the busy status bits on P25 frame data.</summary>
        void addBusyBits(uint8_t* data, uint32_t length, bool b1, bool b2);
    };
} // namespace p25

#endif // __P25_CONTROL_H__
