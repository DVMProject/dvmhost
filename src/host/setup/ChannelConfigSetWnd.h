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
#if !defined(__CHANNEL_CONFIG_SET_WND_H__)
#define __CHANNEL_CONFIG_SET_WND_H__

#include "host/setup/HostSetup.h"
#include "Thread.h"

#include "host/setup/CloseWndBase.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the channel configuration window.
// ---------------------------------------------------------------------------

class HOST_SW_API ChannelConfigSetWnd final : public CloseWndBase
{
public:
    /// <summary>
    /// Initializes a new instance of the ChannelConfigSetWnd class.
    /// </summary>
    /// <param name="setup"></param>
    /// <param name="widget"></param>
    explicit ChannelConfigSetWnd(HostSetup* setup, FWidget* widget = nullptr) : CloseWndBase{setup, widget}
    {
        /* stub */
    }

private:
    FLabel m_channelIdLabel{"Channel ID: ", this};
    FSpinBox m_channelId{this};

    FLabel m_baseFreqLabel{"Base Freq. (Hz): ", this};
    FLabel m_baseFreq{this};
    FLabel m_spaceHzLabel{"Spacing (Hz): ", this};
    FLabel m_spaceHz{this};

    FButtonGroup m_chNoGroup{"Logical Channel Number", this};
    FRadioButton m_radioChNo{"Channel Number", &m_chNoGroup};
    FRadioButton m_radioChFreq{"Tx Frequency", &m_chNoGroup};

    FLabel m_channelNoLabel{"Channel No.: ", this};
    FSpinBox m_channelNo{this};
    bool m_displayChannelFreq = false;
    FLabel m_channelFreqLabel{"Tx Frequency: ", this};
    FSpinBox m_channelFreq{this};

    FLabel m_hzLabel{"Hz", this};

    /// <summary>
    ///
    /// </summary>
    void initLayout() override
    {
        FDialog::setText("Channel Configuration");
        FDialog::setSize(FSize{60, 17});

        m_enableSetButton = false;
        CloseWndBase::initLayout();
    }

    /// <summary>
    ///
    /// </summary>
    void initControls() override
    {
        yaml::Node rfssConfig = m_setup->m_conf["system"]["config"];
        m_setup->m_channelId = (uint8_t)rfssConfig["channelId"].as<uint32_t>(0U);
        IdenTable entry = m_setup->m_idenTable->find(m_setup->m_channelId);

        // channel ID and channel number type
        {
            m_channelIdLabel.setGeometry(FPoint(2, 2), FSize(20, 1));
            m_channelId.setGeometry(FPoint(23, 2), FSize(8, 1));
            m_channelId.setValue(m_setup->m_channelId);
            m_channelId.setRange(0, 15);
            m_channelId.setShadow(false);
            m_channelId.addCallback("changed", [&]() {
                uint8_t prevChannelId = m_setup->m_channelId;
                m_setup->m_channelId = (uint8_t)(m_channelId.getValue());

                entry = m_setup->m_idenTable->find(m_setup->m_channelId);
                if (entry.baseFrequency() == 0U) {
                    std::stringstream ss;
                    ss << "Channel Id " << (uint32_t)(m_setup->m_channelId) << " has an invalid base frequency.";
                    FMessageBox::error(this, ss.str());
                    m_setup->m_channelId = prevChannelId;
                }

                entry = m_setup->m_idenTable->find(m_setup->m_channelId);
                m_baseFreq.setText(__INT_STR(entry.baseFrequency()));
                m_spaceHz.setText(__INT_STR(entry.chSpaceKhz() * 1000));

                m_setup->m_conf["system"]["config"]["channelId"] = __INT_STR(m_setup->m_channelId);
                m_setup->calculateRxTxFreq();
                if (m_setup->m_isConnected) {
                    m_setup->writeRFParams();
                }
            });

            m_baseFreqLabel.setGeometry(FPoint(2, 4), FSize(20, 1));
            m_baseFreq.setGeometry(FPoint(23, 4), FSize(20, 1));
            m_baseFreq.setText(__INT_STR(entry.baseFrequency()));
            m_spaceHzLabel.setGeometry(FPoint(2, 5), FSize(20, 1));
            m_spaceHz.setGeometry(FPoint(23, 5), FSize(20, 1));
            m_spaceHz.setText(__INT_STR(entry.chSpaceKhz() * 1000));

            m_chNoGroup.setGeometry(FPoint(2, 7), FSize(56, 2));
            m_radioChNo.setPos(FPoint(1, 1));
            m_radioChNo.addCallback("toggled", [&]() {
                if (m_radioChNo.isChecked()) {
                    m_displayChannelFreq = false;
                    updateVisibleControls();
                }
            });
            m_radioChFreq.setPos(FPoint(23, 1));
            m_radioChFreq.addCallback("toggled", [&]() {
                if (m_radioChFreq.isChecked()) {
                    m_displayChannelFreq = true;
                    updateVisibleControls();
                }
            });
        }

        // channel number
        {
            m_setup->m_channelNo = (uint32_t)::strtoul(rfssConfig["channelNo"].as<std::string>("1").c_str(), NULL, 16);

            m_channelNoLabel.setGeometry(FPoint(2, 11), FSize(20, 1));
            m_channelNo.setGeometry(FPoint(23, 11), FSize(15, 1));
            m_channelNo.setValue(m_setup->m_channelNo);
            m_channelNo.setRange(0, 4095);
            m_channelNo.setShadow(false);
            m_channelNo.addCallback("changed", [&]() {
                m_setup->m_conf["system"]["config"]["channelNo"] = __INT_HEX_STR(m_channelNo.getValue());
                m_setup->calculateRxTxFreq();
                m_channelFreq.setValue(m_setup->m_txFrequency);
                if (m_setup->m_isConnected) {
                    m_setup->writeRFParams();
                }
            });
        }

        // channel frequency
        {
            m_setup->m_channelNo = (uint32_t)::strtoul(rfssConfig["channelNo"].as<std::string>("1").c_str(), NULL, 16);

            m_channelFreqLabel.setGeometry(FPoint(2, 12), FSize(20, 1));
            m_channelFreq.setGeometry(FPoint(23, 12), FSize(15, 1));
            m_channelFreq.setValue(m_setup->m_txFrequency);
            m_channelFreq.setShadow(false);
            m_channelFreq.addCallback("changed", [&]() {
                entry = m_setup->m_idenTable->find(m_setup->m_channelId);

                uint32_t txFrequency = m_channelFreq.getValue();

                uint32_t prevTxFrequency = m_setup->m_txFrequency;
                m_setup->m_txFrequency = txFrequency;
                uint32_t prevRxFrequency = m_setup->m_rxFrequency;
                m_setup->m_rxFrequency = m_setup->m_txFrequency + (uint32_t)(entry.txOffsetMhz() * 1000000);

                float spaceHz = entry.chSpaceKhz() * 1000;

                uint32_t rootFreq = m_setup->m_txFrequency - entry.baseFrequency();
                uint8_t prevChannelNo = m_setup->m_channelNo;
                m_setup->m_channelNo = (uint32_t)(rootFreq / spaceHz);

                if (m_setup->m_channelNo < 0 || m_setup->m_channelNo > 4096) {
                    m_setup->m_channelNo = prevChannelNo;
                    m_setup->m_txFrequency = prevTxFrequency;
                    m_setup->m_rxFrequency = prevRxFrequency;
                }

                m_setup->m_conf["system"]["config"]["channelNo"] = __INT_HEX_STR(m_setup->m_channelNo);
                m_setup->calculateRxTxFreq();
                m_channelNo.setValue(m_setup->m_channelNo);
                if (m_setup->m_isConnected) {
                    m_setup->writeRFParams();
                }
            });
            m_hzLabel.setGeometry(FPoint(40, 12), FSize(5, 1));
        }

        updateVisibleControls();

        CloseWndBase::initControls();
    }

    /// <summary>
    /// 
    /// </summary>
    void updateVisibleControls()
    {
        if (m_displayChannelFreq) {
            m_channelNoLabel.setDisable();
            m_channelNo.setDisable();

            m_channelFreqLabel.setEnable();
            m_channelFreq.setEnable();

            redraw();
            return;
        }

        m_channelNoLabel.setEnable();
        m_channelNo.setEnable();
        m_channelFreqLabel.setDisable();
        m_channelFreq.setDisable();

        redraw();
    }
};

#endif // __CHANNEL_CONFIG_SET_WND_H__