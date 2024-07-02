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
 * @file MonitorApplication.h
 * @ingroup monitor
 */
#if !defined(__MONITOR_APPLICATION_H__)
#define __MONITOR_APPLICATION_H__

#include "common/Log.h"
#include "MonitorMain.h"
#include "MonitorMainWnd.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the finalcut application.
 * @ingroup monitor
 */
class HOST_SW_API MonitorApplication final : public finalcut::FApplication {
public:
    /**
     * @brief Initializes a new instance of the MonitorApplication class.
     * @param argc Passed argc.
     * @param argv Passed argv.
     */
    explicit MonitorApplication(const int& argc, char** argv) : FApplication{argc, argv}
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

#endif // __MONITOR_APPLICATION_H__