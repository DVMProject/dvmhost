// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017,2020-2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup host_dmr Next Generation Digital Narrowband
 * @brief Implementation for the ETSI TS-102 Digital Mobile Radio (DMR) standard.
 * @ingroup host
 *
 * @defgroup host_dmr_csbk Control Signalling Block
 * @brief Implementation for the data handling of the ETSI TS-102 control signalling block (CSBK)
 * @ingroup host_dmr
 * 
 * @file Control.h
 * @ingroup host_dmr
 * @file Control.cpp
 * @ingroup host_dmr
 */
#if !defined(__DMR_CONTROL_H__)
#define __DMR_CONTROL_H__

#include "Defines.h"
#include "common/dmr/data/NetData.h"
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
    // ---------------------------------------------------------------------------

    /**
     * @brief This class implements core logic for handling DMR.
     * @ingroup host_dmr
     */
    class HOST_SW_API Control {
    public:
        /**
         * @brief Initializes a new instance of the Control class.
         * @param authoritative Flag indicating whether or not the DVM is grant authoritative.
         * @param colorCode DMR access color code.
         * @param callHang Amount of hangtime for a DMR call.
         * @param queueSize Size of DMR data frame ring buffer.
         * @param embeddedLCOnly 
         * @param dumpTAData 
         * @param timeout Transmit timeout.
         * @param tgHang Amount of time to hang on the last talkgroup mode from RF.
         * @param modem Instance of the Modem class.
         * @param network Instance of the BaseNetwork class.
         * @param duplex Flag indicating full-duplex operation.
         * @param chLookup Instance of the ChannelLookup class.
         * @param ridLookup Instance of the RadioIdLookup class.
         * @param tidLookup Instance of the TalkgroupRulesLookup class.
         * @param idenTable Instance of the IdenTableLookup class.
         * @param rssiMapper Instance of the RSSIInterpolator class.
         * @param jitter 
         * @param dumpDataPacket Flag indicating whether data packets are dumped to the log.
         * @param repeatDataPacket Flag indicating whether incoming data packets will be repeated automatically.
         * @param dumpCSBKData Flag indicating whether CSBK data is dumped to the log.
         * @param debug Flag indicating whether DMR debug is enabled.
         * @param verbose Flag indicating whether DMR verbose logging is enabled.
         */
        Control(bool authoritative, uint32_t colorCode, uint32_t callHang, uint32_t queueSize, bool embeddedLCOnly,
            bool dumpTAData, uint32_t timeout, uint32_t tgHang, modem::Modem* modem, network::Network* network, bool duplex, ::lookups::ChannelLookup* chLookup,
            ::lookups::RadioIdLookup* ridLookup, ::lookups::TalkgroupRulesLookup* tidLookup, ::lookups::IdenTableLookup* idenTable, ::lookups::RSSIInterpolator* rssi,
            uint32_t jitter, bool dumpDataPacket, bool repeatDataPacket, bool dumpCSBKData, bool debug, bool verbose);
        /**
         * @brief Finalizes a instance of the Control class.
         */
        ~Control();

        /**
         * @brief Helper to set DMR configuration options.
         * @param conf Instance of the ConfigINI class.
         * @param supervisor Flag indicating whether the DMR has supervisory functions.
         * @param controlChData Control Channel data.
         * @param netId DMR Network ID.
         * @param siteId DMR Site ID.
         * @param channelId Channel ID.
         * @param channelNo Channel Number.
         * @param printOptions Flag indicating whether or not options should be printed to log.
         */
        void setOptions(yaml::Node& conf, bool supervisor, ::lookups::VoiceChData controlChData, 
            uint32_t netId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, bool printOptions);

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
        void setCCRunning(bool ccRunning);
        /**
         * @brief Gets a flag indicating whether the control channel is running.
         * @returns bool Flag indicating whether the control channel is running.
         */
        bool getCCHalted() const { return m_ccHalted; }
        /**
         * @brief Sets a flag indicating whether the control channel is halted.
         * @param ccHalted Flag indicating whether the control channel is halted.
         */
        void setCCHalted(bool ccHalted);
        /** @} */

        /**
         * @brief Helper to process wakeup frames from the RF interface.
         */
        bool processWakeup(const uint8_t* data);

        /** @name Frame Processing */
        /**
         * @brief Process a data frame from the RF interface.
         * @param slotNo DMR slot number.
         * @param data Buffer containing data frame.
         * @param len Length of data frame.
         * @returns bool True, if frame was successfully processed, otherwise false.
         */
        bool processFrame(uint32_t slotNo, uint8_t* data, uint32_t len);
        /**
         * @brief Get the frame data length for the next frame in the data ring buffer.
         * @param slotNo DMR slot number.
         * @returns uint32_t Length of frame data retrieved.
         */
        uint32_t peekFrameLength(uint32_t slotNo);
        /**
         * @brief Helper to determine whether or not the internal frame queue is full.
         * @param slotNo DMR slot number.
         * @returns bool True if frame queue is full, otherwise false.
         */
        bool isQueueFull(uint32_t slotNo);
        /**
         * @brief Get frame data from data ring buffer.
         * @param slotNo DMR slot number.
         * @param[out] data Buffer to store frame data.
         * @returns uint32_t Length of frame data retrieved.
         */
        uint32_t getFrame(uint32_t slotNo, uint8_t* data);
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
        void setSupervisor(bool supervisor);
        /**
         * @brief Permits a TGID on a non-authoritative host.
         * @param dstId Destination ID.
         * @param slot DMR slot number.
         */
        void permittedTG(uint32_t dstId, uint8_t slot);
        /**
         * @brief Grants a TGID on a non-authoritative host.
         * @param srcId Source Radio ID.
         * @param dstId Destination ID.
         * @param slot DMR slot number.
         * @param grp Flag indicating group grant.
         */
        void grantTG(uint32_t srcId, uint32_t dstId, uint8_t slot, bool grp);
        /**
         * @brief Releases a granted TG.
         * @param dstId Destination ID.
         * @param slot DMR slot number.
         */
        void releaseGrantTG(uint32_t dstId, uint8_t slot);
        /**
         * @brief Touches a granted TG to keep a channel grant alive.
         * @param dstId Destination ID.
         * @param slot DMR slot number.
         */
        void touchGrantTG(uint32_t dstId, uint8_t slot);
        /** @} */

        /**
         * @brief Gets instance of the DMRAffiliationLookup class.
         * @returns DMRAffiliationLookup* Instance of the DMRAffiliationLookup class.
         */
        lookups::DMRAffiliationLookup* affiliations();

        /**
         * @brief Helper to return the slot carrying the TSCC.
         * @returns Slot* Instance of Slot carrying the TSCC.
         */
        Slot* getTSCCSlot() const;
        /**
         * @brief Helper to return the slot number carrying the TSCC.
         * @returns uint8_t Slot number for the TSCC.
         */
        uint8_t getTSCCSlotNo() const { return m_tsccSlotNo; }
        /**
         * @brief Helper to payload activate the slot carrying granted payload traffic.
         * @param slotNo DMR slot number.
         * @param dstId Destination ID.
         * @param srcId Source Radio ID.
         * @param group Flag indicating group grant.
         * @param voice Flag indicating if the payload traffic is voice.
         */
        void tsccActivateSlot(uint32_t slotNo, uint32_t dstId, uint32_t srcId, bool group, bool voice);
        /**
         * @brief Helper to clear an activated payload slot.
         * @param slotNo DMR slot number.
         */
        void tsccClearActivatedSlot(uint32_t slotNo);

        /**
         * @brief Helper to write a DMR extended function packet on the RF interface.
         */
        void writeRF_Ext_Func(uint32_t slotNo, uint32_t func, uint32_t arg, uint32_t dstId);
        /**
         * @brief Helper to write a DMR call alert packet on the RF interface.
         */
        void writeRF_Call_Alrt(uint32_t slotNo, uint32_t srcId, uint32_t dstId);

        /**
         * @brief Flag indicating whether the processor or is busy or not.
         * @returns bool True, if processor is busy, otherwise false.
         */
        bool isBusy() const;

        /**
         * @brief Flag indicating whether DMR debug is enabled or not.
         * @returns bool True, if debugging is enabled, otherwise false.
         */
        bool getDebug() const { return m_debug; };
        /**
         * @brief Flag indicating whether DMR verbosity is enabled or not.
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
         * @brief Flag indicating whether DMR CSBK verbosity is enabled or not.
         * @returns bool True, if CSBK verbose logging is enabled, otherwise false.
         */
        bool getCSBKVerbose() const { return m_dumpCSBKData; };
        /**
         * @brief Helper to change the CSBK verbose state.
         * @param verbose Flag indicating whether CSBK verbose logging is enabled or not.
         */
        void setCSBKVerbose(bool verbose);

        /**
         * @brief Helper to get the last transmitted destination ID.
         * @param slotNo DMR slot number.
         * @returns uint32_t Last transmitted Destination ID.
         */
        uint32_t getLastDstId(uint32_t slotNo) const;
        /**
         * @brief Helper to get the last transmitted source ID.
         * @param slotNo DMR slot number.
         * @returns uint32_t Last transmitted source radio ID.
         */
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

        /**
         * @brief Process a data frames from the network.
         */
        void processNetwork();
    };
} // namespace dmr

#endif // __DMR_CONTROL_H__
