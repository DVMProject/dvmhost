// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Talkgroup Editor
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file TGEditPeerListWnd.h
 * @ingroup tged
 */
#if !defined(__TG_EDIT_PEER_LIST_WND_H__)
#define __TG_EDIT_PEER_LIST_WND_H__

#include "common/Log.h"

#include "tged/CloseWndBase.h"
#include "tged/TGEdMain.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the line edit control for peer IDs.
 * @ingroup tged
 */
class HOST_SW_API PeerLineEdit final : public FLineEdit {
public:
    /**
     * @brief Initializes a new instance of the PeerLineEdit class.
     * @param widget 
     */
    explicit PeerLineEdit(FWidget* widget = nullptr) : FLineEdit{widget}
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

        if (key == FKey::Insert) {
            emitCallback("insert-pressed");
            e->accept();
            return;
        } else if (key == FKey::Return) {
            emitCallback("return-pressed");
            e->accept();
            return;
        }

        FLineEdit::onKeyPress(e);
    }
};

/**
 * @brief This class implements the talkgroup peer list editor window.
 * @ingroup tged
 */
class HOST_SW_API TGEditPeerListWnd final : public CloseWndBase {
public:
    /**
     * @brief Initializes a new instance of the TGEditWnd class.
     * @param rule
     * @param peerList
     * @param title 
     * @param widget 
     */
    explicit TGEditPeerListWnd(lookups::TalkgroupRuleGroupVoice rule, std::vector<uint32_t> peerList, 
        std::string title = "Peer List", FWidget *widget = nullptr) : CloseWndBase{widget}
    {
        m_rule = rule;
        this->peerList = peerList;
        m_title = title;
    }

    /**
     * @brief List of peer IDs.
     */
    std::vector<uint32_t> peerList;

private:
    bool m_skipSaving;
    std::string m_title;

    lookups::TalkgroupRuleGroupVoice m_rule;

    FListBox m_listBox{this};

    FButton m_add{"&Add", this};
    FButton m_delete{"&Delete", this};

    FLabel m_entryLabel{"Peer ID: ", this};
    PeerLineEdit m_entry{this};

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FDialog::setText(m_title);
        FDialog::setSize(FSize{40, 21});

        m_enableSetButton = false;
        CloseWndBase::initLayout();
        loadList();
    }

    /**
     * @brief Initializes window controls.
     */
    void initControls() override
    {
        m_closeButton.setText("&OK");

        m_add.setGeometry(FPoint(2, int(getHeight() - 4)), FSize(9, 1));
        m_add.setBackgroundColor(FColor::DarkGreen);
        m_add.setFocusBackgroundColor(FColor::DarkGreen);
        m_add.addCallback("clicked", [&]() { addEntry(); });

        m_delete.setGeometry(FPoint(13, int(getHeight() - 4)), FSize(10, 1));
        m_delete.setBackgroundColor(FColor::DarkRed);
        m_delete.setFocusBackgroundColor(FColor::DarkRed);
        m_delete.addCallback("clicked", [&]() { deleteEntry(); });

        m_entryLabel.setGeometry(FPoint(2, int(getHeight() - 6)), FSize(10, 1));
        m_entry.setGeometry(FPoint(12, int(getHeight() - 6)), FSize(11, 1));
        m_entry.setShadow(false);
        m_entry.addCallback("up-pressed", [&]() {
            uint32_t tgId = ::atoi(m_entry.getText().c_str());
            tgId++;
            if (tgId > 999999999U) {
                tgId = 999999999U;
            }

            m_entry.setText(std::to_string(tgId));
            redraw(); 
        });
        m_entry.addCallback("down-pressed", [&]() {
            uint32_t tgId = ::atoi(m_entry.getText().c_str());
            tgId--;
            if (tgId < 1U) {
                tgId = 1U;
            }

            m_entry.setText(std::to_string(tgId));
            redraw();
        });
        m_entry.addCallback("insert-pressed", [&]() { addEntry(); });
        m_entry.addCallback("return-pressed", [&]() { 
            size_t curItem = m_listBox.currentItem();
            auto item = m_listBox.getItem(curItem);
            LogMessage(LOG_HOST, "Updating %s peer ID %s to %s for TG %s (%u)", m_title.c_str(), item.getText().c_str(), m_entry.getText().c_str(),
                m_rule.name().c_str(), m_rule.source().tgId());
            item.setText(m_entry.getText());
            
            m_listBox.remove(curItem);
            m_listBox.insert(item);

            //setFocusWidget(&m_listBox);
            redraw();
        });

        m_listBox.setGeometry(FPoint{1, 1}, FSize{getWidth() - 1, getHeight() - 7});
        m_listBox.setMultiSelection(false);
        m_listBox.addCallback("row-selected", [&]() { 
            size_t curItem = m_listBox.currentItem();
            auto item = m_listBox.getItem(curItem);
            m_entry.setText(item.getText().c_str());

            setFocusWidget(&m_listBox);
            redraw();
        });

        CloseWndBase::initControls();

        setFocusWidget(&m_listBox);
        redraw();
    }

    /**
     * @brief Populates the peer list.
     */
    void loadList()
    {
        m_listBox.clear();
        for (auto entry : peerList) {
            m_listBox.insert(std::to_string(entry));
        }

        redraw();
    }

    /**
     * @brief 
     */
    void addEntry()
    {
        LogMessage(LOG_HOST, "Adding %s peer ID %s from TG %s (%u)", m_title.c_str(), m_entry.getText().c_str(),
            m_rule.name().c_str(), m_rule.source().tgId());

        if (m_entry.getText() == "") {
            peerList.push_back(0U);
        } else {
            uint32_t peerId = ::atoi(m_entry.getText().c_str());
            peerList.push_back(peerId);
        }

        loadList();

        //setFocusWidget(&m_listBox);
        redraw();
    }

    /**
     * @brief 
     */
    void deleteEntry()
    {
        m_entry.setText("");

        size_t curItem = m_listBox.currentItem();
        auto item = m_listBox.getItem(curItem);
        if (item.getText() != "") {
            uint32_t peerId = ::atoi(item.getText().c_str());
            for (std::vector<uint32_t>::iterator it = peerList.begin(); it != peerList.end(); it++) {
                auto entry = *it;
                if (entry == peerId) {
                    LogMessage(LOG_HOST, "Removing %s peer ID %s from TG %s (%u)", m_title.c_str(), item.getText().c_str(),
                                m_rule.name().c_str(), m_rule.source().tgId());
                    peerList.erase(it);
                    break;
                }
            }
        }

        loadList();

        //setFocusWidget(&m_listBox);
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
        if (key == FKey::Insert) {
            addEntry();
            redraw();
        } else if (key == FKey::Del_char) {
            deleteEntry();
            redraw();
        } else if (key == FKey::Enter || key == FKey::Return) {
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

        for (auto entry : peerList) {
            LogMessage(LOG_HOST, "%s peer ID %u for TG %s (%u)", m_title.c_str(), entry,
                m_rule.name().c_str(), m_rule.source().tgId());
        }

        CloseWndBase::onClose(e);
    }
};

#endif // __TG_EDIT_PEER_LIST_WND_H__