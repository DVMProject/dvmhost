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
 * @file TGListWnd.h
 * @ingroup tged
 */
#if !defined(__TG_LIST_WND_H__)
#define __TG_LIST_WND_H__

#include "common/Log.h"

#include "TGEdMainWnd.h"
#include "TGEditWnd.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define TG_LIST_WIDTH 74
#define TG_LIST_HEIGHT 15

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the talkgroup list window.
 * @ingroup tged
 */
class HOST_SW_API TGListWnd final : public finalcut::FDialog {
public:
    /**
     * @brief Initializes a new instance of the TGListWnd class.
     * @param widget 
     */
    explicit TGListWnd(FWidget* widget = nullptr) : FDialog{widget}
    {
        /* stub */
    }
    /**
     * @brief Copy constructor.
     */
    TGListWnd(const TGListWnd&) = delete;
    /**
     * @brief Move constructor.
     */
    TGListWnd(TGListWnd&&) noexcept = delete;
    /**
     * @brief Finalizes an instance of the TGListWnd class.
     */
    ~TGListWnd() noexcept override = default;

    /**
     * @brief Disable copy assignment operator (=).
     */
    auto operator= (const TGListWnd&) -> TGListWnd& = delete;
    /**
     * @brief Disable move assignment operator (=).
     */
    auto operator= (TGListWnd&&) noexcept -> TGListWnd& = delete;

    /**
     * @brief Disable set X coordinate.
     */
    void setX(int, bool = true) override { }
    /**
     * @brief Disable set Y coordinate.
     */
    void setY(int, bool = true) override { }
    /**
     * @brief Disable set position.
     */
    void setPos(const FPoint&, bool = true) override { }

    /**
     * @brief Populates the talkgroup listview.
     */
    void loadListView()
    {
        m_selected = TalkgroupRuleGroupVoice();
        m_selectedTgId = 0U;

        m_listView.clear();
        for (auto entry : g_tidLookups->groupVoice()) {
            // pad TGs properly
            std::ostringstream oss;
            oss << std::setw(5) << std::setfill('0') << entry.source().tgId();

            // build list view entry
            const std::array<std::string, 8U> columns = {
                entry.name(), entry.nameAlias(), oss.str(),
                (entry.config().active()) ? "X" : "",
                (entry.config().affiliated()) ? "X" : "",
                std::to_string(entry.config().inclusionSize()),
                std::to_string(entry.config().exclusionSize()),
                std::to_string(entry.config().alwaysSendSize())
            };

            const finalcut::FStringList line(columns.cbegin(), columns.cend());
            m_listView.insert(line);
        }

        setFocusWidget(&m_listView);
        redraw();
    }

private:
    lookups::TalkgroupRuleGroupVoice m_selected;
    uint32_t m_selectedTgId;

    FListView m_listView{this};

    FButton m_addTG{"&Add", this};
    FButton m_editTG{"&Edit", this};
    FButton m_deleteTG{"&Delete", this};

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FDialog::setMinimumSize(FSize{TG_LIST_WIDTH, TG_LIST_HEIGHT});

        FDialog::setResizeable(false);
        FDialog::setMinimizable(false);
        FDialog::setTitlebarButtonVisibility(false);
        FDialog::setModal(false);

        FDialog::setText("Talkgroup List");

        initControls();
        loadListView();

        FDialog::initLayout();
    }

    /**
     * @brief Initializes window controls.
     */
    void initControls()
    {
        m_addTG.setGeometry(FPoint(2, int(getHeight() - 4)), FSize(9, 1));
        m_addTG.setBackgroundColor(FColor::DarkGreen);
        m_addTG.setFocusBackgroundColor(FColor::DarkGreen);
        m_addTG.addCallback("clicked", [&]() { addEntry(); });

        m_editTG.setGeometry(FPoint(13, int(getHeight() - 4)), FSize(10, 1));
        m_editTG.setDisable();
        m_editTG.addCallback("clicked", [&]() { editEntry(); });

        m_deleteTG.setGeometry(FPoint(int(getWidth()) - 13, int(getHeight() - 4)), FSize(10, 1));
        m_deleteTG.setDisable();
        m_deleteTG.addCallback("clicked", [&]() { deleteEntry(); });

        m_listView.setGeometry(FPoint{1, 1}, FSize{getWidth() - 1, getHeight() - 5});

        // configure list view columns
        m_listView.addColumn("Name", 25);
        m_listView.addColumn("Alias", 20);
        m_listView.addColumn("TGID", 9);
        m_listView.addColumn("Active", 5);
        m_listView.addColumn("Affiliated", 5);
        m_listView.addColumn("Inclusions", 5);
        m_listView.addColumn("Exclusions", 5);
        m_listView.addColumn("Always", 5);

        // set right alignment for TGID
        m_listView.setColumnAlignment(3, finalcut::Align::Right);
        m_listView.setColumnAlignment(4, finalcut::Align::Center);
        m_listView.setColumnAlignment(5, finalcut::Align::Center);
        m_listView.setColumnAlignment(6, finalcut::Align::Right);
        m_listView.setColumnAlignment(7, finalcut::Align::Right);
        m_listView.setColumnAlignment(8, finalcut::Align::Right);

        // set type of sorting
        m_listView.setColumnSortType(1, finalcut::SortType::Name);
        m_listView.setColumnSortType(2, finalcut::SortType::Name);
        m_listView.setColumnSortType(3, finalcut::SortType::Name);

        // sort by TGID
        m_listView.setColumnSort(2, finalcut::SortOrder::Ascending);
        m_listView.setColumnSort(3, finalcut::SortOrder::Ascending);

        m_listView.addCallback("clicked", [&]() { editEntry(); });
        m_listView.addCallback("row-changed", [&]() {
            FListViewItem* curItem = m_listView.getCurrentItem();
            if (curItem != nullptr) {
                FString strTgid = curItem->getText(3);
                uint32_t tgid = ::atoi(strTgid.c_str());

                if (tgid != m_selectedTgId) {
                    auto entry = g_tidLookups->find(tgid);
                    if (!entry.isInvalid()) {
                        m_selected = entry;
                        if (m_selectedTgId != tgid)
                            LogMessage(LOG_HOST, "Selected TG %s (%u) for editing", m_selected.name().c_str(), m_selected.source().tgId());
                        m_selectedTgId = tgid;

                        m_editTG.setEnable();
                        m_deleteTG.setEnable();
                        m_deleteTG.setBackgroundColor(FColor::DarkRed);
                        m_deleteTG.setFocusBackgroundColor(FColor::DarkRed);
                    } else {
                        m_editTG.setDisable();
                        m_deleteTG.setDisable();
                        m_deleteTG.resetColors();
                    }

                    redraw();
                }
            }
        });

        setFocusWidget(&m_listView);
        redraw();
    }

    /**
     * @brief 
     */
    void addEntry()
    {
        this->lowerWindow();
        this->deactivateWindow();

        TGEditWnd wnd{TalkgroupRuleGroupVoice(), this};
        wnd.show();

        this->raiseWindow();
        this->activateWindow();

        loadListView();
    }

    /**
     * @brief 
     */
    void editEntry()
    {
        if (m_selected.isInvalid())
            return;

        this->lowerWindow();
        this->deactivateWindow();

        TGEditWnd wnd{m_selected, this};
        wnd.show();

        this->raiseWindow();
        this->activateWindow();

        loadListView();
    }

    /**
     * @brief 
     */
    void deleteEntry()
    {
        if (m_selected.isInvalid())
            return;

        LogMessage(LOG_HOST, "Deleting TG %s (%u)", m_selected.name().c_str(), m_selected.source().tgId());
        g_tidLookups->eraseEntry(m_selected.source().tgId(), m_selected.source().tgSlot());
        loadListView();
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
        } else if (key == FKey::Enter || key == FKey::Return) {
            editEntry();
        }
    }
};

#endif // __TG_LIST_WND_H__