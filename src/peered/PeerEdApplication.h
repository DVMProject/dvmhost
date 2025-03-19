// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Peer ID Editor
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file PeerEdApplication.h
 * @ingroup peered
 */
#if !defined(__PEERED_APPLICATION_H__)
#define __PEERED_APPLICATION_H__

#include "common/Log.h"
#include "PeerEdMain.h"
#include "PeerEdMainWnd.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements a color theme for a finalcut application.
 * @ingroup setup
 */
class HOST_SW_API dvmColorTheme final : public FWidgetColors
{
public:
    /**
     * @brief Initializes a new instance of the dvmColorTheme class.
     */
    dvmColorTheme()
    {
        dvmColorTheme::setColorTheme();
    }

    /**
     * @brief Finalizes a instance of the dvmColorTheme class.
     */
    ~dvmColorTheme() noexcept override = default;

    /**
     * @brief Get the Class Name object
     * @return FString 
     */
    auto getClassName() const -> FString override { return "dvmColorTheme"; }
    /**
     * @brief Set the Color Theme object
     */
    void setColorTheme() override
    {
        term_fg                           = FColor::Cyan;
        term_bg                           = FColor::Blue;

        list_fg                           = FColor::Black;
        list_bg                           = FColor::LightGray;
        selected_list_fg                  = FColor::Red;
        selected_list_bg                  = FColor::LightGray;

        dialog_fg                         = FColor::Black;
        dialog_resize_fg                  = FColor::LightBlue;
        dialog_emphasis_fg                = FColor::Blue;
        dialog_bg                         = FColor::LightGray;

        error_box_fg                      = FColor::LightRed;
        error_box_emphasis_fg             = FColor::Yellow;
        error_box_bg                      = FColor::Black;

        tooltip_fg                        = FColor::White;
        tooltip_bg                        = FColor::Black;

        shadow_fg                         = FColor::Black;
        shadow_bg                         = FColor::LightGray;  // only for transparent shadow

        current_element_focus_fg          = FColor::White;
        current_element_focus_bg          = FColor::Blue;
        current_element_fg                = FColor::LightGray;
        current_element_bg                = FColor::DarkGray;

        current_inc_search_element_fg     = FColor::LightRed;

        selected_current_element_focus_fg = FColor::LightRed;
        selected_current_element_focus_bg = FColor::Cyan;
        selected_current_element_fg       = FColor::Red;
        selected_current_element_bg       = FColor::Cyan;

        label_fg                          = FColor::Black;
        label_bg                          = FColor::LightGray;
        label_inactive_fg                 = FColor::DarkGray;
        label_inactive_bg                 = FColor::LightGray;
        label_hotkey_fg                   = FColor::Red;
        label_hotkey_bg                   = FColor::LightGray;
        label_emphasis_fg                 = FColor::Blue;
        label_ellipsis_fg                 = FColor::DarkGray;

        inputfield_active_focus_fg        = FColor::Yellow;
        inputfield_active_focus_bg        = FColor::Blue;
        inputfield_active_fg              = FColor::LightGray;
        inputfield_active_bg              = FColor::Blue;
        inputfield_inactive_fg            = FColor::Black;
        inputfield_inactive_bg            = FColor::DarkGray;

        toggle_button_active_focus_fg     = FColor::Yellow;
        toggle_button_active_focus_bg     = FColor::Blue;
        toggle_button_active_fg           = FColor::LightGray;
        toggle_button_active_bg           = FColor::Blue;
        toggle_button_inactive_fg         = FColor::Black;
        toggle_button_inactive_bg         = FColor::DarkGray;

        button_active_focus_fg            = FColor::Yellow;
        button_active_focus_bg            = FColor::Blue;
        button_active_fg                  = FColor::White;
        button_active_bg                  = FColor::Blue;
        button_inactive_fg                = FColor::Black;
        button_inactive_bg                = FColor::DarkGray;
        button_hotkey_fg                  = FColor::Yellow;

        titlebar_active_fg                = FColor::Blue;
        titlebar_active_bg                = FColor::White;
        titlebar_inactive_fg              = FColor::Blue;
        titlebar_inactive_bg              = FColor::LightGray;
        titlebar_button_fg                = FColor::Yellow;
        titlebar_button_bg                = FColor::LightBlue;
        titlebar_button_focus_fg          = FColor::LightGray;
        titlebar_button_focus_bg          = FColor::Black;

        menu_active_focus_fg              = FColor::Black;
        menu_active_focus_bg              = FColor::White;
        menu_active_fg                    = FColor::Black;
        menu_active_bg                    = FColor::LightGray;
        menu_inactive_fg                  = FColor::DarkGray;
        menu_inactive_bg                  = FColor::LightGray;
        menu_hotkey_fg                    = FColor::Blue;
        menu_hotkey_bg                    = FColor::LightGray;

        statusbar_fg                      = FColor::Black;
        statusbar_bg                      = FColor::LightGray;
        statusbar_hotkey_fg               = FColor::Blue;
        statusbar_hotkey_bg               = FColor::LightGray;
        statusbar_separator_fg            = FColor::Black;
        statusbar_active_fg               = FColor::Black;
        statusbar_active_bg               = FColor::White;
        statusbar_active_hotkey_fg        = FColor::Blue;
        statusbar_active_hotkey_bg        = FColor::White;

        scrollbar_fg                      = FColor::Cyan;
        scrollbar_bg                      = FColor::DarkGray;
        scrollbar_button_fg               = FColor::Yellow;
        scrollbar_button_bg               = FColor::DarkGray;
        scrollbar_button_inactive_fg      = FColor::LightGray;
        scrollbar_button_inactive_bg      = FColor::Black;

        progressbar_fg                    = FColor::Yellow;
        progressbar_bg                    = FColor::Blue;
    }
};

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the finalcut application.
 * @ingroup tged
 */
class HOST_SW_API PeerEdApplication final : public finalcut::FApplication {
public:
    /**
     * @brief Initializes a new instance of the PeerEdApplication class.
     * @param argc Passed argc.
     * @param argv Passed argv.
     */
    explicit PeerEdApplication(const int& argc, char** argv) : FApplication{argc, argv}
    {
        m_statusRefreshTimer = addTimer(1000);
    }

protected:
    /**
     * @brief Process external user events.
     */
    void processExternalUserEvent() override
    {
        /* stub */
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
            if (timer->getTimerId() == m_statusRefreshTimer) {
                /* stub */
            }
        }
    }

private:
    int m_statusRefreshTimer;
};

#endif // __PEERED_APPLICATION_H__