// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file SetupApplication.h
 * @ingroup setup
 */
#if !defined(__SETUP_APPLICATION_H__)
#define __SETUP_APPLICATION_H__

#include "common/Log.h"
#include "setup/HostSetup.h"
#include "setup/SetupMainWnd.h"

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
        current_element_focus_bg          = FColor::Cyan;
        current_element_fg                = FColor::LightBlue;
        current_element_bg                = FColor::Cyan;
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
 * @ingroup setup
 */
class HOST_SW_API SetupApplication final : public finalcut::FApplication {
public:
    /**
     * @brief Initializes a new instance of the SetupApplication class.
     * @param setup Instance of the HostSetup class.
     * @param argc Passed argc.
     * @param argv Passed argv.
     */
    explicit SetupApplication(HostSetup* setup, const int& argc, char** argv) : FApplication{argc, argv},
        m_setup(setup)
    {
        m_statusRefreshTimer = addTimer(1000);
    }

protected:
    /**
     * @brief Process external user events.
     */
    void processExternalUserEvent() override
    {
        using namespace p25::defines;
        if (m_setup->m_isConnected) {
            if (m_setup->m_p25TduTest && m_setup->m_queue.hasSpace(P25_TDU_FRAME_LENGTH_BYTES + 2U)) {
                uint8_t data[P25_TDU_FRAME_LENGTH_BYTES + 2U];
                ::memset(data + 2U, 0x00U, P25_TDU_FRAME_LENGTH_BYTES);

                // generate Sync
                p25::Sync::addP25Sync(data + 2U);

                // generate NID
                std::unique_ptr<p25::NID> nid = std::make_unique<p25::NID>(1U);
                nid->encode(data + 2U, DUID::TDU);

                // add status bits
                p25::P25Utils::addStatusBits(data + 2U, P25_TDU_FRAME_LENGTH_BITS, false, false);

                data[0U] = modem::TAG_EOT;
                data[1U] = 0x00U;

                m_setup->addFrame(data, P25_TDU_FRAME_LENGTH_BYTES + 2U, P25_LDU_FRAME_LENGTH_BYTES);
            }

            // ------------------------------------------------------
            //  -- Modem Clocking                                 --
            // ------------------------------------------------------

            uint32_t ms = m_setup->m_stopWatch.elapsed();
            m_setup->m_stopWatch.start();

            m_setup->m_modem->clock(ms);

            m_setup->timerClock();

            if (ms < 2U)
                Thread::sleep(1U);
        }
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
                m_setup->m_setupWnd->updateMenuStates();

                // display modem status
                if (m_setup->m_isConnected) {
                    if (m_setup->m_setupWnd->m_statusWnd.isShown()) {
                        m_setup->printStatus();
                    }
                }
            }
        }
    }

private:
    HostSetup* m_setup;

    int m_statusRefreshTimer;
};

#endif // __SETUP_APPLICATION_H__