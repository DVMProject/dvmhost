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
 * @file SysViewMainWnd.h
 * @ingroup fneSysView
 */
#if !defined(__AFF_VIEW_WND_H__)
#define __AFF_VIEW_WND_H__

#include "common/Log.h"
#include "common/Thread.h"

using namespace lookups;

#include <final/final.h>
using namespace finalcut;
#undef null

#include "SysViewMain.h"
#include "AffListWnd.h"
#include "PeerListWnd.h"

#include "LogDisplayWnd.h"

#include <vector>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define MINIMUM_SUPPORTED_SIZE_WIDTH 83
#define MINIMUM_SUPPORTED_SIZE_HEIGHT 30

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the root window control.
 * @ingroup fneSysView
 */
class HOST_SW_API SysViewMainWnd final : public finalcut::FWidget {
public:
    /**
     * @brief Initializes a new instance of the SysViewMainWnd class.
     * @param widget 
     */
    explicit SysViewMainWnd(FWidget* widget = nullptr) : FWidget{widget}
    {
        __InternalOutputStream(m_logWnd);

        // file menu
        m_quitItem.addAccelerator(FKey::Meta_x); // Meta/Alt + X
        m_quitItem.addCallback("clicked", getFApplication(), &FApplication::cb_exitApp, this);
        m_keyF3.addCallback("activate", getFApplication(), &FApplication::cb_exitApp, this);

        // help menu
        m_aboutItem.addCallback("clicked", this, [&]() {
            const FString line(2, UniChar::BoxDrawingsHorizontal);
            FMessageBox info("About", line + __PROG_NAME__ + line + L"\n\n"
                L"" + __BANNER__ + L"\n"
                L"Version " + __VER__ + L"\n\n"
                L"Copyright (c) 2017-2024 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors." + L"\n"
                L"Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others", 
                FMessageBox::ButtonType::Ok, FMessageBox::ButtonType::Reject, FMessageBox::ButtonType::Reject, this);
            info.setCenterText();
            info.show();
        });
    }

private:
    friend class AffViewApplication;

    LogDisplayWnd m_logWnd{this};
    AffListWnd* m_wnd;
    PeerListWnd *m_peerWnd;

    FString m_line{13, UniChar::BoxDrawingsHorizontal};

    FMenuBar m_menuBar{this};

    FMenu m_fileMenu{"&File", &m_menuBar};
    FMenuItem m_quitItem{"&Quit", &m_fileMenu};

    FMenu m_helpMenu{"&Help", &m_menuBar};
    FMenuItem m_aboutItem{"&About", &m_helpMenu};

    FStatusBar m_statusBar{this};
    FStatusKey m_keyF3{FKey::F3, "Quit", &m_statusBar};

    /*
    ** Event Handlers
    */

    /**
     * @brief Event that occurs when the window is shown.
     * @param e Show Event
     */
    void onShow(FShowEvent* e) override
    {
        const auto& rootWidget = getRootWidget();

        createPeerNetwork();

        int fullWidth = rootWidget->getWidth();
        if (fullWidth < MINIMUM_SUPPORTED_SIZE_WIDTH) {
            clearArea();
            ::fatal("screen resolution too small must be wider then %u characters, console width = %u", MINIMUM_SUPPORTED_SIZE_WIDTH, fullWidth);
        }

        int fullHeight = rootWidget->getHeight();
        if (fullHeight < MINIMUM_SUPPORTED_SIZE_HEIGHT) {
            clearArea();
            ::fatal("screen resolution too small must be taller then %u characters, console height = %u", MINIMUM_SUPPORTED_SIZE_HEIGHT, fullHeight);
        }

        int maxWidth = 77;
        if (rootWidget) {
            maxWidth = rootWidget->getClientWidth() - 3;
        }

        int maxHeight = AFF_LIST_HEIGHT;
        if (rootWidget) {
            maxHeight = rootWidget->getClientHeight() - 3;
        }

        m_wnd = new AffListWnd(this);
        if (maxHeight - 41 < AFF_LIST_HEIGHT)
            maxHeight = AFF_LIST_HEIGHT + 41;

        m_wnd->setGeometry(FPoint{2, 2}, FSize{AFF_LIST_WIDTH, (size_t)(maxHeight - 41)});

        m_wnd->setModal(false);
        m_wnd->show();

        m_peerWnd = new PeerListWnd(this);
        if (maxHeight - 41 < PEER_LIST_HEIGHT)
            maxHeight = PEER_LIST_HEIGHT + 41;

        m_peerWnd->setGeometry(FPoint{AFF_LIST_WIDTH + 6, 2}, FSize{(size_t)(maxWidth - AFF_LIST_WIDTH - 6), (size_t)(maxHeight - 41)});

        m_peerWnd->setModal(false);
        m_peerWnd->show();

        m_wnd->raiseWindow();
        m_wnd->activateWindow();

        redraw();

        if (g_hideLoggingWnd) {
            const auto& rootWidget = getRootWidget();
            m_logWnd.setGeometry(FPoint{(int)(rootWidget->getClientWidth() - 81), (int)(rootWidget->getClientHeight() - 1)}, FSize{80, 20});

            m_logWnd.minimizeWindow();
        }
    }

    /**
     * @brief Event that occurs when the window is closed.
     * @param e Close Event
     */
    void onClose(FCloseEvent* e) override
    {
        FApplication::closeConfirmationDialog(this, e);
    }
};

#endif // __AFF_VIEW_WND_H__