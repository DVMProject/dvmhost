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
#if !defined(__SYSTEM_CONFIG_SET_WND_H__)
#define __SYSTEM_CONFIG_SET_WND_H__

#include "common/Thread.h"
#include "setup/HostSetup.h"

#include "setup/CloseWndBase.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the system configuration window.
// ---------------------------------------------------------------------------

class HOST_SW_API SystemConfigSetWnd final : public CloseWndBase {
public:
    /// <summary>
    /// Initializes a new instance of the SystemConfigSetWnd class.
    /// </summary>
    /// <param name="setup"></param>
    /// <param name="widget"></param>
    explicit SystemConfigSetWnd(HostSetup* setup, FWidget* widget = nullptr) : CloseWndBase{setup, widget}
    {
        /* stub */
    }

private:
    FLabel m_portAndSpeedLabel{"Modem Port and Speed", this};
    FLabel m_systemSettingsLabel{"System Settings", this};
    FLabel m_modeSettingsLabel{"Mode Settings", this};

    FLabel m_modemPortLabel{"Modem Port: ", this};
    FLineEdit m_modemPort{this};
    FLabel m_modemSpeedLabel{"Modem Speed: ", this};
    FSpinBox m_modemSpeed{this};

    FLabel m_identityLabel{"Identity: ", this};
    FLineEdit m_identity{this};
    FCheckBox m_duplex{"Duplex", this};
    FCheckBox m_simplexFreq{"Simplex Freq", this};
    FLabel m_timeoutLabel{"Timeout: ", this};
    FSpinBox m_timeout{this};
    FLabel m_modeHangLabel{"Mode Hangtime: ", this};
    FSpinBox m_modeHang{this};
    FLabel m_rfTalkgroupLabel{"RF TG Hangtime: ", this};
    FSpinBox m_rfTalkgroup{this};

    FCheckBox m_fixedMode{"Fixed Mode", this};
    FCheckBox m_dmrEnabled{"DMR", this};
    FCheckBox m_p25Enabled{"P25", this};
    FCheckBox m_nxdnEnabled{"NXDN", this};

    /// <summary>
    ///
    /// </summary>
    void initLayout() override
    {
        FDialog::setText("System Configuration");
        FDialog::setSize(FSize{56, 22});

        m_enableSetButton = false;
        CloseWndBase::initLayout();
    }

    /// <summary>
    ///
    /// </summary>
    void initControls() override
    {
        yaml::Node modemConfig = m_setup->m_conf["system"]["modem"];
        m_setup->m_conf["system"]["modem"]["protocol"]["type"] = std::string("uart"); // configuring modem, always sets type to UART

        yaml::Node uartConfig = modemConfig["protocol"]["uart"];

        std::string modemPort = uartConfig["port"].as<std::string>("/dev/ttyUSB0");
        uint32_t portSpeed = uartConfig["speed"].as<uint32_t>(115200);

        // port and speed
        {
            m_portAndSpeedLabel.setGeometry(FPoint(2, 1), FSize(30, 2));
            m_portAndSpeedLabel.setEmphasis();
            m_portAndSpeedLabel.setAlignment(Align::Center);

            m_modemPortLabel.setGeometry(FPoint(2, 3), FSize(20, 1));
            m_modemPort.setGeometry(FPoint(23, 3), FSize(28, 1));
            m_modemPort.setText(modemPort);
            m_modemPort.setShadow(false);
            m_modemPort.addCallback("changed", [&]() {
                m_setup->m_conf["system"]["modem"]["protocol"]["uart"]["port"] = m_modemPort.getText().toString();
            });

            m_modemSpeedLabel.setGeometry(FPoint(2, 4), FSize(20, 1));
            m_modemSpeed.setGeometry(FPoint(23, 4), FSize(10, 1));
            m_modemSpeed.setRange(1200, 460800);
            m_modemSpeed.setValue(portSpeed);
            m_modemSpeed.setShadow(false);
            m_modemSpeed.addCallback("changed", [&]() {
                m_setup->m_conf["system"]["modem"]["protocol"]["uart"]["speed"] = __INT_STR(m_modemSpeed.getValue());
            });
            m_modemSpeed.setDisable(); // don't allow this to be changed right now
        }

        std::string identity = m_setup->m_conf["system"]["identity"].as<std::string>();
        uint32_t timeout = m_setup->m_conf["system"]["timeout"].as<uint32_t>();
        bool duplex = m_setup->m_conf["system"]["duplex"].as<bool>(true);
        bool simplexSameFrequency = m_setup->m_conf["system"]["simplexSameFrequency"].as<bool>(false);
        uint32_t modeHang = m_setup->m_conf["system"]["modeHang"].as<uint32_t>();
        uint32_t rfTalkgroupHang = m_setup->m_conf["system"]["rfTalkgroupHang"].as<uint32_t>();
        bool fixedMode = m_setup->m_conf["system"]["fixedMode"].as<bool>(false);

        // system settings
        {
            m_systemSettingsLabel.setGeometry(FPoint(2, 6), FSize(30, 2));
            m_systemSettingsLabel.setEmphasis();
            m_systemSettingsLabel.setAlignment(Align::Center);

            m_identityLabel.setGeometry(FPoint(2, 8), FSize(20, 1));
            m_identity.setGeometry(FPoint(23, 8), FSize(28, 1));
            m_identity.setText(identity);
            m_identity.setShadow(false);
            m_identity.addCallback("changed", [&]() {
                m_setup->m_conf["system"]["identity"] = m_identity.getText().toString();
            });

            m_duplex.setGeometry(FPoint(2, 9), FSize(10, 1));
            m_duplex.setChecked(duplex);
            m_duplex.addCallback("toggled", [&]() {
                m_setup->m_conf["system"]["duplex"] = __BOOL_STR(m_duplex.isChecked());
            });

            m_simplexFreq.setGeometry(FPoint(15, 9), FSize(10, 1));
            m_simplexFreq.setChecked(simplexSameFrequency);
            m_simplexFreq.addCallback("toggled", [&]() {
                m_setup->m_conf["system"]["duplex"] = __BOOL_STR(m_simplexFreq.isChecked());
            });

            m_timeoutLabel.setGeometry(FPoint(2, 10), FSize(20, 1));
            m_timeout.setGeometry(FPoint(23, 10), FSize(10, 1));
            m_timeout.setValue(timeout);
            m_timeout.setMinValue(0);
            m_timeout.setShadow(false);
            m_timeout.addCallback("changed", [&]() {
                m_setup->m_conf["system"]["timeout"] = __INT_STR(m_modeHang.getValue());
            });

            m_modeHangLabel.setGeometry(FPoint(2, 11), FSize(20, 1));
            m_modeHang.setGeometry(FPoint(23, 11), FSize(10, 1));
            m_modeHang.setValue(modeHang);
            m_modeHang.setMinValue(0);
            m_modeHang.setShadow(false);
            m_modeHang.addCallback("changed", [&]() {
                m_setup->m_conf["system"]["modeHang"] = __INT_STR(m_modeHang.getValue());
            });

            m_rfTalkgroupLabel.setGeometry(FPoint(2, 12), FSize(20, 1));
            m_rfTalkgroup.setGeometry(FPoint(23, 12), FSize(10, 1));
            m_rfTalkgroup.setValue(rfTalkgroupHang);
            m_rfTalkgroup.setMinValue(0);
            m_rfTalkgroup.setShadow(false);
            m_rfTalkgroup.addCallback("changed", [&]() {
                m_setup->m_conf["system"]["rfTalkgroupHang"] = __INT_STR(m_rfTalkgroup.getValue());
            });
        }

        // mode settings
        {
            m_modeSettingsLabel.setGeometry(FPoint(2, 14), FSize(30, 2));
            m_modeSettingsLabel.setEmphasis();
            m_modeSettingsLabel.setAlignment(Align::Center);

            m_fixedMode.setGeometry(FPoint(2, 16), FSize(10, 1));
            m_fixedMode.setChecked(fixedMode);
            m_fixedMode.addCallback("toggled", [&]() {
                m_setup->m_conf["system"]["fixedMode"] = __BOOL_STR(m_fixedMode.isChecked());
            });

            bool dmrEnabled = m_setup->m_conf["protocols"]["dmr"]["enable"].as<bool>(true);

            m_dmrEnabled.setGeometry(FPoint(2, 17), FSize(10, 1));
            m_dmrEnabled.setChecked(dmrEnabled);
            m_dmrEnabled.addCallback("toggled", [&]() {
                m_setup->m_conf["protocols"]["dmr"]["enable"] = __BOOL_STR(m_dmrEnabled.isChecked());
            });

            bool p25Enabled = m_setup->m_conf["protocols"]["p25"]["enable"].as<bool>(true);

            m_p25Enabled.setGeometry(FPoint(12, 17), FSize(10, 1));
            m_p25Enabled.setChecked(p25Enabled);
            m_p25Enabled.addCallback("toggled", [&]() {
                m_setup->m_conf["protocols"]["p25"]["enable"] = __BOOL_STR(m_p25Enabled.isChecked());
            });

            bool nxdnEnabled = m_setup->m_conf["protocols"]["nxdn"]["enable"].as<bool>(true);

            m_nxdnEnabled.setGeometry(FPoint(22, 17), FSize(10, 1));
            m_nxdnEnabled.setChecked(nxdnEnabled);
            m_nxdnEnabled.addCallback("toggled", [&]() {
                m_setup->m_conf["protocols"]["nxdn"]["enable"] = __BOOL_STR(m_nxdnEnabled.isChecked());
            });
        }

        CloseWndBase::initControls();
    }
};

#endif // __SYSTEM_CONFIG_SET_WND_H__