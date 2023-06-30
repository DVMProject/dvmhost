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
        m_cmdMenuSeparator1.setSeparator();
        m_cmdMenuSeparator2.setSeparator();

        // engineering menu
        m_engineeringMenuSeparator1.setSeparator();
        m_engineeringMenuSeparator2.setSeparator();

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

private:
    friend class MonitorApplication;

    LogDisplayWnd m_logWnd{this};
    std::vector<NodeStatusWnd*> m_nodes;

    std::vector<uint32_t> m_voiceChNo;
    std::unordered_map<uint32_t, lookups::VoiceChData> m_voiceChData;

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
    FMenuItem m_cmdMenuSeparator2{&m_cmdMenu};
    FMenuItem m_gaqSU{"&Group Affiliation Query", &m_cmdMenu};
    FMenuItem m_uregSU{"&Force Unit Registration", &m_cmdMenu};

    FMenu m_engineeringMenu{"&Engineering", &m_menuBar};
    FMenuItem m_toggleDMRDebug{"Toggle DMR Debug", &m_engineeringMenu};
    FMenuItem m_toggleDMRCSBKDump{"Toggle DMR CSBK Dump", &m_engineeringMenu};
    FMenuItem m_engineeringMenuSeparator1{&m_engineeringMenu};
    FMenuItem m_toggleP25Debug{"Toggle P25 Debug", &m_engineeringMenu};
    FMenuItem m_toggleP25TSBKDump{"Toggle P25 TSBK Dump", &m_engineeringMenu};
    FMenuItem m_engineeringMenuSeparator2{&m_engineeringMenu};
    FMenuItem m_toggleNXDNDebug{"Toggle NXDN Debug", &m_engineeringMenu};
    FMenuItem m_toggleNXDNRCCHDump{"Toggle NXDN RCCH Dump", &m_engineeringMenu};

    FMenu m_helpMenu{"&Help", &m_menuBar};
    FMenuItem m_aboutItem{"&About", &m_helpMenu};

    FStatusBar m_statusBar{this};
    FStatusKey m_keyF3{FKey::F3, "Quit", &m_statusBar};

    /// <summary>
    /// 
    /// </summary>
    void intializeNodeDisplay()
    {
        const auto& rootWidget = getRootWidget();
        const int defaultOffsX = 2;
        int offsX = defaultOffsX, offsY = 2;

        int maxWidth = 77;
        if (rootWidget) {
            maxWidth = rootWidget->getClientWidth() - 3;
        }

        yaml::Node systemConf = g_conf["system"];
        yaml::Node rfssConfig = systemConf["config"];
        uint8_t channelId = (uint8_t)rfssConfig["channelId"].as<uint32_t>(0U);
        if (channelId > 15U) { // clamp to 15
            channelId = 15U;
        }

        // main/control channel
        uint32_t mainChNo = 0U;
        {
            yaml::Node networkConf = g_conf["network"];
            uint32_t chNo = (uint32_t)::strtoul(rfssConfig["channelNo"].as<std::string>("1").c_str(), NULL, 16);
            if (chNo == 0U) { // clamp to 1
                chNo = 1U;
            }
            if (chNo > 4095U) { // clamp to 4095
                chNo = 4095U;
            }

            mainChNo = chNo;

            std::string restApiAddress = networkConf["restAddress"].as<std::string>("127.0.0.1");
            if (restApiAddress == "0.0.0.0") {
                restApiAddress = std::string("127.0.0.1");
            }
            
            uint16_t restApiPort = (uint16_t)networkConf["restPort"].as<uint32_t>(REST_API_DEFAULT_PORT);
            std::string restApiPassword = networkConf["restPassword"].as<std::string>();

            VoiceChData data = VoiceChData(chNo, restApiAddress, restApiPort, restApiPassword);

            // create configuration file node
            NodeStatusWnd* wnd = new NodeStatusWnd(this);
            wnd->setChData(data);
            wnd->setChannelId(channelId);
            wnd->setChannelNo(chNo);

            wnd->setGeometry(FPoint{offsX, offsY}, FSize{NODE_STATUS_WIDTH, NODE_STATUS_HEIGHT});
            offsX += NODE_STATUS_WIDTH + 2;
            m_nodes.push_back(wnd);
        }

        /*
        ** Voice Channels
        */
        yaml::Node& voiceChList = rfssConfig["voiceChNo"];

        if (voiceChList.size() != 0U) {
            for (size_t i = 0; i < voiceChList.size(); i++) {
                yaml::Node& channel = voiceChList[i];

                uint32_t chNo = (uint32_t)::strtoul(channel["channelNo"].as<std::string>("1").c_str(), NULL, 16);
                if (chNo == 0U) { // clamp to 1
                    chNo = 1U;
                }
                if (chNo > 4095U) { // clamp to 4095
                    chNo = 4095U;
                }

                if (chNo == mainChNo) {
                    continue;
                }

                std::string restApiAddress = channel["restAddress"].as<std::string>("127.0.0.1");
                uint16_t restApiPort = (uint16_t)channel["restPort"].as<uint32_t>(REST_API_DEFAULT_PORT);
                std::string restApiPassword = channel["restPassword"].as<std::string>();

                ::LogInfoEx(LOG_HOST, "Voice Channel Id %u Channel No $%04X REST API Adddress %s:%u", g_channelId, chNo, restApiAddress.c_str(), restApiPort);

                VoiceChData data = VoiceChData(chNo, restApiAddress, restApiPort, restApiPassword);

                NodeStatusWnd* wnd = new NodeStatusWnd(this);
                wnd->setChData(data);
                wnd->setChannelId(channelId);
                wnd->setChannelNo(chNo);

                // set control position
                if (offsX + NODE_STATUS_WIDTH > maxWidth) {
                    offsY += NODE_STATUS_HEIGHT + 2;
                    offsX = defaultOffsX;
                }

                wnd->setGeometry(FPoint{offsX, offsY}, FSize{NODE_STATUS_WIDTH, NODE_STATUS_HEIGHT});
                offsX += NODE_STATUS_WIDTH + 2;
                m_nodes.push_back(wnd);
            }
        }

        // display all the node windows
        for (auto* wnd : m_nodes) {
            wnd->setModal(false);
            wnd->show();
        }

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