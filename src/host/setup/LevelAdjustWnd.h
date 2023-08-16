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
#if !defined(__LEVEL_ADJUST_WND_H__)
#define __LEVEL_ADJUST_WND_H__

#include "host/setup/HostSetup.h"
#include "Thread.h"

#include "host/setup/AdjustWndBase.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the modem level adjustment window.
// ---------------------------------------------------------------------------

class HOST_SW_API LevelAdjustWnd final : public AdjustWndBase {
public:
    /// <summary>
    /// Initializes a new instance of the LevelAdjustWnd class.
    /// </summary>
    /// <param name="setup"></param>
    /// <param name="widget"></param>
    explicit LevelAdjustWnd(HostSetup* setup, FWidget* widget = nullptr) : AdjustWndBase{setup, widget}
    {
        /* stub */
    }

private:
    FLabel m_softwareLevelLabel{"Software Levels", this};
    FLabel m_rxLevelLabel{"Rx Level: ", this};
    FLabel m_rxDCOffsetLabel{"Rx DC Offset: ", this};
    FLabel m_txLevelLabel{"Tx Level: ", this};
    FLabel m_txDCOffsetLabel{"Tx DC Offset: ", this};

    FLabel m_softpotLevelLabel{"Softpot Levels", this};
    FLabel m_rxCoarseLabel{"Rx Coarse: ", this};
    FLabel m_rxFineLabel{"Rx Fine: ", this};
    FLabel m_txCoarseLabel{"Tx Coarse: ", this};
    FLabel m_rssiCoarseLabel{"RSSI Coarse: ", this};

    FLabel m_digitalTimingLabel{"Digital Timing", this};
    FLabel m_fdmaPreambleLabel{"FDMA Preambles: ", this};
    FLabel m_dmrRxDelayLabel{"DMR Rx Delay: ", this};
    FLabel m_p25CorrCountLabel{"P25 Corr. Count: ", this};

    FLabel m_freqAdjustLabel{"Hotspot Frequency Offset", this};
    FLabel m_rxFreqAdjLabel{"Rx Freq. Offset: ", this};
    FLabel m_txFreqAdjLabel{"Tx Freq. Offset: ", this};

    FSpinBox m_rxLevel{this};
    FSpinBox m_rxDCOffsetLevel{this};
    FSpinBox m_txLevel{this};
    FSpinBox m_txDCOffsetLevel{this};

    FSpinBox m_rxCoarseLevel{this};
    FSpinBox m_rxFineLevel{this};
    FSpinBox m_txCoarseLevel{this};
    FSpinBox m_rssiCoarseLevel{this};

    FSpinBox m_fdmaPreambles{this};
    FSpinBox m_dmrRxDelay{this};
    FSpinBox m_p25CorrCount{this};

    FSpinBox m_rxTuning{this};
    FSpinBox m_txTuning{this};

    /// <summary>
    ///
    /// </summary>
    void initLayout() override
    {
        FDialog::setText("Modem Level Adjustment");
        FDialog::setSize(FSize{65, 22});

        AdjustWndBase::initLayout();
    }

    /// <summary>
    ///
    /// </summary>
    void initControls() override
    {
        // software levels
        {
            m_softwareLevelLabel.setGeometry(FPoint(2, 1), FSize(20, 2));
            m_softwareLevelLabel.setEmphasis();
            m_softwareLevelLabel.setAlignment(Align::Center);

            m_rxLevelLabel.setGeometry(FPoint(2, 3), FSize(20, 1));
            m_rxLevel.setGeometry(FPoint(18, 3), FSize(10, 1));
            m_rxLevel.setRange(0, 100);
            m_rxLevel.setValue(m_setup->m_modem->m_rxLevel);
            m_rxLevel.setShadow(false);
            m_rxLevel.addCallback("changed", [&]() {
                m_setup->m_modem->m_rxLevel = m_rxLevel.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });

            m_rxDCOffsetLabel.setGeometry(FPoint(2, 4), FSize(20, 1));
            m_rxDCOffsetLevel.setGeometry(FPoint(18, 4), FSize(10, 1));
            m_rxDCOffsetLevel.setRange(-127, 127);
            m_rxDCOffsetLevel.setValue(m_setup->m_modem->m_rxDCOffset);
            m_rxDCOffsetLevel.setShadow(false);
            m_rxDCOffsetLevel.addCallback("changed", [&]() {
                m_setup->m_modem->m_rxDCOffset = m_rxDCOffsetLevel.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });

            m_txLevelLabel.setGeometry(FPoint(2, 5), FSize(20, 1));
            m_txLevel.setGeometry(FPoint(18, 5), FSize(10, 1));
            m_txLevel.setRange(0, 100);
            m_txLevel.setValue(m_setup->m_modem->m_cwIdTXLevel);
            m_txLevel.setShadow(false);
            m_txLevel.addCallback("changed", [&]() {
                m_setup->m_modem->m_cwIdTXLevel = m_txLevel.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });

            m_txDCOffsetLabel.setGeometry(FPoint(2, 6), FSize(20, 1));
            m_txDCOffsetLevel.setGeometry(FPoint(18, 6), FSize(10, 1));
            m_txDCOffsetLevel.setRange(-127, 127);
            m_txDCOffsetLevel.setValue(m_setup->m_modem->m_txDCOffset);
            m_txDCOffsetLevel.setShadow(false);
            m_txDCOffsetLevel.addCallback("changed", [&]() {
                m_setup->m_modem->m_txDCOffset = m_txDCOffsetLevel.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });
        }

        // digital timing
        {
            m_digitalTimingLabel.setGeometry(FPoint(32, 1), FSize(20, 2));
            m_digitalTimingLabel.setEmphasis();
            m_digitalTimingLabel.setAlignment(Align::Center);

            m_fdmaPreambleLabel.setGeometry(FPoint(32, 3), FSize(20, 1));
            m_fdmaPreambles.setGeometry(FPoint(52, 3), FSize(10, 1));
            m_fdmaPreambles.setRange(0, 255);
            m_fdmaPreambles.setValue(m_setup->m_modem->m_fdmaPreamble);
            m_fdmaPreambles.setShadow(false);
            m_fdmaPreambles.addCallback("changed", [&]() {
                m_setup->m_modem->m_fdmaPreamble = m_fdmaPreambles.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });

            m_dmrRxDelayLabel.setGeometry(FPoint(32, 4), FSize(20, 1));
            m_dmrRxDelay.setGeometry(FPoint(52, 4), FSize(10, 1));
            m_dmrRxDelay.setRange(0, 255);
            m_dmrRxDelay.setValue(m_setup->m_modem->m_dmrRxDelay);
            m_dmrRxDelay.setShadow(false);
            m_dmrRxDelay.addCallback("changed", [&]() {
                m_setup->m_modem->m_dmrRxDelay = m_dmrRxDelay.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });

            m_p25CorrCountLabel.setGeometry(FPoint(32, 5), FSize(20, 1));
            m_p25CorrCount.setGeometry(FPoint(52, 5), FSize(10, 1));
            m_p25CorrCount.setRange(0, 255);
            m_p25CorrCount.setValue(m_setup->m_modem->m_p25CorrCount);
            m_p25CorrCount.setShadow(false);
            m_p25CorrCount.addCallback("changed", [&]() {
                m_setup->m_modem->m_p25CorrCount = m_p25CorrCount.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });
        }

        // softpot levels
        {
            m_softpotLevelLabel.setGeometry(FPoint(2, 8), FSize(20, 2));
            m_softpotLevelLabel.setEmphasis();
            m_softpotLevelLabel.setAlignment(Align::Center);

            m_rxCoarseLabel.setGeometry(FPoint(2, 10), FSize(20, 1));
            m_rxCoarseLevel.setGeometry(FPoint(18, 10), FSize(10, 1));
            m_rxCoarseLevel.setRange(-127, 127);
            m_rxCoarseLevel.setValue(m_setup->m_modem->m_rxCoarsePot);
            m_rxCoarseLevel.setShadow(false);
            m_rxCoarseLevel.addCallback("changed", [&]() {
                m_setup->m_modem->m_rxCoarsePot = m_rxCoarseLevel.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });

            m_rxFineLabel.setGeometry(FPoint(2, 11), FSize(20, 1));
            m_rxFineLevel.setGeometry(FPoint(18, 11), FSize(10, 1));
            m_rxFineLevel.setRange(-127, 127);
            m_rxFineLevel.setValue(m_setup->m_modem->m_rxFinePot);
            m_rxFineLevel.setShadow(false);
            m_rxFineLevel.addCallback("changed", [&]() {
                m_setup->m_modem->m_rxFinePot = m_rxFineLevel.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });

            m_txCoarseLabel.setGeometry(FPoint(2, 12), FSize(20, 1));
            m_txCoarseLevel.setGeometry(FPoint(18, 12), FSize(10, 1));
            m_txCoarseLevel.setRange(-127, 127);
            m_txCoarseLevel.setValue(m_setup->m_modem->m_txCoarsePot);
            m_txCoarseLevel.setShadow(false);
            m_txCoarseLevel.addCallback("changed", [&]() {
                m_setup->m_modem->m_txCoarsePot = m_txCoarseLevel.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });

            m_rssiCoarseLabel.setGeometry(FPoint(2, 13), FSize(20, 1));
            m_rssiCoarseLevel.setGeometry(FPoint(18, 13), FSize(10, 1));
            m_rssiCoarseLevel.setRange(-127, 127);
            m_rssiCoarseLevel.setValue(m_setup->m_modem->m_rssiCoarsePot);
            m_rssiCoarseLevel.setShadow(false);
            m_rssiCoarseLevel.addCallback("changed", [&]() {
                m_setup->m_modem->m_rssiCoarsePot = m_rssiCoarseLevel.getValue();
                Thread::sleep(2);
                m_setup->writeConfig();
            });
        }

        // frequency offset
        {
            m_freqAdjustLabel.setGeometry(FPoint(32, 8), FSize(30, 2));
            m_freqAdjustLabel.setEmphasis();
            m_freqAdjustLabel.setAlignment(Align::Center);

            m_rxFreqAdjLabel.setGeometry(FPoint(32, 10), FSize(20, 1));
            m_rxTuning.setGeometry(FPoint(52, 10), FSize(10, 1));
            m_rxTuning.setRange(-100000, 100000);
            m_rxTuning.setValue(m_setup->m_modem->m_rxTuning);
            m_rxTuning.setShadow(false);
            m_rxTuning.addCallback("changed", [&]() {
                m_setup->m_modem->m_rxTuning = m_rxTuning.getValue();
                Thread::sleep(2);
                m_setup->calculateRxTxFreq();
                m_setup->writeRFParams();
                Thread::sleep(2);
                m_setup->writeConfig();
            });

            m_txFreqAdjLabel.setGeometry(FPoint(32, 12), FSize(20, 1));
            m_txTuning.setGeometry(FPoint(52, 12), FSize(10, 1));
            m_txTuning.setRange(-100000, 100000);
            m_txTuning.setValue(m_setup->m_modem->m_txTuning);
            m_txTuning.setShadow(false);
            m_txTuning.addCallback("changed", [&]() {
                m_setup->m_modem->m_txTuning = m_txTuning.getValue();
                Thread::sleep(2);
                m_setup->calculateRxTxFreq();
                m_setup->writeRFParams();
                Thread::sleep(2);
                m_setup->writeConfig();
            });
        }

        // setup control states
        if (m_setup->m_isConnected) {
            if (m_setup->m_modem->m_isHotspot) {
                m_p25CorrCount.setDisable();
                
                m_rxLevel.setDisable();
                m_rxDCOffsetLevel.setDisable();
                m_txDCOffsetLevel.setDisable();

                m_rxCoarseLevel.setDisable();
                m_rxFineLevel.setDisable();
                m_txCoarseLevel.setDisable();
                m_rssiCoarseLevel.setDisable();

                m_rxTuning.setEnable();
                m_txTuning.setEnable();
            }
            else {
                m_p25CorrCount.setEnable();
                
                m_rxLevel.setEnable();
                m_rxDCOffsetLevel.setEnable();
                m_txDCOffsetLevel.setEnable();

                m_rxCoarseLevel.setEnable();
                m_rxFineLevel.setEnable();
                m_txCoarseLevel.setEnable();
                m_rssiCoarseLevel.setEnable();

                m_rxTuning.setDisable();
                m_txTuning.setDisable();
            }
        }

        AdjustWndBase::initControls();
    }
};

#endif // __LEVEL_ADJUST_WND_H__