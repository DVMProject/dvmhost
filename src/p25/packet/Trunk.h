/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
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
#if !defined(__P25_PACKET_TRUNK_H__)
#define __P25_PACKET_TRUNK_H__

#include "Defines.h"
#include "p25/data/DataHeader.h"
#include "p25/data/DataBlock.h"
#include "p25/lc/TSBK.h"
#include "p25/lc/AMBT.h"
#include "p25/lc/TDULC.h"
#include "p25/Control.h"
#include "network/BaseNetwork.h"
#include "Timer.h"

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
    namespace dfsi { namespace packet { class HOST_SW_API DFSIVoice; } }
    namespace packet { class HOST_SW_API Data; }
    namespace lookups { class HOST_SW_API P25AffiliationLookup; }
    class HOST_SW_API Control;

    namespace packet
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      This class implements handling logic for P25 trunking packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API Trunk {
        public:
            /// <summary>Process a data frame from the RF interface.</summary>
            virtual bool process(uint8_t* data, uint32_t len, std::unique_ptr<lc::TSBK> preDecodedTSBK = nullptr);
            /// <summary>Process a data frame from the network.</summary>
            virtual bool processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, uint8_t& duid);

            /// <summary>Helper used to process AMBTs from PDU data.</summary>
            bool processMBT(data::DataHeader dataHeader, data::DataBlock* blocks);

            /// <summary>Helper to write P25 adjacent site information to the network.</summary>
            void writeAdjSSNetwork();

            /// <summary>Updates the processor by the passed number of milliseconds.</summary>
            void clock(uint32_t ms);

            /// <summary>Helper to write a call alert packet.</summary>
            void writeRF_TSDU_Call_Alrt(uint32_t srcId, uint32_t dstId);
            /// <summary>Helper to write a radio monitor packet.</summary>
            void writeRF_TSDU_Radio_Mon(uint32_t srcId, uint32_t dstId, uint8_t txMult);
            /// <summary>Helper to write a extended function packet.</summary>
            void writeRF_TSDU_Ext_Func(uint32_t func, uint32_t arg, uint32_t dstId);
            /// <summary>Helper to write a group affiliation query packet.</summary>
            void writeRF_TSDU_Grp_Aff_Q(uint32_t dstId);
            /// <summary>Helper to write a unit registration command packet.</summary>
            void writeRF_TSDU_U_Reg_Cmd(uint32_t dstId);
            /// <summary>Helper to write a emergency alarm packet.</summary>
            void writeRF_TSDU_Emerg_Alrm(uint32_t srcId, uint32_t dstId);
            /// <summary>Helper to write a emergency alarm packet.</summary>
            void writeRF_TSDU_Raw(const uint8_t* tsbk);

            /// <summary>Helper to change the conventional fallback state.</summary>
            void setConvFallback(bool fallback);

            /// <summary>Helper to change the last MFId value.</summary>
            void setLastMFId(uint8_t mfId) { m_lastMFID = mfId; }

            /// <summary>Flag indicating whether P25 TSBK verbosity is enabled or not.</summary>
            bool getTSBKVerbose() const { return m_dumpTSBK; };
            /// <summary>Helper to change the TSBK verbose state.</summary>
            void setTSBKVerbose(bool verbose);

        protected:
            friend class packet::Voice;
            friend class dfsi::packet::DFSIVoice;
            friend class packet::Data;
            friend class p25::Control;
            Control* m_p25;
            friend class lookups::P25AffiliationLookup;

            network::BaseNetwork* m_network;

            uint32_t m_patchSuperGroup;

            bool m_verifyAff;
            bool m_verifyReg;

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

            bool m_sndcpChGrant;
            bool m_disableGrantSrcIdCheck;

            bool m_dumpTSBK;

            bool m_verbose;
            bool m_debug;

            /// <summary>Initializes a new instance of the Trunk class.</summary>
            Trunk(Control* p25, network::BaseNetwork* network, bool dumpTSBKData, bool debug, bool verbose);
            /// <summary>Finalizes a instance of the Trunk class.</summary>
            virtual ~Trunk();

            /// <summary>Write data processed from RF to the network.</summary>
            void writeNetworkRF(lc::TSBK* tsbk, const uint8_t* data, bool autoReset);
            /// <summary>Write data processed from RF to the network.</summary>
            void writeNetworkRF(lc::TDULC* tduLc, const uint8_t* data, bool autoReset);

            /// <summary>Helper to write control channel packet data.</summary>
            void writeRF_ControlData(uint8_t frameCnt, uint8_t n, bool adjSS);

            /// <summary>Helper to write a P25 TDU w/ link control packet.</summary>
            virtual void writeRF_TDULC(lc::TDULC* lc, bool noNetwork);
            /// <summary>Helper to write a network P25 TDU w/ link control packet.</summary>
            virtual void writeNet_TDULC(lc::TDULC* lc);
            /// <summary>Helper to write a P25 TDU w/ link control channel release packet.</summary>
            void writeRF_TDULC_ChanRelease(bool grp, uint32_t srcId, uint32_t dstId);

            /// <summary>Helper to write a immediate single-block P25 TSDU packet.</summary>
            virtual void writeRF_TSDU_SBF_Imm(lc::TSBK *tsbk, bool noNetwork) { writeRF_TSDU_SBF(tsbk, noNetwork, false, false, true); }
            /// <summary>Helper to write a single-block P25 TSDU packet.</summary>
            virtual void writeRF_TSDU_SBF(lc::TSBK* tsbk, bool noNetwork, bool clearBeforeWrite = false, bool force = false, bool imm = false);
            /// <summary>Helper to write a network single-block P25 TSDU packet.</summary>
            virtual void writeNet_TSDU(lc::TSBK* tsbk);
            /// <summary>Helper to write a multi-block (3-block) P25 TSDU packet.</summary>
            void writeRF_TSDU_MBF(lc::TSBK* tsbk, bool clearBeforeWrite = false);
            /// <summary>Helper to write a alternate multi-block trunking PDU packet.</summary>
            virtual void writeRF_TSDU_AMBT(lc::AMBT* ambt, bool clearBeforeWrite = false);

            /// <summary>Helper to generate the given control TSBK into the TSDU frame queue.</summary>
            void queueRF_TSBK_Ctrl(uint8_t lco);

            /// <summary>Helper to write a grant packet.</summary>
            bool writeRF_TSDU_Grant(uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp, bool net = false, bool skip = false, uint32_t chNo = 0U);
            /// <summary>Helper to write a SNDCP grant packet.</summary>
            bool writeRF_TSDU_SNDCP_Grant(uint32_t srcId, uint32_t dstId, bool skip = false, bool net = false);
            /// <summary>Helper to write a unit to unit answer request packet.</summary>
            void writeRF_TSDU_UU_Ans_Req(uint32_t srcId, uint32_t dstId);
            /// <summary>Helper to write a acknowledge packet.</summary>
            void writeRF_TSDU_ACK_FNE(uint32_t srcId, uint32_t service, bool extended, bool noActivityLog);
            /// <summary>Helper to write a deny packet.</summary>
            void writeRF_TSDU_Deny(uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service, bool aiv = false);
            /// <summary>Helper to write a group affiliation response packet.</summary>
            bool writeRF_TSDU_Grp_Aff_Rsp(uint32_t srcId, uint32_t dstId);
            /// <summary>Helper to write a unit registration response packet.</summary>
            void writeRF_TSDU_U_Reg_Rsp(uint32_t srcId, uint32_t sysId);
            /// <summary>Helper to write a unit de-registration acknowledge packet.</summary>
            void writeRF_TSDU_U_Dereg_Ack(uint32_t srcId);
            /// <summary>Helper to write a queue packet.</summary>
            void writeRF_TSDU_Queue(uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service, bool aiv = false);
            /// <summary>Helper to write a location registration response packet.</summary>
            bool writeRF_TSDU_Loc_Reg_Rsp(uint32_t srcId, uint32_t dstId, bool grp);

            /// <summary>Helper to write a call termination packet.</summary>
            bool writeNet_TSDU_Call_Term(uint32_t srcId, uint32_t dstId);

            /// <summary>Helper to write a network TSDU from the RF data queue.</summary>
            void writeNet_TSDU_From_RF(lc::TSBK* tsbk, uint8_t * data);

            /// <summary>Helper to automatically inhibit a source ID on a denial.</summary>
            void denialInhibit(uint32_t srcId);

            /// <summary>Helper to add the idle status bits on P25 frame data.</summary>
            void addIdleBits(uint8_t* data, uint32_t length, bool b1, bool b2);
        };
    } // namespace packet
} // namespace p25

#endif // __P25_PACKET_TRUNK_H__
