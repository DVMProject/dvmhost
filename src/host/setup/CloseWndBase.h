/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
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
#if !defined(__CLOSE_WND_BASE_H__)
#define __CLOSE_WND_BASE_H__

#include "host/setup/HostSetup.h"
#include "Thread.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the base class for windows with close buttons.
// ---------------------------------------------------------------------------

class HOST_SW_API CloseWndBase : public finalcut::FDialog
{
public:
    /// <summary>
    /// Initializes a new instance of the CloseWndBase class.
    /// </summary>
    /// <param name="setup"></param>
    /// <param name="widget"></param>
    explicit CloseWndBase(HostSetup* setup, FWidget* widget = nullptr) : FDialog{widget},
        m_setup(setup)
    {
        /* stub */
    }

protected:
    HostSetup *m_setup;

    /// <summary>
    ///
    /// </summary>
    virtual void initLayout() override
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
        m_closeButton.setGeometry(FPoint(int(getWidth()) - 12, int(getHeight()) - 6), FSize(9, 3));
        m_closeButton.addCallback("clicked", [&](){ hide(); });

        focusFirstChild();
    }

    /// <summary>
    ///
    /// </summary>
    virtual void adjustSize() override
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
    virtual void onClose(FCloseEvent* e) override
    {
        hide();
    }

private:
    FButton m_closeButton{"Close", this};
};

#endif // __CLOSE_WND_BASE_H__