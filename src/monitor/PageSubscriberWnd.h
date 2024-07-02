// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Host Monitor Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file PageSubscriberWnd.h
 * @ingroup monitor
 */
#if !defined(__PAGE_SUBSCRIBER_WND_H__)
#define __PAGE_SUBSCRIBER_WND_H__

#include "TransmitWndBase.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the page subscriber window.
 * @ingroup monitor
 */
class HOST_SW_API PageSubscriberWnd final : public TransmitWndBase {
public:
    /**
     * @brief Initializes a new instance of the PageSubscriberWnd class.
     * @param channel Channel data.
     * @param widget 
     */
    explicit PageSubscriberWnd(lookups::VoiceChData channel, FWidget* widget = nullptr) : TransmitWndBase{channel, widget}
    {
        /* stub */
    }

private:
    FLabel m_dialogLabel{"Page Subscriber", this};

    FLabel m_subscriberLabel{"Subscriber ID: ", this};
    FSpinBox m_subscriber{this};

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FDialog::setText("Page Subscriber");
        FDialog::setSize(FSize{60, 16});

        TransmitWndBase::initLayout();
    }

    /**
     * @brief Initializes window controls.
     */
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
                m_dialogLabel.setGeometry(FPoint(6, 6), FSize(20, 2));
            }
            else {
                m_dialogLabel.setGeometry(FPoint(6, 2), FSize(20, 2));
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
            m_subscriber.setRange(0, 16777211);
            m_subscriber.setValue(1);
            m_subscriber.setShadow(false);
            m_subscriber.addCallback("changed", [&]() {
                if (m_subscriber.getValue() >= 1 && m_subscriber.getValue() <= 16777211) {
                    m_txButton.setEnable(true);
                }
                else {
                    m_txButton.setEnable(false);
                }

                redraw();
            });
        }

        m_dialogLabel.redraw();
        m_subscriberLabel.redraw();
        redraw();
    }

    /**
     * @brief Helper to transmit.
     */
    void setTransmit() override
    {
        std::string method = PUT_DMR_RID;
        json::object req = json::object();
        req["command"].set<std::string>(RID_CMD_PAGE);
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
            HTTP_PUT, method, req, m_selectedCh.ssl(), g_debug);
        if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
            ::LogError(LOG_HOST, "failed to send request %s to %s:%u", method.c_str(), m_selectedCh.address().c_str(), m_selectedCh.port());
        }
    }
};

#endif // __PAGE_SUBSCRIBER_WND_H__