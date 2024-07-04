// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file ControlSignaling.h
 * @ingroup host_p25
 * @file ControlSignaling.cpp
 * @ingroup host_p25
 */
#if !defined(__P25_PACKET_CONTROL_SIGNALING_H__)
#define __P25_PACKET_CONTROL_SIGNALING_H__

#include "Defines.h"
#include "common/p25/data/DataHeader.h"
#include "common/p25/data/DataBlock.h"
#include "common/p25/lc/TSBK.h"
#include "common/p25/lc/AMBT.h"
#include "common/p25/lc/TDULC.h"
#include "common/Timer.h"
#include "p25/Control.h"

#include <cstdio>
#include <string>
#include <unordered_map>
#include <algorithm>

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    namespace packet { class HOST_SW_API Voice; }
    namespace packet { class HOST_SW_API Data; }
    namespace lookups { class HOST_SW_API P25AffiliationLookup; }
    class HOST_SW_API Control;

    namespace packet
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements handling logic for P25 TSDU and TDULC packets.
         * @ingroup host_p25
         */
        class HOST_SW_API ControlSignaling {
        public:
            /** @name Frame Processing */
            /**
             * @brief Process a data frame from the RF interface.
             * @param data Buffer containing data frame.
             * @param len Length of data frame.
             * @param preDecodedTSBK Pre-decoded TSBK.
             * @returns bool True, if data frame is processed, otherwise false.
             */
            bool process(uint8_t* data, uint32_t len, std::unique_ptr<lc::TSBK> preDecodedTSBK = nullptr);
            /**
             * @brief Process a data frame from the network.
             * @param data Buffer containing data frame.
             * @param len Length of data frame.
             * @param control Link Control Data.
             * @param lsd Low Speed Data.
             * @param duid Data Unit ID.
             * @returns bool True, if data frame is processed, otherwise false.
             */
            bool processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, defines::DUID::E& duid);
            /** @} */

            /**
             * @brief Helper used to process AMBTs from PDU data.
             * @param dataHeader Instance of a PDU data header.
             * @param blocks Array of PDU data blocks.
             */
            bool processMBT(data::DataHeader& dataHeader, data::DataBlock* blocks);

            /**
             * @brief Helper to write P25 adjacent site information to the network.
             */
            void writeAdjSSNetwork();

            /** @name Externally Callable TSBK Commands */
            /**
             * @brief Helper to write a call alert packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination Radio ID.
             */
            void writeRF_TSDU_Call_Alrt(uint32_t srcId, uint32_t dstId);
            /**
             * @brief Helper to write a radio monitor packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination Radio ID.
             * @param txMult Tx Multiplier.
             */
            void writeRF_TSDU_Radio_Mon(uint32_t srcId, uint32_t dstId, uint8_t txMult);
            /**
             * @brief Helper to write a extended function packet.
             * @param func Extended Function Operation.
             * @param arg Function Argument.
             * @param dstId Destination Radio ID.
             */
            void writeRF_TSDU_Ext_Func(uint32_t func, uint32_t arg, uint32_t dstId);
            /**
             * @brief Helper to write a group affiliation query packet.
             * @param dstId Destination Radio ID.
             */
            void writeRF_TSDU_Grp_Aff_Q(uint32_t dstId);
            /**
             * @brief Helper to write a unit registration command packet.
             * @param dstId Destination Radio ID.
             */
            void writeRF_TSDU_U_Reg_Cmd(uint32_t dstId);
            /**
             * @brief Helper to write a emergency alarm packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination Radio ID.
             */
            void writeRF_TSDU_Emerg_Alrm(uint32_t srcId, uint32_t dstId);
            /**
             * @brief Helper to write a raw TSBK packet.
             * @param tsbk Raw TSBK buffer to write.
             */
            void writeRF_TSDU_Raw(const uint8_t* tsbk);
            /** @} */

            /**
             * @brief Helper to change the conventional fallback state.
             * @param verbose Flag indicating whether conventional fallback is enabled.
             */
            void setConvFallback(bool fallback);

            /**
             * @brief Helper to change the last MFId value.
             * @param mfId Manufacturer ID.
             */
            void setLastMFId(uint8_t mfId) { m_lastMFID = mfId; }

            /**
             * @brief Flag indicating whether P25 TSBK verbosity is enabled or not.
             * @returns bool Flag indicating whether TSBK verbosity is enabled.
             */
            bool getTSBKVerbose() const { return m_dumpTSBK; };
            /**
             * @brief Helper to change the TSBK verbose state.
             * @param verbose Flag indicating whether TSBK verbosity is enabled.
             */
            void setTSBKVerbose(bool verbose);

        protected:
            friend class packet::Voice;
            friend class packet::Data;
            friend class p25::Control;
            Control* m_p25;
            friend class lookups::P25AffiliationLookup;

            uint32_t m_patchSuperGroup;
            uint32_t m_announcementGroup;

            bool m_verifyAff;
            bool m_verifyReg;
            bool m_requireLLAForReg;

            uint8_t* m_rfMBF;
            uint8_t m_mbfCnt;

            uint8_t m_mbfIdenCnt;
            uint8_t m_mbfAdjSSCnt;
            uint8_t m_mbfSCCBCnt;
            uint8_t m_mbfGrpGrntCnt;

            std::unordered_map<uint8_t, SiteData> m_adjSiteTable;
            std::unordered_map<uint8_t, uint8_t> m_adjSiteUpdateCnt;

            std::unordered_map<uint8_t, SiteData> m_sccbTable;
            std::unordered_map<uint8_t, uint8_t> m_sccbUpdateCnt;

            std::unordered_map<uint32_t, ulong64_t> m_llaDemandTable;

            uint8_t m_lastMFID;

            bool m_noStatusAck;
            bool m_noMessageAck;
            bool m_unitToUnitAvailCheck;

            uint8_t m_convFallbackPacketDelay;
            bool m_convFallback;

            Timer m_adjSiteUpdateTimer;
            uint32_t m_adjSiteUpdateInterval;

            uint16_t m_microslotCount;

            bool m_ctrlTimeDateAnn;

            bool m_ctrlTSDUMBF;

            bool m_disableGrantSrcIdCheck;
            bool m_redundantImmediate;
            bool m_redundantGrant;

            bool m_dumpTSBK;

            bool m_verbose;
            bool m_debug;

            /**
             * @brief Initializes a new instance of the ControlSignaling class.
             * @param p25 Instance of the Control class.
             * @param dumpTSBKData Flag indicating whether TSBK data is dumped to the log.
             * @param debug Flag indicating whether P25 debug is enabled.
             * @param verbose Flag indicating whether P25 verbose logging is enabled.
             */
            ControlSignaling(Control* p25, bool dumpTSBKData, bool debug, bool verbose);
            /**
             * @brief Finalizes a instance of the ControlSignaling class.
             */
            virtual ~ControlSignaling();

            /**
             * @brief Write data processed from RF to the network.
             * @param tsbk TSBK to write to network.
             * @param data Buffer containing encoded TSBK.
             * @param autoReset After writing TSBK to network, reset P25 network state.
             */
            void writeNetworkRF(lc::TSBK* tsbk, const uint8_t* data, bool autoReset);
            /**
             * @brief Write data processed from RF to the network.
             * @param tduLc TDULC to write to network.
             * @param data Buffer containing encoded TDULC.
             * @param autoReset After writing TSBK to network, reset P25 network state.
             */
            void writeNetworkRF(lc::TDULC* tduLc, const uint8_t* data, bool autoReset);

            /*
            ** Modem Frame Queuing
            */

            /**
             * @brief Helper to write a P25 TDU w/ link control packet.
             * @param lc TDULC to write to the modem.
             * @param noNetwork Flag indicating not to write the TDULC to the network.
             */
            void writeRF_TDULC(lc::TDULC* lc, bool noNetwork);
            /**
             * @brief Helper to write a network P25 TDU w/ link control packet.
             * @param lc TDULC to write to the modem.
             */
            void writeNet_TDULC(lc::TDULC* lc);

            /**
             * @brief Helper to write a immediate single-block P25 TSDU packet.
             * @param tsbk TSBK to write to the modem.
             * @param noNetwork Flag indicating not to write the TSBK to the network.
             */
            void writeRF_TSDU_SBF_Imm(lc::TSBK *tsbk, bool noNetwork) { writeRF_TSDU_SBF(tsbk, noNetwork, false, true); }
            /**
             * @brief Helper to write a single-block P25 TSDU packet.
             * @param tsbk TSBK to write to the modem.
             * @param noNetwork Flag indicating not to write the TSBK to the network.
             * @param forceSingle Force TSBK to be written as a single block TSDU and not bundled into a multiblock TSDU.
             * @param imm Flag indicating the TSBK should be written to the immediate queue.
             */
            void writeRF_TSDU_SBF(lc::TSBK* tsbk, bool noNetwork, bool forceSingle = false, bool imm = false);
            /**
             * @brief Helper to write a network single-block P25 TSDU packet.
             * @param tsbk TSBK to write to the network.
             */
            void writeNet_TSDU(lc::TSBK* tsbk);
            /**
             * @brief Helper to write a multi-block (3-block) P25 TSDU packet.
             * @param tsbk TSBK to write to the multi-block queue.
             */
            void writeRF_TSDU_MBF(lc::TSBK* tsbk);
            /**
             * @brief Helper to write a alternate multi-block PDU packet.
             * @param tsbk AMBT to write to the modem.
             * @param imm Flag indicating the AMBT should be written to the immediate queue.
             */
            void writeRF_TSDU_AMBT(lc::AMBT* ambt, bool imm = false);

            /*
            ** Control Signalling Logic
            */

            /**
             * @brief Helper to write a P25 TDU w/ link control channel release packet.
             * @param grp Flag indicating the destination ID is a talkgroup.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             */
            void writeRF_TDULC_ChanRelease(bool grp, uint32_t srcId, uint32_t dstId);

            /**
             * @brief Helper to write control channel packet data.
             * @param frameCnt Frame counter.
             * @param n 
             * @param adjSS Flag indicating whether or not adjacent site status should be broadcast.
             */
            void writeRF_ControlData(uint8_t frameCnt, uint8_t n, bool adjSS);

            /**
             * @brief Helper to generate the given control TSBK into the TSDU frame queue.
             * @param lco TSBK LCO to queue into the frame queue.
             */
            void queueRF_TSBK_Ctrl(uint8_t lco);

            /**
             * @brief Helper to write a grant packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param grp Flag indicating the destination ID is a talkgroup.
             * @param net Flag indicating this grant is coming from network traffic.
             * @param skip Flag indicating normal grant checking is skipped.
             * @param chNo Channel Number.
             * @returns True, if granted, otherwise false.
             */
            bool writeRF_TSDU_Grant(uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp, bool net = false, bool skip = false, uint32_t chNo = 0U);
            /**
             * @brief Helper to write a grant update packet.
             */
            void writeRF_TSDU_Grant_Update();
            /**
             * @brief Helper to write a SNDCP grant packet.
             * @param srcId Source Radio ID.
             * @param skip Flag indicating normal grant checking is skipped.
             * @param chNo Channel Number.
             * @returns True, if granted, otherwise false.
             */
            bool writeRF_TSDU_SNDCP_Grant(uint32_t srcId, bool skip = false, uint32_t chNo = 0U);
            /**
             * @brief Helper to write a unit to unit answer request packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             */
            void writeRF_TSDU_UU_Ans_Req(uint32_t srcId, uint32_t dstId);
            /**
             * @brief Helper to write a acknowledge packet.
             * @param srcId Source Radio ID.
             * @param service Service being acknowledged.
             * @param extended Flag indicating this is an extended acknowledgement.
             * @param noActivityLog 
             */
            void writeRF_TSDU_ACK_FNE(uint32_t srcId, uint32_t service, bool extended, bool noActivityLog);
            /**
             * @brief Helper to write a deny packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param reason Denial Reason.
             * @param service Service being denied.
             * @param grp Flag indicating the destination ID is a talkgroup.
             * @param aiv 
             */
            void writeRF_TSDU_Deny(uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service, bool grp = false, bool aiv = false);
            /**
             * @brief Helper to write a group affiliation response packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             */
            bool writeRF_TSDU_Grp_Aff_Rsp(uint32_t srcId, uint32_t dstId);
            /**
             * @brief Helper to write a unit registration response packet.
             * @param srcId Source Radio ID.
             * @param sysId System ID.
             */
            void writeRF_TSDU_U_Reg_Rsp(uint32_t srcId, uint32_t sysId);
            /**
             * @brief Helper to write a unit de-registration acknowledge packet.
             * @param srcId Source Radio ID.
             */
            void writeRF_TSDU_U_Dereg_Ack(uint32_t srcId);
            /**
             * @brief Helper to write a queue packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param reason Queue Reason.
             * @param service Service being queued.
             * @param grp Flag indicating the destination ID is a talkgroup.
             * @param aiv 
             */
            void writeRF_TSDU_Queue(uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service, bool grp = false, bool aiv = false);
            /**
             * @brief Helper to write a location registration response packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param grp Flag indicating the destination ID is a talkgroup.
             */
            bool writeRF_TSDU_Loc_Reg_Rsp(uint32_t srcId, uint32_t dstId, bool grp);

            /**
             * @brief Helper to write a LLA demand.
             * @param srcId Source Radio ID.
             */
            void writeRF_TSDU_Auth_Dmd(uint32_t srcId);

            /**
             * @brief Helper to write a call termination packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             */
            bool writeNet_TSDU_Call_Term(uint32_t srcId, uint32_t dstId);

            /**
             * @brief Helper to write a network TSDU from the RF data queue.
             * @param tsbk TSBK to write to network.
             * @param data Buffer containing encoded TSBK.
             */
            void writeNet_TSDU_From_RF(lc::TSBK* tsbk, uint8_t* data);

            /**
             * @brief Helper to automatically inhibit a source ID on a denial.
             * @param srcId Source Radio ID.
             */
            void denialInhibit(uint32_t srcId);
        };
    } // namespace packet
} // namespace p25

#endif // __P25_PACKET_CONTROL_SIGNALING_H__
