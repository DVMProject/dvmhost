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
*   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2021 by Bryan Biedenkapp N2PLL
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
#if !defined(__DMR_SLOT_H__)
#define __DMR_SLOT_H__

#include "Defines.h"
#include "dmr/Control.h"
#include "dmr/SiteData.h"
#include "dmr/ControlPacket.h"
#include "dmr/DataPacket.h"
#include "dmr/VoicePacket.h"
#include "modem/Modem.h"
#include "network/BaseNetwork.h"
#include "lookups/RSSIInterpolator.h"
#include "lookups/IdenTableLookup.h"
#include "lookups/RadioIdLookup.h"
#include "lookups/TalkgroupIdLookup.h"
#include "RingBuffer.h"
#include "StopWatch.h"
#include "Timer.h"

#include <vector>

namespace dmr
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------
    class HOST_SW_API VoicePacket;
    class HOST_SW_API DataPacket;
    class HOST_SW_API ControlPacket;

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

        /// <summary>Sets a flag indicating whether the DMR control channel is running.</summary>
        void setCCRunning(bool ccRunning);

        /// <summary>Process a data frame from the RF interface.</summary>
        bool processFrame(uint8_t* data, uint32_t len);
        /// <summary>Get frame data from data ring buffer.</summary>
        uint32_t getFrame(uint8_t* data);

        /// <summary>Process a data frames from the network.</summary>
        void processNetwork(const data::Data& data);

        /// <summary>Updates the slot processor.</summary>
        void clock();

        /// <summary>Gets instance of the ControlPacket class.</summary>
        ControlPacket* control() { return m_control; }

        /// <summary>Helper to change the debug and verbose state.</summary>
        void setDebugVerbose(bool debug, bool verbose);

        /// <summary>Helper to enable and configure TSCC support for this slot.</summary>
        void setTSCC(bool enable, bool dedicated);
        /// <summary>Helper to set the voice error silence threshold.</summary>
        void setSilenceThreshold(uint32_t threshold);

        /// <summary>Helper to initialize the slot processor.</summary>
        static void init(uint32_t colorCode, SiteData siteData, bool embeddedLCOnly, bool dumpTAData, uint32_t callHang, modem::Modem* modem,
            network::BaseNetwork* network, bool duplex, lookups::RadioIdLookup* ridLookup, lookups::TalkgroupIdLookup* tidLookup,
            lookups::IdenTableLookup* idenTable, lookups::RSSIInterpolator* rssiMapper, uint32_t jitter);
        /// <summary>Sets local configured site data.</summary>
        static void setSiteData(uint32_t netId, uint8_t siteId, uint8_t channelId, uint32_t channelNo);

    private:
        friend class VoicePacket;
        VoicePacket* m_voice;
        friend class DataPacket;
        DataPacket* m_data;
        friend class ControlPacket;
        ControlPacket* m_control;

        uint32_t m_slotNo;

        RingBuffer<uint8_t> m_queue;

        RPT_RF_STATE m_rfState;
        uint32_t m_rfLastDstId;
        RPT_NET_STATE m_netState;
        uint32_t m_netLastDstId;

        lc::LC* m_rfLC;
        lc::PrivacyLC* m_rfPrivacyLC;
        data::DataHeader* m_rfDataHeader;

        uint8_t m_rfSeqNo;

        lc::LC* m_netLC;
        lc::PrivacyLC* m_netPrivacyLC;
        data::DataHeader* m_netDataHeader;

        Timer m_networkWatchdog;
        Timer m_rfTimeoutTimer;
        Timer m_rfTGHang;
        Timer m_netTimeoutTimer;
        Timer m_packetTimer;

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

        uint8_t m_ccSeq;
        bool m_ccRunning;

        bool m_enableTSCC;
        bool m_dedicatedTSCC;

        bool m_dumpCSBKData;
        bool m_verbose;
        bool m_debug;

        static uint32_t m_colorCode;

        static SiteData m_siteData;
        static uint32_t m_channelNo;

        static bool m_embeddedLCOnly;
        static bool m_dumpTAData;

        static modem::Modem* m_modem;
        static network::BaseNetwork* m_network;

        static bool m_duplex;

        static lookups::IdenTableLookup* m_idenTable;
        static lookups::RadioIdLookup* m_ridLookup;
        static lookups::TalkgroupIdLookup* m_tidLookup;

        static lookups::IdenTable m_idenEntry;

        static uint32_t m_hangCount;

        static lookups::RSSIInterpolator* m_rssiMapper;

        static uint32_t m_jitterTime;
        static uint32_t m_jitterSlots;

        static uint8_t* m_idle;

        static uint8_t m_flco1;
        static uint8_t m_id1;
        static bool m_voice1;

        static uint8_t m_flco2;
        static uint8_t m_id2;
        static bool m_voice2;

        static uint16_t m_tsccCnt;

        /// <summary>Write data processed from RF to the data ring buffer.</summary>
        void writeQueueRF(const uint8_t* data);
        /// <summary>Write data processed from the network to the data ring buffer.</summary>
        void writeQueueNet(const uint8_t* data);
        /// <summary>Write data processed from RF to the network.</summary>
        void writeNetworkRF(const uint8_t* data, uint8_t dataType, uint8_t errors = 0U);
        /// <summary>Write data processed from RF to the network.</summary>
        void writeNetworkRF(const uint8_t* data, uint8_t dataType, uint8_t flco, uint32_t srcId,
            uint32_t dstId, uint8_t errors = 0U);

        /// <summary>Helper to write RF end of frame data.</summary>
        void writeEndRF(bool writeEnd = false);
        /// <summary>Helper to write network end of frame data.</summary>
        void writeEndNet(bool writeEnd = false);

        /// <summary>Helper to write control channel packet data.</summary>
        void writeRF_ControlData(uint16_t frameCnt, uint8_t n);

        /// <summary>Helper to set the DMR short LC.</summary>
        static void setShortLC(uint32_t slotNo, uint32_t id, uint8_t flco = FLCO_GROUP, bool voice = true);
        /// <summary>Helper to set the DMR short LC for TSCC.</summary>
        static void setShortLC_TSCC(SiteData siteData, uint16_t counter);
    };
} // namespace dmr

#endif // __DMR_SLOT_H__
