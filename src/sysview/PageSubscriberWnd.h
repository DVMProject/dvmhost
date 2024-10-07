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
 * @file PageSubscriberWnd.h
 * @ingroup fneSysView
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
 * @ingroup fneSysView
 */
class HOST_SW_API PageSubscriberWnd final : public TransmitWndBase {
public:
    /**
     * @brief Initializes a new instance of the PageSubscriberWnd class.
     * @param widget 
     */
    explicit PageSubscriberWnd(FWidget* widget = nullptr) : TransmitWndBase{widget}
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

        // subscriber entry
        {
            m_dialogLabel.setGeometry(FPoint(6, 6), FSize(20, 2));
            m_dialogLabel.setEmphasis();
            m_dialogLabel.setAlignment(Align::Center);

            m_subscriberLabel.setGeometry(FPoint(2, 8), FSize(25, 1));
            m_subscriber.setGeometry(FPoint(28, 8), FSize(20, 1));
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
        switch (m_mode) {
        case modem::STATE_DMR:
        {
            using namespace dmr;
            using namespace dmr::lc::csbk;

            std::unique_ptr<CSBK_CALL_ALRT> csbk = std::make_unique<CSBK_CALL_ALRT>();
            csbk->setGI(false);
            csbk->setSrcId(DMRDEF::WUID_ALL);
            csbk->setDstId((uint32_t)m_subscriber.getValue());

            write_CSBK((uint8_t)m_dmrSlot.getValue(), csbk.get());
        }
        break;

        case modem::STATE_P25:
        {
            using namespace p25;
            using namespace p25::lc::tsbk;

            std::unique_ptr<IOSP_CALL_ALRT> iosp = std::make_unique<IOSP_CALL_ALRT>();
            iosp->setSrcId(P25DEF::WUID_FNE);
            iosp->setDstId((uint32_t)m_subscriber.getValue());

            write_TSDU(iosp.get());
        }
        break;

        default:
            break;
        }
    }
};

#endif // __PAGE_SUBSCRIBER_WND_H__