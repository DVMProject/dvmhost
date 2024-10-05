// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - FNE System View
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file SysViewApplication.h
 * @ingroup fneSysView
 */
#if !defined(__SYS_VIEW_APPLICATION_H__)
#define __SYS_VIEW_APPLICATION_H__

#include "common/Clock.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/lc/csbk/CSBKFactory.h"
#include "common/dmr/lc/LC.h"
#include "common/dmr/lc/FullLC.h"
#include "common/dmr/SlotType.h"
#include "common/dmr/Sync.h"
#include "common/p25/P25Defines.h"
#include "common/p25/lc/tdulc/TDULCFactory.h"
#include "common/p25/lc/tsbk/TSBKFactory.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/lc/RTCH.h"
#include "common/Log.h"
#include "common/StopWatch.h"
#include "network/PeerNetwork.h"
#include "SysViewMain.h"
#include "SysViewMainWnd.h"

using namespace system_clock;
using namespace network;

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements a color theme for a finalcut application.
 * @ingroup fneSysView
 */
class HOST_SW_API dvmColorTheme final : public FWidgetColors
{
public:
    /**
     * @brief Initializes a new instance of the dvmColorTheme class.
     */
    dvmColorTheme()
    {
        dvmColorTheme::setColorTheme();
    }

    /**
     * @brief Finalizes a instance of the dvmColorTheme class.
     */
    ~dvmColorTheme() noexcept override = default;

    /**
     * @brief Get the Class Name object
     * @return FString 
     */
    auto getClassName() const -> FString override { return "dvmColorTheme"; }
    /**
     * @brief Set the Color Theme object
     */
    void setColorTheme() override
    {
        term_fg                           = FColor::Cyan;
        term_bg                           = FColor::Blue;

        list_fg                           = FColor::Black;
        list_bg                           = FColor::LightGray;
        selected_list_fg                  = FColor::Red;
        selected_list_bg                  = FColor::LightGray;

        dialog_fg                         = FColor::Black;
        dialog_resize_fg                  = FColor::LightBlue;
        dialog_emphasis_fg                = FColor::Blue;
        dialog_bg                         = FColor::LightGray;

        error_box_fg                      = FColor::LightRed;
        error_box_emphasis_fg             = FColor::Yellow;
        error_box_bg                      = FColor::Black;

        tooltip_fg                        = FColor::White;
        tooltip_bg                        = FColor::Black;

        shadow_fg                         = FColor::Black;
        shadow_bg                         = FColor::LightGray;  // only for transparent shadow

        current_element_focus_fg          = FColor::White;
        current_element_focus_bg          = FColor::Cyan;
        current_element_fg                = FColor::LightBlue;
        current_element_bg                = FColor::Cyan;
        current_inc_search_element_fg     = FColor::LightRed;
        selected_current_element_focus_fg = FColor::LightRed;
        selected_current_element_focus_bg = FColor::Cyan;
        selected_current_element_fg       = FColor::Red;
        selected_current_element_bg       = FColor::Cyan;

        label_fg                          = FColor::Black;
        label_bg                          = FColor::LightGray;
        label_inactive_fg                 = FColor::DarkGray;
        label_inactive_bg                 = FColor::LightGray;
        label_hotkey_fg                   = FColor::Red;
        label_hotkey_bg                   = FColor::LightGray;
        label_emphasis_fg                 = FColor::Blue;
        label_ellipsis_fg                 = FColor::DarkGray;

        inputfield_active_focus_fg        = FColor::Yellow;
        inputfield_active_focus_bg        = FColor::Blue;
        inputfield_active_fg              = FColor::LightGray;
        inputfield_active_bg              = FColor::Blue;
        inputfield_inactive_fg            = FColor::Black;
        inputfield_inactive_bg            = FColor::DarkGray;

        toggle_button_active_focus_fg     = FColor::Yellow;
        toggle_button_active_focus_bg     = FColor::Blue;
        toggle_button_active_fg           = FColor::LightGray;
        toggle_button_active_bg           = FColor::Blue;
        toggle_button_inactive_fg         = FColor::Black;
        toggle_button_inactive_bg         = FColor::DarkGray;

        button_active_focus_fg            = FColor::Yellow;
        button_active_focus_bg            = FColor::Blue;
        button_active_fg                  = FColor::White;
        button_active_bg                  = FColor::Blue;
        button_inactive_fg                = FColor::Black;
        button_inactive_bg                = FColor::DarkGray;
        button_hotkey_fg                  = FColor::Yellow;

        titlebar_active_fg                = FColor::Blue;
        titlebar_active_bg                = FColor::White;
        titlebar_inactive_fg              = FColor::Blue;
        titlebar_inactive_bg              = FColor::LightGray;
        titlebar_button_fg                = FColor::Yellow;
        titlebar_button_bg                = FColor::LightBlue;
        titlebar_button_focus_fg          = FColor::LightGray;
        titlebar_button_focus_bg          = FColor::Black;

        menu_active_focus_fg              = FColor::Black;
        menu_active_focus_bg              = FColor::White;
        menu_active_fg                    = FColor::Black;
        menu_active_bg                    = FColor::LightGray;
        menu_inactive_fg                  = FColor::DarkGray;
        menu_inactive_bg                  = FColor::LightGray;
        menu_hotkey_fg                    = FColor::Blue;
        menu_hotkey_bg                    = FColor::LightGray;

        statusbar_fg                      = FColor::Black;
        statusbar_bg                      = FColor::LightGray;
        statusbar_hotkey_fg               = FColor::Blue;
        statusbar_hotkey_bg               = FColor::LightGray;
        statusbar_separator_fg            = FColor::Black;
        statusbar_active_fg               = FColor::Black;
        statusbar_active_bg               = FColor::White;
        statusbar_active_hotkey_fg        = FColor::Blue;
        statusbar_active_hotkey_bg        = FColor::White;

        scrollbar_fg                      = FColor::Cyan;
        scrollbar_bg                      = FColor::DarkGray;
        scrollbar_button_fg               = FColor::Yellow;
        scrollbar_button_bg               = FColor::DarkGray;
        scrollbar_button_inactive_fg      = FColor::LightGray;
        scrollbar_button_inactive_bg      = FColor::Black;

        progressbar_fg                    = FColor::Yellow;
        progressbar_bg                    = FColor::Blue;
    }
};

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the finalcut application.
 * @ingroup fneSysView
 */
class HOST_SW_API SysViewApplication final : public finalcut::FApplication {
public:
    /**
     * @brief Initializes a new instance of the SysViewApplication class.
     * @param argc Passed argc.
     * @param argv Passed argv.
     */
    explicit SysViewApplication(const int& argc, char** argv) : FApplication{argc, argv}
    {
        m_stopWatch.start();
    }
    /**
     * @brief Finalizes an instance of the SysViewApplication class.
     */
    ~SysViewApplication() noexcept override
    {
        closePeerNetwork();
    }

    /**
     * @brief Initializes peer network connectivity. 
     * @returns bool 
     */
    bool createPeerNetwork()
    {
        yaml::Node fne = g_conf["fne"];

        std::string password = fne["password"].as<std::string>();

        std::string address = fne["masterAddress"].as<std::string>();
        uint16_t port = (uint16_t)fne["masterPort"].as<uint32_t>();
        uint32_t id = fne["peerId"].as<uint32_t>();

        bool encrypted = fne["encrypted"].as<bool>(false);
        std::string key = fne["presharedKey"].as<std::string>();
        uint8_t presharedKey[AES_WRAPPED_PCKT_KEY_LEN];
        if (!key.empty()) {
            if (key.size() == 32) {
                // bryanb: shhhhhhh....dirty nasty hacks
                key = key.append(key); // since the key is 32 characters (16 hex pairs), double it on itself for 64 characters (32 hex pairs)
                LogWarning(LOG_HOST, "Half-length network preshared encryption key detected, doubling key on itself.");
            }

            if (key.size() == 64) {
                if ((key.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos)) {
                    const char* keyPtr = key.c_str();
                    ::memset(presharedKey, 0x00U, AES_WRAPPED_PCKT_KEY_LEN);

                    for (uint8_t i = 0; i < AES_WRAPPED_PCKT_KEY_LEN; i++) {
                        char t[4] = {keyPtr[0], keyPtr[1], 0};
                        presharedKey[i] = (uint8_t)::strtoul(t, NULL, 16);
                        keyPtr += 2 * sizeof(char);
                    }
                }
                else {
                    LogWarning(LOG_HOST, "Invalid characters in the network preshared encryption key. Encryption disabled.");
                    encrypted = false;
                }
            }
            else {
                LogWarning(LOG_HOST, "Invalid  network preshared encryption key length, key should be 32 hex pairs, or 64 characters. Encryption disabled.");
                encrypted = false;
            }
        }

        std::string identity = fne["identity"].as<std::string>();

        LogInfo("Network Parameters");
        LogInfo("    Peer ID: %u", id);
        LogInfo("    Address: %s", address.c_str());
        LogInfo("    Port: %u", port);

        LogInfo("    Encrypted: %s", encrypted ? "yes" : "no");

        if (id > 999999999U) {
            ::LogError(LOG_HOST, "Network Peer ID cannot be greater then 999999999.");
            return false;
        }

        // initialize networking
        network = new PeerNetwork(address, port, 0U, id, password, true, g_debug, true, true, true, true, true, true, true, true, false);
        network->setMetadata(identity, 0U, 0U, 0.0F, 0.0F, 0, 0, 0, 0.0F, 0.0F, 0, "");
        network->setLookups(g_ridLookup, g_tidLookup);

        ::LogSetNetwork(network);

        if (encrypted) {
            network->setPresharedKey(presharedKey);
        }

        network->enable(true);
        bool ret = network->open();
        if (!ret) {
            delete network;
            network = nullptr;
            LogError(LOG_HOST, "failed to initialize traffic networking for PEER %u", id);
            return false;
        }

        ::LogSetNetwork(network);

        return true;
    }

    /**
     * @brief Shuts down peer networking.
     */
    void closePeerNetwork()
    {
        if (network != nullptr) {
            network->close();
            delete network;
        }
    }

    /**
     * @brief Instance of the peer network.
     */
    network::PeerNetwork* network;

protected:
    /**
     * @brief Process external user events.
     */
    void processExternalUserEvent() override
    {
        uint32_t ms = m_stopWatch.elapsed();

        ms = m_stopWatch.elapsed();
        m_stopWatch.start();

        // ------------------------------------------------------
        //  -- Network Clocking                               --
        // ------------------------------------------------------

        if (network != nullptr) {
            network->clock(ms);

            hrc::hrc_t pktTime = hrc::now();

            uint32_t length = 0U;
            bool netReadRet = false;
            UInt8Array dmrBuffer = network->readDMR(netReadRet, length);
            if (netReadRet) {
                using namespace dmr;

                uint8_t seqNo = dmrBuffer[4U];

                uint32_t srcId = __GET_UINT16(dmrBuffer, 5U);
                uint32_t dstId = __GET_UINT16(dmrBuffer, 8U);

                DMRDEF::FLCO::E flco = (dmrBuffer[15U] & 0x40U) == 0x40U ? DMRDEF::FLCO::PRIVATE : DMRDEF::FLCO::GROUP;

                uint32_t slotNo = (dmrBuffer[15U] & 0x80U) == 0x80U ? 2U : 1U;

                DMRDEF::DataType::E dataType = (DMRDEF::DataType::E)(dmrBuffer[15U] & 0x0FU);

                data::NetData dmrData;
                dmrData.setSeqNo(seqNo);
                dmrData.setSlotNo(slotNo);
                dmrData.setSrcId(srcId);
                dmrData.setDstId(dstId);
                dmrData.setFLCO(flco);

                bool dataSync = (dmrBuffer[15U] & 0x20U) == 0x20U;
                bool voiceSync = (dmrBuffer[15U] & 0x10U) == 0x10U;

                if (dataSync) {
                    dmrData.setData(dmrBuffer.get() + 20U);
                    dmrData.setDataType(dataType);
                    dmrData.setN(0U);
                }
                else if (voiceSync) {
                    dmrData.setData(dmrBuffer.get() + 20U);
                    dmrData.setDataType(DMRDEF::DataType::VOICE_SYNC);
                    dmrData.setN(0U);
                }
                else {
                    uint8_t n = dmrBuffer[15U] & 0x0FU;
                    dmrData.setData(dmrBuffer.get() + 20U);
                    dmrData.setDataType(DMRDEF::DataType::VOICE);
                    dmrData.setN(n);
                }

                // is this the end of the call stream?
                if (dataSync && (dataType == DMRDEF::DataType::TERMINATOR_WITH_LC)) {
                    if (srcId == 0U && dstId == 0U) {
                        LogWarning(LOG_NET, "DMR, invalid TERMINATOR, srcId = %u (%s), dstId = %u (%s)", 
                            srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                    }

                    RxStatus status;
                    auto it = std::find_if(m_dmrStatus.begin(), m_dmrStatus.end(), [&](StatusMapPair x) { return (x.second.dstId == dstId && x.second.slotNo == slotNo); });
                    if (it == m_dmrStatus.end()) {
                        LogError(LOG_NET, "DMR, tried to end call for non-existent call in progress?, srcId = %u (%s), dstId = %u (%s)",
                            srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                    }
                    else {
                        status = it->second;
                    }

                    uint64_t duration = hrc::diff(pktTime, status.callStartTime);

                    if (std::find_if(m_dmrStatus.begin(), m_dmrStatus.end(), [&](StatusMapPair x) { return (x.second.dstId == dstId && x.second.slotNo == slotNo); }) != m_dmrStatus.end()) {
                        m_dmrStatus.erase(dstId);

                        LogMessage(LOG_NET, "DMR, Call End, srcId = %u (%s), dstId = %u (%s), duration = %u",
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str(), duration / 1000);
                    }
                }

                // is this a new call stream?
                if (dataSync && (dataType == DMRDEF::DataType::VOICE_LC_HEADER)) {
                    if (srcId == 0U && dstId == 0U) {
                        LogWarning(LOG_NET, "DMR, invalid call, srcId = %u (%s), dstId = %u (%s)", 
                            srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                    }

                    auto it = std::find_if(m_dmrStatus.begin(), m_dmrStatus.end(), [&](StatusMapPair x) { return (x.second.dstId == dstId && x.second.slotNo == slotNo); });
                    if (it == m_dmrStatus.end()) {
                        // this is a new call stream
                        RxStatus status = RxStatus();
                        status.callStartTime = pktTime;
                        status.srcId = srcId;
                        status.dstId = dstId;
                        status.slotNo = slotNo;
                        m_dmrStatus[dstId] = status; // this *could* be an issue if a dstId appears on both slots somehow...

                        LogMessage(LOG_NET, "DMR, Call Start, srcId = %u (%s), dstId = %u (%s)", 
                            srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                    }
                }

                // are we receiving a CSBK?
                if (dmrData.getDataType() == DMRDEF::DataType::CSBK) {
                    uint8_t data[DMRDEF::DMR_FRAME_LENGTH_BYTES + 2U];
                    dmrData.getData(data + 2U);

                    std::unique_ptr<lc::CSBK> csbk = lc::csbk::CSBKFactory::createCSBK(data + 2U, DMRDEF::DataType::CSBK);
                    if (csbk != nullptr) {
                        switch (csbk->getCSBKO()) {
                        case DMRDEF::CSBKO::BROADCAST:
                            {
                                lc::csbk::CSBK_BROADCAST* osp = static_cast<lc::csbk::CSBK_BROADCAST*>(csbk.get());
                                if (osp->getAnncType() == DMRDEF::BroadcastAnncType::ANN_WD_TSCC) {
                                    LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, %s, sysId = $%03X, chNo = %u", dmrData.getSlotNo(), csbk->toString().c_str(),
                                        osp->getSystemId(), osp->getLogicalCh1());
                                }
                            }
                            break;
                        default:
                            LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, %s, srcId = %u (%s), dstId = %u (%s)", dmrData.getSlotNo(), csbk->toString().c_str(), 
                                srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                            break;
                        }
                    }
                }

                if (g_debug)
                    LogMessage(LOG_NET, "DMR, slotNo = %u, seqNo = %u, flco = $%02X, srcId = %u, dstId = %u, len = %u", slotNo, seqNo, flco, srcId, dstId, length);
            }

            UInt8Array p25Buffer = network->readP25(netReadRet, length);
            if (netReadRet) {
                using namespace p25;

                uint8_t duid = p25Buffer[22U];
                uint8_t MFId = p25Buffer[15U];

                // process raw P25 data bytes
                UInt8Array data;
                uint8_t frameLength = p25Buffer[23U];
                if (duid == P25DEF::DUID::PDU) {
                    frameLength = length;
                    data = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
                    ::memset(data.get(), 0x00U, length);
                    ::memcpy(data.get(), p25Buffer.get(), length);
                }
                else {
                    if (frameLength <= 24) {
                        data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
                        ::memset(data.get(), 0x00U, frameLength);
                    }
                    else {
                        data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
                        ::memset(data.get(), 0x00U, frameLength);
                        ::memcpy(data.get(), p25Buffer.get() + 24U, frameLength);
                    }
                }

                uint8_t lco = p25Buffer[4U];

                uint32_t srcId = __GET_UINT16(p25Buffer, 5U);
                uint32_t dstId = __GET_UINT16(p25Buffer, 8U);

                uint32_t sysId = (p25Buffer[11U] << 8) | (p25Buffer[12U] << 0);
                uint32_t netId = __GET_UINT16(p25Buffer, 16U);

                // log call status
                if (duid != P25DEF::DUID::TSDU && duid != P25DEF::DUID::PDU) {
                    // is this the end of the call stream?
                    if ((duid == P25DEF::DUID::TDU) || (duid == P25DEF::DUID::TDULC)) {
                        if (srcId == 0U && dstId == 0U) {
                            LogWarning(LOG_NET, "P25, invalid TDU, srcId = %u (%s), dstId = %u (%s)", 
                                srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                        }

                        RxStatus status = m_p25Status[dstId];
                        uint64_t duration = hrc::diff(pktTime, status.callStartTime);

                        if (std::find_if(m_p25Status.begin(), m_p25Status.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; }) != m_p25Status.end()) {
                            m_p25Status.erase(dstId);

                            LogMessage(LOG_NET, "P25, Call End, srcId = %u (%s), dstId = %u (%s), duration = %u",
                                srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str(), duration / 1000);
                        }
                    }

                    // is this a new call stream?
                    if ((duid != P25DEF::DUID::TDU) && (duid != P25DEF::DUID::TDULC)) {
                        if (srcId == 0U && dstId == 0U) {
                            LogWarning(LOG_NET, "P25, invalid call, srcId = %u (%s), dstId = %u (%s)", 
                                srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                        }

                        auto it = std::find_if(m_p25Status.begin(), m_p25Status.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; });
                        if (it == m_p25Status.end()) {
                            // this is a new call stream
                            RxStatus status = RxStatus();
                            status.callStartTime = pktTime;
                            status.srcId = srcId;
                            status.dstId = dstId;
                            m_p25Status[dstId] = status;

                            LogMessage(LOG_NET, "P25, Call Start, srcId = %u (%s), dstId = %u (%s)", 
                                srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                        }
                    }
                }

                switch (duid) {
                case P25DEF::DUID::TDU:
                case P25DEF::DUID::TDULC:
                    if (duid == P25DEF::DUID::TDU) {
                        LogMessage(LOG_NET, P25_TDU_STR ", srcId = %u (%s), dstId = %u (%s)", 
                                srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                    }
                    else {
                        std::unique_ptr<lc::TDULC> tdulc = lc::tdulc::TDULCFactory::createTDULC(data.get());
                        if (tdulc == nullptr) {
                            LogWarning(LOG_NET, P25_TDULC_STR ", undecodable TDULC");
                        }
                        else {
                            LogMessage(LOG_NET, P25_TDULC_STR ", srcId = %u (%s), dstId = %u (%s)", 
                                srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                        }
                    }
                    break;

                case P25DEF::DUID::TSDU:
                    std::unique_ptr<lc::TSBK> tsbk = lc::tsbk::TSBKFactory::createTSBK(data.get());
                    if (tsbk == nullptr) {
                        LogWarning(LOG_NET, P25_TSDU_STR ", undecodable TSBK");
                    }
                    else {
                        switch (tsbk->getLCO()) {
                            case P25DEF::TSBKO::IOSP_GRP_VCH:
                            case P25DEF::TSBKO::IOSP_UU_VCH:
                            {
                                LogMessage(LOG_NET, P25_TSDU_STR ", %s, emerg = %u, encrypt = %u, prio = %u, chNo = %u, srcId = %u (%s), dstId = %u (%s)",
                                    tsbk->toString(true).c_str(), tsbk->getEmergency(), tsbk->getEncrypted(), tsbk->getPriority(), tsbk->getGrpVchNo(), 
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                            }
                            break;
                            case P25DEF::TSBKO::IOSP_UU_ANS:
                            {
                                lc::tsbk::IOSP_UU_ANS* iosp = static_cast<lc::tsbk::IOSP_UU_ANS*>(tsbk.get());
                                if (iosp->getResponse() > 0U) {
                                    LogMessage(LOG_NET, P25_TSDU_STR ", %s, response = $%02X, srcId = %u (%s), dstId = %u (%s)",
                                        tsbk->toString(true).c_str(), iosp->getResponse(),
                                        srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                                }
                            }
                            break;
                            case P25DEF::TSBKO::IOSP_STS_UPDT:
                            {
                                lc::tsbk::IOSP_STS_UPDT* iosp = static_cast<lc::tsbk::IOSP_STS_UPDT*>(tsbk.get());
                                LogMessage(LOG_NET, P25_TSDU_STR ", %s, status = $%02X, srcId = %u (%s)",
                                    tsbk->toString(true).c_str(), iosp->getStatus(), srcId, resolveRID(srcId).c_str());
                            }
                            break;
                            case P25DEF::TSBKO::IOSP_MSG_UPDT:
                            {
                                lc::tsbk::IOSP_MSG_UPDT* iosp = static_cast<lc::tsbk::IOSP_MSG_UPDT*>(tsbk.get());
                                LogMessage(LOG_NET, P25_TSDU_STR ", %s, message = $%02X, srcId = %u (%s), dstId = %u (%s)",
                                    tsbk->toString(true).c_str(), iosp->getMessage(), 
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                            }
                            break;
                            case P25DEF::TSBKO::IOSP_RAD_MON:
                            {
                                lc::tsbk::IOSP_RAD_MON* iosp = static_cast<lc::tsbk::IOSP_RAD_MON*>(tsbk.get());
                                LogMessage(LOG_NET, P25_TSDU_STR ", %s, srcId = %u (%s), dstId = %u (%s)", tsbk->toString(true).c_str(), 
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                            }
                            break;
                            case P25DEF::TSBKO::IOSP_CALL_ALRT:
                            {
                                LogMessage(LOG_NET, P25_TSDU_STR ", %s, srcId = %u (%s), dstId = %u (%s)", tsbk->toString(true).c_str(), 
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                            }
                            break;
                            case P25DEF::TSBKO::IOSP_ACK_RSP:
                            {
                                lc::tsbk::IOSP_ACK_RSP* iosp = static_cast<lc::tsbk::IOSP_ACK_RSP*>(tsbk.get());
                                LogMessage(LOG_NET, P25_TSDU_STR ", %s, AIV = %u, serviceType = $%02X, srcId = %u (%s), dstId = %u (%s)",
                                    tsbk->toString(true).c_str(), iosp->getAIV(), iosp->getService(), 
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                            }
                            break;
                            case P25DEF::TSBKO::IOSP_EXT_FNCT:
                            {
                                lc::tsbk::IOSP_EXT_FNCT* iosp = static_cast<lc::tsbk::IOSP_EXT_FNCT*>(tsbk.get());
                                LogMessage(LOG_NET, P25_TSDU_STR ", %s, serviceType = $%02X, arg = %u, tgt = %u",
                                    tsbk->toString(true).c_str(), iosp->getService(), srcId, dstId);
                            }
                            break;
                            case P25DEF::TSBKO::ISP_EMERG_ALRM_REQ:
                            {
                                // non-emergency mode is a TSBKO::OSP_DENY_RSP
                                if (!tsbk->getEmergency()) {
                                    lc::tsbk::OSP_DENY_RSP* osp = static_cast<lc::tsbk::OSP_DENY_RSP*>(tsbk.get());
                                    LogMessage(LOG_NET, P25_TSDU_STR ", %s, AIV = %u, reason = $%02X, srcId = %u (%s), dstId = %u (%s)",
                                        osp->toString().c_str(), osp->getAIV(), osp->getResponse(), 
                                        osp->getSrcId(), resolveRID(osp->getSrcId()).c_str(), osp->getDstId(), resolveTGID(osp->getDstId()).c_str());
                                } else {
                                    LogMessage(LOG_NET, P25_TSDU_STR ", %s, srcId = %u (%s), dstId = %u (%s)", tsbk->toString().c_str(), 
                                        srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                                }
                            }
                            break;
                            case P25DEF::TSBKO::IOSP_GRP_AFF:
                            {
                                lc::tsbk::IOSP_GRP_AFF* iosp = static_cast<lc::tsbk::IOSP_GRP_AFF*>(tsbk.get());
                                LogMessage(LOG_NET, P25_TSDU_STR ", %s, sysId = $%03X, anncId = %u (%s), srcId = %u (%s), dstId = %u (%s), response = $%02X", tsbk->toString().c_str(),
                                        iosp->getSysId(), iosp->getAnnounceGroup(), resolveTGID(iosp->getAnnounceGroup()).c_str(),
                                        srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str(),
                                        iosp->getResponse());
                            }
                            break;
                            case P25DEF::TSBKO::OSP_U_DEREG_ACK:
                            {
                                lc::tsbk::OSP_U_DEREG_ACK* iosp = static_cast<lc::tsbk::OSP_U_DEREG_ACK*>(tsbk.get());
                                LogMessage(LOG_NET, P25_TSDU_STR ", %s, srcId = %u (%s)",
                                    tsbk->toString(true).c_str(), srcId, resolveRID(srcId).c_str());
                            }
                            break;
                            case P25DEF::TSBKO::OSP_LOC_REG_RSP:
                            {
                                lc::tsbk::OSP_LOC_REG_RSP* osp = static_cast<lc::tsbk::OSP_LOC_REG_RSP*>(tsbk.get());
                                LogMessage(LOG_NET, P25_TSDU_STR ", %s, srcId = %u (%s), dstId = %u (%s)", osp->toString().c_str(), 
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                            }
                            break;
                            case P25DEF::TSBKO::OSP_ADJ_STS_BCAST:
                            {
                                lc::tsbk::OSP_ADJ_STS_BCAST* osp = static_cast<lc::tsbk::OSP_ADJ_STS_BCAST*>(tsbk.get());
                                LogMessage(LOG_NET, P25_TSDU_STR ", %s, sysId = $%03X, rfss = $%02X, site = $%02X, chId = %u, chNo = %u, svcClass = $%02X", tsbk->toString().c_str(),
                                    osp->getAdjSiteSysId(), osp->getAdjSiteRFSSId(), osp->getAdjSiteId(), osp->getAdjSiteChnId(), osp->getAdjSiteChnNo(), osp->getAdjSiteSvcClass());
                            }
                            break;
                            default:
                                LogMessage(LOG_NET, P25_TSDU_STR ", %s, srcId = %u (%s), dstId = %u (%s)", tsbk->toString().c_str(), 
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                                break;
                        }
                    }
                    break;
                }

                if (g_debug)
                    LogMessage(LOG_NET, "P25, duid = $%02X, lco = $%02X, MFId = $%02X, srcId = %u, dstId = %u, len = %u", duid, lco, MFId, srcId, dstId, length);
            }

            UInt8Array nxdnBuffer = network->readNXDN(netReadRet, length);
            if (netReadRet) {
                using namespace nxdn;

                uint8_t messageType = nxdnBuffer[4U];

                uint32_t srcId = __GET_UINT16(nxdnBuffer, 5U);
                uint32_t dstId = __GET_UINT16(nxdnBuffer, 8U);

                lc::RTCH lc;

                lc.setMessageType(messageType);
                lc.setSrcId((uint16_t)srcId & 0xFFFFU);
                lc.setDstId((uint16_t)dstId & 0xFFFFU);

                bool group = (nxdnBuffer[15U] & 0x40U) == 0x40U ? false : true;
                lc.setGroup(group);

                // specifically only check the following logic for end of call, voice or data frames
                if ((messageType == NXDDEF::MessageType::RTCH_TX_REL || messageType == NXDDEF::MessageType::RTCH_TX_REL_EX) ||
                    (messageType == NXDDEF::MessageType::RTCH_VCALL || messageType == NXDDEF::MessageType::RTCH_DCALL_HDR ||
                    messageType == NXDDEF::MessageType::RTCH_DCALL_DATA)) {
                    // is this the end of the call stream?
                    if (messageType == NXDDEF::MessageType::RTCH_TX_REL || messageType == NXDDEF::MessageType::RTCH_TX_REL_EX) {
                        if (srcId == 0U && dstId == 0U) {
                            LogWarning(LOG_NET, "NXDN, invalid TX_REL, srcId = %u (%s), dstId = %u (%s)", 
                                srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                        }

                        RxStatus status = m_nxdnStatus[dstId];
                        uint64_t duration = hrc::diff(pktTime, status.callStartTime);

                        if (std::find_if(m_nxdnStatus.begin(), m_nxdnStatus.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; }) != m_nxdnStatus.end()) {
                            m_nxdnStatus.erase(dstId);

                            LogMessage(LOG_NET, "NXDN, Call End, srcId = %u (%s), dstId = %u (%s), duration = %u",
                                srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str(), duration / 1000);
                        }
                    }

                    // is this a new call stream?
                    if ((messageType != NXDDEF::MessageType::RTCH_TX_REL && messageType != NXDDEF::MessageType::RTCH_TX_REL_EX)) {
                        if (srcId == 0U && dstId == 0U) {
                            LogWarning(LOG_NET, "NXDN, invalid call, srcId = %u (%s), dstId = %u (%s)", 
                                srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                        }

                        auto it = std::find_if(m_nxdnStatus.begin(), m_nxdnStatus.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; });
                        if (it == m_nxdnStatus.end()) {
                            // this is a new call stream
                            RxStatus status = RxStatus();
                            status.callStartTime = pktTime;
                            status.srcId = srcId;
                            status.dstId = dstId;
                            m_nxdnStatus[dstId] = status;

                            LogMessage(LOG_NET, "NXDN, Call Start, srcId = %u (%s), dstId = %u (%s)", 
                                srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                        }
                    }
                }

                if (g_debug)
                    LogMessage(LOG_NET, "NXDN, messageType = $%02X, srcId = %u, dstId = %u, len = %u", messageType, srcId, dstId, length);
            }
        }

        if (ms < 2U)
            Thread::sleep(1U);
    }

private:
    /**
     * @brief Represents the receive status of a call.
     */
    class RxStatus {
    public:
        system_clock::hrc::hrc_t callStartTime;
        system_clock::hrc::hrc_t lastPacket;
        uint32_t srcId;
        uint32_t dstId;
        uint8_t slotNo;
        uint32_t streamId;
    };
    typedef std::pair<const uint32_t, RxStatus> StatusMapPair;
    std::unordered_map<uint32_t, RxStatus> m_dmrStatus;
    std::unordered_map<uint32_t, RxStatus> m_p25Status;
    std::unordered_map<uint32_t, RxStatus> m_nxdnStatus;

    StopWatch m_stopWatch;
};

#endif // __SYS_VIEW_APPLICATION_H__