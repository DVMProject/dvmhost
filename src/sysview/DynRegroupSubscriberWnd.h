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
 * @file DynRegroupSubscriberWnd.h
 * @ingroup fneSysView
 */
#if !defined(__DYN_REGROUP_SUBSCRIBER_WND_H__)
#define __DYN_REGROUP_SUBSCRIBER_WND_H__

#include "TransmitWndBase.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the line edit control for TGIDs.
 * @ingroup tged
 */
class HOST_SW_API TGIdLineEdit final : public FLineEdit {
public:
    /**
     * @brief Initializes a new instance of the TGIdLineEdit class.
     * @param widget 
     */
    explicit TGIdLineEdit(FWidget* widget = nullptr) : FLineEdit{widget}
    {
        setInputFilter("[[:digit:]]");
    }

    /*
    ** Event Handlers
    */

    /**
     * @brief Event that occurs on keyboard key press.
     * @param e Keyboard Event.
     */
    void onKeyPress(finalcut::FKeyEvent* e) override
    {
        const auto key = e->key();
        if (key == FKey::Up) {
            emitCallback("up-pressed");
            e->accept();
            return;
        } else if (key == FKey::Down) {
            emitCallback("down-pressed");
            e->accept();
            return;
        }

        FLineEdit::onKeyPress(e);
    }
};

/**
 * @brief This class implements the dynamic regroup subscriber window.
 * @ingroup fneSysView
 */
class HOST_SW_API DynRegroupSubscriberWnd final : public TransmitWndBase {
public:
    /**
     * @brief Initializes a new instance of the DynRegroupSubscriberWnd class.
     * @param widget 
     */
    explicit DynRegroupSubscriberWnd(FWidget* widget = nullptr) : TransmitWndBase{widget}
    {
        /* stub */
    }

    /**
     * @brief Flag indicating a dynamic regroup lock operation should be performed.
     */
    bool lock;
    /**
     * @brief Flag indicating a dynamic regroup unlock operation should be performed.
     */
    bool unlock;

private:
    FLabel m_dialogLabel{"Dynamic Regroup Subscriber", this};

    FLabel m_subscriberLabel{"Subscriber ID: ", this};
    FSpinBox m_subscriber{this};
    FLabel m_tgLabel{"Talkgroup ID: ", this};
    TGIdLineEdit m_tgId{this};

    uint32_t m_selectedTgId = 1U;

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FDialog::setText("Dynamic Regroup Subscriber");
        FDialog::setSize(FSize{60, 17});

        TransmitWndBase::initLayout();

        m_dmrSlotLabel.setVisible(false);
        m_dmrSlot.setDisable();
        m_dmrSlot.setVisible(false);

        m_mode = modem::STATE_P25;
        m_digModeGroup.setVisible(false);
        m_modeDMR.setDisable();
        m_modeDMR.setVisible(false);
        m_modeP25.setDisable();
        m_modeP25.setVisible(false);

        redraw();
        focusFirstChild();
    }

    /**
     * @brief Initializes window controls.
     */
    void initControls() override
    {
        TransmitWndBase::initControls();

        // subscriber entry
        {
            m_dialogLabel.setGeometry(FPoint(6, 6), FSize(35, 2));
            if (lock)
                m_dialogLabel.setText("Dynamic Regroup - Lock");
            if (unlock)
                m_dialogLabel.setText("Dynamic Regroup - Unlock");
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

            m_tgLabel.setGeometry(FPoint(2, 9), FSize(25, 1));
            m_tgId.setGeometry(FPoint(28, 9), FSize(20, 1));
            m_tgId.setAlignment(finalcut::Align::Right);
            m_tgId.setText("1");
            m_tgId.setShadow(false);
            m_tgId.addCallback("up-pressed", [&]() {
                uint32_t tgId = ::atoi(m_tgId.getText().c_str());
                tgId++;
                if (tgId > 16777215U) {
                    tgId = 16777215U;
                }

                m_tgId.setText(std::to_string(tgId));
                redraw(); 
            });
            m_tgId.addCallback("down-pressed", [&]() {
                uint32_t tgId = ::atoi(m_tgId.getText().c_str());
                tgId--;
                if (tgId < 1U) {
                    tgId = 1U;
                }

                m_tgId.setText(std::to_string(tgId));
                redraw();
            });
            m_tgId.addCallback("changed", [&]() { 
                uint32_t tgId = ::atoi(m_tgId.getText().c_str());
                if (tgId < 1U) {
                    tgId = 1U;
                }

                if (tgId > 16777215U) {
                    tgId = 16777215U;
                }

                m_tgId.setText(std::to_string(tgId));

                m_selectedTgId = tgId;
            });

            if (lock || unlock) {
                m_tgId.setDisable();
            }
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
        case modem::STATE_P25:
        {
            using namespace p25;
            using namespace p25::defines;

            if (lock) {
                writeP25_Ext_Func(ExtendedFunctions::DYN_REGRP_LOCK, WUID_FNE, (uint32_t)m_subscriber.getValue());
                break;
            }

            if (unlock) {
                writeP25_Ext_Func(ExtendedFunctions::DYN_REGRP_UNLOCK, WUID_FNE, (uint32_t)m_subscriber.getValue());
                break;
            }

            writeP25_Ext_Func(ExtendedFunctions::DYN_REGRP_REQ, m_selectedTgId, (uint32_t)m_subscriber.getValue());
        }
        break;

        default:
            break;
        }
    }
};

#endif // __DYN_REGROUP_SUBSCRIBER_WND_H__