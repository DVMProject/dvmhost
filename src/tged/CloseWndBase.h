// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Talkgroup Editor
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file CloseWndBase.h
 * @ingroup tged
 */
#if !defined(__CLOSE_WND_BASE_H__)
#define __CLOSE_WND_BASE_H__

#include "common/Thread.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the base class for windows with close buttons.
 * @ingroup tged
 */
class HOST_SW_API CloseWndBase : public finalcut::FDialog {
public:
    /**
     * @brief Initializes a new instance of the CloseWndBase class.
     * @param widget 
     */
    explicit CloseWndBase(FWidget* widget = nullptr) : FDialog{widget}
    {
        /* stub */
    }

protected:
    bool m_enableSetButton = false;
    FButton m_setButton{"Set", this};
    bool m_enableCloseButton = true;
    FButton m_closeButton{"&Close", this};

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
        m_closeButton.addCallback("clicked", [&]() { close(); });
        if (!m_enableCloseButton) {
            m_closeButton.setDisable();
            m_closeButton.setVisible(false);
        }

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
     * @brief Event that occurs when the window is closed.
     * @param e Close event.
     */
    void onClose(FCloseEvent* e) override
    {
        hide();
    }
};

#endif // __CLOSE_WND_BASE_H__