// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Talkgroup Editor
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file TGEditWnd.h
 * @ingroup tged
 */
#if !defined(__TG_EDIT_WND_H__)
#define __TG_EDIT_WND_H__

#include "common/Log.h"

#include "tged/CloseWndBase.h"
#include "tged/TGEdMain.h"
#include "tged/TGEditPeerListWnd.h"

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
 * @brief This class implements the talkgroup editor window.
 * @ingroup tged
 */
class HOST_SW_API TGEditWnd final : public CloseWndBase {
public:
    /**
     * @brief Initializes a new instance of the TGEditWnd class.
     * 
     * @param rule
     * @param widget 
     */
    explicit TGEditWnd(lookups::TalkgroupRuleGroupVoice rule, FWidget* widget = nullptr) : CloseWndBase{widget}
    {
        m_rule = rule;
        if (m_rule.isInvalid()) {
            m_new = true;
        } else {
            m_origTgId = m_rule.source().tgId();
            m_origTgSlot = m_rule.source().tgSlot();
        }
    }

private:
    bool m_new;
    bool m_skipSaving;
    lookups::TalkgroupRuleGroupVoice m_rule;

    uint32_t m_origTgId;
    uint8_t m_origTgSlot;

    FLabel m_tgNameLabel{"Name: ", this};
    FLineEdit m_tgName{this};
    FLabel m_tgAliasLabel{"Alias: ", this};
    FLineEdit m_tgAlias{this};

    FCheckBox m_saveCopy{"Save Copy", this};
    FCheckBox m_incOnSave{"Increment On Save", this};

    FButtonGroup m_sourceGroup{"Source", this};
    FLabel m_tgIdLabel{"TGID: ", &m_sourceGroup};
    TGIdLineEdit m_tgId{&m_sourceGroup};
    FLabel m_tgSlotLabel{"Slot: ", &m_sourceGroup};
    FSpinBox m_tgSlot{&m_sourceGroup};

    FButtonGroup m_configGroup{"Configuration", this};
    FCheckBox m_activeEnabled{"Active", &m_configGroup};
    FCheckBox m_affiliatedEnabled{"Affiliated", &m_configGroup};
    FCheckBox m_parrotEnabled{"Parrot", &m_configGroup};
    FButton m_inclusionList{"&Inclusions...", this};
    FButton m_exclusionList{"&Exclusions...", this};

    FButton m_alwaysList{"&Always...", this};
    FButton m_preferredList{"&Preferred...", this};

    FButton m_rewriteList{"&Rewrites...", this};

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FDialog::setText("Talkgroup");
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

        m_tgNameLabel.setGeometry(FPoint(2, 2), FSize(8, 1));
        m_tgName.setGeometry(FPoint(11, 2), FSize(24, 1));
        if (!m_rule.isInvalid()) {
            m_tgName.setText(m_rule.name());
        }
        m_tgName.setShadow(false);
        m_tgName.addCallback("changed", [&]() { m_rule.name(m_tgName.getText().toString()); });

        m_tgAliasLabel.setGeometry(FPoint(2, 3), FSize(8, 1));
        m_tgAlias.setGeometry(FPoint(11, 3), FSize(24, 1));
        if (!m_rule.isInvalid()) {
            m_tgAlias.setText(m_rule.nameAlias());
        }
        m_tgAlias.setShadow(false);
        m_tgAlias.addCallback("changed", [&]() { m_rule.nameAlias(m_tgAlias.getText().toString()); });

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
            m_tgIdLabel.setGeometry(FPoint(2, 1), FSize(6, 1));
            m_tgId.setGeometry(FPoint(11, 1), FSize(17, 1));
            m_tgId.setAlignment(finalcut::Align::Right);
            if (!m_rule.isInvalid()) {
                m_tgId.setText(std::to_string(m_rule.source().tgId()));
            } else {
                auto source = m_rule.source();
                source.tgId(1U);
                m_rule.source(source);

                m_tgId.setText("1");
            }
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

                auto source = m_rule.source();
                source.tgId(tgId);
                m_rule.source(source); 
            });

            m_tgSlotLabel.setGeometry(FPoint(2, 2), FSize(6, 1));
            m_tgSlot.setGeometry(FPoint(11, 2), FSize(17, 1));
            if (m_rule.source().tgSlot() != 0U)
                m_tgSlot.setValue(m_rule.source().tgSlot());
            else {
                auto source = m_rule.source();
                source.tgSlot(1U);
                m_rule.source(source);

                m_tgSlot.setValue(1U);
            }
            m_tgSlot.setMinValue(1U);
            m_tgSlot.setMaxValue(2U);
            m_tgSlot.setShadow(false);
            m_tgSlot.addCallback("changed", [&]() { 
                auto source = m_rule.source();
                source.tgSlot(m_tgSlot.getValue());
                m_rule.source(source);
            });
        }

        // configuration
        {
            m_configGroup.setGeometry(FPoint(34, 5), FSize(23, 5));

            m_activeEnabled.setGeometry(FPoint(2, 1), FSize(10, 1));
            m_activeEnabled.setChecked(m_rule.config().active());
            m_activeEnabled.addCallback("toggled", [&]() {
                auto config = m_rule.config();
                config.active(m_activeEnabled.isChecked());
                m_rule.config(config);
            });

            m_affiliatedEnabled.setGeometry(FPoint(2, 2), FSize(10, 1));
            m_affiliatedEnabled.setChecked(m_rule.config().affiliated());
            m_affiliatedEnabled.addCallback("toggled", [&]() {
                auto config = m_rule.config();
                config.affiliated(m_affiliatedEnabled.isChecked());
                m_rule.config(config);
            });

            m_parrotEnabled.setGeometry(FPoint(2, 3), FSize(10, 1));
            m_parrotEnabled.setChecked(m_rule.config().parrot());
            m_parrotEnabled.addCallback("toggled", [&]() {
                auto config = m_rule.config();
                config.parrot(m_parrotEnabled.isChecked());
                m_rule.config(config);
            });
        }

        m_inclusionList.setGeometry(FPoint(2, 10), FSize(16, 1));
        m_inclusionList.addCallback("clicked", [&]() {
            TGEditPeerListWnd wnd{m_rule, m_rule.config().inclusion(), "Peer Inclusions", this};
            wnd.show();

            auto config = m_rule.config();
            config.inclusion(wnd.peerList);
            m_rule.config(config);
            LogMessage(LOG_HOST, "Updated %s (%u) peer inclusion list", m_rule.name().c_str(), m_rule.source().tgId());
        });

        m_exclusionList.setGeometry(FPoint(20, 10), FSize(16, 1));
        m_exclusionList.addCallback("clicked", [&]() { 
            TGEditPeerListWnd wnd{m_rule, m_rule.config().exclusion(), "Peer Exclusions", this};
            wnd.show();

            auto config = m_rule.config();
            config.exclusion(wnd.peerList);
            m_rule.config(config);
            LogMessage(LOG_HOST, "Updated %s (%u) peer exclusion list", m_rule.name().c_str(), m_rule.source().tgId());
        });

        m_alwaysList.setGeometry(FPoint(2, 12), FSize(16, 1));
        m_alwaysList.addCallback("clicked", [&]() { 
            TGEditPeerListWnd wnd{m_rule, m_rule.config().alwaysSend(), "Peers Always Receiving", this};
            wnd.show();

            auto config = m_rule.config();
            config.alwaysSend(wnd.peerList);
            m_rule.config(config);
            LogMessage(LOG_HOST, "Updated %s (%u) peer always receiving list", m_rule.name().c_str(), m_rule.source().tgId());
        });

        m_preferredList.setGeometry(FPoint(20, 12), FSize(16, 1));
        m_preferredList.addCallback("clicked", [&]() { 
            TGEditPeerListWnd wnd{m_rule, m_rule.config().preferred(), "Peer Preference", this};
            wnd.show();

            auto config = m_rule.config();
            config.preferred(wnd.peerList);
            m_rule.config(config);
            LogMessage(LOG_HOST, "Updated %s (%u) peer preference list", m_rule.name().c_str(), m_rule.source().tgId());
        });

        m_rewriteList.setGeometry(FPoint(2, 14), FSize(16, 1));
        m_rewriteList.setDisable();
        m_rewriteList.addCallback("clicked", [&]() { 
            // TODO
        });

        CloseWndBase::initControls();
    }

    /**
     * @brief 
     */
    void logRuleInfo()
    {
        std::string groupName = m_rule.name();
        uint32_t tgId = m_rule.source().tgId();
        uint8_t tgSlot = m_rule.source().tgSlot();
        bool active = m_rule.config().active();
        bool parrot = m_rule.config().parrot();
        bool affil = m_rule.config().affiliated();

        uint32_t incCount = m_rule.config().inclusion().size();
        uint32_t excCount = m_rule.config().exclusion().size();
        uint32_t rewrCount = m_rule.config().rewrite().size();
        uint32_t alwyCount = m_rule.config().alwaysSend().size();
        uint32_t prefCount = m_rule.config().preferred().size();

        if (incCount > 0 && excCount > 0) {
            ::LogWarning(LOG_HOST, "Talkgroup (%s) defines both inclusions and exclusions! Inclusion rules take precedence and exclusion rules will be ignored.", groupName.c_str());
        }

        if (alwyCount > 0 && affil) {
            ::LogWarning(LOG_HOST, "Talkgroup (%s) is marked as affiliation required and has a defined always send list! Always send peers take rule precedence and defined peers will always receive traffic.", groupName.c_str());
        }

        ::LogInfoEx(LOG_HOST, "Talkgroup NAME: %s SRC_TGID: %u SRC_TS: %u ACTIVE: %u PARROT: %u AFFILIATED: %u INCLUSIONS: %u EXCLUSIONS: %u REWRITES: %u ALWAYS: %u PREFERRED: %u", groupName.c_str(), tgId, tgSlot, active, parrot, affil, incCount, excCount, rewrCount, alwyCount, prefCount);
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

        if (!m_rule.isInvalid()) {
            if (m_incOnSave.isChecked()) {
                auto source = m_rule.source();
                uint32_t tgId = source.tgId();
                tgId++;

                if (tgId > 16777215U) {
                    tgId = 16777215U;
                }

                source.tgId(tgId);
                m_rule.source(source);

                m_tgId.setText(std::to_string(tgId));
                redraw();
            }

            if (m_origTgId != 0U && m_origTgSlot != 0U && !m_saveCopy.isChecked()) {
                if (m_rule.name() == "") {
                    LogError(LOG_HOST, "Not saving talkgroup, TG %s (%u), talkgroup must be named.", m_rule.name().c_str(), m_rule.source().tgId());
                    FMessageBox::error(this, "Talkgroup must be named.");
                    return;
                }

                // update TG
                auto groupVoice = g_tidLookups->groupVoice();
                auto it = std::find_if(groupVoice.begin(), groupVoice.end(),
                    [&](lookups::TalkgroupRuleGroupVoice x)
                    {
                        return x.source().tgId() == m_origTgId && x.source().tgSlot() == m_origTgSlot;
                    });
                if (it != groupVoice.end()) {
                    LogMessage(LOG_HOST, "Updating TG %s (%u) to %s (%u)", it->name().c_str(), it->source().tgId(), m_rule.name().c_str(), m_rule.source().tgId());
                    g_tidLookups->eraseEntry(m_origTgId, m_origTgSlot);
                    g_tidLookups->addEntry(m_rule);

                    logRuleInfo();
                }
            } else {
                if (m_rule.name() == "") {
                    LogError(LOG_HOST, "Not saving talkgroup, TG %s (%u), talkgroup must be named.", m_rule.name().c_str(), m_rule.source().tgId());
                    FMessageBox::error(this, "Talkgroup must be named.");
                    return;
                }

                auto groupVoice = g_tidLookups->groupVoice();
                auto it = std::find_if(groupVoice.begin(), groupVoice.end(),
                    [&](lookups::TalkgroupRuleGroupVoice x)
                    {
                        return x.source().tgId() == m_origTgId && x.source().tgSlot() == m_origTgSlot;
                    });
                if (it != groupVoice.end()) {
                    LogError(LOG_HOST, "Not saving duplicate talkgroup, TG %s (%u), talkgroups must be unique.", m_rule.name().c_str(), m_rule.source().tgId());
                    FMessageBox::error(this, "Duplicate talkgroup, change TGID. Talkgroups must be unique.");
                    if (m_saveCopy.isChecked())
                        m_saveCopy.setChecked(false);
                    return;
                }

                // add new TG
                if (m_saveCopy.isChecked()) {
                    LogMessage(LOG_HOST, "Copying TG. Adding TG %s (%u)", m_rule.name().c_str(), m_rule.source().tgId());
                } else {
                    LogMessage(LOG_HOST, "Adding TG %s (%u)", m_rule.name().c_str(), m_rule.source().tgId());
                }
                g_tidLookups->addEntry(m_rule);

                logRuleInfo();

                // don't actually close the modal on a copy save
                if (m_saveCopy.isChecked()) {
                    return;
                }
            }
        } else {
            LogError(LOG_HOST, "Not saving talkgroup, TG %s (%u), have a TGID greater than 0.", m_rule.name().c_str(), m_rule.source().tgId());
            FMessageBox::error(this, "Talkgroup must have a TGID greater than 0.");
            return;
        }

        CloseWndBase::onClose(e);
    }
};

#endif // __TG_EDIT_WND_H__