// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Host Monitor Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file SelectedNodeWnd.h
 * @ingroup monitor
 */
#if !defined(__SELECTED_NODE_WND_H__)
#define __SELECTED_NODE_WND_H__

#include "MonitorMainWnd.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the selected node display window.
 * @ingroup monitor
 */
class HOST_SW_API SelectedNodeWnd final : public finalcut::FDialog {
public:
    /**
     * @brief Initializes a new instance of the SelectedNodeWnd class.
     * @param widget 
     */
    explicit SelectedNodeWnd(FWidget* widget = nullptr) : FDialog{widget}
    {
        /* stub */
    }
    /**
     * @brief Copy constructor.
     */
    SelectedNodeWnd(const SelectedNodeWnd&) = delete;
    /**
     * @brief Move constructor.
     */
    SelectedNodeWnd(SelectedNodeWnd&&) noexcept = delete;
    /**
     * @brief Finalizes an instance of the SelectedNodeWnd class.
     */
    ~SelectedNodeWnd() noexcept override = default;

    /**
     * @brief Disable copy assignment operator (=).
     */
    auto operator= (const SelectedNodeWnd&) -> SelectedNodeWnd& = delete;
    /**
     * @brief Disable move assignment operator (=).
     */
    auto operator= (SelectedNodeWnd&&) noexcept -> SelectedNodeWnd& = delete;

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
     * @brief Helper to set the selected host text.
     * @param str Text.
     */
    void setSelectedText(std::string str) 
    {
        m_selectedHost.setText(str);
        redraw();
    }

private:
    FLabel m_selectedHostLabel{"Selected Host: ", this};
    FLabel m_selectedHost{this};

    /**
     * @brief Initializes the window layout.
     */
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

    /**
     * @brief Draws the window.
     */
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