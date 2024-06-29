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
 * @file ModemStatusWnd.h
 * @ingroup setup
 */
#if !defined(__MODEM_STATUS_WND_H__)
#define __MODEM_STATUS_WND_H__

#include "setup/HostSetup.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the modem status display window.
 * @ingroup setup
 */
class HOST_SW_API ModemStatusWnd final : public finalcut::FDialog {
public:
    /**
     * @brief Initializes a new instance of the ModemStatusWnd class.
     * @param widget 
     */
    explicit ModemStatusWnd(FWidget* widget = nullptr) : FDialog{widget}
    {
        m_scrollText.ignorePadding();
    }
    /**
     * @brief Copy constructor.
     */
    ModemStatusWnd(const ModemStatusWnd&) = delete;
    /**
     * @brief Move constructor.
     */
    ModemStatusWnd(ModemStatusWnd&&) noexcept = delete;
    /**
     * @brief Finalizes an instance of the ModemStatusWnd class.
     */
    ~ModemStatusWnd() noexcept override = default;

    /**
     * @brief Disable copy assignment operator (=).
     */
    auto operator= (const ModemStatusWnd&) -> ModemStatusWnd& = delete;
    /**
     * @brief Disable move assignment operator (=).
     */
    auto operator= (ModemStatusWnd&&) noexcept -> ModemStatusWnd& = delete;

    /**
     * @brief Disable set X coordinate.
     */
    void setX(int, bool = true) override { }
    /**
     * @brief Disable set Y coordinate.
     */
    void setY(int, bool = true) override { }
    /**
     * @brief Disable set position.
     */
    void setPos(const FPoint&, bool = true) override { }

    /**
     * @brief Helper to append text.
     * @param str Text to append.
     */
    void append(std::string str) 
    {
        if (str.empty()) {
            return;
        }

        m_scrollText.append(str);
        m_scrollText.scrollToEnd();
        redraw();
    }

    /**
     * @brief Helper to clear the text.
     */
    void clear() { m_scrollText.clear(); }

private:
    FTextView m_scrollText{this};

    /**
     * @brief Initializes the window layout.
     */
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