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
#if !defined(__MODEM_STATUS_WND_H__)
#define __MODEM_STATUS_WND_H__

#include "setup/HostSetup.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the modem status display window.
// ---------------------------------------------------------------------------

class HOST_SW_API ModemStatusWnd final : public finalcut::FDialog {
public:
    /// <summary>
    /// Initializes a new instance of the ModemStatusWnd class.
    /// </summary>
    /// <param name="widget"></param>
    explicit ModemStatusWnd(FWidget* widget = nullptr) : FDialog{widget}
    {
        m_scrollText.ignorePadding();
    }
    /// <summary>Copy constructor.</summary>
    ModemStatusWnd(const ModemStatusWnd&) = delete;
    /// <summary>Move constructor.</summary>
    ModemStatusWnd(ModemStatusWnd&&) noexcept = delete;
    /// <summary>Finalizes an instance of the ModemStatusWnd class.</summary>
    ~ModemStatusWnd() noexcept override = default;

    /// <summary>Disable copy assignment operator (=).</summary>
    auto operator= (const ModemStatusWnd&) -> ModemStatusWnd& = delete;
    /// <summary>Disable move assignment operator (=).</summary>
    auto operator= (ModemStatusWnd&&) noexcept -> ModemStatusWnd& = delete;

    /// <summary>Disable set X coordinate.</summary>
    void setX(int, bool = true) override { }
    /// <summary>Disable set Y coordinate.</summary>
    void setY(int, bool = true) override { }
    /// <summary>Disable set position.</summary>
    void setPos(const FPoint&, bool = true) override { }

    /// <summary>
    /// Helper to append text.
    /// </summary>
    void append(std::string str) 
    {
        if (str.empty()) {
            return;
        }

        m_scrollText.append(str);
        m_scrollText.scrollToEnd();
        redraw();
    }

    /// <summary>
    /// Helper to clear the text.
    /// </summary>
    void clear() { m_scrollText.clear(); }

private:
    FTextView m_scrollText{this};

    /// <summary>
    ///
    /// </summary>
    void initLayout() override
    {
        FDialog::setText("Modem Status (update every 1s)");

        std::size_t maxWidth;
        const auto& rootWidget = getRootWidget();

        if (rootWidget) {
            maxWidth = rootWidget->getClientWidth() - 3;
        }
        else {
            // fallback to xterm default size
            maxWidth = 77;
        }

        FDialog::setGeometry(FPoint{2, 23}, FSize{maxWidth, 25});
        FDialog::setMinimumSize(FSize{80, 5});
        FDialog::setResizeable(false);
        FDialog::setMinimizable(false);
        FDialog::setTitlebarButtonVisibility(false);
        FDialog::setShadow();

        m_scrollText.setGeometry(FPoint{1, 2}, FSize{getWidth(), getHeight() - 1});

        FDialog::initLayout();
    }
};

#endif // __MODEM_STATUS_WND_H__