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
#if !defined(__SELECTED_NODE_WND_H__)
#define __SELECTED_NODE_WND_H__

#include "MonitorMainWnd.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the selected node display window.
// ---------------------------------------------------------------------------

class HOST_SW_API SelectedNodeWnd final : public finalcut::FDialog {
public:
    /// <summary>
    /// Initializes a new instance of the SelectedNodeWnd class.
    /// </summary>
    /// <param name="widget"></param>
    explicit SelectedNodeWnd(FWidget* widget = nullptr) : FDialog{widget}
    {
        /* stub */
    }
    /// <summary>Copy constructor.</summary>
    SelectedNodeWnd(const SelectedNodeWnd&) = delete;
    /// <summary>Move constructor.</summary>
    SelectedNodeWnd(SelectedNodeWnd&&) noexcept = delete;
    /// <summary>Finalizes an instance of the SelectedNodeWnd class.</summary>
    ~SelectedNodeWnd() noexcept override = default;

    /// <summary>Disable copy assignment operator (=).</summary>
    auto operator= (const SelectedNodeWnd&) -> SelectedNodeWnd& = delete;
    /// <summary>Disable move assignment operator (=).</summary>
    auto operator= (SelectedNodeWnd&&) noexcept -> SelectedNodeWnd& = delete;

    /// <summary>Disable set X coordinate.</summary>
    void setX(int, bool = true) override { }
    /// <summary>Disable set Y coordinate.</summary>
    void setY(int, bool = true) override { }
    /// <summary>Disable set position.</summary>
    void setPos(const FPoint&, bool = true) override { }

    /// <summary></summary>
    void setSelectedText(std::string str) 
    {
        m_selectedHost.setText(str);
        redraw();
    }

private:
    FLabel m_selectedHostLabel{"Selected Host: ", this};
    FLabel m_selectedHost{this};

    /// <summary>
    ///
    /// </summary>
    void initLayout() override
    {
        std::size_t maxWidth;
        const auto& rootWidget = getRootWidget();

        if (rootWidget) {
            maxWidth = rootWidget->getClientWidth() - 3;
        }
        else {
            // fallback to xterm default size
            maxWidth = 77;
        }

        FDialog::setGeometry(FPoint{2, 2}, FSize{maxWidth, 2});
        FDialog::setMinimumSize(FSize{80, 5});
        FDialog::setResizeable(false);
        FDialog::setMinimizable(false);
        FDialog::setTitlebarButtonVisibility(false);
        FDialog::setShadow(false);

        m_selectedHostLabel.setGeometry(FPoint(2, 1), FSize(18, 1));
        m_selectedHost.setGeometry(FPoint(20, 1), FSize(60, 1));
        m_selectedHost.setText("None");

        FDialog::initLayout();
    }

    /// <summary>
    ///
    /// </summary>
    void draw() override
    {
        setColor();
        clearArea();

        const auto& wc = getColorTheme();
        setColor(wc->dialog_resize_fg, getBackgroundColor());

        finalcut::drawBorder(this, FRect(FPoint{1, 1}, FPoint{(int)getWidth(), (int)getHeight()}));
    }
};

#endif // __SELECTED_NODE_WND_H__