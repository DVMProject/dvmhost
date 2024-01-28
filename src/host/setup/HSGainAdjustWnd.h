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
#if !defined(__HS_GAIN_ADJUST_WND_H__)
#define __HS_GAIN_ADJUST_WND_H__

#include "common/Thread.h"
#include "setup/HostSetup.h"

#include "setup/AdjustWndBase.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the hotspot gain adjustment window.
// ---------------------------------------------------------------------------

class HOST_SW_API HSGainAdjustWnd final : public AdjustWndBase {
public:
    /// <summary>
    /// Initializes a new instance of the HSGainAdjustWnd class.
    /// </summary>
    /// <param name="setup"></param>
    /// <param name="widget"></param>
    explicit HSGainAdjustWnd(HostSetup* setup, FWidget* widget = nullptr) : AdjustWndBase{setup, widget}
    {
        /* stub */
    }

private:
    FLabel m_adjGainLabel{"Gain Adjustment", this};

    FButtonGroup m_gainButtonGroup{"Gain", this};
    FRadioButton m_gainAHL{"Auto High Linearity", &m_gainButtonGroup};
    FRadioButton m_gainLow{"Low", &m_gainButtonGroup};
    FRadioButton m_gainHigh{"High", &m_gainButtonGroup};
    FRadioButton m_gainAuto{"Auto", &m_gainButtonGroup};

    FLabel m_adjAFCLabel{"AFC Adjustment", this};

    FCheckBox m_afcEnabled{"Enabled", this};
    FLabel m_afcRangeLabel{"Range: ", this};
    FSpinBox m_afcRange{this};
    FLabel m_afcKILabel{"KI: ", this};
    FSpinBox m_afcKI{this};
    FLabel m_afcKPLabel{"KP: ", this};
    FSpinBox m_afcKP{this};

    /// <summary>
    ///
    /// </summary>
    void initLayout() override
    {
        FDialog::setText("Hotspot Gain & AFC Adjustment");
        FDialog::setSize(FSize{50, 22});

        AdjustWndBase::initLayout();
    }

    /// <summary>
    ///
    /// </summary>
    void initControls() override
    {
        // gain
        {
            m_adjGainLabel.setGeometry(FPoint(2, 1), FSize(30, 2));
            m_adjGainLabel.setEmphasis();
            m_adjGainLabel.setAlignment(Align::Center);

            m_gainButtonGroup.setGeometry(FPoint(2, 3), FSize(30, 6));
            m_gainAHL.setPos(FPoint(1, 1));
            m_gainAHL.addCallback("toggled", [&]() {
                if (m_gainAHL.isChecked()) {
                    m_setup->m_modem->m_adfGainMode = ADF_GAIN_AUTO_LIN;
                    m_setup->writeRFParams();
                }
            });
            
            if (m_setup->m_modem->m_adfGainMode == ADF_GAIN_AUTO_LIN) {
                m_gainAHL.setChecked();
            }

            m_gainLow.setPos(FPoint(1, 2));
            m_gainLow.addCallback("toggled", [&]() {
                if (m_gainLow.isChecked()) {
                    m_setup->m_modem->m_adfGainMode = ADF_GAIN_LOW;
                    m_setup->writeRFParams();
                }
            });
            
            if (m_setup->m_modem->m_adfGainMode == ADF_GAIN_LOW) {
                m_gainLow.setChecked();
            }

            m_gainHigh.setPos(FPoint(1, 3));
            m_gainHigh.addCallback("toggled", [&]() {
                if (m_gainHigh.isChecked()) {
                    m_setup->m_modem->m_adfGainMode = ADF_GAIN_HIGH;
                    m_setup->writeRFParams();
                }
            });

            
            if (m_setup->m_modem->m_adfGainMode == ADF_GAIN_HIGH) {
                m_gainHigh.setChecked();
            }

            m_gainAuto.setPos(FPoint(1, 4));
            m_gainAuto.addCallback("toggled", [&]() {
                if (m_gainAuto.isChecked()) {
                    m_setup->m_modem->m_adfGainMode = ADF_GAIN_AUTO;
                    m_setup->writeRFParams();
                }
            });

            
            if (m_setup->m_modem->m_adfGainMode == ADF_GAIN_AUTO) {
                m_gainAuto.setChecked();
            }
        }

        // afc
        {
            m_adjAFCLabel.setGeometry(FPoint(2, 10), FSize(30, 2));
            m_adjAFCLabel.setEmphasis();
            m_adjAFCLabel.setAlignment(Align::Center);

            m_afcEnabled.setGeometry(FPoint(2, 12), FSize(10, 1));
            m_afcEnabled.setChecked(m_setup->m_modem->m_afcEnable);
            m_afcEnabled.addCallback("toggled", [&]() {
                m_setup->m_modem->m_afcEnable = m_afcEnabled.isChecked();
                Thread::sleep(2);
                m_setup->writeRFParams();
            });

            m_afcRangeLabel.setGeometry(FPoint(24, 12), FSize(10, 1));
            m_afcRange.setGeometry(FPoint(33, 12), FSize(10, 1));
            m_afcRange.setRange(0, 256);
            m_afcRange.setValue(m_setup->m_modem->m_afcRange);
            m_afcRange.setShadow(false);
            m_afcRange.addCallback("changed", [&]() {
                m_setup->m_modem->m_afcRange = m_afcRange.getValue();
                Thread::sleep(2);
                m_setup->writeRFParams();
            });

            m_afcKILabel.setGeometry(FPoint(2, 13), FSize(10, 1));
            m_afcKI.setGeometry(FPoint(10, 13), FSize(10, 1));
            m_afcKI.setRange(0, 16);
            m_afcKI.setValue(m_setup->m_modem->m_afcKI);
            m_afcKI.setShadow(false);
            m_afcKI.addCallback("changed", [&]() {
                m_setup->m_modem->m_afcKI = m_afcKI.getValue();
                Thread::sleep(2);
                m_setup->writeRFParams();
            });

            m_afcKPLabel.setGeometry(FPoint(24, 13), FSize(10, 1));
            m_afcKP.setGeometry(FPoint(33, 13), FSize(10, 1));
            m_afcKP.setRange(0, 8);
            m_afcKP.setValue(m_setup->m_modem->m_afcKP);
            m_afcKP.setShadow(false);
            m_afcKP.addCallback("changed", [&]() {
                m_setup->m_modem->m_afcKP = m_afcKP.getValue();
                Thread::sleep(2);
                m_setup->writeRFParams();
            });
        }

        AdjustWndBase::initControls();
    }
};

#endif // __HS_GAIN_ADJUST_WND_H__