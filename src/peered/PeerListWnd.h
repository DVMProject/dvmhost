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
 * @file PeerListWnd.h
 * @ingroup peered
 */
#if !defined(__PEER_LIST_WND_H__)
#define __PEER_LIST_WND_H__

#include "common/Log.h"

#include "FDblDialog.h"
#include "PeerEdMainWnd.h"
#include "PeerEditWnd.h"

#include <final/final.h>
using namespace finalcut;

struct PrivateFListViewScrollToY { typedef void(FListView::*type)(int); };
template class HackTheGibson<PrivateFListViewScrollToY, &FListView::scrollToY>;
struct PrivateFListViewIteratorFirst { typedef FListViewIterator FListView::*type; };
template class HackTheGibson<PrivateFListViewIteratorFirst, &FListView::first_visible_line>;
struct PrivateFListViewVBarPtr { typedef FScrollbarPtr FListView::*type; };
template class HackTheGibson<PrivateFListViewVBarPtr, &FListView::vbar>;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define PEER_LIST_WIDTH 74
#define PEER_LIST_HEIGHT 15

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the peer list window.
 * @ingroup peered
 */
class HOST_SW_API PeerListWnd final : public FDblDialog {
public:
    /**
     * @brief Initializes a new instance of the PeerListWnd class.
     * @param widget 
     */
    explicit PeerListWnd(FWidget* widget = nullptr) : FDblDialog{widget}
    {
        /* stub */
    }
    /**
     * @brief Copy constructor.
     */
    PeerListWnd(const PeerListWnd&) = delete;
    /**
     * @brief Move constructor.
     */
    PeerListWnd(PeerListWnd&&) noexcept = delete;
    /**
     * @brief Finalizes an instance of the PeerListWnd class.
     */
    ~PeerListWnd() noexcept override = default;

    /**
     * @brief Disable copy assignment operator (=).
     */
    auto operator= (const PeerListWnd&) -> PeerListWnd& = delete;
    /**
     * @brief Disable move assignment operator (=).
     */
    auto operator= (PeerListWnd&&) noexcept -> PeerListWnd& = delete;

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
        m_selected = PeerId();
        m_selectedPeerId = 0U;

        auto entry = g_pidLookups->tableAsList()[0U];
        m_selected = entry;

        // bryanb: HACK -- use HackTheGibson to access the private current listview iterator to get the scroll position
        /*
         * This uses the RTTI hack to access private members on FListView; and this code *could* break as a consequence.
         */
        int firstScrollLinePos = 0;
        if (m_listView.getCount() > 0) {
            firstScrollLinePos = (m_listView.*RTTIResult<PrivateFListViewIteratorFirst>::ptr).getPosition();
        }

        m_listView.clear();
        for (auto entry : g_pidLookups->tableAsList()) {
            // pad peer ID properly
            std::ostringstream oss;
            oss << std::setw(7) << std::setfill('0') << entry.peerId();

            bool masterPassword = (entry.peerPassword().size() == 0U);

            // build list view entry
            const std::array<std::string, 6U> columns = {
                oss.str(),
                (masterPassword) ? "X" : "",
                (entry.peerLink()) ? "X" : "",
                (entry.canRequestKeys()) ? "X" : "",
                (entry.canIssueInhibit()) ? "X" : "",
                entry.peerAlias()
            };

            const finalcut::FStringList line(columns.cbegin(), columns.cend());
            m_listView.insert(line);
        }

        // bryanb: HACK -- use HackTheGibson to access the private set scroll Y to set the scroll position
        /*
         * This uses the RTTI hack to access private members on FListView; and this code *could* break as a consequence.
         */
        if ((size_t)firstScrollLinePos > m_listView.getCount())
            firstScrollLinePos = 0;
        if (firstScrollLinePos > 0 && m_listView.getCount() > 0) {
            (m_listView.*RTTIResult<PrivateFListViewScrollToY>::ptr)(firstScrollLinePos);
            (m_listView.*RTTIResult<PrivateFListViewVBarPtr>::ptr)->setValue(firstScrollLinePos);
        }

        // generate dialog title        
        uint32_t len = g_pidLookups->tableAsList().size();
        std::stringstream ss;
        ss << "Peer ID List (" << len << " Peers)";
        FDialog::setText(ss.str());

        setFocusWidget(&m_listView);
        redraw();
    }

private:
    lookups::PeerId m_selected;
    uint32_t m_selectedPeerId;

    FListView m_listView{this};

    FButton m_addPeer{"&Add", this};
    FButton m_editPeer{"&Edit", this};
    FLabel m_fileName{"/path/to/peer.dat", this};
    FButton m_deletePeer{"&Delete", this};

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FDialog::setMinimumSize(FSize{PEER_LIST_WIDTH, PEER_LIST_HEIGHT});

        FDialog::setResizeable(false);
        FDialog::setMinimizable(false);
        FDialog::setTitlebarButtonVisibility(false);
        FDialog::setModal(false);

        FDialog::setText("Peer ID List");

        initControls();
        loadListView();

        FDialog::initLayout();
    }

    /**
     * @brief Initializes window controls.
     */
    void initControls()
    {
        m_addPeer.setGeometry(FPoint(2, int(getHeight() - 4)), FSize(9, 1));
        m_addPeer.setBackgroundColor(FColor::DarkGreen);
        m_addPeer.setFocusBackgroundColor(FColor::DarkGreen);
        m_addPeer.addCallback("clicked", [&]() { addEntry(); });

        m_editPeer.setGeometry(FPoint(13, int(getHeight() - 4)), FSize(10, 1));
        m_editPeer.setDisable();
        m_editPeer.addCallback("clicked", [&]() { editEntry(); });

        m_fileName.setGeometry(FPoint(27, int(getHeight() - 4)), FSize(42, 1));
        m_fileName.setText(g_iniFile);

        m_deletePeer.setGeometry(FPoint(int(getWidth()) - 13, int(getHeight() - 4)), FSize(10, 1));
        m_deletePeer.setDisable();
        m_deletePeer.addCallback("clicked", [&]() { deleteEntry(); });

        m_listView.setGeometry(FPoint{1, 1}, FSize{getWidth() - 1, getHeight() - 5});

        // configure list view columns
        m_listView.addColumn("Peer ID", 10);
        m_listView.addColumn("Master Password", 16);
        m_listView.addColumn("Peer Link", 12);
        m_listView.addColumn("Request Keys", 12);
        m_listView.addColumn("Can Inhibit", 12);
        m_listView.addColumn("Alias", 40);

        // set right alignment for peer ID
        m_listView.setColumnAlignment(2, finalcut::Align::Center);
        m_listView.setColumnAlignment(3, finalcut::Align::Center);
        m_listView.setColumnAlignment(4, finalcut::Align::Center);
        m_listView.setColumnAlignment(5, finalcut::Align::Center);
        m_listView.setColumnAlignment(6, finalcut::Align::Left);

        // set type of sorting
        m_listView.setColumnSortType(1, finalcut::SortType::Name);

        // sort by peer ID
        m_listView.setColumnSort(1, finalcut::SortOrder::Ascending);

        m_listView.addCallback("clicked", [&]() { editEntry(); });
        m_listView.addCallback("row-changed", [&]() {
            FListViewItem* curItem = m_listView.getCurrentItem();
            if (curItem != nullptr) {
                FString strPeerId = curItem->getText(1);
                uint32_t peerId = ::atoi(strPeerId.c_str());

                if (peerId != m_selectedPeerId) {
                    auto entry = g_pidLookups->find(peerId);
                    if (!entry.peerDefault()) {
                        m_selected = entry;

                        m_selectedPeerId = peerId;

                        m_editPeer.setEnable();
                        m_deletePeer.setEnable();
                        m_deletePeer.setBackgroundColor(FColor::DarkRed);
                        m_deletePeer.setFocusBackgroundColor(FColor::DarkRed);
                    } else {
                        m_editPeer.setDisable();
                        m_deletePeer.setDisable();
                        m_deletePeer.resetColors();
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

        PeerEditWnd wnd{PeerId(), this};
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
        if (m_selected.peerDefault())
            return;

        this->lowerWindow();
        this->deactivateWindow();

        PeerEditWnd wnd{m_selected, this};
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
        if (m_selected.peerDefault())
            return;

        LogMessage(LOG_HOST, "Deleting peer ID %s (%u)", m_selected.peerAlias().c_str(), m_selected.peerId());
        g_pidLookups->eraseEntry(m_selected.peerId());

        // bryanb: HACK -- use HackTheGibson to access the private current listview iterator to get the scroll position
        /*
         * This uses the RTTI hack to access private members on FListView; and this code *could* break as a consequence.
         */
        int firstScrollLinePos = 0;
        if (m_listView.getCount() > 0) {
            firstScrollLinePos = (m_listView.*RTTIResult<PrivateFListViewIteratorFirst>::ptr).getPosition();
        }
        if ((size_t)firstScrollLinePos > m_listView.getCount())
            firstScrollLinePos = 0;
        if (firstScrollLinePos > 0 && m_listView.getCount() > 0) {
            --firstScrollLinePos;
            (m_listView.*RTTIResult<PrivateFListViewScrollToY>::ptr)(firstScrollLinePos);
            (m_listView.*RTTIResult<PrivateFListViewVBarPtr>::ptr)->setValue(firstScrollLinePos);
        }

        loadListView();
    }

    /**
     * @brief 
     */
    void drawBorder() override
    {
        if (!hasBorder())
            return;        

        setColor();

        FRect box{{1, 2}, getSize()};
        box.scaleBy(0, -1);

        FRect rect = box;
        if (rect.x1_ref() > rect.x2_ref())
            std::swap(rect.x1_ref(), rect.x2_ref());

        if (rect.y1_ref() > rect.y2_ref())
            std::swap(rect.y1_ref(), rect.y2_ref());

        rect.x1_ref() = std::max(rect.x1_ref(), 1);
        rect.y1_ref() = std::max(rect.y1_ref(), 1);
        rect.x2_ref() = std::min(rect.x2_ref(), rect.x1_ref() + int(getWidth()) - 1);
        rect.y2_ref() = std::min(rect.y2_ref(), rect.y1_ref() + int(getHeight()) - 1);

        if (box.getWidth() < 3)
            return;

        // Use box-drawing characters to draw a border
        constexpr std::array<wchar_t, 8> box_char
        {{
            static_cast<wchar_t>(0x2554),   // ╔
            static_cast<wchar_t>(0x2550),   // ═
            static_cast<wchar_t>(0x2557),   // ╗
            static_cast<wchar_t>(0x2551),   // ║
            static_cast<wchar_t>(0x2551),   // ║
            static_cast<wchar_t>(0x255A),   // ╚
            static_cast<wchar_t>(0x2550),   // ═
            static_cast<wchar_t>(0x255D)    // ╝
        }};

        drawGenericBox(this, box, box_char);
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

#endif // __PEER_LIST_WND_H__