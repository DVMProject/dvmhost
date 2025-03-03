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
 * @file AffListWnd.h
 * @ingroup fneSysView
 */
#if !defined(__AFF_LIST_WND_H__)
#define __AFF_LIST_WND_H__

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

#define AFF_LIST_WIDTH 64
#define AFF_LIST_HEIGHT 15

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the affiliations list window.
 * @ingroup fneSysView
 */
class HOST_SW_API AffListWnd final : public FDblDialog {
public:
    /**
     * @brief Initializes a new instance of the AffListWnd class.
     * @param widget 
     */
    explicit AffListWnd(FWidget* widget = nullptr) : FDblDialog{widget}
    {
        m_timerId = addTimer(10000); // starts the timer every 10 seconds
    }
    /**
     * @brief Copy constructor.
     */
    AffListWnd(const AffListWnd&) = delete;
    /**
     * @brief Move constructor.
     */
    AffListWnd(AffListWnd&&) noexcept = delete;
    /**
     * @brief Finalizes an instance of the AffListWnd class.
     */
    ~AffListWnd() noexcept override = default;

    /**
     * @brief Disable copy assignment operator (=).
     */
    auto operator= (const AffListWnd&) -> AffListWnd& = delete;
    /**
     * @brief Disable move assignment operator (=).
     */
    auto operator= (AffListWnd&&) noexcept -> AffListWnd& = delete;

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
            HTTP_GET, FNE_GET_AFF_LIST, req, rsp, fneSSL, g_debug);
        if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
            ::LogError(LOG_HOST, "[AFFVIEW] failed to get affiliations for %s:%u", fneRESTAddress.c_str(), fneRESTPort);
        }
        else {
            try {
                m_listView.clear();

                json::array fneAffils = rsp["affiliations"].get<json::array>();
                uint32_t cnt = 0U;
                for (auto entry : fneAffils) {
                    json::object peerAffils = entry.get<json::object>();
                    uint32_t peerId = peerAffils["peerId"].getDefault<uint32_t>(0U);
                    json::array affils = peerAffils["affiliations"].get<json::array>();
                    for (auto pentry : affils) {
                        json::object peerEntry = pentry.get<json::object>();
                        uint32_t dstId = peerEntry["dstId"].getDefault<uint32_t>(0U);
                        uint32_t srcId = peerEntry["srcId"].getDefault<uint32_t>(0U);

                        // pad peer IDs properly
                        std::ostringstream peerOss;
                        peerOss << std::setw(9) << std::setfill('0') << peerId;

                        std::string srcAlias = resolveRID(srcId);

                        // pad TGs properly
                        std::ostringstream tgidOss;
                        tgidOss << std::setw(5) << std::setfill('0') << dstId;

                        std::string dstAlias = resolveTGID(dstId);

                        // build list view entry
                        const std::array<std::string, 5U> columns = {
                            peerOss.str(),
                            std::to_string(srcId), srcAlias,
                            tgidOss.str(), dstAlias
                        };

                        const finalcut::FStringList line(columns.cbegin(), columns.cend());
                        m_listView.insert(line);

                        cnt++;
                    }
                }

                std::ostringstream wndTitle;
                wndTitle << "Affiliations View" << " [" << cnt << "] (10s)";
                FDialog::setText(wndTitle.str());
            }
            catch (std::exception& e) {
                ::LogWarning(LOG_HOST, "[AFFVIEW] %s:%u, failed to properly handle affiliation request, %s", fneRESTAddress.c_str(), fneRESTPort, e.what());
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
        FDialog::setMinimumSize(FSize{AFF_LIST_WIDTH, AFF_LIST_HEIGHT});

        FDialog::setResizeable(false);
        FDialog::setMinimizable(false);
        FDialog::setTitlebarButtonVisibility(false);
        FDialog::setModal(false);

        FDialog::setText("Affiliations View (10s)");

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
        m_listView.addColumn("RID", 10);
        m_listView.addColumn("RID Alias", 15);
        m_listView.addColumn("TGID", 9);
        m_listView.addColumn("TGID Alias", 15);

        // set right alignment for TGID
        m_listView.setColumnAlignment(1, finalcut::Align::Right);
        m_listView.setColumnAlignment(2, finalcut::Align::Right);
        m_listView.setColumnAlignment(4, finalcut::Align::Right);

        // set type of sorting
        m_listView.setColumnSortType(1, finalcut::SortType::Name);
        m_listView.setColumnSortType(2, finalcut::SortType::Name);
        m_listView.setColumnSortType(4, finalcut::SortType::Name);

        // sort by TGID
        m_listView.setColumnSort(1, finalcut::SortOrder::Ascending);
        m_listView.setColumnSort(4, finalcut::SortOrder::Ascending);

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

#endif // __AFF_LIST_WND_H__