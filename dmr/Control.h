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
*   Copyright (C) 2017,2020 by Bryan Biedenkapp N2PLL
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
#if !defined(__DMR_CONTROL_H__)
#define __DMR_CONTROL_H__

#include "Defines.h"
#include "dmr/data/Data.h"
#include "dmr/Slot.h"
#include "modem/Modem.h"
#include "network/BaseNetwork.h"
#include "network/RemoteControl.h"
#include "lookups/RSSIInterpolator.h"
#include "lookups/IdenTableLookup.h"
#include "lookups/RadioIdLookup.h"
#include "lookups/TalkgroupIdLookup.h"
#include "yaml/Yaml.h"

namespace dmr
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------
    class HOST_SW_API Slot;

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements core logic for handling DMR.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Control {
    public:
        /// <summary>Initializes a new instance of the Control class.</summary>
        Control(uint32_t colorCode, uint32_t callHang, uint32_t queueSize, bool embeddedLCOnly,
            bool dumpTAData, uint32_t timeout, uint32_t tgHang, modem::Modem* modem, network::BaseNetwork* network, bool duplex,
            lookups::RadioIdLookup* ridLookup, lookups::TalkgroupIdLookup* tidLookup, lookups::IdenTableLookup* idenTable, lookups::RSSIInterpolator* rssi,
            uint32_t jitter, bool dumpDataPacket, bool repeatDataPacket, bool dumpCSBKData, bool debug, bool verbose);
        /// <summary>Finalizes a instance of the Control class.</summary>
        ~Control();

        /// <summary>Helper to set DMR configuration options.</summary>
        void setOptions(yaml::Node& conf, uint32_t netId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, bool printOptions);
        /// <summary>Gets a flag indicating whether the DMR control channel is running.</summary>
        bool getCCRunning() { return m_ccRunning; }
        /// <summary>Sets a flag indicating whether the DMR control channel is running.</summary>
        void setCCRunning(bool ccRunning);

        /// <summary>Helper to process wakeup frames from the RF interface.</summary>
        bool processWakeup(const uint8_t* data);

        /// <summary>Process a data frame for slot 1, from the RF interface.</summary>
        bool processFrame1(uint8_t* data, uint32_t len);
        /// <summary>Get a frame data for slot 1, from data ring buffer.</summary>
        uint32_t getFrame1(uint8_t* data);
        /// <summary>Process a data frame for slot 2, from the RF interface.</summary>
        bool processFrame2(uint8_t* data, uint32_t len);
        /// <summary>Get a frame data for slot 2, from data ring buffer.</summary>
        uint32_t getFrame2(uint8_t* data);

        /// <summary>Updates the processor.</summary>
        void clock();

        /// <summary>Helper to write a DMR extended function packet on the RF interface.</summary>
        void writeRF_Ext_Func(uint32_t slotNo, uint32_t func, uint32_t arg, uint32_t dstId);
        /// <summary>Helper to write a DMR call alert packet on the RF interface.</summary>
        void writeRF_Call_Alrt(uint32_t slotNo, uint32_t srcId, uint32_t dstId);

        /// <summary>Helper to change the debug and verbose state.</summary>
        void setDebugVerbose(bool debug, bool verbose);

    private:
        uint32_t m_colorCode;

        modem::Modem* m_modem;
        network::BaseNetwork* m_network;

        Slot* m_slot1;
        Slot* m_slot2;

        lookups::IdenTableLookup* m_idenTable;
        lookups::RadioIdLookup* m_ridLookup;
        lookups::TalkgroupIdLookup* m_tidLookup;

        uint8_t m_tsccSlotNo;
        bool m_ccRunning;

        bool m_dumpCSBKData;
        bool m_verbose;
        bool m_debug;
    };
} // namespace dmr

#endif // __DMR_CONTROL_H__
