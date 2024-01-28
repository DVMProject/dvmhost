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
#if !defined(__SITE_PARAM_SET_WND_H__)
#define __SITE_PARAM_SET_WND_H__

#include "common/Thread.h"
#include "setup/HostSetup.h"

#include "setup/CloseWndBase.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the site parameters configuration window.
// ---------------------------------------------------------------------------

class HOST_SW_API SiteParamSetWnd final : public CloseWndBase {
public:
    /// <summary>
    /// Initializes a new instance of the SiteParamSetWnd class.
    /// </summary>
    /// <param name="setup"></param>
    /// <param name="widget"></param>
    explicit SiteParamSetWnd(HostSetup* setup, FWidget* widget = nullptr) : CloseWndBase{setup, widget}
    {
        /* stub */
    }

private:
    FLabel m_cwParams{"CW Configuration", this};
    FLabel m_siteParams{"Parameters", this};

    FCheckBox m_cwEnabled{"Enabled", this};
    FLabel m_cwCallsignLabel{"Callsign: ", this};
    FLineEdit m_cwCallsign{this};
    FLabel m_cwTimeLabel{"CW Interval: ", this};
    FSpinBox m_cwTime{this};

    FLabel m_dmrColorCodeLabel{"DMR CC: ", this};
    FSpinBox m_dmrColorCode{this};
    FLabel m_p25NACLabel{"P25 NAC: ", this};
    FLineEdit m_p25NAC{this};
    FLabel m_nxdnRANLabel{"NXDN RAN: ", this};
    FSpinBox m_nxdnRAN{this};

    FLabel m_siteIdLabel{"Site ID: ", this};
    FLineEdit m_siteId{this};
    FLabel m_dmrNetIdLabel{"DMR Net. ID: ", this};
    FLineEdit m_dmrNetId{this};
    FLabel m_p25NetIdLabel{"P25 Net. ID: ", this};
    FLineEdit m_p25NetId{this};
    FLabel m_p25SysIdLabel{"P25 System ID: ", this};
    FLineEdit m_p25SysId{this};
    FLabel m_p25RfssIdLabel{"P25 RFSS ID: ", this};
    FLineEdit m_p25RfssId{this};

    /// <summary>
    ///
    /// </summary>
    void initLayout() override
    {
        FDialog::setText("Site Parameters");
        FDialog::setSize(FSize{63, 20});

        m_enableSetButton = false;
        CloseWndBase::initLayout();
    }

    /// <summary>
    ///
    /// </summary>
    void initControls() override
    {
        yaml::Node cwId = m_setup->m_conf["system"]["cwId"];
        bool enabled = cwId["enable"].as<bool>(false);
        uint32_t cwTime = cwId["time"].as<uint32_t>(10U);
        std::string callsign = cwId["callsign"].as<std::string>();

        // cw configuration
        {
            m_cwParams.setGeometry(FPoint(2, 1), FSize(30, 2));
            m_cwParams.setEmphasis();
            m_cwParams.setAlignment(Align::Center);

            m_cwEnabled.setGeometry(FPoint(2, 3), FSize(10, 1));
            m_cwEnabled.setChecked(enabled);
            m_cwEnabled.addCallback("toggled", [&]() {
                m_setup->m_conf["system"]["cwId"]["enable"] = __BOOL_STR(m_cwEnabled.isChecked());
            });

            m_cwCallsignLabel.setGeometry(FPoint(2, 4), FSize(20, 1));
            m_cwCallsign.setGeometry(FPoint(23, 4), FSize(28, 1));
            m_cwCallsign.setText(callsign);
            m_cwCallsign.setShadow(false);
            m_cwCallsign.addCallback("changed", [&]() {
                m_setup->m_conf["system"]["cwId"]["callsign"] = m_cwCallsign.getText().toString();
            });

            m_cwTimeLabel.setGeometry(FPoint(2, 5), FSize(20, 1));
            m_cwTime.setGeometry(FPoint(23, 5), FSize(10, 1));
            m_cwTime.setValue(cwTime);
            m_cwTime.setMinValue(0);
            m_cwTime.setShadow(false);
            m_cwTime.addCallback("changed", [&]() {
                m_setup->m_conf["system"]["cwId"]["time"] = __INT_STR(m_cwTime.getValue());
            });
        }

        yaml::Node rfssConfig = m_setup->m_conf["system"]["config"];

        // site parameters
        {
            m_siteParams.setGeometry(FPoint(2, 7), FSize(30, 2));
            m_siteParams.setEmphasis();
            m_siteParams.setAlignment(Align::Center);

            uint32_t dmrColorCode = rfssConfig["colorCode"].as<uint32_t>(2U);
            m_dmrColorCodeLabel.setGeometry(FPoint(2, 9), FSize(8, 1));
            m_dmrColorCode.setGeometry(FPoint(12, 9), FSize(8, 1));
            m_dmrColorCode.setValue(dmrColorCode);
            m_dmrColorCode.setRange(0, 15);
            m_dmrColorCode.setShadow(false);
            m_dmrColorCode.addCallback("changed", [&]() {
                m_setup->m_conf["system"]["config"]["colorCode"] = __INT_STR(m_dmrColorCode.getValue());
            });

            m_p25NACLabel.setGeometry(FPoint(23, 9), FSize(10, 1));
            m_p25NAC.setGeometry(FPoint(33, 9), FSize(8, 1));
            m_p25NAC.setText(m_setup->m_conf["system"]["config"]["nac"].as<std::string>("1").c_str());
            m_p25NAC.setShadow(false);
            m_p25NAC.setMaxLength(3);
            m_p25NAC.setInputFilter("[[:xdigit:]]");
            m_p25NAC.addCallback("changed", [&]() {
                uint32_t nac = (uint32_t)::strtoul(std::string(m_p25NAC.getText().toString()).c_str(), NULL, 16);
                nac = p25::P25Utils::nac(nac);

                m_setup->m_conf["system"]["config"]["nac"] = __INT_HEX_STR(nac);
            });

            uint32_t nxdnRAN = rfssConfig["ran"].as<uint32_t>(1U);
            m_nxdnRANLabel.setGeometry(FPoint(42, 9), FSize(10, 1));
            m_nxdnRAN.setGeometry(FPoint(53, 9), FSize(8, 1));
            m_nxdnRAN.setValue(nxdnRAN);
            m_nxdnRAN.setRange(0, 15);
            m_nxdnRAN.setShadow(false);
            m_nxdnRAN.addCallback("changed", [&]() {
                m_setup->m_conf["system"]["config"]["ran"] = __INT_STR(m_nxdnRAN.getValue());
            });

            m_siteIdLabel.setGeometry(FPoint(2, 10), FSize(20, 1));
            m_siteId.setGeometry(FPoint(23, 10), FSize(10, 1));
            m_siteId.setText(rfssConfig["siteId"].as<std::string>("1").c_str());
            m_siteId.setShadow(false);
            m_siteId.setMaxLength(3);
            m_siteId.setInputFilter("[[:xdigit:]]");
            m_siteId.addCallback("changed", [&]() {
                uint32_t id = (uint32_t)::strtoul(std::string(m_siteId.getText().toString()).c_str(), NULL, 16);
                id = p25::P25Utils::siteId(id);

                m_setup->m_conf["system"]["config"]["siteId"] = __INT_HEX_STR(id);
            });

            m_dmrNetIdLabel.setGeometry(FPoint(2, 11), FSize(20, 1));
            m_dmrNetId.setGeometry(FPoint(23, 11), FSize(10, 1));
            m_dmrNetId.setText(rfssConfig["dmrNetId"].as<std::string>("1").c_str());
            m_dmrNetId.setShadow(false);
            m_dmrNetId.setMaxLength(6);
            m_dmrNetId.setInputFilter("[[:xdigit:]]");
            m_dmrNetId.addCallback("changed", [&]() {
                uint32_t id = (uint32_t)::strtoul(std::string(m_dmrNetId.getText().toString()).c_str(), NULL, 16);
                id = dmr::DMRUtils::netId(id, dmr::SITE_MODEL_TINY);

                m_setup->m_conf["system"]["config"]["dmrNetId"] = __INT_HEX_STR(id);
            });

            m_p25NetIdLabel.setGeometry(FPoint(2, 12), FSize(20, 1));
            m_p25NetId.setGeometry(FPoint(23, 12), FSize(10, 1));
            m_p25NetId.setText(rfssConfig["netId"].as<std::string>("1").c_str());
            m_p25NetId.setShadow(false);
            m_p25NetId.setMaxLength(6);
            m_p25NetId.setInputFilter("[[:xdigit:]]");
            m_p25NetId.addCallback("changed", [&]() {
                uint32_t id = (uint32_t)::strtoul(std::string(m_p25NetId.getText().toString()).c_str(), NULL, 16);
                id = p25::P25Utils::netId(id);

                m_setup->m_conf["system"]["config"]["netId"] = __INT_HEX_STR(id);
            });

            m_p25SysIdLabel.setGeometry(FPoint(2, 13), FSize(20, 1));
            m_p25SysId.setGeometry(FPoint(23, 13), FSize(10, 1));
            m_p25SysId.setText(rfssConfig["sysId"].as<std::string>("1").c_str());
            m_p25SysId.setShadow(false);
            m_p25SysId.setMaxLength(4);
            m_p25SysId.setInputFilter("[[:xdigit:]]");
            m_p25SysId.addCallback("changed", [&]() {
                uint32_t id = (uint32_t)::strtoul(std::string(m_p25SysId.getText().toString()).c_str(), NULL, 16);
                id = p25::P25Utils::sysId(id);

                m_setup->m_conf["system"]["config"]["sysId"] = __INT_HEX_STR(id);
            });

            m_p25RfssIdLabel.setGeometry(FPoint(2, 14), FSize(20, 1));
            m_p25RfssId.setGeometry(FPoint(23, 14), FSize(10, 1));
            m_p25RfssId.setText(rfssConfig["dmrNetId"].as<std::string>("1").c_str());
            m_p25RfssId.setShadow(false);
            m_p25RfssId.setMaxLength(3);
            m_p25RfssId.setInputFilter("[[:xdigit:]]");
            m_p25RfssId.addCallback("changed", [&]() {
                uint32_t id = (uint8_t)::strtoul(std::string(m_p25RfssId.getText().toString()).c_str(), NULL, 16);
                id = p25::P25Utils::rfssId(id);

                m_setup->m_conf["system"]["config"]["rfssId"] = __INT_HEX_STR(id);
            });
        }

        CloseWndBase::initControls();
    }
};

#endif // __SITE_PARAM_SET_WND_H__