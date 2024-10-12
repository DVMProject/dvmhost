// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - FNE System View
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file InhibitSubscriberWnd.h
 * @ingroup fneSysView
 */
#if !defined(__INHIBIT_SUBSCRIBER_WND_H__)
#define __INHIBIT_SUBSCRIBER_WND_H__

#include "TransmitWndBase.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the inhibit subscriber window.
 * @ingroup fneSysView
 */
class HOST_SW_API InhibitSubscriberWnd final : public TransmitWndBase {
public:
    /**
     * @brief Initializes a new instance of the InhibitSubscriberWnd class.
     * @param channel Channel data.
     * @param widget 
     */
    explicit InhibitSubscriberWnd(FWidget* widget = nullptr) : TransmitWndBase{widget}
    {
        /* stub */
    }

private:
    FLabel m_dialogLabel{"Inhibit Subscriber", this};

    FLabel m_subscriberLabel{"Subscriber ID: ", this};
    RIDLineEdit m_subscriber{this};

    uint32_t m_srcId = 1U;

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FDialog::setText("Inhibit Subscriber");
        FDialog::setSize(FSize{60, 16});

        TransmitWndBase::initLayout();
    }

    /**
     * @brief Initializes window controls.
     */
    void initControls() override
    {
        TransmitWndBase::initControls();

        // subscriber entry
        {
            m_dialogLabel.setGeometry(FPoint(6, 6), FSize(20, 2));
            m_dialogLabel.setEmphasis();
            m_dialogLabel.setAlignment(Align::Center);

            m_subscriberLabel.setGeometry(FPoint(2, 8), FSize(25, 1));
            m_subscriber.setGeometry(FPoint(28, 8), FSize(20, 1));
            m_subscriber.setText(std::to_string(m_srcId));
            m_subscriber.setShadow(false);
            m_subscriber.addCallback("up-pressed", [&]() {
                uint32_t rid = ::atoi(m_subscriber.getText().c_str());
                rid++;
                if (rid > 16777215U) {
                    rid = 16777215U;
                }

                m_subscriber.setText(std::to_string(rid));

                m_srcId = rid;
                redraw();
            });
            m_subscriber.addCallback("down-pressed", [&]() {
                uint32_t rid = ::atoi(m_subscriber.getText().c_str());
                rid--;
                if (rid < 1U) {
                    rid = 1U;
                }

                m_subscriber.setText(std::to_string(rid));

                m_srcId = rid;
                redraw();
            });
            m_subscriber.addCallback("changed", [&]() { 
                if (m_subscriber.getText().getLength() == 0) {
                    m_srcId = 1U;
                    return;
                }

                uint32_t rid = ::atoi(m_subscriber.getText().c_str());
                if (rid < 1U) {
                    rid = 1U;
                }

                if (rid > 16777215U) {
                    rid = 16777215U;
                }

                m_subscriber.setText(std::to_string(rid));

                m_srcId = rid;
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
        switch (m_mode) {
        case modem::STATE_DMR:
        {
            using namespace dmr;
            using namespace dmr::defines;

            writeDMR_Ext_Func((uint8_t)m_dmrSlot.getValue(), ExtendedFunctions::INHIBIT, WUID_STUNI, m_srcId);
        }
        break;

        case modem::STATE_P25:
        {
            using namespace p25;
            using namespace p25::defines;

            writeP25_Ext_Func(ExtendedFunctions::INHIBIT, WUID_FNE, m_srcId);
        }
        break;

        default:
            break;
        }
    }
};

#endif // __INHIBIT_SUBSCRIBER_WND_H__