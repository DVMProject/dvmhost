/**
* Digital Voice Modem - Host Monitor Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Monitor Software
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
#if !defined(__TRANSMIT_WND_BASE_H__)
#define __TRANSMIT_WND_BASE_H__

#include "common/lookups/AffiliationLookup.h"
#include "host/network/RESTDefines.h"
#include "host/modem/Modem.h"
#include "remote/RESTClient.h"
#include "MonitorMain.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the base class for transmit windows.
// ---------------------------------------------------------------------------

class HOST_SW_API TransmitWndBase : public finalcut::FDialog {
public:
    /// <summary>
    /// Initializes a new instance of the TransmitWndBase class.
    /// </summary>
    /// <param name="channel"></param>
    /// <param name="widget"></param>
    explicit TransmitWndBase(lookups::VoiceChData channel, FWidget* widget = nullptr) : FDialog{widget},
        m_selectedCh(channel)
    {
        /* stub */
    }

protected:
    bool m_hideModeSelect = false;
    lookups::VoiceChData m_selectedCh;

    uint8_t m_mode = modem::STATE_DMR;

    /// <summary>
    ///
    /// </summary>
    virtual void initLayout() override
    {
        FDialog::setMinimizable(true);
        FDialog::setShadow();

        std::size_t maxWidth, maxHeight;
        const auto& rootWidget = getRootWidget();

        if (rootWidget) {
            maxWidth = rootWidget->getClientWidth();
            maxHeight = rootWidget->getClientHeight();
        }
        else {
            // fallback to xterm default size
            maxWidth = 80;
            maxHeight = 24;
        }

        const int x = 1 + int((maxWidth - getWidth()) / 2);
        const int y = 1 + int((maxHeight - getHeight()) / 3);
        FWindow::setPos(FPoint{x, y}, false);
        FDialog::adjustSize();

        FDialog::setModal();

        initControls();

        FDialog::initLayout();

        rootWidget->redraw(); // bryanb: wtf?
        redraw();
    }

    /// <summary>
    ///
    /// </summary>
    virtual void initControls()
    {
        resizeControls();

        m_dmrSlotLabel.setGeometry(FPoint(2, 4), FSize(10, 1));
        m_dmrSlot.setGeometry(FPoint(18, 4), FSize(5, 1));
        m_dmrSlot.setRange(1, 2);
        m_dmrSlot.setValue(1);
        m_dmrSlot.setShadow(false);

        // callback REST API to get status of the channel
        json::object req = json::object();
        json::object rsp = json::object();
    
        int ret = RESTClient::send(m_selectedCh.address(), m_selectedCh.port(), m_selectedCh.password(),
            HTTP_GET, GET_STATUS, req, rsp, g_debug);
        if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
            ::LogError(LOG_HOST, "failed to get status for %s:%u", m_selectedCh.address().c_str(), m_selectedCh.port());
        }

        try {
            if (rsp["fixedMode"].get<bool>()) {
                m_hideModeSelect = true;
            }

            m_mode = rsp["state"].get<uint8_t>();

            bool dmrCC = rsp["dmrCC"].get<bool>();
            bool p25CC = rsp["p25CC"].get<bool>();
            bool nxdnCC = rsp["nxdnCC"].get<bool>();

            // are we a dedicated control channel?
            if (dmrCC || p25CC || nxdnCC) {
                m_hideModeSelect = true;
                if (dmrCC) {
                    m_mode = modem::STATE_DMR;
                }

                if (p25CC) {
                    m_mode = modem::STATE_P25;
                    m_dmrSlot.setEnable(false);
                    redraw();
                }

                if (nxdnCC) {
                    m_mode = modem::STATE_NXDN;
                    m_dmrSlot.setEnable(false);
                    redraw();
                }
            }

            // are we hiding the mode select?
            if (!m_hideModeSelect) {
                bool dmrEnabled = rsp["dmrEnabled"].get<bool>();
                bool p25Enabled = rsp["p25Enabled"].get<bool>();
                bool nxdnEnabled = rsp["nxdnEnabled"].get<bool>();

                m_digModeGroup.setGeometry(FPoint(2, 1), FSize(56, 2));
                if (dmrEnabled) {
                    m_modeDMR.setPos(FPoint(1, 1));
                    m_modeDMR.addCallback("toggled", [&]() {
                        if (m_modeDMR.isChecked()) {
                            m_mode = modem::STATE_DMR;
                            m_dmrSlot.setEnable(true);
                            redraw();
                        }
                    });
                }
                else {
                    m_modeDMR.setVisible(false);
                }

                if (p25Enabled) {
                    m_modeP25.setPos(FPoint(13, 1));
                    m_modeP25.addCallback("toggled", [&]() {
                        if (m_modeP25.isChecked()) {
                            m_mode = modem::STATE_P25;
                            m_dmrSlot.setEnable(false);
                            redraw();
                        }
                    });
                }
                else {
                    m_modeP25.setVisible(false);
                }

                if (nxdnEnabled) {
                    m_modeNXDN.setPos(FPoint(22, 1));
                    m_modeNXDN.addCallback("toggled", [&]() {
                        if (m_modeNXDN.isChecked()) {
                            m_mode = modem::STATE_NXDN;
                            m_dmrSlot.setEnable(false);
                            redraw();
                        }
                    });
                }
                else {
                    m_modeNXDN.setVisible(false);
                }
            }
            else {
                m_digModeGroup.setVisible(false);
                m_modeDMR.setVisible(false);
                m_modeP25.setVisible(false);
                m_modeNXDN.setVisible(false);
                m_dmrSlotLabel.setVisible(false);
                m_dmrSlot.setVisible(false);
                redraw();
            }
        }
        catch (std::exception&) {
            /* stub */
        }

        focusFirstChild();
    }

    /// <summary>
    ///
    /// </summary>
    void resizeControls()
    {
        // transmit button and close button logic
        m_txButton.setGeometry(FPoint(3, int(getHeight()) - 6), FSize(10, 3));
        m_txButton.addCallback("clicked", [&]() { setTransmit(); });

        m_closeButton.setGeometry(FPoint(17, int(getHeight()) - 6), FSize(9, 3));
        m_closeButton.addCallback("clicked", [&]() { hide(); });
    }

    /// <summary>
    ///
    /// </summary>
    virtual void adjustSize() override
    {
        FDialog::adjustSize();
    }

    /*
    ** Event Handlers
    */

    /// <summary>
    /// 
    /// </summary>
    /// <param name="e"></param>
    virtual void onKeyPress(finalcut::FKeyEvent* e)
    {
        const auto key = e->key();
        if (key == FKey::F12) {
            setTransmit();
        }
    }

    /// <summary>
    /// 
    /// </summary>
    /// <param name="e"></param>
    virtual void onClose(FCloseEvent* e) override
    {
        hide();
    }

protected:
    /// <summary>
    ///
    /// </summary>
    virtual void setTransmit()
    {
        /* stub */
    }

    FButton m_txButton{"Transmit", this};
    FButton m_closeButton{"Close", this};

    FButtonGroup m_digModeGroup{"Digital Mode", this};
    FRadioButton m_modeDMR{"DMR", &m_digModeGroup};
    FRadioButton m_modeP25{"P25", &m_digModeGroup};
    FRadioButton m_modeNXDN{"NXDN", &m_digModeGroup};

    FLabel m_dmrSlotLabel{"DMR Slot: ", this};
    FSpinBox m_dmrSlot{this};
};

#endif // __TRANSMIT_WND_BASE_H__