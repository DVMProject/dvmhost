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

                // Generate Sync
                p25::Sync::addP25Sync(data + 2U);

                // Generate NID
                std::unique_ptr<p25::NID> nid = std::make_unique<p25::NID>(1U);
                nid->encode(data + 2U, DUID::TDU);

                // Add busy bits
                p25::P25Utils::addBusyBits(data + 2U, P25_TDU_FRAME_LENGTH_BITS, true, true);

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