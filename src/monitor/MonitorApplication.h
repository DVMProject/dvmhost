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
#if !defined(__MONITOR_APPLICATION_H__)
#define __MONITOR_APPLICATION_H__

#include "common/Log.h"
#include "MonitorMain.h"
#include "MonitorMainWnd.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the finalcut application.
// ---------------------------------------------------------------------------

class HOST_SW_API MonitorApplication final : public finalcut::FApplication {
public:
    /// <summary>
    /// Initializes a new instance of the MonitorApplication class.
    /// </summary>
    /// <param name="argc"></param>
    /// <param name="argv"></param>
    explicit MonitorApplication(const int& argc, char** argv) : FApplication{argc, argv}
    {
        m_statusRefreshTimer = addTimer(1000);
    }

protected:
    /// <summary>
    ///
    /// </summary>
    void processExternalUserEvent() override
    {
        /* stub */
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
                /* stub */
            }
        }
    }

private:
    int m_statusRefreshTimer;
};

#endif // __MONITOR_APPLICATION_H__