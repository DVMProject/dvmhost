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
 * @file PeerListWnd.h
 * @ingroup fneSysView
 */
#if !defined(__PEER_LIST_WND_H__)
#define __PEER_LIST_WND_H__

#include "common/lookups/AffiliationLookup.h"
#include "common/Log.h"
#include "fne/network/RESTDefines.h"
#include "remote/RESTClient.h"

#include "FDblDialog.h"
#include "SysViewMainWnd.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define PEER_LIST_WIDTH 56
#define PEER_LIST_HEIGHT 15

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the peer list window.
 * @ingroup fneSysView
 */
class HOST_SW_API PeerListWnd final : public FDblDialog {
public:
    /**
     * @brief Initializes a new instance of the PeerListWnd class.
     * @param widget 
     */
    explicit PeerListWnd(FWidget* widget = nullptr) : FDblDialog{widget}
    {
        m_timerId = addTimer(25000); // starts the timer every 25 seconds
    }
    /**
     * @brief Copy constructor.
     */
    PeerListWnd(const PeerListWnd&) = delete;
    /**
     * @brief Move constructor.
     */
    PeerListWnd(PeerListWnd&&) noexcept = delete;
    /**
     * @brief Finalizes an instance of the PeerListWnd class.
     */
    ~PeerListWnd() noexcept override = default;

    /**
     * @brief Disable copy assignment operator (=).
     */
    auto operator= (const PeerListWnd&) -> PeerListWnd& = delete;
    /**
     * @brief Disable move assignment operator (=).
     */
    auto operator= (PeerListWnd&&) noexcept -> PeerListWnd& = delete;

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
     * @brief Populates the talkgroup listview.
     */
    void loadListView()
    {
        yaml::Node fne = g_conf["fne"];
        std::string fneRESTAddress = fne["restAddress"].as<std::string>("127.0.0.1");
        uint16_t fneRESTPort = (uint16_t)fne["restPort"].as<uint32_t>(9990U);
        std::string fnePassword = fne["restPassword"].as<std::string>("PASSWORD");
        bool fneSSL = fne["restSsl"].as<bool>(false);

        // callback REST API to get status of the channel we represent
        json::object req = json::object();
        json::object rsp = json::object();
    
        int ret = RESTClient::send(fneRESTAddress, fneRESTPort, fnePassword,
            HTTP_GET, FNE_GET_PEER_QUERY, req, rsp, fneSSL, g_debug);
        if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
            ::LogError(LOG_HOST, "[AFFVIEW] failed to query peers for %s:%u", fneRESTAddress.c_str(), fneRESTPort);
        }
        else {
            try {
                m_listView.clear();
                
                json::array fnePeers = rsp["peers"].get<json::array>();
                uint32_t cnt = 0U;
                for (auto entry : fnePeers) {
                    json::object peerObj = entry.get<json::object>();
                    uint32_t peerId = peerObj["peerId"].getDefault<uint32_t>(0U);
                    std::string peerAddress = peerObj["address"].getDefault<std::string>("");
                    uint16_t port = (uint16_t)peerObj["port"].getDefault<uint32_t>(0U);
                    bool connected = peerObj["connected"].getDefault<bool>(false);
                    uint32_t connectionState = (uint32_t)peerObj["connectionState"].getDefault<uint32_t>(0U);
                    std::string strConnState = "";
                    switch (connectionState) {
                    case network::NET_CONN_STATUS::NET_STAT_RUNNING:
                        strConnState = "Connected";
                        break;
                    case network::NET_CONN_STATUS::NET_STAT_WAITING_LOGIN:
                        strConnState = "Waiting for Login";
                        break;
                    case network::NET_CONN_STATUS::NET_STAT_WAITING_AUTHORISATION:
                        strConnState = "Waiting for Auth";
                        break;
                    case network::NET_CONN_STATUS::NET_STAT_WAITING_CONFIG:
                        strConnState = "Waiting for Config";
                        break;
                    default:
                        strConnState = " ?? ";
                        break;
                    }

                    uint32_t pingsReceived = (uint32_t)peerObj["pingsReceived"].getDefault<uint32_t>(0U);
                    uint64_t lastPing = (uint64_t)peerObj["lastPing"].getDefault<uint32_t>(0U);

                    uint32_t parentPeerId = 0U;
                    if (peerObj["parentPeerId"].is<uint32_t>())
                        parentPeerId = (uint32_t)peerObj["parentPeerId"].getDefault<uint32_t>(0U);

                    uint32_t ccPeerId = (uint32_t)peerObj["controlChannel"].getDefault<uint32_t>(0U);

                    std::vector<uint32_t> voiceChannelPeers;
                    if (peerObj["voiceChannels"].is<json::array>()) {
                        json::array voiceChannels = peerObj["voiceChannels"].get<json::array>();
                        for (auto vcEntry : voiceChannels) {
                            uint32_t vcPeerId = vcEntry.getDefault<uint32_t>(0U);
                            voiceChannelPeers.push_back(vcPeerId);
                        }
                    }

                    std::string identity = "* UNK *";
                    std::string software = "**INVALID**";
                    uint32_t chNo = 0U;
                    uint8_t chId = 0U;

                    std::ostringstream txOss;
                    std::ostringstream rxOss;

                    if (peerObj["config"].is<json::object>()) {
                        json::object peerConfig = peerObj["config"].get<json::object>();
                        identity = peerConfig["identity"].getDefault<std::string>("* UNK *");
                        software = peerConfig["software"].getDefault<std::string>("**INVALID**");

                        if (peerConfig["channel"].is<json::object>()) {
                            json::object channel = peerConfig["channel"].get<json::object>();
                            chNo = (uint32_t)channel["channelNo"].getDefault<int>(1);
                            chId = channel["channelId"].getDefault<uint8_t>(0U);

                            g_peerIdentityNameMap[peerId] = std::string(identity);

                            if (chNo > 0U) {
                                IdenTable idenEntry = g_idenTable->find(chId);
                                if (idenEntry.baseFrequency() == 0U) {
                                    ::LogError(LOG_HOST, "Channel Id %u has an invalid base frequency.", chId);
                                }

                                if (idenEntry.txOffsetMhz() == 0U) {
                                    ::LogError(LOG_HOST, "Channel Id %u has an invalid Tx offset.", chId);
                                }

                                uint32_t calcSpace = (uint32_t)(idenEntry.chSpaceKhz() / 0.125);
                                float calcTxOffset = idenEntry.txOffsetMhz() * 1000000.0;

                                uint32_t rxFrequency = (uint32_t)((idenEntry.baseFrequency() + ((calcSpace * 125) * chNo)) + (int32_t)calcTxOffset);
                                uint32_t txFrequency = (uint32_t)((idenEntry.baseFrequency() + ((calcSpace * 125) * chNo)));

                                txOss << std::fixed << std::setprecision(5) << (double)(txFrequency / 1000000.0);
                                rxOss << std::fixed << std::setprecision(5) << (double)(rxFrequency / 1000000.0);
                            }
                        }
                    }

                    // pad peer IDs properly
                    std::ostringstream peerOss;
                    peerOss << std::setw(9) << std::setfill('0') << peerId;

                    // pad peer IDs properly
                    std::ostringstream parentPeerOss;
                    parentPeerOss << std::setw(9) << std::setfill('0') << parentPeerId;

                    // pad peer IDs properly
                    std::ostringstream ccPeerOss;
                    ccPeerOss << std::setw(9) << std::setfill('0') << ccPeerId;

                    // build list view entry
                    const std::array<std::string, 15U> columns = {
                        peerOss.str(),
                        identity, software,
                        peerAddress, std::to_string(port),
                        parentPeerOss.str(), ccPeerOss.str(),
                        std::to_string(voiceChannelPeers.size()),
                        (connected) ? "X" : "",
                        strConnState,
                        std::to_string(pingsReceived),
                        std::to_string(chId), std::to_string(chNo),
                        rxOss.str(), txOss.str()
                    };

                    const finalcut::FStringList line(columns.cbegin(), columns.cend());
                    m_listView.insert(line);

                    cnt++;
                }

                std::ostringstream wndTitle;
                wndTitle << "Peers View" << " [" << cnt << "] (10s)";
                FDialog::setText(wndTitle.str());
            }
            catch (std::exception& e) {
                ::LogWarning(LOG_HOST, "[AFFVIEW] %s:%u, failed to properly handle peer query request, %s", fneRESTAddress.c_str(), fneRESTPort, e.what());
            }
        }

        redraw();
    }

private:
    int m_timerId;

    FListView m_listView{this};

    FButton m_refresh{"&Refresh", this};

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FDialog::setMinimumSize(FSize{PEER_LIST_WIDTH, PEER_LIST_HEIGHT});

        FDialog::setResizeable(false);
        FDialog::setMinimizable(false);
        FDialog::setTitlebarButtonVisibility(false);
        FDialog::setModal(false);

        FDialog::setText("Peers View (25s)");

        initControls();
        loadListView();

        FDialog::initLayout();
    }

    /**
     * @brief Initializes window controls.
     */
    void initControls()
    {
        m_refresh.setGeometry(FPoint(int(getWidth()) - 12, 1), FSize(9, 1));
        m_refresh.addCallback("clicked", [&]() { loadListView(); });

        m_listView.setGeometry(FPoint{1, 3}, FSize{getWidth() - 1, getHeight() - 5});

        // configure list view columns
        m_listView.addColumn("Peer ID", 10);
        m_listView.addColumn("Identity", 10);
        m_listView.addColumn("Software", 15);
        m_listView.addColumn("IP Address", 15);
        m_listView.addColumn("Port", 8);
        m_listView.addColumn("Link Peer ID", 10);
        m_listView.addColumn("CC Peer ID", 10);
        m_listView.addColumn("VC Count", 8);
        m_listView.addColumn("Connected", 5);
        m_listView.addColumn("State", 18);
        m_listView.addColumn("Pings Received", 8);
        m_listView.addColumn("Ch. ID", 8);
        m_listView.addColumn("Ch. No", 8);
        m_listView.addColumn("Rx Freq", 9);
        m_listView.addColumn("Tx Freq", 9);

        // set right alignment for TGID
        m_listView.setColumnAlignment(1, finalcut::Align::Right);
        m_listView.setColumnAlignment(4, finalcut::Align::Right);
        m_listView.setColumnAlignment(6, finalcut::Align::Center);
        m_listView.setColumnAlignment(7, finalcut::Align::Center);

        // set type of sorting
        m_listView.setColumnSortType(1, finalcut::SortType::Name);

        // sort by TGID
        m_listView.setColumnSort(1, finalcut::SortOrder::Ascending);

        setFocusWidget(&m_listView);
        redraw();
    }

    /*
    ** Event Handlers
    */

    /**
     * @brief Event that occurs on keyboard key press.
     * @param e Keyboard Event.
     */
    void onKeyPress(finalcut::FKeyEvent* e) override
    {
        const auto key = e->key();
        if (key == FKey::F5) {
            loadListView();
        }
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
                loadListView();
                redraw();
            }
        }
    }
};

#endif // __PEER_LIST_WND_H__