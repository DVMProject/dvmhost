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
#if !defined(__HS_BANDWIDTH_ADJUST_WND_H__)
#define __HS_BANDWIDTH_ADJUST_WND_H__

#include "host/setup/HostSetup.h"
#include "Thread.h"

#include "host/setup/AdjustWndBase.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the hotspot bandwidth adjustment window.
// ---------------------------------------------------------------------------

class HOST_SW_API HSBandwidthAdjustWnd final : public AdjustWndBase
{
public:
    /// <summary>
    /// Initializes a new instance of the HSBandwidthAdjustWnd class.
    /// </summary>
    /// <param name="setup"></param>
    /// <param name="widget"></param>
    explicit HSBandwidthAdjustWnd(HostSetup* setup, FWidget* widget = nullptr) : AdjustWndBase{setup, widget}
    {
        /* stub */
    }

private:
    FLabel m_adjLevelLabel{"Bandwidth Adjustment", this};
    FLabel m_dmrDiscBWLabel{"DMR Disc. BW Offset: ", this};
    FLabel m_dmrPostBWLabel{"DMR Post Demod BW Offset: ", this};
    FLabel m_p25DiscBWLabel{"P25 Disc. BW Offset: ", this};
    FLabel m_p25PostBWLabel{"P25 Post Demod BW Offset: ", this};
    FLabel m_nxdnDiscBWLabel{"NXDN Disc. BW Offset: ", this};
    FLabel m_nxdnPostBWLabel{"NXDN Post Demod BW Offset: ", this};

    FSpinBox m_dmrDiscBW{this};
    FSpinBox m_dmrPostBW{this};
    FSpinBox m_p25DiscBW{this};
    FSpinBox m_p25PostBW{this};
    FSpinBox m_nxdnDiscBW{this};
    FSpinBox m_nxdnPostBW{this};

    /// <summary>
    ///
    /// </summary>
    void initLayout() override
    {
        FDialog::setText("Hotspot Bandwidth Adjustment");
        FDialog::setSize(FSize{60, 15});

        AdjustWndBase::initLayout();
    }

    /// <summary>
    ///
    /// </summary>
    void initControls() override
    {
        // symbol levels
        {
            m_adjLevelLabel.setGeometry(FPoint(2, 1), FSize(30, 2));
            m_adjLevelLabel.setEmphasis();
            m_adjLevelLabel.setAlignment(Align::Center);

            m_dmrDiscBWLabel.setGeometry(FPoint(2, 3), FSize(30, 1));
            m_dmrDiscBW.setGeometry(FPoint(33, 3), FSize(10, 1));
            m_dmrDiscBW.setRange(-127, 127);
            m_dmrDiscBW.setValue(m_setup->m_modem->m_dmrDiscBWAdj);
            m_dmrDiscBW.setShadow(false);
            m_dmrDiscBW.addCallback("changed", [&]() {
                m_setup->m_modem->m_dmrDiscBWAdj = m_dmrDiscBW.getValue();
                Thread::sleep(2);
                m_setup->writeRFParams();
            });

            m_dmrPostBWLabel.setGeometry(FPoint(2, 4), FSize(30, 1));
            m_dmrPostBW.setGeometry(FPoint(33, 4), FSize(10, 1));
            m_dmrPostBW.setRange(-127, 127);
            m_dmrPostBW.setValue(m_setup->m_modem->m_dmrPostBWAdj);
            m_dmrPostBW.setShadow(false);
            m_dmrPostBW.addCallback("changed", [&]() {
                m_setup->m_modem->m_dmrPostBWAdj = m_dmrPostBW.getValue();
                Thread::sleep(2);
                m_setup->writeRFParams();
            });

            m_p25DiscBWLabel.setGeometry(FPoint(2, 5), FSize(30, 1));
            m_p25DiscBW.setGeometry(FPoint(33, 5), FSize(10, 1));
            m_p25DiscBW.setRange(-127, 127);
            m_p25DiscBW.setValue(m_setup->m_modem->m_p25DiscBWAdj);
            m_p25DiscBW.setShadow(false);
            m_p25DiscBW.addCallback("changed", [&]() {
                m_setup->m_modem->m_p25DiscBWAdj = m_p25DiscBW.getValue();
                Thread::sleep(2);
                m_setup->writeRFParams();
            });

            m_p25PostBWLabel.setGeometry(FPoint(2, 6), FSize(30, 1));
            m_p25PostBW.setGeometry(FPoint(33, 6), FSize(10, 1));
            m_p25PostBW.setRange(-127, 127);
            m_p25PostBW.setValue(m_setup->m_modem->m_p25PostBWAdj);
            m_p25PostBW.setShadow(false);
            m_p25PostBW.addCallback("changed", [&]() {
                m_setup->m_modem->m_p25PostBWAdj = m_p25PostBW.getValue();
                Thread::sleep(2);
                m_setup->writeRFParams();
            });

            m_nxdnDiscBWLabel.setGeometry(FPoint(2, 6), FSize(30, 1));
            m_nxdnDiscBW.setGeometry(FPoint(33, 6), FSize(10, 1));
            m_nxdnDiscBW.setRange(-127, 127);
            m_nxdnDiscBW.setValue(m_setup->m_modem->m_nxdnDiscBWAdj);
            m_nxdnDiscBW.setShadow(false);
            m_nxdnDiscBW.addCallback("changed", [&]() {
                m_setup->m_modem->m_nxdnDiscBWAdj = m_nxdnDiscBW.getValue();
                Thread::sleep(2);
                m_setup->writeRFParams();
            });

            m_nxdnPostBWLabel.setGeometry(FPoint(2, 7), FSize(30, 1));
            m_nxdnPostBW.setGeometry(FPoint(33, 7), FSize(10, 1));
            m_nxdnPostBW.setRange(-127, 127);
            m_nxdnPostBW.setValue(m_setup->m_modem->m_nxdnPostBWAdj);
            m_nxdnPostBW.setShadow(false);
            m_nxdnPostBW.addCallback("changed", [&]() {
                m_setup->m_modem->m_nxdnPostBWAdj = m_nxdnPostBW.getValue();
                Thread::sleep(2);
                m_setup->writeRFParams();
            });
        }

        // setup control states
        if (m_setup->m_isConnected) {
            if (m_setup->m_modem->m_isHotspot) {
                m_dmrDiscBW.setEnable();
                m_dmrPostBW.setEnable();
                m_p25DiscBW.setEnable();
                m_p25PostBW.setEnable();
                m_nxdnDiscBW.setEnable();
                m_nxdnPostBW.setEnable();
            }
            else {
                m_dmrDiscBW.setDisable();
                m_dmrPostBW.setDisable();
                m_p25DiscBW.setDisable();
                m_p25PostBW.setDisable();
                m_nxdnDiscBW.setDisable();
                m_nxdnPostBW.setDisable();
            }
        }

        AdjustWndBase::initControls();
    }
};

#endif // __HS_BANDWIDTH_ADJUST_WND_H__