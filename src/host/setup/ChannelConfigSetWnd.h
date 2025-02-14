// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file ChannelConfigSetWnd.h
 * @ingroup setup
 */
#if !defined(__CHANNEL_CONFIG_SET_WND_H__)
#define __CHANNEL_CONFIG_SET_WND_H__

#include "common/Thread.h"
#include "setup/HostSetup.h"

#include "setup/CloseWndBase.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the channel configuration window.
 * @ingroup setup
 */
class HOST_SW_API ChannelConfigSetWnd final : public CloseWndBase {
public:
    /**
     * @brief Initializes a new instance of the ChannelConfigSetWnd class.
     * 
     * @param setup 
     * @param widget 
     */
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

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FDialog::setText("Channel Configuration");
        FDialog::setSize(FSize{60, 17});

        m_enableSetButton = false;
        CloseWndBase::initLayout();
    }

    /**
     * @brief Initializes window controls.
     */
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
                if (!m_radioChNo.isChecked())
                    return;

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
                if (!m_radioChFreq.isChecked())
                    return;

                uint32_t txFrequency = (uint32_t)(m_channelFreq.getValue());
                m_setup->calculateRxTxFreq(false, txFrequency);
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

    /**
     * @brief Helper to update control visibility.
     */
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