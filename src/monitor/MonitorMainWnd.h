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
#if !defined(__MONITOR_WND_H__)
#define __MONITOR_WND_H__

#include "lookups/AffiliationLookup.h"
#include "Log.h"
#include "Thread.h"

using namespace lookups;

#include <final/final.h>
using namespace finalcut;
#undef null

#include "monitor/MonitorMain.h"

#include "monitor/LogDisplayWnd.h"
#include "monitor/NodeStatusWnd.h"
#include "monitor/SelectedNodeWnd.h"
#include "monitor/PageSubscriberWnd.h"
#include "monitor/RadioCheckSubscriberWnd.h"
#include "monitor/InhibitSubscriberWnd.h"
#include "monitor/UninhibitSubscriberWnd.h"

#include <vector>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

class HOST_SW_API MonitorApplication;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the root window control.
// ---------------------------------------------------------------------------

class HOST_SW_API MonitorMainWnd final : public finalcut::FWidget {
public:
    /// <summary>
    /// Initializes a new instance of the MonitorMainWnd class.
    /// </summary>
    /// <param name="widget"></param>
    explicit MonitorMainWnd(FWidget* widget = nullptr) : FWidget{widget}
    {
        __InternalOutputStream(m_logWnd);

        // file menu
        m_quitItem.addAccelerator(FKey::Meta_x); // Meta/Alt + X
        m_quitItem.addCallback("clicked", getFApplication(), &FApplication::cb_exitApp, this);
        m_keyF3.addCallback("activate", getFApplication(), &FApplication::cb_exitApp, this);

        // command menu
        m_pageSU.addCallback("clicked", this, [&]() {
            PageSubscriberWnd wnd{m_selectedCh, this};
            wnd.show();
        });
        m_keyF5.addCallback("activate", this, [&]() {
            PageSubscriberWnd wnd{m_selectedCh, this};
            wnd.show();
        });
        m_radioCheckSU.addCallback("clicked", this, [&]() {
            RadioCheckSubscriberWnd wnd{m_selectedCh, this};
            wnd.show();
        });
        m_cmdMenuSeparator1.setSeparator();
        m_inhibitSU.addCallback("clicked", this, [&]() {
            InhibitSubscriberWnd wnd{m_selectedCh, this};
            wnd.show();
        });
        m_keyF7.addCallback("activate", this, [&]() {
            InhibitSubscriberWnd wnd{m_selectedCh, this};
            wnd.show();
        });
        m_uninhibitSU.addCallback("clicked", this, [&]() {
            UninhibitSubscriberWnd wnd{m_selectedCh, this};
            wnd.show();
        });
        m_keyF8.addCallback("activate", this, [&]() {
            UninhibitSubscriberWnd wnd{m_selectedCh, this};
            wnd.show();
        });

        // help menu
        m_aboutItem.addCallback("clicked", this, [&]() {
            const FString line(2, UniChar::BoxDrawingsHorizontal);
            FMessageBox info("About", line + __PROG_NAME__ + line + L"\n\n"
                L"Version " + __VER__ + L"\n\n"
                L"Copyright (c) 2017-2023 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors." + L"\n"
                L"Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others", 
                FMessageBox::ButtonType::Ok, FMessageBox::ButtonType::Reject, FMessageBox::ButtonType::Reject, this);
            info.setCenterText();
            info.show();
        });
    }

    /// <summary></summary>
    lookups::VoiceChData getSelectedCh() { return m_selectedCh; }

private:
    friend class MonitorApplication;

    LogDisplayWnd m_logWnd{this};
    SelectedNodeWnd m_selectWnd{this};
    std::vector<NodeStatusWnd*> m_nodes;

    lookups::VoiceChData m_selectedCh;

    FString m_line{13, UniChar::BoxDrawingsHorizontal};

    FMenuBar m_menuBar{this};

    FMenu m_fileMenu{"&File", &m_menuBar};
    FMenuItem m_quitItem{"&Quit", &m_fileMenu};

    FMenu m_cmdMenu{"&Commands", &m_menuBar};
    FMenuItem m_pageSU{"&Page Subscriber", &m_cmdMenu};
    FMenuItem m_radioCheckSU{"Radio &Check Subscriber", &m_cmdMenu};
    FMenuItem m_cmdMenuSeparator1{&m_cmdMenu};
    FMenuItem m_inhibitSU{"&Inhibit Subscriber", &m_cmdMenu};
    FMenuItem m_uninhibitSU{"&Uninhibit Subscriber", &m_cmdMenu};

    FMenu m_helpMenu{"&Help", &m_menuBar};
    FMenuItem m_aboutItem{"&About", &m_helpMenu};

    FStatusBar m_statusBar{this};
    FStatusKey m_keyF3{FKey::F3, "Quit", &m_statusBar};
    FStatusKey m_keyF5{FKey::F5, "Page Subscriber", &m_statusBar};
    FStatusKey m_keyF7{FKey::F7, "Inhibit Subscriber", &m_statusBar};
    FStatusKey m_keyF8{FKey::F8, "Uninhibit Subscriber", &m_statusBar};

    /// <summary>
    /// 
    /// </summary>
    void intializeNodeDisplay()
    {
        const auto& rootWidget = getRootWidget();
        const int defaultOffsX = 2;
        int offsX = defaultOffsX, offsY = 8;

        int maxWidth = 77;
        if (rootWidget) {
            maxWidth = rootWidget->getClientWidth() - 3;
        }

        /*
        ** Channels
        */
        yaml::Node& voiceChList = g_conf["channels"];

        if (voiceChList.size() != 0U) {
            for (size_t i = 0; i < voiceChList.size(); i++) {
                yaml::Node& channel = voiceChList[i];

                std::string restApiAddress = channel["restAddress"].as<std::string>("127.0.0.1");
                uint16_t restApiPort = (uint16_t)channel["restPort"].as<uint32_t>(REST_API_DEFAULT_PORT);
                std::string restApiPassword = channel["restPassword"].as<std::string>();

                ::LogInfoEx(LOG_HOST, "Channel REST API Adddress %s:%u", restApiAddress.c_str(), restApiPort);

                VoiceChData data = VoiceChData(0U, restApiAddress, restApiPort, restApiPassword);

                NodeStatusWnd* wnd = new NodeStatusWnd(this);
                wnd->setChData(data);

                // set control position
                if (offsX + NODE_STATUS_WIDTH > maxWidth) {
                    offsY += NODE_STATUS_HEIGHT + 2;
                    offsX = defaultOffsX;
                }

                wnd->setGeometry(FPoint{offsX, offsY}, FSize{NODE_STATUS_WIDTH, NODE_STATUS_HEIGHT});

                wnd->addCallback("update-selected", this, [&](NodeStatusWnd* wnd) {
                    std::stringstream ss;
                    ss << (uint32_t)(wnd->getChannelId()) << "-" << wnd->getChannelNo() << " / "
                       << wnd->getChData().address() << ":" << wnd->getChData().port();

                    m_selectWnd.setSelectedText(ss.str());
                    m_selectedCh = wnd->getChData();
                }, wnd);

                offsX += NODE_STATUS_WIDTH + 2;
                m_nodes.push_back(wnd);
            }
        }

        // display all the node windows
        for (auto* wnd : m_nodes) {
            wnd->setModal(false);
            wnd->show();

            wnd->lowerWindow();
            wnd->deactivateWindow();
        }

        // raise and activate first window
        m_nodes.at(0)->raiseWindow();
        m_nodes.at(0)->activateWindow();

        redraw();
    }

    /*
    ** Event Handlers
    */

    /// <summary>
    /// 
    /// </summary>
    /// <param name="e"></param>
    void onShow(FShowEvent* e) override
    {
        intializeNodeDisplay();
        if (g_hideLoggingWnd) {
            const auto& rootWidget = getRootWidget();
            m_logWnd.setGeometry(FPoint{(int)(rootWidget->getClientWidth() - 81), (int)(rootWidget->getClientHeight() - 1)}, FSize{80, 20});

            m_logWnd.minimizeWindow();
        }
    }

    /// <summary>
    /// 
    /// </summary>
    /// <param name="e"></param>
    void onClose(FCloseEvent* e) override
    {
        FApplication::closeConfirmationDialog(this, e);
    }
};

#endif // __MONITOR_WND_H__