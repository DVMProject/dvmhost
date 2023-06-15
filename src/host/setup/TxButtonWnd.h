/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the finalcut project. (https://github.com/gansm/finalcut)
// Licensed under the LGPLv2 License (https://opensource.org/licenses/LGPL-2.0)
//
/*
*   Copyright (C) 2012-2023 by Markus Gans
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
#if !defined(__TX_BUTTON_WND_H__)
#define __TX_BUTTON_WND_H__

#include "host/setup/HostSetup.h"

#include <array>
#include <map>
#include <vector>

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the transmit button window.
// ---------------------------------------------------------------------------

class HOST_SW_API TxButtonWnd final : public finalcut::FDialog
{
public:
    /// <summary>
    /// Initializes a new instance of the TxButtonWnd class.
    /// </summary>
    /// <param name="widget"></param>
    explicit TxButtonWnd(FWidget* widget = nullptr) : FDialog{widget}
    {
        m_timerId = addTimer(250);
    }
    /// <summary>Copy constructor.</summary>
    TxButtonWnd(const TxButtonWnd&) = delete;
    /// <summary>Move constructor.</summary>
    TxButtonWnd(TxButtonWnd&&) noexcept = delete;
    /// <summary>Finalizes an instance of the ModemStatusWnd class.</summary>
    ~TxButtonWnd() noexcept override = default;

    /// <summary>Disable copy assignment operator (=).</summary>
    auto operator= (const TxButtonWnd&) -> TxButtonWnd& = delete;
    /// <summary>Disable move assignment operator (=).</summary>
    auto operator= (TxButtonWnd&&) noexcept -> TxButtonWnd& = delete;

    /// <summary>Disable set X coordinate.</summary>
    void setX(int, bool = true) override { }
    /// <summary>Disable set Y coordinate.</summary>
    void setY(int, bool = true) override { }
    /// <summary>Disable set position.</summary>
    void setPos(const FPoint&, bool = true) override { }

    /// <summary>Helper to set the instance of HostSetup.</summary>
    void setSetup(HostSetup* setup) { m_setup = setup; }

private:
    HostSetup *m_setup;
    FButton m_txButton{"Transmit", this};
    int m_timerId;

    /// <summary>
    ///
    /// </summary>
    void initLayout() override
    {
        FDialog::setText("Transmit");

        const auto& rootWidget = getRootWidget();

        FDialog::setGeometry(FPoint{(int)rootWidget->getClientWidth() - 27, 2}, FSize{26, 7});
        FDialog::setMinimumSize(FSize{26, 7});
        FDialog::setResizeable(false);
        FDialog::setMinimizable(false);
        FDialog::setTitlebarButtonVisibility(false);
        FDialog::setShadow(false);
        FDialog::setAlwaysOnTop(true);

        m_txButton.setGeometry(FPoint(2, 1), FSize(22, 3));
        m_txButton.setDisable();
        m_txButton.addCallback("clicked", [&]() {
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
        });

        FDialog::initLayout();
    }

    /*
    ** Event Handlers
    */

    /// <summary>
    ///
    /// </summary>
    /// <param name="timer"></param>
    void onTimer(FTimerEvent* timer) override
    {
        if (timer != nullptr) {
            if (timer->getTimerId() == m_timerId) {
                // transmit button and close button logic
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
                }
            }
        }
    }
};

#endif // __TX_BUTTON_WND_H__