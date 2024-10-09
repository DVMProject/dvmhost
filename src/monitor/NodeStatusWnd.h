// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Host Monitor Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file NodeStatusWnd.h
 * @ingroup monitor
 */
#if !defined(__NODE_STATUS_WND_H__)
#define __NODE_STATUS_WND_H__

#include "common/lookups/AffiliationLookup.h"
#include "host/network/RESTDefines.h"
#include "host/modem/Modem.h"
#include "remote/RESTClient.h"

#include "MonitorMainWnd.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define NODE_STATUS_WIDTH 28
#define NODE_STATUS_HEIGHT 8
#define NODE_UPDATE_FAIL_CNT 4

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the node status display window.
 * @ingroup monitor
 */
class HOST_SW_API NodeStatusWnd final : public finalcut::FDialog {
public:
    /**
     * @brief Initializes a new instance of the NodeStatusWnd class.
     * @param widget 
     */
    explicit NodeStatusWnd(FWidget* widget = nullptr) : FDialog{widget}
    {
        m_timerId = addTimer(250); // starts the timer every 250 milliseconds
        m_reconnectTimerId = addTimer(15000); // starts the timer every 10 seconds
    }
    /**
     * @brief Copy constructor.
     */
    NodeStatusWnd(const NodeStatusWnd&) = delete;
    /**
     * @brief Move constructor.
     */
    NodeStatusWnd(NodeStatusWnd&&) noexcept = delete;
    /**
     * @brief Finalizes an instance of the NodeStatusWnd class.
     */
    ~NodeStatusWnd() noexcept override = default;

    /**
     * @brief Disable copy assignment operator (=).
     */
    auto operator= (const NodeStatusWnd&) -> NodeStatusWnd& = delete;
    /**
     * @brief Disable move assignment operator (=).
     */
    auto operator= (NodeStatusWnd&&) noexcept -> NodeStatusWnd& = delete;

    /**
     * @brief Disable set X coordinate.
     */
    void setX(int, bool = true) override { }
    /**
     * @brief Disable set Y coordinate.
     */
    void setY(int, bool = true) override { }
    /**
     * @brief Disable set position.
     */
    void setPos(const FPoint&, bool = true) override { }

    /**
     * @brief Gets the channel ID.
     * @returns uint8_t Channel ID.
     */
    uint8_t getChannelId() const { return m_channelId; }
    /**
     * @brief Gets the channel number.
     * @returns uint32_t Channel Number.
     */
    uint32_t getChannelNo() const { return m_channelNo; }
    /**
     * @brief Gets the channel data.
     * @returns lookups::VoiceChData Channel Data.
     */
    lookups::VoiceChData getChData() { return m_chData; }
    /**
     * @brief Sets the channel data.
     * @param chData Channel Data.
     */
    void setChData(lookups::VoiceChData chData) { m_chData = chData; }
    /**
     * @brief Gets the peer ID.
     * @param uint32_t Peer ID.
     */
    uint32_t getPeerId() const { return m_peerId; }

private:
    int m_timerId;
    int m_reconnectTimerId;

    uint8_t m_failCnt = 0U;
    bool m_failed;
    bool m_control;
    bool m_tx;

    lookups::VoiceChData m_chData;
    uint8_t m_channelId;
    uint32_t m_channelNo;
    uint32_t m_peerId;

    FLabel m_modeStr{this};
    FLabel m_peerIdStr{this};

    FLabel m_channelNoLabel{"Ch. No.: ", this};
    FLabel m_chanNo{this};

    FLabel m_txFreqLabel{"Tx: ", this};
    FLabel m_txFreq{this};
    FLabel m_rxFreqLabel{"Rx: ", this};
    FLabel m_rxFreq{this};

    FLabel m_lastDstLabel{"Last Dst: ", this};
    FLabel m_lastDst{this};
    FLabel m_lastSrcLabel{"Last Src: ", this};
    FLabel m_lastSrc{this};

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FDialog::setMinimumSize(FSize{NODE_STATUS_WIDTH, NODE_STATUS_HEIGHT});

        FDialog::setResizeable(false);
        FDialog::setMinimizable(false);
        FDialog::setTitlebarButtonVisibility(false);
        FDialog::setShadow(false);
        FDialog::setModal(false);

        FDialog::setText("UNKNOWN");

        initControls();

        FDialog::initLayout();
    }

    /**
     * @brief Draws the window.
     */
    void draw() override
    {
        FDialog::draw();

        if (m_failed) {
            setColor(FColor::LightGray, FColor::LightRed);
        }
        else if (m_control) {
            setColor(FColor::LightGray, FColor::Purple1);
        }
        else if (m_tx) {
            setColor(FColor::LightGray, FColor::LightGreen);
        }
        else {
            setColor(FColor::LightGray, FColor::Black);
        }

        finalcut::drawBorder(this, FRect(FPoint{1, 2}, FPoint{NODE_STATUS_WIDTH, NODE_STATUS_HEIGHT}));
    }

    /**
     * @brief Initializes window controls.
     */
    void initControls()
    {
        m_modeStr.setGeometry(FPoint(22, 1), FSize(4, 1));
        m_modeStr.setAlignment(Align::Right);
        m_modeStr.setEmphasis();

        m_peerIdStr.setGeometry(FPoint(17, 2), FSize(9, 1));
        m_peerIdStr.setAlignment(Align::Right);

        // channel number
        {
            m_channelNoLabel.setGeometry(FPoint(2, 1), FSize(10, 1));

            m_chanNo.setGeometry(FPoint(11, 1), FSize(8, 1));
            m_chanNo.setText("");
        }

        // channel frequency
        {
            m_txFreqLabel.setGeometry(FPoint(2, 2), FSize(4, 1));
            m_txFreq.setGeometry(FPoint(6, 2), FSize(9, 1));
            m_txFreq.setText("");

            m_rxFreqLabel.setGeometry(FPoint(2, 3), FSize(4, 1));
            m_rxFreq.setGeometry(FPoint(6, 3), FSize(9, 1));
            m_rxFreq.setText("");
        }

        // last TG
        {
            m_lastDstLabel.setGeometry(FPoint(2, 4), FSize(11, 1));

            m_lastDst.setGeometry(FPoint(13, 4), FSize(8, 1));
            m_lastDst.setText("None");
        }

        // last source
        {
            m_lastSrcLabel.setGeometry(FPoint(2, 5), FSize(11, 1));

            m_lastSrc.setGeometry(FPoint(13, 5), FSize(8, 1));
            m_lastSrc.setText("None");
        }
    }

    /**
     * @brief Helper to calculate the Tx/Rx frequencies of a channel.
     */
    void calculateRxTx()
    {
        IdenTable entry = g_idenTable->find(m_channelId);
        if (entry.baseFrequency() == 0U) {
            ::LogError(LOG_HOST, "Channel Id %u has an invalid base frequency.", m_channelId);
        }

        if (entry.txOffsetMhz() == 0U) {
            ::LogError(LOG_HOST, "Channel Id %u has an invalid Tx offset.", m_channelId);
        }

        m_chanNo.setText(__INT_STR(m_channelId) + "-" + __INT_STR(m_channelNo));

        uint32_t calcSpace = (uint32_t)(entry.chSpaceKhz() / 0.125);
        float calcTxOffset = entry.txOffsetMhz() * 1000000.0;

        uint32_t rxFrequency = (uint32_t)((entry.baseFrequency() + ((calcSpace * 125) * m_channelNo)) + (uint32_t)calcTxOffset);
        uint32_t txFrequency = (uint32_t)((entry.baseFrequency() + ((calcSpace * 125) * m_channelNo)));

        std::stringstream ss;
        ss << std::fixed << std::setprecision(5) << (double)(txFrequency / 1000000.0);

        m_txFreq.setText(ss.str());

        ss.str(std::string());
        ss << std::fixed << std::setprecision(5) << (double)(rxFrequency / 1000000.0);

        m_rxFreq.setText(ss.str());

        if (isWindowActive()) {
            emitCallback("update-selected");
        }
    }

    /*
    ** Event Handlers
    */

    /**
     * @brief Event that occurs when the window is raised.
     * @param e Event.
     */
    void onWindowRaised(FEvent* e) override
    {
        FDialog::onWindowLowered(e);
        emitCallback("update-selected");
    }

    /**
     * @brief Event that occurs on interval by timer.
     * @param timer Timer Event
     */
    void onTimer(FTimerEvent* timer) override
    {
        if (timer != nullptr) {
            // update timer
            if (timer->getTimerId() == m_timerId) {
                if (!m_failed) {
                    // callback REST API to get status of the channel we represent
                    json::object req = json::object();
                    json::object rsp = json::object();
                
                    int ret = RESTClient::send(m_chData.address(), m_chData.port(), m_chData.password(),
                        HTTP_GET, GET_STATUS, req, rsp, m_chData.ssl(), g_debug);
                    if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
                        ::LogError(LOG_HOST, "failed to get status for %s:%u, chNo = %u", m_chData.address().c_str(), m_chData.port(), m_channelNo);
                        ++m_failCnt;
                        if (m_failCnt > NODE_UPDATE_FAIL_CNT) {
                            m_failed = true;
                            setText("FAILED");
                        }
                    }
                    else {
                        try {
                            m_failCnt = 0U;

                            uint8_t mode = rsp["state"].get<uint8_t>();
                            switch (mode) {
                            case modem::STATE_DMR:
                                m_modeStr.setText("DMR");
                                break;
                            case modem::STATE_P25:
                                m_modeStr.setText("P25");
                                break;
                            case modem::STATE_NXDN:
                                m_modeStr.setText("NXDN");
                                break;
                            default:
                                m_modeStr.setText("");
                                break;
                            }

                            if (rsp["peerId"].is<uint32_t>()) {
                                m_peerId = rsp["peerId"].get<uint32_t>();
                                m_peerIdStr.setText(__INT_STR(m_peerId));
                            }

                            // get remote node state
                            if (rsp["dmrTSCCEnable"].is<bool>() && rsp["p25CtrlEnable"].is<bool>() &&
                                rsp["nxdnCtrlEnable"].is<bool>()) {
                                bool dmrTSCCEnable = rsp["dmrTSCCEnable"].get<bool>();
                                bool dmrCC = rsp["dmrCC"].get<bool>();
                                bool p25CtrlEnable = rsp["p25CtrlEnable"].get<bool>();
                                bool p25CC = rsp["p25CC"].get<bool>();
                                bool nxdnCtrlEnable = rsp["nxdnCtrlEnable"].get<bool>();
                                bool nxdnCC = rsp["nxdnCC"].get<bool>();

                                // are we a dedicated control channel?
                                if (dmrCC || p25CC || nxdnCC) {
                                    m_control = true;
                                    setText("CONTROL");
                                }

                                // if we aren't a dedicated control channel; set our
                                // title bar appropriately and set Tx state
                                if (!m_control) {
                                    if (dmrTSCCEnable || p25CtrlEnable || nxdnCtrlEnable) {
                                        setText("ENH. VOICE/CONV");
                                    }
                                    else {
                                        setText("VOICE/CONV");
                                    }

                                    // are we transmitting?
                                    if (rsp["tx"].is<bool>()) {
                                        m_tx = rsp["tx"].get<bool>();
                                    }
                                    else {
                                        ::LogWarning(LOG_HOST, "%s:%u, does not report Tx status");
                                        m_tx = false;
                                    }
                                }
                            }

                            // get the remote node channel information
                            if (rsp["channelId"].is<uint8_t>() && rsp["channelNo"].is<uint32_t>()) {
                                uint8_t channelId = rsp["channelId"].get<uint8_t>();
                                uint32_t channelNo = rsp["channelNo"].get<uint32_t>();

                                if (m_channelId != channelId && m_channelNo != channelNo) {
                                    m_channelId = channelId;
                                    m_channelNo = channelNo;

                                    calculateRxTx();
                                }
                            }
                            else {
                                ::LogWarning(LOG_HOST, "%s:%u, does not report channel information");
                            }

                            // report last known transmitted destination ID
                            if (rsp["lastDstId"].is<uint32_t>()) {
                                uint32_t lastDstId = rsp["lastDstId"].get<uint32_t>();
                                if (lastDstId == 0) {
                                    m_lastDst.setText("None");
                                }
                                else {
                                    m_lastDst.setText(__INT_STR(lastDstId));
                                }
                            }
                            else {
                                ::LogWarning(LOG_HOST, "%s:%u, does not report last TG information");
                            }

                            // report last known transmitted source ID
                            if (rsp["lastSrcId"].is<uint32_t>()) {
                                uint32_t lastSrcId = rsp["lastSrcId"].get<uint32_t>();
                                if (lastSrcId == 0) {
                                    m_lastSrc.setText("None");
                                }
                                else {
                                    m_lastSrc.setText(__INT_STR(lastSrcId));
                                }
                            }
                            else {
                                ::LogWarning(LOG_HOST, "%s:%u, does not report last source information");
                            }
                        }
                        catch (std::exception& e) {
                            ::LogWarning(LOG_HOST, "%s:%u, failed to properly handle status, %s", m_chData.address().c_str(), m_chData.port(), e.what());
                        }
                    }
                }

                redraw();
            }

            // reconnect timer
            if (timer->getTimerId() == m_reconnectTimerId) {
                if (m_failed) {
                    ::LogInfoEx(LOG_HOST, "attempting to reconnect to %s:%u, chNo = %u", m_chData.address().c_str(), m_chData.port(), m_channelNo);
                    // callback REST API to get status of the channel we represent
                    json::object req = json::object();
                    int ret = RESTClient::send(m_chData.address(), m_chData.port(), m_chData.password(),
                        HTTP_GET, GET_STATUS, req, m_chData.ssl(), g_debug);
                    if (ret == network::rest::http::HTTPPayload::StatusType::OK) {
                        m_failed = false;
                        m_failCnt = 0U;
                        setText("UNKNOWN");
                    }
                }

                redraw();
            }
        }
    }
};

#endif // __NODE_STATUS_WND_H__