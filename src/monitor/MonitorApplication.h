/**
* Digital Voice Modem - Monitor
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Monitor
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
    virtual void processExternalUserEvent()
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