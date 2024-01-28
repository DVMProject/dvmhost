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
#if !defined(__SETUP_WND_H__)
#define __SETUP_WND_H__

#include "common/Log.h"
#include "common/Thread.h"
#include "modem/Modem.h"
#include "setup/HostSetup.h"

using namespace modem;

#include "setup/LogDisplayWnd.h"
#include "setup/ModemStatusWnd.h"
#include "setup/BERDisplayWnd.h"

#include "setup/LevelAdjustWnd.h"
#include "setup/SymbLevelAdjustWnd.h"
#include "setup/HSBandwidthAdjustWnd.h"
#include "setup/HSGainAdjustWnd.h"
#include "setup/FIFOBufferAdjustWnd.h"

#include "setup/LoggingAndDataSetWnd.h"
#include "setup/SystemConfigSetWnd.h"
#include "setup/SiteParamSetWnd.h"
#include "setup/ChannelConfigSetWnd.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

class HOST_SW_API SetupApplication;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the root window control.
// ---------------------------------------------------------------------------

class HOST_SW_API SetupMainWnd final : public finalcut::FWidget {
public:
    /// <summary>
    /// Initializes a new instance of the SetupMainWnd class.
    /// </summary>
    /// <param name="setup"></param>
    /// <param name="widget"></param>
    explicit SetupMainWnd(HostSetup* setup, FWidget* widget = nullptr) : FWidget{widget},
        m_setup(setup)
    {
        __InternalOutputStream(m_logWnd);
        m_statusWnd.hide();

        resetBERWnd();

        // file menu
        m_fileMenuSeparator1.setSeparator();
        m_fileMenuSeparator2.setSeparator();
        m_connectToModemItem.addAccelerator(FKey::Meta_c); // Meta/Alt + C
        m_connectToModemItem.addCallback("clicked", this, &SetupMainWnd::cb_connectToModemClick);
        m_keyF8.addCallback("activate", this, &SetupMainWnd::cb_connectToModemClick);
        m_saveSettingsItem.addAccelerator(FKey::Meta_s); // Meta/Alt + S
        m_saveSettingsItem.addCallback("clicked", this, [&]() { m_setup->saveConfig(); });
        m_keyF2.addCallback("activate", this, [&]() { m_setup->saveConfig(); });
        m_quitItem.addAccelerator(FKey::Meta_x); // Meta/Alt + X
        m_quitItem.addCallback("clicked", getFApplication(), &FApplication::cb_exitApp, this);
        m_keyF3.addCallback("activate", getFApplication(), &FApplication::cb_exitApp, this);
        m_keyF12.addCallback("activate", this, [&]() {
            if (m_setup->m_isConnected) {
                if (!m_setup->setTransmit()) {
                    FMessageBox::error(this, "Failed to enable modem transmit!");
                }
            }
        });

        // setup menu
        m_setupMenuSeparator1.setSeparator();
        m_setupMenuSeparator2.setSeparator();
        m_setLoggingDataConfig.addCallback("clicked", this, [&]() {
            FMessageBox::error(this, "NOTE: These settings will take effect on restart of dvmhost.");
            LoggingAndDataSetWnd wnd{m_setup, this};
            wnd.show();
        });
        m_systemConfig.addCallback("clicked", this, [&]() {
            SystemConfigSetWnd wnd{m_setup, this};
            wnd.show();
        });
        m_siteParams.addCallback("clicked", this, [&]() {
            SiteParamSetWnd wnd{m_setup, this};
            wnd.show();
        });
        m_chConfig.addCallback("clicked", this, [&]() {
            ChannelConfigSetWnd wnd{m_setup, this};
            wnd.show();
        });

        // calibrate menu
        m_calibrateMenuSeparator1.setSeparator();
        m_calibrateMenuSeparator2.setSeparator();
        m_calibrateMenuSeparator3.setSeparator();
        m_dmrCal.addCallback("toggled", [&]() {
            m_setup->m_mode = STATE_DMR_CAL;
            m_setup->m_modeStr = DMR_CAL_STR;
            m_setup->m_duplex = true;
            m_setup->m_dmrEnabled = false;
            m_setup->m_dmrRx1K = false;
            m_setup->m_p25Enabled = false;
            m_setup->m_p25Rx1K = false;
            m_setup->m_p25TduTest = false;
            m_setup->m_nxdnEnabled = false;

            updateDuplexState();
            resetBERWnd();

            LogMessage(LOG_CAL, " - %s", m_setup->m_modeStr.c_str());
            m_setup->writeConfig();
        });
        m_p25Cal.addCallback("toggled", [&]() {
            m_setup->m_mode = STATE_P25_CAL;
            m_setup->m_modeStr = P25_CAL_STR;
            m_setup->m_duplex = true;
            m_setup->m_dmrEnabled = false;
            m_setup->m_dmrRx1K = false;
            m_setup->m_p25Enabled = false;
            m_setup->m_p25Rx1K = false;
            m_setup->m_p25TduTest = false;
            m_setup->m_nxdnEnabled = false;

            updateDuplexState();
            resetBERWnd();

            LogMessage(LOG_CAL, " - %s", m_setup->m_modeStr.c_str());
            m_setup->writeConfig();
        });
        m_dmrLFCal.addCallback("toggled", [&]() {
            m_setup->m_mode = STATE_DMR_LF_CAL;
            m_setup->m_modeStr = DMR_LF_CAL_STR;
            m_setup->m_duplex = true;
            m_setup->m_dmrEnabled = false;
            m_setup->m_dmrRx1K = false;
            m_setup->m_p25Enabled = false;
            m_setup->m_p25Rx1K = false;
            m_setup->m_p25TduTest = false;
            m_setup->m_nxdnEnabled = false;

            updateDuplexState();
            resetBERWnd();

            LogMessage(LOG_CAL, " - %s", m_setup->m_modeStr.c_str());
            m_setup->writeConfig();
        });
        m_dmrCal1K.addCallback("toggled", [&]() {
            m_setup->m_mode = STATE_DMR_CAL_1K;
            m_setup->m_modeStr = DMR_CAL_1K_STR;
            m_setup->m_duplex = true;
            m_setup->m_dmrEnabled = false;
            m_setup->m_dmrRx1K = false;
            m_setup->m_p25Enabled = false;
            m_setup->m_p25Rx1K = false;
            m_setup->m_p25TduTest = false;
            m_setup->m_nxdnEnabled = false;

            updateDuplexState();
            resetBERWnd();

            LogMessage(LOG_CAL, " - %s", m_setup->m_modeStr.c_str());
            m_setup->writeConfig();
        });
        m_dmrDMOCal1K.addCallback("toggled", [&]() {
            m_setup->m_mode = STATE_DMR_DMO_CAL_1K;
            m_setup->m_modeStr = DMR_DMO_CAL_1K_STR;
            m_setup->m_duplex = true;
            m_setup->m_dmrEnabled = false;
            m_setup->m_dmrRx1K = false;
            m_setup->m_p25Enabled = false;
            m_setup->m_p25Rx1K = false;
            m_setup->m_p25TduTest = false;
            m_setup->m_nxdnEnabled = false;

            updateDuplexState();
            resetBERWnd();

            LogMessage(LOG_CAL, " - %s", m_setup->m_modeStr.c_str());
            m_setup->writeConfig();
        });
        m_p25Cal1K.addCallback("toggled", [&]() {
            m_setup->m_mode = STATE_P25_CAL_1K;
            m_setup->m_modeStr = P25_CAL_1K_STR;
            m_setup->m_duplex = true;
            m_setup->m_dmrEnabled = false;
            m_setup->m_dmrRx1K = false;
            m_setup->m_p25Enabled = false;
            m_setup->m_p25Rx1K = false;
            m_setup->m_p25TduTest = false;
            m_setup->m_nxdnEnabled = false;

            updateDuplexState();
            resetBERWnd();

            LogMessage(LOG_CAL, " - %s", m_setup->m_modeStr.c_str());
            m_setup->writeConfig();
        });
        m_p25TDUTest.addCallback("toggled", [&]() {
            m_setup->m_mode = STATE_P25;
            m_setup->m_modeStr = P25_TDU_TEST_STR;
            m_setup->m_duplex = true;
            m_setup->m_dmrEnabled = false;
            m_setup->m_p25Enabled = true;
            m_setup->m_p25TduTest = true;
            m_setup->m_nxdnEnabled = false;

            updateDuplexState();
            resetBERWnd();

            m_setup->m_queue.clear();

            LogMessage(LOG_CAL, " - %s", m_setup->m_modeStr.c_str());
            m_setup->writeConfig();
        });
        m_nxdnCal1K.addCallback("toggled", [&]() {
            // are we on a protocol version 3 firmware?
            if (m_setup->m_modem->getVersion() >= 3U) {
                m_setup->m_mode = STATE_NXDN_CAL;
                m_setup->m_modeStr = NXDN_CAL_1K_STR;
                m_setup->m_duplex = true;
                m_setup->m_dmrEnabled = false;
                m_setup->m_dmrRx1K = false;
                m_setup->m_p25Enabled = false;
                m_setup->m_p25Rx1K = false;
                m_setup->m_p25TduTest = false;
                m_setup->m_nxdnEnabled = false;

                updateDuplexState();
                resetBERWnd();

                LogMessage(LOG_CAL, " - %s", m_setup->m_modeStr.c_str());
                m_setup->writeConfig();
            }
            else {
                FMessageBox::error(this, NXDN_CAL_1K_STR " test mode is not supported on your firmware!");
            }
        });
        m_dmrFEC.addCallback("toggled", [&]() {
            m_setup->m_mode = STATE_DMR;
            m_setup->m_modeStr = DMR_FEC_STR;
            m_setup->m_dmrRx1K = false;
            m_setup->m_duplex = m_setup->m_startupDuplex;
            m_setup->m_dmrEnabled = true;
            m_setup->m_p25Enabled = false;
            m_setup->m_nxdnEnabled = false;

            updateDuplexState();
            resetBERWnd(true);

            LogMessage(LOG_CAL, " - %s", m_setup->m_modeStr.c_str());
            m_setup->writeConfig();
        });
        m_dmrFEC1K.addCallback("toggled", [&]() {
            m_setup->m_mode = STATE_DMR;
            m_setup->m_modeStr = DMR_FEC_1K_STR;
            m_setup->m_dmrRx1K = true;
            m_setup->m_duplex = m_setup->m_startupDuplex;
            m_setup->m_dmrEnabled = true;
            m_setup->m_p25Enabled = false;
            m_setup->m_nxdnEnabled = false;

            updateDuplexState();
            resetBERWnd(true);

            LogMessage(LOG_CAL, " - %s", m_setup->m_modeStr.c_str());
            m_setup->writeConfig();
        });
        m_p25FEC.addCallback("toggled", [&]() {
            m_setup->m_mode = STATE_P25;
            m_setup->m_modeStr = P25_FEC_STR;
            m_setup->m_p25Rx1K = false;
            m_setup->m_duplex = m_setup->m_startupDuplex;
            m_setup->m_dmrEnabled = false;
            m_setup->m_p25Enabled = true;
            m_setup->m_p25TduTest = false;
            m_setup->m_nxdnEnabled = false;

            updateDuplexState();
            resetBERWnd(true);

            LogMessage(LOG_CAL, " - %s", m_setup->m_modeStr.c_str());
            m_setup->writeConfig();
        });
        m_p25FEC1K.addCallback("toggled", [&]() {
            m_setup->m_mode = STATE_P25;
            m_setup->m_modeStr = P25_FEC_1K_STR;
            m_setup->m_p25Rx1K = true;
            m_setup->m_duplex = m_setup->m_startupDuplex;
            m_setup->m_dmrEnabled = false;
            m_setup->m_p25Enabled = true;
            m_setup->m_p25TduTest = false;
            m_setup->m_nxdnEnabled = false;

            updateDuplexState();
            resetBERWnd(true);

            LogMessage(LOG_CAL, " - %s", m_setup->m_modeStr.c_str());
            m_setup->writeConfig();
        });
        m_nxdnFEC.addCallback("toggled", [&]() {
            // are we on a protocol version 3 firmware?
            if (m_setup->m_modem->getVersion() >= 3U) {
                m_setup->m_mode = STATE_NXDN;
                m_setup->m_modeStr = NXDN_FEC_STR;
                m_setup->m_duplex = m_setup->m_startupDuplex;
                m_setup->m_dmrEnabled = false;
                m_setup->m_p25Enabled = false;
                m_setup->m_p25Rx1K = false;
                m_setup->m_p25TduTest = false;
                m_setup->m_nxdnEnabled = true;

                updateDuplexState();
                resetBERWnd(true);

                LogMessage(LOG_CAL, " - %s", m_setup->m_modeStr.c_str());
                m_setup->writeConfig();
            }
            else {
                FMessageBox::error(this, NXDN_FEC_STR " test mode is not supported on your firmware!");
            }
        });
        m_rssiCal.addCallback("toggled", [&]() {
            m_setup->m_mode = STATE_RSSI_CAL;
            m_setup->m_modeStr = RSSI_CAL_STR;
            m_setup->m_duplex = true;
            m_setup->m_dmrEnabled = false;
            m_setup->m_dmrRx1K = false;
            m_setup->m_p25Enabled = false;
            m_setup->m_p25Rx1K = false;
            m_setup->m_p25TduTest = false;

            resetBERWnd();

            LogMessage(LOG_CAL, " - %s", m_setup->m_modeStr.c_str());
            m_setup->writeConfig();
        });

        m_toggleTxInvert.addCallback("toggled", this, [&]() {
            if (!m_setup->m_isHotspot) {
                m_setup->m_modem->m_txInvert = !m_setup->m_modem->m_txInvert;
                LogMessage(LOG_CAL, "Tx Invert: %s", m_setup->m_modem->m_txInvert ? "on" : "off");
                m_setup->writeConfig();
            }
        });
        m_toggleRxInvert.addCallback("toggled", this, [&]() {
            if (!m_setup->m_isHotspot) {
                m_setup->m_modem->m_rxInvert = !m_setup->m_modem->m_rxInvert;
                LogMessage(LOG_CAL, "Rx Invert: %s", m_setup->m_modem->m_rxInvert ? "on" : "off");
                m_setup->writeConfig();
            }
        });
        m_togglePTTInvert.addCallback("toggled", this, [&]() {
            if (!m_setup->m_isHotspot) {
                m_setup->m_modem->m_pttInvert = !m_setup->m_modem->m_pttInvert;
                LogMessage(LOG_CAL, "PTT Invert: %s", m_setup->m_modem->m_pttInvert ? "on" : "off");
                m_setup->writeConfig();
            }
        });
        m_toggleDCBlocker.addCallback("toggled", this, [&]() {
            if (!m_setup->m_isHotspot) {
                m_setup->m_modem->m_dcBlocker = !m_setup->m_modem->m_dcBlocker;
                LogMessage(LOG_CAL, "DC Blocker: %s", m_setup->m_modem->m_dcBlocker ? "on" : "off");
                m_setup->writeConfig();
            }
        });
        m_toggleDuplex.addCallback("toggled", this, [&]() {
            if (m_setup->m_isHotspot && m_setup->m_isConnected) {
                m_setup->m_duplex = !m_setup->m_duplex;
                LogMessage(LOG_CAL, "Hotspot Rx: %s", m_setup->m_duplex ? "Rx Antenna" : "Tx Antenna");
                m_setup->writeConfig();
            }
        });

        m_adjustLevel.addAccelerator(FKey::Meta_l); // Meta/Alt + L
        m_adjustLevel.addCallback("clicked", this, [&]() {
            LevelAdjustWnd wnd{m_setup, this};
            wnd.show();
        });
        m_keyF5.addCallback("activate", this, [&]() {
            LevelAdjustWnd wnd{m_setup, this};
            wnd.show();
        });

        // engineering menu
        m_engineeringMenuSeparator1.setSeparator();
        m_engineeringMenuSeparator2.setSeparator();
        m_engineeringMenuSeparator3.setSeparator();
        m_adjSymLevel.addAccelerator(FKey::Meta_s); // Meta/Alt + S
        m_adjSymLevel.addCallback("clicked", this, [&]() {
            SymbLevelAdjustWnd wnd{m_setup, this};
            wnd.show();
        });

        m_adjFifoBuffers.addAccelerator(FKey::Meta_f); // Meta/Alt + F
        m_adjFifoBuffers.addCallback("clicked", this, [&]() {
            FIFOBufferAdjustWnd wnd{m_setup, this};
            wnd.show();
        });

        m_adjHSBandwidth.addAccelerator(FKey::Meta_b); // Meta/Alt + B
        m_adjHSBandwidth.addCallback("clicked", this, [&]() {
            HSBandwidthAdjustWnd wnd{m_setup, this};
            wnd.show();
        });
        m_adjHSGain.addAccelerator(FKey::Meta_g); // Meta/Alt + G
        m_adjHSGain.addCallback("clicked", this, [&]() {
            HSGainAdjustWnd wnd{m_setup, this};
            wnd.show();
        });

        m_eraseConfigArea.addCallback("clicked", this, [&]() {
            FMessageBox wait("", L"Wait...",
                FMessageBox::ButtonType::Reject, FMessageBox::ButtonType::Reject, FMessageBox::ButtonType::Reject, this);
            wait.setCenterText();
            wait.setModal(false);
            wait.show();

            m_setup->eraseFlash();
            wait.hide();
        });
        m_readConfigArea.addCallback("clicked", this, [&]() {
            FMessageBox wait("", L"Wait...",
                FMessageBox::ButtonType::Reject, FMessageBox::ButtonType::Reject, FMessageBox::ButtonType::Reject, this);
            wait.setCenterText();
            wait.setModal(false);
            wait.show();

            m_setup->m_updateConfigFromModem = true;
            m_setup->readFlash();
            wait.hide();
        });

        m_forceHotspot.addCallback("toggled", this, [&]() {
            if (m_setup->m_isConnected && !m_setup->m_isHotspot && m_forceHotspot.isChecked()) {
                FMessageBox::error(this, "NOTICE: Enabling hotspot options on non-hotspot modems may result in undesired operation.");
            }
            m_setup->m_isHotspot = m_forceHotspot.isChecked();
            m_setup->m_modem->m_forceHotspot = m_forceHotspot.isChecked();
            m_setup->m_modem->m_isHotspot = m_forceHotspot.isChecked();
            setMenuStates();
        });
        m_modemDebug.addCallback("toggled", this, [&]() {
            m_setup->m_modem->m_debug = m_modemDebug.isChecked();
            m_setup->m_debug = m_modemDebug.isChecked();
            m_setup->writeConfig();
        });

        // help menu
        m_aboutItem.addCallback("clicked", this, [&]() {
            const FString line(2, UniChar::BoxDrawingsHorizontal);
            FMessageBox info("About", line + __PROG_NAME__ + line + L"\n\n"
                L"Version " + __VER__ + L"\n\n"
                L"Copyright (c) 2017-2024 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors." + L"\n"
                L"Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others", 
                FMessageBox::ButtonType::Ok, FMessageBox::ButtonType::Reject, FMessageBox::ButtonType::Reject, this);
            info.setCenterText();
            info.show();
        });
    }

    /// <summary>
    /// Helper to set menu states.
    /// </summary>
    void setMenuStates() 
    {
        m_dmrCal.setChecked();
        if (m_setup->m_modem->m_debug) {
            m_modemDebug.setChecked();
        }

        if (m_setup->m_modem->m_txInvert) {
            m_toggleTxInvert.setChecked();
        }

        if (m_setup->m_modem->m_rxInvert) {
            m_toggleRxInvert.setChecked();
        }

        if (m_setup->m_modem->m_pttInvert) {
            m_togglePTTInvert.setChecked();
        }

        if (m_setup->m_modem->m_dcBlocker) {
            m_toggleDCBlocker.setChecked();
        }

        updateDuplexState();
        updateMenuStates();
    }

    /// <summary>
    /// Helper to update duplex toggle menu state.
    /// </summary>
    void updateDuplexState()
    {
        if (m_setup->m_duplex) {
            m_toggleDuplex.setChecked();
        }
    }

    /// <summary>
    /// Helper to update menu states.
    /// </summary>
    void updateMenuStates()
    {
        if (!m_setup->m_isConnected) {
            m_eraseConfigArea.setDisable();
            m_readConfigArea.setDisable();
        }
        else {
            m_eraseConfigArea.setEnable();
            m_readConfigArea.setEnable();
        }

        if (m_setup->m_isConnected) {
            if (m_setup->m_isHotspot) {
                m_toggleTxInvert.setDisable();
                m_toggleRxInvert.setDisable();
                m_togglePTTInvert.setDisable();
                m_toggleDCBlocker.setDisable();

                m_toggleDuplex.setEnable();

                m_adjSymLevel.setDisable();
                m_adjHSBandwidth.setEnable();
                m_adjHSGain.setEnable();
            }
            else {
                m_toggleTxInvert.setEnable();
                m_toggleRxInvert.setEnable();
                m_togglePTTInvert.setEnable();
                m_toggleDCBlocker.setEnable();

                m_toggleDuplex.setDisable();

                m_adjSymLevel.setEnable();
                m_adjHSBandwidth.setDisable();
                m_adjHSGain.setDisable();
            }
        }
    }

    /// <summary>Gets the instance of HostSetup.</summary>
    const HostSetup* setup() { return m_setup; };

private:
    friend class HostSetup;
    friend class SetupApplication;
    HostSetup* m_setup;

    LogDisplayWnd m_logWnd{this};
    ModemStatusWnd m_statusWnd{this};
    BERDisplayWnd m_berWnd{this};

    FString m_line{13, UniChar::BoxDrawingsHorizontal};

    FMenuBar m_menuBar{this};

    FMenu m_fileMenu{"&File", &m_menuBar};
    FMenuItem m_connectToModemItem{"&Connect to Modem", &m_fileMenu};
    FMenuItem m_fileMenuSeparator1{&m_fileMenu};
    FMenuItem m_saveSettingsItem{"&Save Settings", &m_fileMenu};
    FCheckMenuItem m_saveOnCloseToggle{"Save on Close?", &m_fileMenu};
    FMenuItem m_fileMenuSeparator2{&m_fileMenu};
    FMenuItem m_quitItem{"&Quit", &m_fileMenu};

    FMenu m_setupMenu{"&Setup", &m_menuBar};
    FMenuItem m_setLoggingDataConfig{"&Logging & Data Configuration", &m_setupMenu};
    FMenuItem m_setupMenuSeparator1{&m_setupMenu};
    FMenuItem m_systemConfig{"&System Configuration", &m_setupMenu};
    FMenuItem m_siteParams{"Site &Parameters", &m_setupMenu};
    FMenuItem m_setupMenuSeparator2{&m_setupMenu};
    FMenuItem m_chConfig{"C&hannel Configuration", &m_setupMenu};

    FMenu m_calibrateMenu{"&Calibrate", &m_menuBar};
    FMenu m_opMode{"Operational Mode", &m_calibrateMenu};
    FRadioMenuItem m_dmrCal{DMR_CAL_STR, &m_opMode};
    FRadioMenuItem m_p25Cal{P25_CAL_STR, &m_opMode};
    FRadioMenuItem m_dmrLFCal{DMR_LF_CAL_STR, &m_opMode};
    FRadioMenuItem m_dmrCal1K{DMR_CAL_1K_STR, &m_opMode};
    FRadioMenuItem m_dmrDMOCal1K{DMR_DMO_CAL_1K_STR, &m_opMode};
    FRadioMenuItem m_p25Cal1K{P25_CAL_1K_STR, &m_opMode};
    FRadioMenuItem m_p25TDUTest{P25_TDU_TEST_STR, &m_opMode};
    FRadioMenuItem m_nxdnCal1K{NXDN_CAL_1K_STR, &m_opMode};
    FRadioMenuItem m_dmrFEC{DMR_FEC_STR, &m_opMode};
    FRadioMenuItem m_dmrFEC1K{DMR_FEC_1K_STR, &m_opMode};
    FRadioMenuItem m_p25FEC{P25_FEC_STR, &m_opMode};
    FRadioMenuItem m_p25FEC1K{P25_FEC_1K_STR, &m_opMode};
    FRadioMenuItem m_nxdnFEC{NXDN_FEC_STR, &m_opMode};
    FRadioMenuItem m_rssiCal{RSSI_CAL_STR, &m_opMode};
    FMenuItem m_calibrateMenuSeparator1{&m_calibrateMenu};
    FCheckMenuItem m_toggleTxInvert{"Transmit Invert", &m_calibrateMenu};
    FCheckMenuItem m_toggleRxInvert{"Receive Invert", &m_calibrateMenu};
    FCheckMenuItem m_togglePTTInvert{"PTT Invert", &m_calibrateMenu};
    FCheckMenuItem m_toggleDCBlocker{"DC Blocker", &m_calibrateMenu};
    FMenuItem m_calibrateMenuSeparator2{&m_calibrateMenu};
    FCheckMenuItem m_toggleDuplex{"Rx on Hotspot Rx Antenna", &m_calibrateMenu};
    FMenuItem m_calibrateMenuSeparator3{&m_calibrateMenu};
    FMenuItem m_adjustLevel{"&Level Adjustment", &m_calibrateMenu};

    FMenu m_engineeringMenu{"&Engineering", &m_menuBar};
    FMenuItem m_adjSymLevel{"&Symbol Level Adjustment", &m_engineeringMenu};
    FMenuItem m_adjHSBandwidth{"Hotspot &Bandwidth Adjustment", &m_engineeringMenu};
    FMenuItem m_adjHSGain{"Hotspot &Gain & AFC", &m_engineeringMenu};
    FMenuItem m_engineeringMenuSeparator1{&m_engineeringMenu};
    FMenuItem m_adjFifoBuffers{"&FIFO Buffers", &m_engineeringMenu};
    FMenuItem m_engineeringMenuSeparator3{&m_engineeringMenu};
    FMenuItem m_eraseConfigArea{"Erase Modem Configuration Area", &m_engineeringMenu};
    FMenuItem m_readConfigArea{"Read Modem Configuration Area", &m_engineeringMenu};
    FMenuItem m_engineeringMenuSeparator2{&m_engineeringMenu};
    FCheckMenuItem m_forceHotspot{"Force Hotspot Settings", &m_engineeringMenu};
    FCheckMenuItem m_modemDebug{"Modem Debug", &m_engineeringMenu};

    FMenu m_helpMenu{"&Help", &m_menuBar};
    FMenuItem m_aboutItem{"&About", &m_helpMenu};

    FStatusBar m_statusBar{this};
    FStatusKey m_keyF2{FKey::F2, "Save Settings", &m_statusBar};
    FStatusKey m_keyF3{FKey::F3, "Quit", &m_statusBar};
    FStatusKey m_keyF5{FKey::F5, "Level Adjustment", &m_statusBar};
    FStatusKey m_keyF8{FKey::F8, "Connect to Modem", &m_statusBar};
    FStatusKey m_keyF12{FKey::F12, "Transmit", &m_statusBar};

    /// <summary>
    /// Helper to reset the BER window to a default state.
    /// </summary>
    void resetBERWnd(bool show = false)
    {
        if (show) {
            m_berWnd.show();
        }
        else {
            m_berWnd.hide();
        }
        m_berWnd.ber("-.---");
        m_berWnd.segmentColor(FColor::LightGray);
    }

    /*
    ** Event Handlers
    */

    /// <summary>
    /// 
    /// </summary>
    /// <param name="e"></param>
    void onClose(FCloseEvent* e) override
    {
        // if we are saving on close -- fire off the file save event
        if (m_saveOnCloseToggle.isChecked()) {
            m_setup->saveConfig();
        }

        if (m_setup->m_isConnected) {
            if (m_setup->m_transmit)
                m_setup->setTransmit();

            m_setup->m_isConnected = false;
            m_setup->m_modem->close();
            Thread::sleep(250);
        }

        FApplication::closeConfirmationDialog(this, e);
    }

    /*
    ** Callbacks
    */

    /// <summary>
    /// "Save Settings" menu item click callback.
    /// </summary>
    void cb_connectToModemClick()
    {
        if (!m_setup->m_isConnected) {
            bool modemDebugState = g_modemDebug;
            if (g_modemDebug) {
                g_modemDebug = false;
            }

            m_setup->m_debug = false;
            m_setup->m_modem->m_debug = false;

            FMessageBox wait("Wait", L"Please wait...\nConnecting to modem...",
                FMessageBox::ButtonType::Reject, FMessageBox::ButtonType::Reject, FMessageBox::ButtonType::Reject, this);
            wait.setCenterText();
            wait.setModal(false);
            wait.show();

            // open modem and initialize
            bool ret = m_setup->m_modem->open();
            wait.hide();
            if (!ret) {
                FMessageBox::error(this, L"Failed to connect to modem!");
                return;
            }

            FMessageBox initWait("Wait", L"Please wait...\nInitializing modem...",
                FMessageBox::ButtonType::Reject, FMessageBox::ButtonType::Reject, FMessageBox::ButtonType::Reject, this);
            initWait.setCenterText();
            initWait.setModal(false);
            initWait.show();

            m_setup->readFlash();

            m_setup->writeFifoLength();
            m_setup->writeConfig();
            m_setup->writeRFParams();

            m_setup->getStatus();
            uint8_t timeout = 0U;
            while (!m_setup->m_hasFetchedStatus) {
                m_setup->m_modem->clock(0U);

                timeout++;
                if (timeout >= 75U) {
                    break;
                }

                sleep(5U);
            }

            if (!m_setup->m_hasFetchedStatus) {
                FMessageBox::error(this, L"Failed to get status from the modem!");

                m_setup->m_isConnected = false;
                m_connectToModemItem.setEnable();

                m_setup->m_modem->close();
                return;
            }

            if (g_modemDebug != modemDebugState) {
                g_modemDebug = modemDebugState;
                m_setup->m_debug = g_modemDebug;
                m_setup->m_modem->m_debug = g_modemDebug;
                m_setup->writeConfig();
            }

            m_setup->m_isConnected = true;
            m_connectToModemItem.setDisable();

            m_statusWnd.show();

            m_setup->m_modem->m_statusTimer.start();
            m_setup->m_stopWatch.start();

            setMenuStates();
            m_setup->printStatus();

            // set default state
            m_setup->m_mode = STATE_DMR_CAL;
            m_setup->m_modeStr = DMR_CAL_STR;
            m_setup->m_duplex = true;
            m_setup->m_dmrEnabled = false;
            m_setup->m_dmrRx1K = false;
            m_setup->m_p25Enabled = false;
            m_setup->m_p25Rx1K = false;
            m_setup->m_p25TduTest = false;
            m_setup->m_nxdnEnabled = false;
            m_setup->writeConfig();

            initWait.hide();
        }
        else {
            FMessageBox::error(this, L"Cannot connect to a modem when already connected.");
        }
    }
};

#endif // __SETUP_WND_H__