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
#if !defined(__SETUP_APPLICATION_H__)
#define __SETUP_APPLICATION_H__

#include "common/Log.h"
#include "setup/HostSetup.h"
#include "setup/SetupMainWnd.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the finalcut application.
// ---------------------------------------------------------------------------

class HOST_SW_API SetupApplication final : public finalcut::FApplication {
public:
    /// <summary>
    /// Initializes a new instance of the SetupApplication class.
    /// </summary>
    /// <param name="setup"></param>
    /// <param name="argc"></param>
    /// <param name="argv"></param>
    explicit SetupApplication(HostSetup* setup, const int& argc, char** argv) : FApplication{argc, argv},
        m_setup(setup)
    {
        m_statusRefreshTimer = addTimer(1000);
    }

protected:
    /// <summary>
    ///
    /// </summary>
    void processExternalUserEvent() override
    {
        if (m_setup->m_isConnected) {
            if (m_setup->m_p25TduTest && m_setup->m_queue.hasSpace(p25::P25_TDU_FRAME_LENGTH_BYTES + 2U)) {
                uint8_t data[p25::P25_TDU_FRAME_LENGTH_BYTES + 2U];
                ::memset(data + 2U, 0x00U, p25::P25_TDU_FRAME_LENGTH_BYTES);

                // Generate Sync
                p25::Sync::addP25Sync(data + 2U);

                // Generate NID
                std::unique_ptr<p25::NID> nid = std::make_unique<p25::NID>(1U);
                nid->encode(data + 2U, p25::P25_DUID_TDU);

                // Add busy bits
                p25::P25Utils::addBusyBits(data + 2U, p25::P25_TDU_FRAME_LENGTH_BITS, true, true);

                data[0U] = modem::TAG_EOT;
                data[1U] = 0x00U;

                m_setup->addFrame(data, p25::P25_TDU_FRAME_LENGTH_BYTES + 2U, p25::P25_LDU_FRAME_LENGTH_BYTES);
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

    /// <summary>
    ///
    /// </summary>
    /// <param name="timer"></param>
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