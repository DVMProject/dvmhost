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
#if !defined(__LOGGING_AND_DATA_SET_WND_H__)
#define __LOGGING_AND_DATA_SET_WND_H__

#include "common/Thread.h"
#include "setup/HostSetup.h"

#include "setup/CloseWndBase.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the logging and data configuration window.
// ---------------------------------------------------------------------------

class HOST_SW_API LoggingAndDataSetWnd final : public CloseWndBase {
public:
    /// <summary>
    /// Initializes a new instance of the LoggingAndDataSetWnd class.
    /// </summary>
    /// <param name="setup"></param>
    /// <param name="widget"></param>
    explicit LoggingAndDataSetWnd(HostSetup* setup, FWidget* widget = nullptr) : CloseWndBase{setup, widget}
    {
        /* stub */
    }

private:
    FLabel m_loggingLabel{"Logging", this};
    FLabel m_dataLabel{"Data Paths", this};

    FLabel m_logFilePathLabel{"Log File Path: ", this};
    FLineEdit m_logFilePath{this};
    FLabel m_actFilePathLabel{"Activity File Path: ", this};
    FLineEdit m_actFilePath{this};
    FLabel m_logLevelLabel{"Logging Level (1-6 lowest): ", this};
    FSpinBox m_logLevel{this};

    FLabel m_chIdTablePathLabel{"Ch. Identity Table File Path: ", this};
    FLineEdit m_chIdTablePath{this};
    FLabel m_radioIdPathLabel{"Radio ID ACL File Path: ", this};
    FLineEdit m_radioIdPath{this};
    FLabel m_tgIdPathLabel{"Talkgroup ACL File Path: ", this};
    FLineEdit m_tgIdPath{this};

    /// <summary>
    ///
    /// </summary>
    void initLayout() override
    {
        FDialog::setText("Logging and Data Configuration");
        FDialog::setSize(FSize{68, 19});

        m_enableSetButton = false;
        CloseWndBase::initLayout();
    }

    /// <summary>
    ///
    /// </summary>
    void initControls() override
    {
        yaml::Node logConf = m_setup->m_conf["log"];
        uint32_t logLevel = logConf["fileLevel"].as<uint32_t>(1U);
        std::string logFilePath = logConf["filePath"].as<std::string>();
        std::string logActFilePath = logConf["activityFilePath"].as<std::string>();

        // logging
        {
            m_loggingLabel.setGeometry(FPoint(2, 1), FSize(20, 2));
            m_loggingLabel.setEmphasis();
            m_loggingLabel.setAlignment(Align::Center);

            m_logFilePathLabel.setGeometry(FPoint(2, 3), FSize(30, 1));
            m_logFilePath.setGeometry(FPoint(33, 3), FSize(32, 1));
            m_logFilePath.setText(logFilePath);
            m_logFilePath.setShadow(false);
            m_logFilePath.addCallback("changed", [&]() {
                m_setup->m_conf["log"]["filePath"] = m_logFilePath.getText().toString();
            });

            m_actFilePathLabel.setGeometry(FPoint(2, 4), FSize(30, 1));
            m_actFilePath.setGeometry(FPoint(33, 4), FSize(32, 1));
            m_actFilePath.setText(logActFilePath);
            m_actFilePath.setShadow(false);
            m_actFilePath.addCallback("changed", [&]() {
                m_setup->m_conf["log"]["activityFilePath"] = m_actFilePath.getText().toString();
            });

            m_logLevelLabel.setGeometry(FPoint(2, 5), FSize(30, 1));
            m_logLevel.setGeometry(FPoint(33, 5), FSize(10, 1));
            m_logLevel.setRange(1, 6);
            m_logLevel.setValue(logLevel);
            m_logLevel.setShadow(false);
            m_logLevel.addCallback("changed", [&]() {
                m_setup->m_conf["log"]["displayLevel"] = __INT_STR(m_logLevel.getValue());
                m_setup->m_conf["log"]["fileLevel"] = __INT_STR(m_logLevel.getValue());
            });
        }

        yaml::Node idenTable = m_setup->m_conf["system"]["iden_table"];
        std::string idenFilePath = idenTable["file"].as<std::string>();
        yaml::Node radioId = m_setup->m_conf["system"]["radio_id"];
        std::string ridFilePath = radioId["file"].as<std::string>();
        yaml::Node talkgroupId = m_setup->m_conf["system"]["talkgroup_id"];
        std::string tgidFilePath = talkgroupId["file"].as<std::string>();

        // data paths
        {
            m_dataLabel.setGeometry(FPoint(2, 7), FSize(20, 2));
            m_dataLabel.setEmphasis();
            m_dataLabel.setAlignment(Align::Center);

            m_chIdTablePathLabel.setGeometry(FPoint(2, 9), FSize(30, 1));
            m_chIdTablePath.setGeometry(FPoint(33, 9), FSize(32, 1));
            m_chIdTablePath.setText(idenFilePath);
            m_chIdTablePath.setShadow(false);
            m_chIdTablePath.addCallback("changed", [&]() {
                m_setup->m_conf["system"]["iden_table"]["file"] = m_chIdTablePath.getText().toString();
            });

            m_radioIdPathLabel.setGeometry(FPoint(2, 10), FSize(30, 1));
            m_radioIdPath.setGeometry(FPoint(33, 10), FSize(32, 1));
            m_radioIdPath.setText(ridFilePath);
            m_radioIdPath.setShadow(false);
            m_radioIdPath.addCallback("changed", [&]() {
                m_setup->m_conf["system"]["radio_id"]["file"] = m_radioIdPath.getText().toString();
            });

            m_tgIdPathLabel.setGeometry(FPoint(2, 11), FSize(30, 1));
            m_tgIdPath.setGeometry(FPoint(33, 11), FSize(32, 1));
            m_tgIdPath.setText(tgidFilePath);
            m_tgIdPath.setShadow(false);
            m_tgIdPath.addCallback("changed", [&]() {
                m_setup->m_conf["system"]["talkgroup_id"]["file"] = m_tgIdPath.getText().toString();
            });
        }

        CloseWndBase::initControls();
    }
};

#endif // __LOGGING_AND_DATA_SET_WND_H__