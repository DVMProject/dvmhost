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
#if !defined(__LOG_DISPLAY_WND_H__)
#define __LOG_DISPLAY_WND_H__

#include "setup/HostSetup.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the log display window.
// ---------------------------------------------------------------------------

class HOST_SW_API LogDisplayWnd final : public finalcut::FDialog, public std::ostringstream {
public:
    /// <summary>
    /// Initializes a new instance of the LogDisplayWnd class.
    /// </summary>
    /// <param name="widget"></param>
    explicit LogDisplayWnd(FWidget* widget = nullptr) : FDialog{widget}
    {
        m_scrollText.ignorePadding();

        m_timerId = addTimer(250); // starts the timer every 250 milliseconds
    }
    /// <summary>Copy constructor.</summary>
    LogDisplayWnd(const LogDisplayWnd&) = delete;
    /// <summary>Move constructor.</summary>
    LogDisplayWnd(LogDisplayWnd&&) noexcept = delete;
    /// <summary>Finalizes an instance of the LogDisplayWnd class.</summary>
    ~LogDisplayWnd() noexcept override = default;

    /// <summary>Disable copy assignment operator (=).</summary>
    auto operator= (const LogDisplayWnd&) -> LogDisplayWnd& = delete;
    /// <summary>Disable move assignment operator (=).</summary>
    auto operator= (LogDisplayWnd&&) noexcept -> LogDisplayWnd& = delete;

    /// <summary>Disable set X coordinate.</summary>
    void setX(int, bool = true) override { }
    /// <summary>Disable set Y coordinate.</summary>
    void setY(int, bool = true) override { }
    /// <summary>Disable set position.</summary>
    void setPos(const FPoint&, bool = true) override { }

private:
    FTextView m_scrollText{this};
    int m_timerId;

    /// <summary>
    ///
    /// </summary>
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

    /// <summary>
    ///
    /// </summary>
    /// <param name="timer"></param>
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