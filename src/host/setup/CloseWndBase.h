// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file CloseWndBase.h
 * @ingroup setup
 */
#if !defined(__CLOSE_WND_BASE_H__)
#define __CLOSE_WND_BASE_H__

#include "common/Thread.h"
#include "setup/HostSetup.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the base class for windows with close buttons.
 * @ingroup setup
 */
class HOST_SW_API CloseWndBase : public finalcut::FDialog {
public:
    /**
     * @brief Initializes a new instance of the CloseWndBase class.
     * @param setup Instance of the HostSetup class.
     * @param widget 
     */
    explicit CloseWndBase(HostSetup* setup, FWidget* widget = nullptr) : FDialog{widget},
        m_setup(setup)
    {
        /* stub */
    }

protected:
    HostSetup *m_setup;

    bool m_enableSetButton;
    FButton m_setButton{"Set", this};

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FDialog::setMinimizable(true);
        FDialog::setShadow();

        std::size_t maxWidth, maxHeight;
        const auto& rootWidget = getRootWidget();

        if (rootWidget) {
            maxWidth = rootWidget->getClientWidth();
            maxHeight = rootWidget->getClientHeight();
        }
        else {
            // fallback to xterm default size
            maxWidth = 80;
            maxHeight = 24;
        }

        const int x = 1 + int((maxWidth - getWidth()) / 2);
        const int y = 1 + int((maxHeight - getHeight()) / 3);
        FWindow::setPos(FPoint{x, y}, false);
        FDialog::adjustSize();

        FDialog::setModal();

        initControls();

        FDialog::initLayout();

        rootWidget->redraw(); // bryanb: wtf?
        redraw();
    }

    /**
     * @brief Initializes window controls.
     */
    virtual void initControls()
    {
        m_closeButton.setGeometry(FPoint(int(getWidth()) - 12, int(getHeight()) - 6), FSize(9, 3));
        m_closeButton.addCallback("clicked", [&]() { hide(); });

        m_setButton.setDisable();
        m_setButton.setVisible(false);
        if (m_enableSetButton) {
            m_setButton.setEnable();
            m_setButton.setVisible(true);
            m_setButton.setGeometry(FPoint(int(getWidth()) - 24, int(getHeight()) - 6), FSize(9, 3));
        }

        focusFirstChild();
    }

    /**
     * @brief Adjusts window size.
     */
    void adjustSize() override
    {
        FDialog::adjustSize();
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
        if (key == FKey::F2) {
            m_setup->saveConfig();
        }
    }

    /**
     * @brief Event that occurs when the window is closed.
     * @param e Close event.
     */
    void onClose(FCloseEvent* e) override
    {
        hide();
    }

private:
    FButton m_closeButton{"Close", this};
};

#endif // __CLOSE_WND_BASE_H__