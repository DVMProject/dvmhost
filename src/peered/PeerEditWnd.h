// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Peer ID Editor
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file PeerEditWnd.h
 * @ingroup peered
 */
#if !defined(__PEER_EDIT_WND_H__)
#define __PEER_EDIT_WND_H__

#include "common/Log.h"

#include "peered/CloseWndBase.h"
#include "peered/PeerEdMain.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the line edit control for peer IDs.
 * @ingroup peered
 */
class HOST_SW_API PeerIdLineEdit final : public FLineEdit {
public:
    /**
     * @brief Initializes a new instance of the PeerIdLineEdit class.
     * @param widget 
     */
    explicit PeerIdLineEdit(FWidget* widget = nullptr) : FLineEdit{widget}
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
 * @brief This class implements the peer ID editor window.
 * @ingroup peered
 */
class HOST_SW_API PeerEditWnd final : public CloseWndBase {
public:
    /**
     * @brief Initializes a new instance of the PeerEditWnd class.
     * 
     * @param rule
     * @param widget 
     */
    explicit PeerEditWnd(lookups::PeerId rule, FWidget* widget = nullptr) : CloseWndBase{widget}
    {
        m_rule = rule;
        if (m_rule.peerDefault()) {
            m_new = true;
        } else {
            m_origPeerId = m_rule.peerId();
        }
    }

private:
    bool m_new;
    bool m_skipSaving;
    lookups::PeerId m_rule;

    uint32_t m_origPeerId;

    FLabel m_peerAliasLabel{"Alias: ", this};
    FLineEdit m_peerAlias{this};

    FCheckBox m_saveCopy{"Save Copy", this};
    FCheckBox m_incOnSave{"Increment On Save", this};

    FButtonGroup m_sourceGroup{"Peer ID", this};
    FLabel m_peerIdLabel{"Peer ID: ", &m_sourceGroup};
    PeerIdLineEdit m_peerId{&m_sourceGroup};
    FLabel m_peerPasswordLabel{"Password: ", &m_sourceGroup};
    FLineEdit m_peerPassword{&m_sourceGroup};

    FButtonGroup m_configGroup{"Configuration", this};
    FCheckBox m_peerLinkEnabled{"Peer Link", &m_configGroup};
    FCheckBox m_canReqKeysEnabled{"Request Keys", &m_configGroup};
    FCheckBox m_canInhibitEnabled{"Issue Inhibit", &m_configGroup};

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FDialog::setText("Peer ID");
        FDialog::setSize(FSize{60, 18});

        m_enableSetButton = false;
        CloseWndBase::initLayout();
    }

    /**
     * @brief Initializes window controls.
     */
    void initControls() override
    {
        m_closeButton.setText("&OK");

        m_peerAliasLabel.setGeometry(FPoint(2, 2), FSize(8, 1));
        m_peerAlias.setGeometry(FPoint(11, 2), FSize(24, 1));
        if (!m_rule.peerDefault()) {
            m_peerAlias.setText(m_rule.peerAlias());
        }
        m_peerAlias.setShadow(false);
        m_peerAlias.addCallback("changed", [&]() { m_rule.peerAlias(m_peerAlias.getText().toString()); });

        m_saveCopy.setGeometry(FPoint(36, 2), FSize(18, 1));
        m_saveCopy.addCallback("toggled", [&]() {
            if (m_saveCopy.isChecked()) {
                m_incOnSave.setEnable();
            } else {
                m_incOnSave.setChecked(false);
                m_incOnSave.setDisable();
            }

            redraw();
        });
        m_incOnSave.setGeometry(FPoint(36, 3), FSize(18, 1));
        m_incOnSave.setDisable();

        // talkgroup source
        {
            m_sourceGroup.setGeometry(FPoint(2, 5), FSize(30, 5));
            m_peerIdLabel.setGeometry(FPoint(2, 1), FSize(10, 1));
            m_peerId.setGeometry(FPoint(11, 1), FSize(17, 1));
            m_peerId.setAlignment(finalcut::Align::Right);
            if (!m_rule.peerDefault()) {
                m_peerId.setText(std::to_string(m_rule.peerId()));
            } else {
                m_rule.peerId(1U);
                m_peerId.setText("1");
            }
            m_peerId.setShadow(false);
            m_peerId.addCallback("up-pressed", [&]() {
                uint32_t peerId = ::atoi(m_peerId.getText().c_str());
                peerId++;
                if (peerId > 999999999U) {
                    peerId = 999999999U;
                }

                m_peerId.setText(std::to_string(peerId));

                m_rule.peerId(peerId);
                redraw(); 
            });
            m_peerId.addCallback("down-pressed", [&]() {
                uint32_t peerId = ::atoi(m_peerId.getText().c_str());
                peerId--;
                if (peerId < 1U) {
                    peerId = 1U;
                }

                m_peerId.setText(std::to_string(peerId));

                m_rule.peerId(peerId);
                redraw(); 
            });
            m_peerId.addCallback("changed", [&]() { 
                if (m_peerId.getText().getLength() == 0) {
                    m_rule.peerId(1U);
                    return;
                }

                uint32_t peerId = ::atoi(m_peerId.getText().c_str());
                if (peerId < 1U) {
                    peerId = 1U;
                }

                if (peerId > 999999999U) {
                    peerId = 999999999U;
                }

                m_peerId.setText(std::to_string(peerId));

                m_rule.peerId(peerId);
            });

            m_peerPasswordLabel.setGeometry(FPoint(2, 2), FSize(10, 1));
            m_peerPassword.setGeometry(FPoint(11, 2), FSize(17, 1));
            if (!m_rule.peerDefault()) {
                m_peerPassword.setText(m_rule.peerPassword());
            }
            m_peerPassword.setShadow(false);
            m_peerPassword.addCallback("changed", [&]() { m_rule.peerPassword(m_peerPassword.getText().toString()); });
        }

        // configuration
        {
            m_configGroup.setGeometry(FPoint(34, 5), FSize(23, 5));

            m_peerLinkEnabled.setGeometry(FPoint(2, 1), FSize(10, 1));
            m_peerLinkEnabled.setChecked(m_rule.peerLink());
            m_peerLinkEnabled.addCallback("toggled", [&]() {
                m_rule.peerLink(m_peerLinkEnabled.isChecked());
            });

            m_canReqKeysEnabled.setGeometry(FPoint(2, 2), FSize(10, 1));
            m_canReqKeysEnabled.setChecked(m_rule.canRequestKeys());
            m_canReqKeysEnabled.addCallback("toggled", [&]() {
                m_rule.canRequestKeys(m_canReqKeysEnabled.isChecked());
            });

            m_canInhibitEnabled.setGeometry(FPoint(2, 3), FSize(10, 1));
            m_canInhibitEnabled.setChecked(m_rule.canIssueInhibit());
            m_canInhibitEnabled.addCallback("toggled", [&]() {
                m_rule.canIssueInhibit(m_canInhibitEnabled.isChecked());
            });
        }

        CloseWndBase::initControls();
    }

    /**
     * @brief 
     */
    void logRuleInfo()
    {
        std::string peerAlias = m_rule.peerAlias();
        uint32_t peerId = m_rule.peerId();
        bool peerLink = m_rule.peerLink();
        bool canRequestKeys = m_rule.canRequestKeys();

        ::LogInfoEx(LOG_HOST, "Peer ALIAS: %s PEERID: %u PEER LINK: %u CAN REQUEST KEYS: %u", peerAlias.c_str(), peerId, peerLink, canRequestKeys);
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
        if (key == FKey::Enter) {
            this->close();
        } else if (key == FKey::Escape) {
            m_skipSaving = true;
            this->close();
        }
    }

    /**
     * @brief Event that occurs when the window is closed.
     * @param e Close event.
     */
    void onClose(FCloseEvent* e) override
    {
        if (m_skipSaving) {
            m_skipSaving = false;
            CloseWndBase::onClose(e);
            return;
        }

        if (!m_rule.peerDefault()) {
            if (m_incOnSave.isChecked()) {
                uint32_t peerId = m_rule.peerId();
                peerId++;

                if (peerId > 999999999U) {
                    peerId = 999999999U;
                }

                m_rule.peerId(peerId);

                m_peerId.setText(std::to_string(peerId));
                redraw();
            }

            if (m_origPeerId != 0U && !m_saveCopy.isChecked()) {
                if (m_rule.peerId() == 0U) {
                    LogError(LOG_HOST, "Not saving peer, peer %s (%u), peer ID must be greater then 0.", m_rule.peerAlias().c_str(), m_rule.peerId());
                    FMessageBox::error(this, "Peer ID must be valid.");
                    return;
                }

                // update peer
                auto peers = g_pidLookups->tableAsList();
                auto it = std::find_if(peers.begin(), peers.end(),
                    [&](lookups::PeerId x)
                    {
                        return x.peerId() == m_origPeerId;
                    });
                if (it != peers.end()) {
                    LogMessage(LOG_HOST, "Updating peer %s (%u) to %s (%u)", it->peerAlias().c_str(), it->peerId(), m_rule.peerAlias().c_str(), m_rule.peerId());
                    g_pidLookups->eraseEntry(m_origPeerId);

                    lookups::PeerId entry = lookups::PeerId(m_rule.peerId(), m_rule.peerAlias(), m_rule.peerPassword(), false);
                    entry.peerLink(m_rule.peerLink());
                    entry.canRequestKeys(m_rule.canRequestKeys());
                    entry.canIssueInhibit(m_rule.canIssueInhibit());

                    g_pidLookups->addEntry(m_rule.peerId(), entry);

                    logRuleInfo();
                }
            } else {
                if (m_rule.peerId() == 0U) {
                    LogError(LOG_HOST, "Not saving peer, peer %s (%u), peer ID must be greater then 0.", m_rule.peerAlias().c_str(), m_rule.peerId());
                    FMessageBox::error(this, "Peer ID must be valid.");
                    return;
                }

                auto peers = g_pidLookups->tableAsList();
                auto it = std::find_if(peers.begin(), peers.end(),
                    [&](lookups::PeerId x)
                    {
                        return x.peerId() == m_rule.peerId();
                    });
                if (it != peers.end()) {
                    LogError(LOG_HOST, "Not saving duplicate peer, peer %s (%u), peers must be unique.", m_rule.peerAlias().c_str(), m_rule.peerId());
                    FMessageBox::error(this, "Duplicate peer, change peer ID. Peers must be unique.");
                    if (m_saveCopy.isChecked())
                        m_saveCopy.setChecked(false);
                    return;
                }

                // add new peer
                if (m_saveCopy.isChecked()) {
                    LogMessage(LOG_HOST, "Copying Peer. Adding Peer %s (%u)", m_rule.peerAlias().c_str(), m_rule.peerId());
                } else {
                    LogMessage(LOG_HOST, "Adding Peer %s (%u)", m_rule.peerAlias().c_str(), m_rule.peerId());
                }

                lookups::PeerId entry = lookups::PeerId(m_rule.peerId(), m_rule.peerAlias(), m_rule.peerPassword(), false);
                entry.peerLink(m_rule.peerLink());
                entry.canRequestKeys(m_rule.canRequestKeys());
                entry.canIssueInhibit(m_rule.canIssueInhibit());

                g_pidLookups->addEntry(m_rule.peerId(), entry);

                logRuleInfo();

                // don't actually close the modal on a copy save
                if (m_saveCopy.isChecked()) {
                    return;
                }
            }
        } else {
            LogError(LOG_HOST, "Not saving peer, peer %s (%u), have a peer ID greater than 0.", m_rule.peerAlias().c_str(), m_rule.peerId());
            FMessageBox::error(this, "Talkgroup must have a peer ID greater than 0.");
            return;
        }

        CloseWndBase::onClose(e);
    }
};

#endif // __PEER_EDIT_WND_H__