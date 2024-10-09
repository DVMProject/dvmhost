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
class HOST_SW_API PeerListWnd final : public finalcut::FDialog {
public:
    /**
     * @brief Initializes a new instance of the PeerListWnd class.
     * @param widget 
     */
    explicit PeerListWnd(FWidget* widget = nullptr) : FDialog{widget}
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
                for (auto entry : fnePeers) {
                    json::object peerObj = entry.get<json::object>();
                    uint32_t peerId = peerObj["peerId"].get<uint32_t>();
                    std::string peerAddress = peerObj["address"].get<std::string>();
                    uint16_t port = (uint16_t)peerObj["port"].get<uint32_t>();
                    bool connected = peerObj["connected"].get<bool>();
                    uint32_t connectionState = (uint32_t)peerObj["connectionState"].get<uint32_t>();
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

                    uint32_t pingsReceived = (uint32_t)peerObj["pingsReceived"].get<uint32_t>();
                    uint64_t lastPing = (uint64_t)peerObj["lastPing"].get<uint32_t>();

                    uint32_t ccPeerId = (uint32_t)peerObj["controlChannel"].get<uint32_t>();

                    json::array voiceChannels = peerObj["voiceChannels"].get<json::array>();
                    std::vector<uint32_t> voiceChannelPeers;
                    for (auto vcEntry : voiceChannels) {
                        uint32_t vcPeerId = vcEntry.get<uint32_t>();
                        voiceChannelPeers.push_back(vcPeerId);
                    }

                    json::object peerConfig = peerObj["config"].get<json::object>();
                    std::string identity = peerConfig["identity"].get<std::string>();
                    std::string software = peerConfig["software"].get<std::string>();

                    json::object channel = peerConfig["channel"].get<json::object>();
                    uint32_t chNo = (uint32_t)channel["channelNo"].get<int>();
                    uint8_t chId = channel["channelId"].get<uint8_t>();

                    g_peerIdentityNameMap[peerId] = std::string(identity);

                    // pad peer IDs properly
                    std::ostringstream peerOss;
                    peerOss << std::setw(9) << std::setfill('0') << peerId;

                    // pad peer IDs properly
                    std::ostringstream ccPeerOss;
                    ccPeerOss << std::setw(9) << std::setfill('0') << ccPeerId;

                    // build list view entry
                    const std::array<std::string, 12U> columns = {
                        peerOss.str(),
                        identity, software,
                        peerAddress, std::to_string(port),
                        ccPeerOss.str(),
                        (voiceChannelPeers.size() > 0U) ? "X" : "",
                        (connected) ? "X" : "",
                        strConnState,
                        std::to_string(pingsReceived),
                        std::to_string(chId), std::to_string(chNo)
                    };

                    const finalcut::FStringList line(columns.cbegin(), columns.cend());
                    m_listView.insert(line);
                }
            }
            catch (std::exception& e) {
                ::LogWarning(LOG_HOST, "[AFFVIEW] %s:%u, failed to properly handle peer query request, %s", fneRESTAddress.c_str(), fneRESTPort, e.what());
            }
        }

        setFocusWidget(&m_listView);
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
        m_listView.addColumn("CC Peer ID", 10);
        m_listView.addColumn("VC Count", 8);
        m_listView.addColumn("Connected", 5);
        m_listView.addColumn("State", 15);
        m_listView.addColumn("Pings Received", 8);
        m_listView.addColumn("Ch. ID", 8);
        m_listView.addColumn("Ch. No", 8);

        // set right alignment for TGID
        m_listView.setColumnAlignment(1, finalcut::Align::Right);
        m_listView.setColumnAlignment(4, finalcut::Align::Right);
        m_listView.setColumnAlignment(6, finalcut::Align::Center);

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