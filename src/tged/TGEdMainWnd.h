// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Talkgroup Editor
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file TGEdMainWnd.h
 * @ingroup tged
 */
#if !defined(__TGED_MAIN_WND_H__)
#define __TGED_MAIN_WND_H__

#include "common/lookups/AffiliationLookup.h"
#include "common/Log.h"
#include "common/Thread.h"

using namespace lookups;

#include <final/final.h>
using namespace finalcut;
#undef null

#include "TGEdMain.h"

#include "LogDisplayWnd.h"
#include "TGListWnd.h"
#include "TGEditWnd.h"

#include <vector>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define MINIMUM_SUPPORTED_SIZE_WIDTH 83
#define MINIMUM_SUPPORTED_SIZE_HEIGHT 30

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

class HOST_SW_API TGEdApplication;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the root window control.
 * @ingroup tged
 */
class HOST_SW_API TGEdMainWnd final : public finalcut::FWidget {
public:
    /**
     * @brief Initializes a new instance of the TGEdMainWnd class.
     * @param widget 
     */
    explicit TGEdMainWnd(FWidget* widget = nullptr) : FWidget{widget}
    {
        __InternalOutputStream(m_logWnd);

        // file menu
        m_fileMenuSeparator1.setSeparator();
        m_fileMenuSeparator2.setSeparator();
        m_saveSettingsItem.addAccelerator(FKey::Meta_s); // Meta/Alt + S
        m_saveSettingsItem.addCallback("clicked", this, [&]() { save(); });
        m_reloadSettingsItem.addAccelerator(FKey::Meta_r); // Meta/Alt + R
        m_reloadSettingsItem.addCallback("clicked", this, [&]() { g_tidLookups->reload(); m_wnd->loadListView(); });
        m_keyF2.addCallback("activate", this, [&]() { save(); });
        m_quitItem.addAccelerator(FKey::Meta_x); // Meta/Alt + X
        m_quitItem.addCallback("clicked", getFApplication(), &FApplication::cb_exitApp, this);
        m_keyF3.addCallback("activate", getFApplication(), &FApplication::cb_exitApp, this);
        m_keyF5.addCallback("activate", this, [&]() { g_tidLookups->reload(); m_wnd->loadListView(); LogMessage(LOG_HOST, "Loaded talkgroup rules file: %s", g_iniFile.c_str()); });

        m_backupOnSave.setChecked();

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
    friend class TGEdApplication;

    LogDisplayWnd m_logWnd{this};
    TGListWnd* m_wnd;

    FMenuBar m_menuBar{this};

    FMenu m_fileMenu{"&File", &m_menuBar};
    FMenuItem m_reloadSettingsItem{"&Reload", &m_fileMenu};
    FMenuItem m_saveSettingsItem{"&Save", &m_fileMenu};
    FMenuItem m_fileMenuSeparator2{&m_fileMenu};
    FCheckMenuItem m_saveOnCloseToggle{"Save on Close?", &m_fileMenu};
    FCheckMenuItem m_backupOnSave{"Backup Rules File?", &m_fileMenu};
    FMenuItem m_fileMenuSeparator1{&m_fileMenu};
    FMenuItem m_quitItem{"&Quit", &m_fileMenu};

    FMenu m_helpMenu{"&Help", &m_menuBar};
    FMenuItem m_aboutItem{"&About", &m_helpMenu};

    FStatusBar m_statusBar{this};
    FStatusKey m_keyF2{FKey::F2, "Save", &m_statusBar};
    FStatusKey m_keyF3{FKey::F3, "Quit", &m_statusBar};
    FStatusKey m_keyF5{FKey::F5, "Reload", &m_statusBar};

    /**
     * @brief 
     */
    void save()
    {
        if (m_backupOnSave.isChecked()) {
            std::string bakFile = g_iniFile + ".bak";
            LogMessage(LOG_HOST, "Backing up existing file %s to %s", g_iniFile.c_str(), bakFile.c_str());
            copyFile(g_iniFile.c_str(), bakFile.c_str());
        }

        g_tidLookups->commit(); 
    }

    /**
     * @brief Helper to copy one file to another.
     * @param src Source file.
     * @param dest Destination File.
     * @returns bool True, if file copied, otherwise false.
     */
    bool copyFile(const char *srcFilePath, const char* destFilePath)
    {
        std::ifstream src(srcFilePath, std::ios::binary);
        std::ofstream dest(destFilePath, std::ios::binary);
        dest << src.rdbuf();
        return src && dest;
    }

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

        int maxHeight = TG_LIST_HEIGHT;
        if (rootWidget) {
            maxHeight = rootWidget->getClientHeight() - 3;
        }

        m_wnd = new TGListWnd(this);
        if (maxHeight - 21 < TG_LIST_HEIGHT)
            maxHeight = TG_LIST_HEIGHT + 21;

        m_wnd->setGeometry(FPoint{2, 2}, FSize{(size_t)(maxWidth - 1), (size_t)(maxHeight - 21)});

        m_wnd->setModal(false);
        m_wnd->show();

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
        // if we are saving on close -- fire off the file save event
        if (m_saveOnCloseToggle.isChecked()) {
            g_tidLookups->commit();
        }

        FApplication::closeConfirmationDialog(this, e);
    }
};

#endif // __TGED_MAIN_WND_H__