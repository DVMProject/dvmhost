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
#if !defined(__RADIO_CHECK_SUBSCRIBER_WND_H__)
#define __RADIO_CHECK_SUBSCRIBER_WND_H__

#include "monitor/TransmitWndBase.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the radio check subscriber window.
// ---------------------------------------------------------------------------

class HOST_SW_API RadioCheckSubscriberWnd final : public TransmitWndBase {
public:
    /// <summary>
    /// Initializes a new instance of the RadioCheckSubscriberWnd class.
    /// </summary>
    /// <param name="channel"></param>
    /// <param name="widget"></param>
    explicit RadioCheckSubscriberWnd(lookups::VoiceChData channel, FWidget* widget = nullptr) : TransmitWndBase{channel, widget}
    {
        /* stub */
    }

private:
    FLabel m_dialogLabel{"Radio Check Subscriber", this};

    FLabel m_subscriberLabel{"Subscriber ID: ", this};
    FSpinBox m_subscriber{this};

    /// <summary>
    ///
    /// </summary>
    void initLayout() override
    {
        FDialog::setText("Radio Check Subscriber");
        FDialog::setSize(FSize{60, 16});

        TransmitWndBase::initLayout();
    }

    /// <summary>
    ///
    /// </summary>
    void initControls() override
    {
        TransmitWndBase::initControls();

        if (m_hideModeSelect) {
            FDialog::setSize(FSize{60, 12});
            resizeControls();
        }

        // subscriber entry
        {
            if (!m_hideModeSelect) {
                m_dialogLabel.setGeometry(FPoint(2, 6), FSize(20, 2));
            }
            else {
                m_dialogLabel.setGeometry(FPoint(2, 2), FSize(20, 2));
            }
            m_dialogLabel.setEmphasis();
            m_dialogLabel.setAlignment(Align::Center);

            if (!m_hideModeSelect) {
                m_subscriberLabel.setGeometry(FPoint(2, 8), FSize(25, 1));
                m_subscriber.setGeometry(FPoint(28, 8), FSize(20, 1));
            }
            else {
                m_subscriberLabel.setGeometry(FPoint(2, 4), FSize(25, 1));
                m_subscriber.setGeometry(FPoint(28, 4), FSize(20, 1));
            }
            m_subscriber.setRange(1, 16777211);
            m_subscriber.setValue(1);
            m_subscriber.setShadow(false);
        }

        m_subscriberLabel.redraw();
        redraw();
    }

    /// <summary>
    ///
    /// </summary>
    void setTransmit() override
    {
        std::string method = PUT_DMR_RID;
        json::object req = json::object();
        req["command"].set<std::string>(RID_CMD_CHECK);
        uint32_t dstId = m_subscriber.getValue();
        req["dstId"].set<uint32_t>(dstId);

        switch (m_mode) {
        case modem::STATE_DMR:
            {
                uint8_t slot = m_dmrSlot.getValue();
                req["slot"].set<uint8_t>(slot);
            }
            break;
        case modem::STATE_P25:
            {
                method = PUT_P25_RID;
            }
            break;
        case modem::STATE_NXDN:
            return;
        }

        // callback REST API
        int ret = RESTClient::send(m_selectedCh.address(), m_selectedCh.port(), m_selectedCh.password(),
            HTTP_PUT, method, req, g_debug);
        if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
            ::LogError(LOG_HOST, "failed to send request to %s:%u", m_selectedCh.address().c_str(), m_selectedCh.port());
        }
    }
};

#endif // __RADIO_CHECK_SUBSCRIBER_WND_H__