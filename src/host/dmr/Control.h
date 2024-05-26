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
*   Copyright (C) 2017,2020-2023 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DMR_CONTROL_H__)
#define __DMR_CONTROL_H__

#include "Defines.h"
#include "common/dmr/data/Data.h"
#include "common/lookups/RSSIInterpolator.h"
#include "common/lookups/IdenTableLookup.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/yaml/Yaml.h"
#include "dmr/lookups/DMRAffiliationLookup.h"
#include "dmr/Slot.h"
#include "modem/Modem.h"
#include "network/Network.h"

namespace dmr
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    class HOST_SW_API Slot;
    namespace packet { class HOST_SW_API ControlSignaling; }

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements core logic for handling DMR.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Control {
    public:
        /// <summary>Initializes a new instance of the Control class.</summary>
        Control(bool authoritative, uint32_t colorCode, uint32_t callHang, uint32_t queueSize, bool embeddedLCOnly,
            bool dumpTAData, uint32_t timeout, uint32_t tgHang, modem::Modem* modem, network::Network* network, bool duplex, ::lookups::ChannelLookup* chLookup,
            ::lookups::RadioIdLookup* ridLookup, ::lookups::TalkgroupRulesLookup* tidLookup, ::lookups::IdenTableLookup* idenTable, ::lookups::RSSIInterpolator* rssi,
            uint32_t jitter, bool dumpDataPacket, bool repeatDataPacket, bool dumpCSBKData, bool debug, bool verbose);
        /// <summary>Finalizes a instance of the Control class.</summary>
        ~Control();

        /// <summary>Helper to set DMR configuration options.</summary>
        void setOptions(yaml::Node& conf, bool supervisor, ::lookups::VoiceChData controlChData, 
            uint32_t netId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, bool printOptions);

        /// <summary>Gets a flag indicating whether the DMR control channel is running.</summary>
        bool getCCRunning() const { return m_ccRunning; }
        /// <summary>Sets a flag indicating whether the DMR control channel is running.</summary>
        void setCCRunning(bool ccRunning);
        /// <summary>Gets a flag indicating whether the DMR control channel is running.</summary>
        bool getCCHalted() const { return m_ccHalted; }
        /// <summary>Sets a flag indicating whether the DMR control channel is halted.</summary>
        void setCCHalted(bool ccHalted);

        /// <summary>Helper to process wakeup frames from the RF interface.</summary>
        bool processWakeup(const uint8_t* data);

        /// <summary>Process a data frame for slot, from the RF interface.</summary>
        bool processFrame(uint32_t slotNo, uint8_t* data, uint32_t len);
        /// <summary>Get the frame data length for the next frame in the data ring buffer.</summary>
        uint32_t peekFrameLength(uint32_t slotNo);
        /// <summary>Get a data frame for slot, from data ring buffer.</summary>
        uint32_t getFrame(uint32_t slotNo, uint8_t* data);

        /// <summary>Updates the processor.</summary>
        void clock(uint32_t ms);

        /// <summary>Sets a flag indicating whether DMR has supervisory functions and can send permit TG to voice channels.</summary>
        void setSupervisor(bool supervisor);
        /// <summary>Permits a TGID on a non-authoritative host.</summary>
        void permittedTG(uint32_t dstId, uint8_t slot);
        /// <summary>Grants a TGID on a non-authoritative host.</summary>
        void grantTG(uint32_t srcId, uint32_t dstId, uint8_t slot, bool grp);
        /// <summary>Releases a granted TG.</summary>
        void releaseGrantTG(uint32_t dstId, uint8_t slot);
        /// <summary>Touchs a granted TG to keep a channel grant alive.</summary>
        void touchGrantTG(uint32_t dstId, uint8_t slot);

        /// <summary>Gets instance of the DMRAffiliationLookup class.</summary>
        lookups::DMRAffiliationLookup* affiliations();

        /// <summary>Helper to return the slot carrying the TSCC.</summary>
        Slot* getTSCCSlot() const;
        /// <summary>Helper to return the slot number carrying the TSCC.</summary>
        uint8_t getTSCCSlotNo() const { return m_tsccSlotNo; }
        /// <summary>Helper to payload activate the slot carrying granted payload traffic.</summary>
        void tsccActivateSlot(uint32_t slotNo, uint32_t dstId, uint32_t srcId, bool group, bool voice);
        /// <summary>Helper to clear an activated payload slot.</summary>
        void tsccClearActivatedSlot(uint32_t slotNo);

        /// <summary>Helper to write a DMR extended function packet on the RF interface.</summary>
        void writeRF_Ext_Func(uint32_t slotNo, uint32_t func, uint32_t arg, uint32_t dstId);
        /// <summary>Helper to write a DMR call alert packet on the RF interface.</summary>
        void writeRF_Call_Alrt(uint32_t slotNo, uint32_t srcId, uint32_t dstId);

        /// <summary>Flag indicating whether the processor or is busy or not.</summary>
        bool isBusy() const;

        /// <summary>Flag indicating whether DMR debug is enabled or not.</summary>
        bool getDebug() const { return m_debug; };
        /// <summary>Flag indicating whether DMR verbosity is enabled or not.</summary>
        bool getVerbose() const { return m_verbose; };
        /// <summary>Helper to change the debug and verbose state.</summary>
        void setDebugVerbose(bool debug, bool verbose);
        /// <summary>Flag indicating whether DMR CSBK verbosity is enabled or not.</summary>
        bool getCSBKVerbose() const { return m_dumpCSBKData; };
        /// <summary>Helper to change the CSBK verbose state.</summary>
        void setCSBKVerbose(bool verbose);

        /// <summary>Helper to get the last transmitted destination ID.</summary>
        uint32_t getLastDstId(uint32_t slotNo) const;
        /// <summary>Helper to get the last transmitted source ID.</summary>
        uint32_t getLastSrcId(uint32_t slotNo) const;

    private:
        friend class Slot;

        bool m_authoritative;
        bool m_supervisor;

        uint32_t m_colorCode;

        modem::Modem* m_modem;
        network::Network* m_network;

        Slot* m_slot1;
        Slot* m_slot2;

        ::lookups::IdenTableLookup* m_idenTable;
        ::lookups::RadioIdLookup* m_ridLookup;
        ::lookups::TalkgroupRulesLookup* m_tidLookup;

        bool m_enableTSCC;

        uint16_t m_tsccCnt;
        Timer m_tsccCntInterval;

        uint8_t m_tsccSlotNo;
        bool m_tsccPayloadActive;
        bool m_ccRunning;
        bool m_ccHalted;

        bool m_dumpCSBKData;
        bool m_verbose;
        bool m_debug;

        /// <summary>Process a data frames from the network.</summary>
        void processNetwork();
    };
} // namespace dmr

#endif // __DMR_CONTROL_H__
