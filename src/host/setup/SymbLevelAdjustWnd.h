/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
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
#if !defined(__SYMB_LEVEL_ADJUST_WND_H__)
#define __SYMB_LEVEL_ADJUST_WND_H__

#include "host/setup/HostSetup.h"
#include "Thread.h"

#include "host/setup/AdjustWndBase.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the symbol level adjustment window.
// ---------------------------------------------------------------------------

class HOST_SW_API SymbLevelAdjustWnd final : public AdjustWndBase
{
public:
    /// <summary>
    /// Initializes a new instance of the SymbLevelAdjustWnd class.
    /// </summary>
    /// <param name="setup"></param>
    /// <param name="widget"></param>
    explicit SymbLevelAdjustWnd(HostSetup* setup, FWidget* widget = nullptr) : AdjustWndBase{setup, widget}
    {
        /* stub */
    }

private:
    FLabel m_symbLevelLabel{"Symbol Levels", this};
    FLabel m_dmr3LevelLabel{"DMR +/- 3 Symbol Level: ", this};
    FLabel m_dmr1LevelLabel{"DMR +/- 1 Symbol Level: ", this};
    FLabel m_p253LevelLabel{"P25 +/- 3 Symbol Level: ", this};
    FLabel m_p251LevelLabel{"P25 +/- 1 Symbol Level: ", this};
    FLabel m_nxdn3LevelLabel{"NXDN +/- 3 Symbol Level: ", this};
    FLabel m_nxdn1LevelLabel{"NXDN +/- 1 Symbol Level: ", this};

    FSpinBox m_dmr3Level{this};
    FSpinBox m_dmr1Level{this};
    FSpinBox m_p253Level{this};
    FSpinBox m_p251Level{this};
    FSpinBox m_nxdn3Level{this};
    FSpinBox m_nxdn1Level{this};

    /// <summary>
    ///
    /// </summary>
    void initLayout() override
    {
        FDialog::setText("Symbol Level Adjustment");
        FDialog::setSize(FSize{60, 16});

        AdjustWndBase::initLayout();
    }

    /// <summary>
    ///
    /// </summary>
    void initControls() override
    {
        // symbol levels
        {
            m_symbLevelLabel.setGeometry(FPoint(2, 1), FSize(20, 2));
            m_symbLevelLabel.setEmphasis();
            m_symbLevelLabel.setAlignment(Align::Center);

            m_dmr3LevelLabel.setGeometry(FPoint(2, 3), FSize(25, 1));
            m_dmr3Level.setGeometry(FPoint(28, 3), FSize(10, 1));
            m_dmr3Level.setRange(-127, 127);
            m_dmr3Level.setValue(m_setup->m_modem->m_dmrSymLevel3Adj);
            m_dmr3Level.setShadow(false);
            m_dmr3Level.addCallback("changed", [&]() {
                m_setup->m_modem->m_dmrSymLevel3Adj = m_dmr3Level.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });

            m_dmr1LevelLabel.setGeometry(FPoint(2, 4), FSize(25, 1));
            m_dmr1Level.setGeometry(FPoint(28, 4), FSize(10, 1));
            m_dmr1Level.setRange(-127, 127);
            m_dmr1Level.setValue(m_setup->m_modem->m_dmrSymLevel1Adj);
            m_dmr1Level.setShadow(false);
            m_dmr1Level.addCallback("changed", [&]() {
                m_setup->m_modem->m_dmrSymLevel1Adj = m_dmr1Level.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });

            m_p253LevelLabel.setGeometry(FPoint(2, 5), FSize(25, 1));
            m_p253Level.setGeometry(FPoint(28, 5), FSize(10, 1));
            m_p253Level.setRange(-127, 127);
            m_p253Level.setValue(m_setup->m_modem->m_p25SymLevel3Adj);
            m_p253Level.setShadow(false);
            m_p253Level.addCallback("changed", [&]() {
                m_setup->m_modem->m_p25SymLevel3Adj = m_p253Level.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });

            m_p251LevelLabel.setGeometry(FPoint(2, 6), FSize(25, 1));
            m_p251Level.setGeometry(FPoint(28, 6), FSize(10, 1));
            m_p251Level.setRange(-127, 127);
            m_p251Level.setValue(m_setup->m_modem->m_p25SymLevel1Adj);
            m_p251Level.setShadow(false);
            m_p251Level.addCallback("changed", [&]() {
                m_setup->m_modem->m_p25SymLevel1Adj = m_p251Level.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });

            m_nxdn3LevelLabel.setGeometry(FPoint(2, 7), FSize(25, 1));
            m_nxdn3Level.setGeometry(FPoint(28, 7), FSize(10, 1));
            m_nxdn3Level.setRange(-127, 127);
            m_nxdn3Level.setValue(m_setup->m_modem->m_nxdnSymLevel3Adj);
            m_nxdn3Level.setShadow(false);
            m_nxdn3Level.addCallback("changed", [&]() {
                m_setup->m_modem->m_nxdnSymLevel3Adj = m_nxdn3Level.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });

            m_nxdn1LevelLabel.setGeometry(FPoint(2, 8), FSize(25, 1));
            m_nxdn1Level.setGeometry(FPoint(28, 8), FSize(10, 1));
            m_nxdn1Level.setRange(-127, 127);
            m_nxdn1Level.setValue(m_setup->m_modem->m_nxdnSymLevel1Adj);
            m_nxdn1Level.setShadow(false);
            m_nxdn1Level.addCallback("changed", [&]() {
                m_setup->m_modem->m_nxdnSymLevel1Adj = m_nxdn1Level.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });
        }

        // setup control states
        if (m_setup->m_isConnected) {
            if (m_setup->m_modem->m_isHotspot) {
                m_dmr3Level.setDisable();
                m_dmr1Level.setDisable();
                m_p253Level.setDisable();
                m_p251Level.setDisable();
                m_nxdn3Level.setDisable();
                m_nxdn1Level.setDisable();
            }
            else {
                m_dmr3Level.setEnable();
                m_dmr1Level.setEnable();
                m_p253Level.setEnable();
                m_p251Level.setEnable();
                m_nxdn3Level.setEnable();
                m_nxdn1Level.setEnable();
            }
        }

        AdjustWndBase::initControls();
    }
};

#endif // __SYMB_LEVEL_ADJUST_WND_H__