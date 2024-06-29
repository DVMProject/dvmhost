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
 * @file LogDisplayWnd.h
 * @ingroup setup
 */
#if !defined(__LOG_DISPLAY_WND_H__)
#define __LOG_DISPLAY_WND_H__

#include "setup/HostSetup.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the log display window.
 * @ingroup setup
 */
class HOST_SW_API LogDisplayWnd final : public finalcut::FDialog, public std::ostringstream {
public:
    /**
     * @brief Initializes a new instance of the LogDisplayWnd class.
     * @param widget 
     */
    explicit LogDisplayWnd(FWidget* widget = nullptr) : FDialog{widget}
    {
        m_scrollText.ignorePadding();

        m_timerId = addTimer(250); // starts the timer every 250 milliseconds
    }
    /**
     * @brief Copy constructor.
     */
    LogDisplayWnd(const LogDisplayWnd&) = delete;
    /**
     * @brief Move constructor.
     */
    LogDisplayWnd(LogDisplayWnd&&) noexcept = delete;
    /**
     * @brief Finalizes an instance of the LogDisplayWnd class.
     */
    ~LogDisplayWnd() noexcept override = default;

    /**
     * @brief Disable copy assignment operator (=).
     */
    auto operator= (const LogDisplayWnd&) -> LogDisplayWnd& = delete;
    /**
     * @brief Disable move assignment operator (=).
     */
    auto operator= (LogDisplayWnd&&) noexcept -> LogDisplayWnd& = delete;

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

private:
    FTextView m_scrollText{this};
    int m_timerId;

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        using namespace std::string_literals;
        auto lightning = "\u26a1";
        FDialog::setText("System Log"s + lightning);

        std::size_t maxWidth;
        const auto& rootWidget = getRootWidget();

        if (rootWidget) {
            maxWidth = rootWidget->getClientWidth() - 3;
        }
        else {
            // fallback to xterm default size
            maxWidth = 77;
        }

        FDialog::setGeometry(FPoint{2, 2}, FSize{maxWidth, 20});
        FDialog::setMinimumSize(FSize{80, 5});
        FDialog::setResizeable(false);
        FDialog::setMinimizable(false);
        FDialog::setTitlebarButtonVisibility(false);
        FDialog::setShadow();

        m_scrollText.setGeometry(FPoint{1, 2}, FSize{getWidth(), getHeight() - 1});

        FDialog::initLayout();
    }

    /*
    ** Event Handlers
    */

    /**
     * @brief Event that occurs on interval by timer.
     * @param timer Timer Event
     */
    void onTimer(FTimerEvent* timer) override
    {
        if (timer != nullptr) {
            if (timer->getTimerId() == m_timerId) {
                if (str().empty()) {
                    return;
                }

                m_scrollText.append(str());
                str("");
                m_scrollText.scrollToEnd();
                redraw();
            }
        }
    }
};

#endif // __LOG_DISPLAY_WND_H__