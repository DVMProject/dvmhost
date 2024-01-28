// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__ADJUST_WND_BASE_H__)
#define __ADJUST_WND_BASE_H__

#include "common/Thread.h"
#include "setup/HostSetup.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the base class for adjustment windows.
// ---------------------------------------------------------------------------

class HOST_SW_API AdjustWndBase : public finalcut::FDialog {
public:
    /// <summary>
    /// Initializes a new instance of the AdjustWndBase class.
    /// </summary>
    /// <param name="setup"></param>
    /// <param name="widget"></param>
    explicit AdjustWndBase(HostSetup* setup, FWidget* widget = nullptr) : FDialog{widget},
        m_setup(setup)
    {
        /* stub */
    }

protected:
    HostSetup *m_setup;

    /// <summary>
    ///
    /// </summary>
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

    /// <summary>
    ///
    /// </summary>
    virtual void initControls()
    {
        // transmit button and close button logic
        m_txButton.setGeometry(FPoint(3, int(getHeight()) - 6), FSize(10, 3));
        if (!m_setup->m_isConnected) {
            m_txButton.setDisable();
        }

        // set transmit button color state if connected
        if (m_setup->m_isConnected) {
            if (m_setup->m_transmit) {
                m_txButton.setBackgroundColor(FColor::Red3);
                m_txButton.setFocusBackgroundColor(FColor::Red3);
            }
            else {
                m_txButton.resetColors();
            }

            m_txButton.redraw();
        }

        m_txButton.addCallback("clicked", [&]() { setTransmit(); });


        m_closeButton.setGeometry(FPoint(17, int(getHeight()) - 6), FSize(9, 3));
        m_closeButton.addCallback("clicked", [&]() { hide(); });

        m_connectedLabel.setGeometry(FPoint(36, int(getHeight()) - 3), FSize(20, 3));
        if (m_setup->m_isConnected) {
            m_connectedLabel.setText("Modem Connected");
            m_connectedLabel.setForegroundColor(FColor::Green3);
        }
        else {
            m_connectedLabel.setText("Modem Disconnected");
            m_connectedLabel.setForegroundColor(FColor::Red3);
        }

        focusFirstChild();
    }

    /// <summary>
    ///
    /// </summary>
    void adjustSize() override
    {
        FDialog::adjustSize();
    }

    /*
    ** Event Handlers
    */

    /// <summary>
    /// 
    /// </summary>
    /// <param name="e"></param>
    void onKeyPress(finalcut::FKeyEvent* e) override
    {
        const auto key = e->key();
        if (key == FKey::F12) {
            setTransmit();
        }
        else if (key == FKey::F2) {
            m_setup->saveConfig();
        }
    }

    /// <summary>
    /// 
    /// </summary>
    /// <param name="e"></param>
    void onClose(FCloseEvent* e) override
    {
        hide();
    }

private:
    FLabel m_connectedLabel{"Modem Disconnected", this};

    FButton m_txButton{"Transmit", this};
    FButton m_closeButton{"Close", this};

    /// <summary>
    ///
    /// </summary>
    void setTransmit() 
    {
        if (!m_setup->setTransmit()) {
            FMessageBox::error(this, "Failed to enable modem transmit!");
        }
        if (m_setup->m_transmit) {
            m_txButton.setBackgroundColor(FColor::Red3);
            m_txButton.setFocusBackgroundColor(FColor::Red3);
        }
        else {
            m_txButton.resetColors();
        }

        m_txButton.redraw();
    }
};

#endif // __ADJUST_WND_BASE_H__