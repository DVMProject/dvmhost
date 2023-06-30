/**
* Digital Voice Modem - Monitor
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Monitor
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
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
#if !defined(__NODE_STATUS_WND_H__)
#define __NODE_STATUS_WND_H__

#include "lookups/AffiliationLookup.h"
#include "modem/Modem.h"
#include "network/RESTDefines.h"
#include "remote/RESTClient.h"

#include "monitor/MonitorMainWnd.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define NODE_STATUS_WIDTH 28
#define NODE_STATUS_HEIGHT 7

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the node status display window.
// ---------------------------------------------------------------------------

class HOST_SW_API NodeStatusWnd final : public finalcut::FDialog
{
public:
    /// <summary>
    /// Initializes a new instance of the NodeStatusWnd class.
    /// </summary>
    /// <param name="widget"></param>
    explicit NodeStatusWnd(FWidget* widget = nullptr) : FDialog{widget}
    {
        m_timerId = addTimer(250); // starts the timer every 250 milliseconds
        m_reconnectTimerId = addTimer(15000); // starts the timer every 10 seconds
    }
    /// <summary>Copy constructor.</summary>
    NodeStatusWnd(const NodeStatusWnd&) = delete;
    /// <summary>Move constructor.</summary>
    NodeStatusWnd(NodeStatusWnd&&) noexcept = delete;
    /// <summary>Finalizes an instance of the NodeStatusWnd class.</summary>
    ~NodeStatusWnd() noexcept override = default;

    /// <summary>Disable copy assignment operator (=).</summary>
    auto operator= (const NodeStatusWnd&) -> NodeStatusWnd& = delete;
    /// <summary>Disable move assignment operator (=).</summary>
    auto operator= (NodeStatusWnd&&) noexcept -> NodeStatusWnd& = delete;

    /// <summary>Disable set X coordinate.</summary>
    void setX(int, bool = true) override { }
    /// <summary>Disable set Y coordinate.</summary>
    void setY(int, bool = true) override { }
    /// <summary>Disable set position.</summary>
    void setPos(const FPoint&, bool = true) override { }

    /// <summary>Sets the channel data.</summary>
    void setChData(lookups::VoiceChData chData) { m_chData = chData; }
    /// <summary>Sets the channel number.</summary>
    void setChannelId(uint8_t channelId) { m_channelId = channelId; }
    /// <summary>Sets the channel number.</summary>
    void setChannelNo(uint32_t channelNo) { m_channelNo = channelNo; }

private:
    int m_timerId;
    int m_reconnectTimerId;

    bool m_failed;
    bool m_control;
    bool m_tx;

    lookups::VoiceChData m_chData;
    uint8_t m_channelId;
    uint32_t m_channelNo;

    FLabel m_channelNoLabel{"Ch. No.: ", this};
    FLabel m_chanNo{this};

    FLabel m_txFreqLabel{"Tx: ", this};
    FLabel m_txFreq{this};
    FLabel m_rxFreqLabel{"Rx: ", this};
    FLabel m_rxFreq{this};

    FLabel m_lastTGLabel{"Last TG: ", this};
    FLabel m_lastTG{this};

    /// <summary>
    ///
    /// </summary>
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

    /// <summary>
    ///
    /// </summary>
    void draw() override
    {
        FDialog::draw();

        if (m_failed) {
            setColor(FColor::Yellow1, FColor::Red3);
        }
        else if (m_control) {
            setColor(FColor::LightGray, FColor::Purple3);
        }
        else if (m_tx) {
            setColor(FColor::LightGray, FColor::Green3);
        }
        else {
            setColor(FColor::LightGray, FColor::Black);
        }

        finalcut::drawBorder(this, FRect(FPoint{1, 2}, FPoint{NODE_STATUS_WIDTH, NODE_STATUS_HEIGHT}));
    }

    /// <summary>
    ///
    /// </summary>
    void initControls()
    {
        // channel number
        {
            m_channelNoLabel.setGeometry(FPoint(2, 1), FSize(10, 1));

            m_chanNo.setGeometry(FPoint(11, 1), FSize(8, 1));
            m_chanNo.setText(__INT_STR(m_channelNo));
        }

        // channel frequency
        {
            IdenTable entry = g_idenTable->find(m_channelId);
            if (entry.baseFrequency() == 0U) {
                ::LogError(LOG_HOST, "Channel Id %u has an invalid base frequency.", m_channelId);
            }

            if (entry.txOffsetMhz() == 0U) {
                ::LogError(LOG_HOST, "Channel Id %u has an invalid Tx offset.", m_channelId);
            }

            uint32_t calcSpace = (uint32_t)(entry.chSpaceKhz() / 0.125);
            float calcTxOffset = entry.txOffsetMhz() * 1000000;

            uint32_t rxFrequency = (uint32_t)((entry.baseFrequency() + ((calcSpace * 125) * m_channelNo)) + calcTxOffset);
            uint32_t txFrequency = (uint32_t)((entry.baseFrequency() + ((calcSpace * 125) * m_channelNo)));

            m_txFreqLabel.setGeometry(FPoint(2, 2), FSize(4, 1));
            m_txFreq.setGeometry(FPoint(6, 2), FSize(8, 1));

            std::stringstream ss;
            ss << std::fixed << std::setprecision(4) << (float)(txFrequency / 1000000.0f);

            m_txFreq.setText(ss.str());

            m_rxFreqLabel.setGeometry(FPoint(2, 3), FSize(4, 1));
            m_rxFreq.setGeometry(FPoint(6, 3), FSize(8, 1));

            ss.str(std::string());
            ss << std::fixed << std::setprecision(4) << (float)(rxFrequency / 1000000.0f);

            m_rxFreq.setText(ss.str());
        }

        // last TG
        {
            m_lastTGLabel.setGeometry(FPoint(2, 4), FSize(10, 1));

            m_lastTG.setGeometry(FPoint(11, 4), FSize(8, 1));
            m_lastTG.setText("None");
        }
    }

    /// <summary>
    ///
    /// </summary>
    /// <param name="timer"></param>
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
                        HTTP_GET, GET_STATUS, req, rsp, g_debug);
                    if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
                        ::LogError(LOG_HOST, "failed to get status for %s:%u, chNo = %u", m_chData.address().c_str(), m_chData.port(), m_channelNo);
                        m_failed = true;
                        setText("FAILED");
                    }
                    else {
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
                            bool resetControls = false;

                            // verify channel ID matches our configuration
                            if (channelId != m_channelId) {
                                ::LogWarning(LOG_HOST, "%s:%u, reports chId = %u, disagrees with chId = %u", m_chData.address().c_str(), m_chData.port(), channelId, m_channelId);
                                m_channelId = channelId;
                                resetControls = true;
                            }

                            // verify channel number matches our configuration
                            if (channelNo != m_channelNo) {
                                ::LogWarning(LOG_HOST, "%s:%u, reports chNo = %u, disagrees with chNo = %u", m_chData.address().c_str(), m_chData.port(), channelNo, m_channelNo);
                                m_channelNo = channelNo;
                                resetControls = true;
                            }

                            // reset controls for our display if necessary
                            if (resetControls) {
                                initControls();
                                setText(getText() + "*");
                            }
                        }
                        else {
                            ::LogWarning(LOG_HOST, "%s:%u, does not report channel information");
                        }

                        // report last known transmitted destination ID
                        if (rsp["lastDstId"].is<uint32_t>()) {
                            uint32_t lastDstId = rsp["lastDstId"].get<uint32_t>();
                            if (lastDstId == 0) {
                                m_lastTG.setText("None");
                            }
                            else {
                                m_lastTG.setText(__INT_STR(lastDstId));
                            }
                        }
                        else {
                            ::LogWarning(LOG_HOST, "%s:%u, does not report last TG information");
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
                        HTTP_GET, GET_STATUS, req, g_debug);
                    if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
                        ::LogError(LOG_HOST, "failed to get status for %s:%u, chNo = %u", m_chData.address().c_str(), m_chData.port(), m_channelNo);
                    }
                    else {
                        m_failed = false;
                        setText("UNKNOWN");
                    }
                }

                redraw();
            }
        }
    }
};

#endif // __NODE_STATUS_WND_H__