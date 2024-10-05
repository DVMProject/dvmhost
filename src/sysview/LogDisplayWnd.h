// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - FNE System View
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file LogDisplayWnd.h
 * @ingroup fneSysView
 */
#if !defined(__LOG_DISPLAY_WND_H__)
#define __LOG_DISPLAY_WND_H__

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the log display window.
 * @ingroup fneSysView
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

        FDialog::setGeometry(FPoint{2, (int)(rootWidget->getClientHeight() - 40)}, FSize{maxWidth, 40});
        FDialog::setMinimumSize(FSize{80, 20});
        FDialog::setResizeable(true);
        FDialog::setMinimizable(true);
        FDialog::setTitlebarButtonVisibility(true);
        FDialog::setShadow();

        minimizeWindow();

        m_scrollText.setGeometry(FPoint{1, 2}, FSize{getWidth(), getHeight() - 1});

        FDialog::initLayout();
    }

    /**
     * @brief Adjusts window size.
     */
    void adjustSize() override
    {
        FDialog::adjustSize();

        m_scrollText.setGeometry(FPoint{1, 2}, FSize{getWidth(), getHeight() - 1});
    }

    /*
    ** Event Handlers
    */

    /**
     * @brief Event that occurs when the window is closed.
     * @param e Close Event
     */
    void onClose(FCloseEvent* e) override
    {
        minimizeWindow();
    }

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