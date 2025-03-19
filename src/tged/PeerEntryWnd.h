// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Talkgroup Editor
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file PeerEntryWnd.h
 * @ingroup tged
 */
#if !defined(__PEER_ENTRY_WND_H__)
#define __PEER_ENTRY_WND_H__

#include "common/Log.h"

#include "tged/CloseWndBase.h"
#include "tged/TGEdMain.h"
#include "tged/TGEditPeerListWnd.h"

#include <final/final.h>
using namespace finalcut;

/**
 * @brief This class implements the peer ID entry window.
 * @ingroup tged
 */
class HOST_SW_API PeerEntryWnd final : public CloseWndBase {
public:
    /**
     * @brief Initializes a new instance of the TGEditWnd class.
     * @param rule
     * @param peerList
     * @param title 
     * @param widget 
     */
    explicit PeerEntryWnd(std::string title = "Peer Entry", FWidget *widget = nullptr) : CloseWndBase{widget}
    {
        m_title = title;
        peerId = 0U;
    }

    /**
     * @brief Entered peer Id.
     */
    uint32_t peerId;

private:
    std::string m_title;

    FLabel m_entryLabel{"Peer ID: ", this};
    PeerLineEdit m_entry{this};

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FDialog::setText(m_title);
        FDialog::setSize(FSize{40, 6});

        m_enableSetButton = false;
        CloseWndBase::initLayout();
    }

    /**
     * @brief Initializes window controls.
     */
    void initControls() override
    {
        m_closeButton.setText("&OK");

        m_entryLabel.setGeometry(FPoint(2, int(getHeight() - 4)), FSize(10, 1));
        m_entry.setGeometry(FPoint(12, int(getHeight() - 4)), FSize(15, 1));
        m_entry.setShadow(false);
        m_entry.addCallback("up-pressed", [&]() {
            peerId = ::atoi(m_entry.getText().c_str());
            peerId++;
            if (peerId > 999999999U) {
                peerId = 999999999U;
            }

            m_entry.setText(std::to_string(peerId));
            redraw(); 
        });
        m_entry.addCallback("down-pressed", [&]() {
            peerId = ::atoi(m_entry.getText().c_str());
            peerId--;
            if (peerId < 1U) {
                peerId = 1U;
            }

            m_entry.setText(std::to_string(peerId));
            redraw();
        });

        CloseWndBase::initControls();

        m_closeButton.setGeometry(FPoint(int(getWidth()) - 12, int(getHeight()) - 4), FSize(9, 1));
        m_closeButton.setText("OK");

        setFocusWidget(&m_entry);
        redraw();
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
        if (key == FKey::Enter || key == FKey::Return) {
            this->close();
        } else if (key == FKey::Escape) {
            this->close();
        }
    }

    /**
     * @brief Event that occurs when the window is closed.
     * @param e Close event.
     */
    void onClose(FCloseEvent* e) override
    {
        if (m_entry.getText() != "") {
            peerId = ::atoi(m_entry.getText().c_str());
        } else {
            peerId = 0U;
        }

        CloseWndBase::onClose(e);
    }
};

#endif // __PEER_ENTRY_WND_H__