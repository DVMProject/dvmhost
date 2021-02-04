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
*   Copyright (C) 2017-2019 by Bryan Biedenkapp N2PLL
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
#if !defined(__P25_TRUNK_PACKET_H__)
#define __P25_TRUNK_PACKET_H__

#include "Defines.h"
#include "p25/lc/TSBK.h"
#include "p25/lc/TDULC.h"
#include "p25/Control.h"
#include "p25/SiteData.h"
#include "network/BaseNetwork.h"
#include "network/RemoteControl.h"
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
    class HOST_SW_API VoicePacket;
    class HOST_SW_API DataPacket;
    class HOST_SW_API Control;

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements handling logic for P25 trunking packets.
    // ---------------------------------------------------------------------------

    class HOST_SW_API TrunkPacket {
    public:
        /// <summary>Sets local configured site data.</summary>
        void setSiteData(uint32_t netId, uint32_t sysId, uint8_t rfssId, uint8_t siteId, uint8_t lra,
            uint8_t channelId, uint32_t channelNo);
        /// <summary>Sets local configured site callsign.</summary>
        void setCallsign(std::string callsign);
        /// <summary>Sets a flag indicating whether or not networking is active.</summary>
        void setNetActive(bool active);
        /// <summary>Sets the total number of channels at the site.</summary>
        void setSiteChCnt(uint8_t chCnt);

        /// <summary>Resets the data states for the RF interface.</summary>
        void resetRF();
        /// <summary>Resets the data states for the network.</summary>
        void resetNet();
        /// <summary>Sets the RF TSBK and TDULC data to match the given LC data.</summary>
        void setRFLC(const lc::LC& lc);
        /// <summary>Sets the network TSBK and TDULC data to match the given LC data.</summary>
        void setNetLC(const lc::LC& lc);

        /// <summary>Process a data frame from the RF interface.</summary>
        bool process(uint8_t* data, uint32_t len);
        /// <summary>Process a data frame from the network.</summary>
        bool processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, uint8_t& duid);

        /// <summary>Helper to write P25 adjacent site information to the network.</summary>
        void writeAdjSSNetwork();

        /// <summary>Helper to determine if the source ID has affiliated to the group destination ID.</summary>
        bool hasSrcIdGrpAff(uint32_t srcId, uint32_t dstId) const;
        /// <summary>Helper to determine if the source ID has unit registered.</summary>
        bool hasSrcIdUnitReg(uint32_t srcId) const;

        /// <summary>Helper to determine if the channel number is busy.</summary>
        bool isChBusy(uint32_t chNo) const;
        /// <summary>Helper to determine if the destination ID is already granted.</summary>
        bool hasDstIdGranted(uint32_t dstId) const;
        /// <summary>Helper to start the destination ID grant timer.</summary>
        void touchDstIdGrant(uint32_t dstId);
        /// <summary>Helper to release the channel grant for the destination ID.</summary>
        void releaseDstIdGrant(uint32_t dstId, bool releaseAll);
        /// <summary>Helper to release group affiliations.</summary>
        void clearGrpAff(uint32_t dstId, bool releaseAll);

        /// <summary></summary>
        void resetStatusCommand();

        /// <summary>Updates the processor by the passed number of milliseconds.</summary>
        void clock(uint32_t ms);

        /// <summary></summary>
        void setMFId(uint8_t val) { m_rfTSBK.setMFId(val); }
        /// <summary>Helper to write a call alert packet.</summary>
        void writeRF_TSDU_Call_Alrt(uint32_t srcId, uint32_t dstId);
        /// <summary>Helper to write a extended function packet.</summary>
        void writeRF_TSDU_Ext_Func(uint32_t func, uint32_t arg, uint32_t dstId);
        /// <summary>Helper to write a group affiliation query packet.</summary>
        void writeRF_TSDU_Grp_Aff_Q(uint32_t dstId);
        /// <summary>Helper to write a unit registration command packet.</summary>
        void writeRF_TSDU_U_Reg_Cmd(uint32_t dstId);

        /// <summary>Helper to write a Motorola patch packet.</summary>
        void writeRF_TSDU_Mot_Patch(uint32_t group1, uint32_t group2, uint32_t group3);

    private:
        friend class VoicePacket;
        friend class DataPacket;
        friend class Control;
        Control* m_p25;

        network::BaseNetwork* m_network;

        uint32_t m_patchSuperGroup;

        bool m_verifyAff;
        bool m_verifyReg;

        lc::TSBK m_rfTSBK;
        lc::TSBK m_netTSBK;
        uint8_t* m_rfMBF;
        uint8_t m_mbfCnt;

        uint8_t m_mbfIdenCnt;
        uint8_t m_mbfAdjSSCnt;
        uint8_t m_mbfSCCBCnt;

        lc::TDULC m_rfTDULC;
        lc::TDULC m_netTDULC;

        std::vector<uint32_t> m_voiceChTable;

        std::unordered_map<uint8_t, SiteData> m_adjSiteTable;
        std::unordered_map<uint8_t, uint8_t> m_adjSiteUpdateCnt;

        std::unordered_map<uint8_t, SiteData> m_sccbTable;
        std::unordered_map<uint8_t, uint8_t> m_sccbUpdateCnt;

        std::vector<uint32_t> m_unitRegTable;
        std::unordered_map<uint32_t, uint32_t> m_grpAffTable;

        std::unordered_map<uint32_t, uint32_t> m_grantChTable;
        std::unordered_map<uint32_t, Timer> m_grantTimers;

        uint8_t m_voiceChCnt;
        uint8_t m_voiceGrantChCnt;

        bool m_noStatusAck;
        bool m_noMessageAck;

        bool m_statusCmdEnable;
        uint8_t m_statusRadioCheck;
        uint8_t m_statusRadioInhibit;
        uint8_t m_statusRadioUninhibit;
        uint8_t m_statusRadioForceReg;
        uint8_t m_statusRadioForceDereg;

        uint32_t m_statusSrcId;
        uint8_t m_statusValue;

        SiteData m_siteData;

        Timer m_adjSiteUpdateTimer;
        uint32_t m_adjSiteUpdateInterval;

        bool m_verbose;
        bool m_debug;

        /// <summary>Initializes a new instance of the TrunkPacket class.</summary>
        TrunkPacket(Control* p25, network::BaseNetwork* network, bool dumpTSBKData, bool debug, bool verbose);
        /// <summary>Finalizes a instance of the TrunkPacket class.</summary>
        ~TrunkPacket();

        /// <summary>Write data processed from RF to the network.</summary>
        void writeNetworkRF(const uint8_t* data, bool autoReset);

        /// <summary>Helper to write control channel packet data.</summary>
        void writeRF_ControlData(uint8_t frameCnt, uint8_t n, bool adjSS);

        /// <summary>Helper to write a P25 TDU w/ link control packet.</summary>
        void writeRF_TDULC(uint8_t duid, bool noNetwork);
        /// <summary>Helper to write a P25 TDU w/ link control channel grant packet.</summary>
        void writeRF_TDULC_ChanGrant(bool grp, uint32_t srcId, uint32_t dstId);
        /// <summary>Helper to write a P25 TDU w/ link control channel release packet.</summary>
        void writeRF_TDULC_ChanRelease(bool grp, uint32_t srcId, uint32_t dstId);

        /// <summary>Helper to write a single-block P25 TSDU packet.</summary>
        void writeRF_TSDU_SBF(bool noNetwork, bool clearBeforeWrite = false);
        /// <summary>Helper to write a multi-block P25 TSDU packet.</summary>
        void writeRF_TSDU_MBF(bool clearBeforeWrite = false);

        /// <summary>Helper to queue the given control TSBK into the MBF queue.</summary>
        void queueRF_TSBK_Ctrl_MBF(uint8_t lco);

        /// <summary>Helper to write a grant packet.</summary>
        bool writeRF_TSDU_Grant(bool grp, bool skip);
        /// <summary>Helper to write a unit to unit answer request packet.</summary>
        void writeRF_TSDU_UU_Ans_Req(uint32_t srcId, uint32_t dstId);
        /// <summary>Helper to write a acknowledge packet.</summary>
        void writeRF_TSDU_ACK_FNE(uint32_t srcId, uint32_t service, bool extended, bool noNetwork);
        /// <summary>Helper to write a deny packet.</summary>
        void writeRF_TSDU_Deny(uint8_t reason, uint8_t service);
        /// <summary>Helper to write a group affiliation response packet.</summary>
        bool writeRF_TSDU_Grp_Aff_Rsp(uint32_t srcId, uint32_t dstId);
        /// <summary>Helper to write a unit registration response packet.</summary>
        void writeRF_TSDU_U_Reg_Rsp(uint32_t srcId);
        /// <summary>Helper to write a unit de-registration acknowledge packet.</summary>
        void writeRF_TSDU_U_Dereg_Ack(uint32_t srcId);
        /// <summary>Helper to write a queue packet.</summary>
        void writeRF_TSDU_Queue(uint8_t reason, uint8_t service);

        /// <summary>Helper to write a network TSDU from the RF data queue.</summary>
        void writeNet_TSDU_From_RF(uint8_t* data);

        /// <summary>Helper to write a network P25 TDU w/ link control packet.</summary>
        void writeNet_TDULC();
        /// <summary>Helper to write a network single-block P25 TSDU packet.</summary>
        void writeNet_TSDU();

        /// <summary>Helper to automatically inhibit a source ID on a denial.</summary>
        void denialInhibit(uint32_t srcId);

        /// <summary></summary>
        void resetStatusCommand(const lc::TSBK& tsbk);
        /// <summary></summary>
        void preprocessStatusCommand();
        /// <summary></summary>
        bool processStatusCommand(uint32_t srcId, uint32_t dstId);

        /// <summary>Helper to add the idle status bits on P25 frame data.</summary>
        void addIdleBits(uint8_t* data, uint32_t length, bool b1, bool b2);
    };
} // namespace p25

#endif // __P25_TRUNK_PACKET_H__
