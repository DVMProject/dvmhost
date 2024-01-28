// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Host Monitor Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Monitor Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__LOG_DISPLAY_WND_H__)
#define __LOG_DISPLAY_WND_H__

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

        const auto& rootWidget = getRootWidget();

        FDialog::setGeometry(FPoint{(int)(rootWidget->getClientWidth() - 81), (int)(rootWidget->getClientHeight() - 20)}, FSize{80, 20});
        FDialog::setMinimumSize(FSize{80, 20});
        FDialog::setResizeable(true);
        FDialog::setMinimizable(true);
        FDialog::setTitlebarButtonVisibility(true);
        FDialog::setShadow();

        minimizeWindow();

        m_scrollText.setGeometry(FPoint{1, 2}, FSize{getWidth(), getHeight() - 1});

        FDialog::initLayout();
    }

    /// <summary>
    ///
    /// </summary>
    void adjustSize() override
    {
        FDialog::adjustSize();

        m_scrollText.setGeometry(FPoint{1, 2}, FSize{getWidth(), getHeight() - 1});
    }

    /*
    ** Event Handlers
    */

    /// <summary>
    /// 
    /// </summary>
    /// <param name="e"></param>
    void onClose(FCloseEvent* e) override
    {
        minimizeWindow();
    }

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