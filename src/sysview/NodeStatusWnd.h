// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - FNE System View
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file NodeStatusWnd.h
 * @ingroup fneSysView
 */
#if !defined(__NODE_STATUS_WND_H__)
#define __NODE_STATUS_WND_H__

#include "host/modem/Modem.h"
#include "common/Log.h"
#include "common/Thread.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define NODE_STATUS_WIDTH 28
#define NODE_STATUS_HEIGHT 8
#define NODE_UPDATE_FAIL_CNT 4

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the node status display window.
 * @ingroup fneSysView
 */
class HOST_SW_API NodeStatusWidget final : public finalcut::FWidget {
public:
    /**
     * @brief Initializes a new instance of the NodeStatusWidget class.
     * @param widget 
     */
    explicit NodeStatusWidget(FWidget* widget = nullptr) : FWidget{widget}
    {
        /* stub */
    }

    lookups::VoiceChData chData;
    uint8_t channelId;
    uint32_t channelNo;
    uint32_t peerId;
    int32_t uniqueId;

    json::object peerStatus;

    /**
     * @brief 
     * @param status 
     */
    void setFailed(bool status) { m_failed = status; }

    /**
     * @brief 
     */
    void update()
    {
        IdenTable entry = g_idenTable->find(channelId);
        if (entry.baseFrequency() == 0U) {
            ::LogError(LOG_HOST, "Channel Id %u has an invalid base frequency.", channelId);
        }

        if (entry.txOffsetMhz() == 0U) {
            ::LogError(LOG_HOST, "Channel Id %u has an invalid Tx offset.", channelId);
        }

        m_chanNo.setText(__INT_STR(channelId) + "-" + __INT_STR(channelNo));

        uint32_t calcSpace = (uint32_t)(entry.chSpaceKhz() / 0.125);
        float calcTxOffset = entry.txOffsetMhz() * 1000000;

        uint32_t rxFrequency = (uint32_t)((entry.baseFrequency() + ((calcSpace * 125) * channelNo)) + calcTxOffset);
        uint32_t txFrequency = (uint32_t)((entry.baseFrequency() + ((calcSpace * 125) * channelNo)));

        std::stringstream ss;
        ss << std::fixed << std::setprecision(4) << (float)(txFrequency / 1000000.0f);

        m_txFreq.setText(ss.str());

        ss.str(std::string());
        ss << std::fixed << std::setprecision(4) << (float)(rxFrequency / 1000000.0f);

        m_rxFreq.setText(ss.str());

        uint8_t mode = peerStatus["state"].get<uint8_t>();
        switch (mode) {
        case modem::STATE_DMR:
            m_modeStr.setText("DMR");
            break;
        case modem::STATE_P25:
            m_modeStr.setText("P25");
            break;
        case modem::STATE_NXDN:
            m_modeStr.setText("NXDN");
            break;
        default:
            m_modeStr.setText("");
            break;
        }

        // pad peer IDs properly
        std::ostringstream peerOss;
        peerOss << std::setw(9) << std::setfill('0') << peerId;
        m_peerIdStr.setText(peerOss.str());

        if (peerStatus["lastDstId"].is<uint32_t>()) {
            uint32_t lastDstId = peerStatus["lastDstId"].get<uint32_t>();

            // pad TGs properly
            std::ostringstream tgidOss;
            tgidOss << std::setw(5) << std::setfill('0') << lastDstId;

            m_lastDst.setText(tgidOss.str());
        }

        if (peerStatus["lastSrcId"].is<uint32_t>()) {
            uint32_t lastSrcId = peerStatus["lastSrcId"].get<uint32_t>();
            m_lastSrc.setText(__INT_STR(lastSrcId));
        }

        // get remote node state
        if (peerStatus["dmrTSCCEnable"].is<bool>() && peerStatus["p25CtrlEnable"].is<bool>() &&
            peerStatus["nxdnCtrlEnable"].is<bool>()) {
            bool dmrTSCCEnable = peerStatus["dmrTSCCEnable"].get<bool>();
            bool dmrCC = peerStatus["dmrCC"].get<bool>();
            bool p25CtrlEnable = peerStatus["p25CtrlEnable"].get<bool>();
            bool p25CC = peerStatus["p25CC"].get<bool>();
            bool nxdnCtrlEnable = peerStatus["nxdnCtrlEnable"].get<bool>();
            bool nxdnCC = peerStatus["nxdnCC"].get<bool>();

            // are we a dedicated control channel?
            if (dmrCC || p25CC || nxdnCC) {
                m_control = true;
                tbText = "CONTROL";
            }

            // if we aren't a dedicated control channel; set our
            // title bar appropriately and set Tx state
            if (!m_control) {
                if (dmrTSCCEnable || p25CtrlEnable || nxdnCtrlEnable) {
                    tbText = "ENH. VOICE/CONV";
                }
                else {
                    tbText = "VOICE/CONV";
                }

                // are we transmitting?
                if (peerStatus["tx"].is<bool>()) {
                    m_tx = peerStatus["tx"].get<bool>();
                }
                else {
                    m_tx = false;
                }
            }
        }

        redraw();
    }

private:
    bool m_failed;
    bool m_control;
    bool m_tx;

    FString tbText{}; // title bar text

    FLabel m_modeStr{this};
    FLabel m_peerIdStr{this};

    FLabel m_channelNoLabel{"Ch. No.: ", this};
    FLabel m_chanNo{this};

    FLabel m_txFreqLabel{"Tx: ", this};
    FLabel m_txFreq{this};
    FLabel m_rxFreqLabel{"Rx: ", this};
    FLabel m_rxFreq{this};

    FLabel m_lastDstLabel{"Last Dst: ", this};
    FLabel m_lastDst{this};
    FLabel m_lastSrcLabel{"Last Src: ", this};
    FLabel m_lastSrc{this};

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        setSize(FSize{NODE_STATUS_WIDTH, NODE_STATUS_HEIGHT});

        tbText = "UNKNOWN";

        initControls();
    }

    /**
     * @brief Draws the window.
     */
    void draw() override
    {
        setColor();

        const auto& wc = getColorTheme();
        setForegroundColor(wc->dialog_fg);
        setBackgroundColor(wc->dialog_bg);

        if (FVTerm::getFOutput()->isMonochron())
            setReverse(true);

        drawTitleBar();
        setCursorPos({2, int(getHeight()) - 1});

        if (FVTerm::getFOutput()->isMonochron())
            setReverse(false);

        if (m_failed) {
            setColor(FColor::LightGray, FColor::LightRed);
        }
        else if (m_control) {
            setColor(FColor::LightGray, FColor::Purple1);
        }
        else if (m_tx) {
            setColor(FColor::LightGray, FColor::LightGreen);
        }
        else {
            setColor(FColor::LightGray, FColor::Black);
        }

        finalcut::drawBorder(this, FRect(FPoint{1, 2}, FPoint{NODE_STATUS_WIDTH, NODE_STATUS_HEIGHT}));
    }

    /**
     * @brief 
     */
    void drawTitleBar()
    {
        print() << FPoint{1, 1};

        // Fill with spaces (left of the title)
        if (FVTerm::getFOutput()->getMaxColor() < 16)
            setBold();

        const auto& wc = getColorTheme();

        if (!m_tx) {
            if (m_failed) {
                setColor(FColor::Black, FColor::LightRed);
            }
            else if (m_control) {
                setColor(FColor::LightGray, FColor::Purple1);
            }
            else {
                setColor(FColor::Black, FColor::White);
            }
        } else {
            setColor(FColor::Black, FColor::LightGreen);
        }

        const auto width = getWidth();
        auto textWidth = getColumnWidth(tbText);
        std::size_t leadingSpace{0};

        if (width > textWidth)
            leadingSpace = (width - textWidth) / 2;

        // Print leading whitespace
        print(FString(leadingSpace, L' '));

        // Print title bar text
        if (!tbText.isEmpty()) {
            if (textWidth <= width)
                print(tbText);
            else {
                // Print ellipsis
                const auto len = getLengthFromColumnWidth(tbText, width - 2);
                print(tbText.left(len));
                print("..");
                textWidth = len + 2;
            }
        }

        // Print trailing whitespace
        std::size_t trailingSpace = width - leadingSpace - textWidth;
        print(FString(trailingSpace, L' '));

        if (FVTerm::getFOutput()->getMaxColor() < 16)
            unsetBold();
    }

    /**
     * @brief Initializes window controls.
     */
    void initControls()
    {
        // My hope is that this code is so awful I'm never allowed to write UI code again.
        // For whatever reason FinalCut refuses to properly set the background color of the controls
        // in this widget...so we're forcing them to LightGray, this will be a problem if the overall palette
        // changes in the future.

        m_modeStr.setGeometry(FPoint(24, 3), FSize(4, 1));
        m_modeStr.setAlignment(Align::Right);
        m_modeStr.setForegroundColor(FColor::DarkBlue);                 // why?
        m_modeStr.setBackgroundColor(FColor::LightGray);                // why?

        m_peerIdStr.setGeometry(FPoint(19, 4), FSize(9, 1));
        m_peerIdStr.setForegroundColor(FColor::DarkBlue);               // why?
        m_peerIdStr.setBackgroundColor(FColor::LightGray);              // why?
        m_peerIdStr.setAlignment(Align::Right);

        // channel number
        {
            m_channelNoLabel.setGeometry(FPoint(2, 3), FSize(10, 1));
            m_channelNoLabel.setForegroundColor(FColor::Black);         // why?
            m_channelNoLabel.setBackgroundColor(FColor::LightGray);     // why?

            m_chanNo.setGeometry(FPoint(11, 3), FSize(8, 1));
            m_chanNo.setForegroundColor(FColor::Black);                 // why?
            m_chanNo.setBackgroundColor(FColor::LightGray);             // why?
            m_chanNo.setText("");
        }

        // channel frequency
        {
            m_txFreqLabel.setGeometry(FPoint(2, 4), FSize(4, 1));
            m_txFreqLabel.setForegroundColor(FColor::Black);            // why?
            m_txFreqLabel.setBackgroundColor(FColor::LightGray);        // why?
            m_txFreq.setGeometry(FPoint(6, 4), FSize(8, 1));
            m_txFreq.setForegroundColor(FColor::Black);                 // why?
            m_txFreq.setBackgroundColor(FColor::LightGray);             // why?
            m_txFreq.setText("");

            m_rxFreqLabel.setGeometry(FPoint(2, 5), FSize(4, 1));
            m_rxFreqLabel.setForegroundColor(FColor::Black);            // why?
            m_rxFreqLabel.setBackgroundColor(FColor::LightGray);        // why?
            m_rxFreq.setGeometry(FPoint(6, 5), FSize(8, 1));
            m_rxFreq.setForegroundColor(FColor::Black);                 // why?
            m_rxFreq.setBackgroundColor(FColor::LightGray);             // why?
            m_rxFreq.setText("");
        }

        // last TG
        {
            m_lastDstLabel.setGeometry(FPoint(2, 6), FSize(11, 1));
            m_lastDstLabel.setForegroundColor(FColor::Black);           // why?
            m_lastDstLabel.setBackgroundColor(FColor::LightGray);       // why?

            m_lastDst.setGeometry(FPoint(13, 6), FSize(8, 1));
            m_lastDst.setForegroundColor(FColor::Black);                // why?
            m_lastDst.setBackgroundColor(FColor::LightGray);            // why?
            m_lastDst.setText("None");
        }

        // last source
        {
            m_lastSrcLabel.setGeometry(FPoint(2, 7), FSize(11, 1));
            m_lastSrcLabel.setForegroundColor(FColor::Black);           // why?
            m_lastSrcLabel.setBackgroundColor(FColor::LightGray);       // why?

            m_lastSrc.setGeometry(FPoint(13, 7), FSize(8, 1));
            m_lastSrc.setForegroundColor(FColor::Black);                // why?
            m_lastSrc.setBackgroundColor(FColor::LightGray);            // why?
            m_lastSrc.setText("None");
        }
    }
};

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the node status window.
 * @ingroup fneSysView
 */
class HOST_SW_API ScrollView final : public finalcut::FScrollView {
public:
    /**
     * @brief Initializes a new instance of the ScrollView class.
     * @param widget 
     */
    explicit ScrollView(finalcut::FWidget* widget = nullptr) : finalcut::FScrollView{widget}
    {
        /* stub */
    }

    /**
     * @brief 
     */
    void update()
    {
        const auto& rootWidget = getRootWidget();
        std::map<uint32_t, json::object> peerStatus(getNetwork()->peerStatus.begin(), getNetwork()->peerStatus.end());
        for (auto entry : peerStatus) {
            auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [&](NodeStatusWidget* wdgt) {
                if (wdgt->peerId == entry.first && wdgt->uniqueId == (int32_t)entry.first)
                    return true;
                return false;
            });

            if (it == m_nodes.end()) {
                json::object peerObj = entry.second;
                addNode(entry.first, peerObj);

                uint32_t peerId = entry.first;
                uint8_t channelId = peerObj["channelId"].get<uint8_t>();
                uint32_t channelNo = peerObj["channelNo"].get<uint32_t>();

                bool dmrTSCCEnable = peerObj["dmrTSCCEnable"].get<bool>();
                bool dmrCC = peerObj["dmrCC"].get<bool>();
                bool p25CtrlEnable = peerObj["p25CtrlEnable"].get<bool>();
                bool p25CC = peerObj["p25CC"].get<bool>();
                bool nxdnCtrlEnable = peerObj["nxdnCtrlEnable"].get<bool>();
                bool nxdnCC = peerObj["nxdnCC"].get<bool>();

                if ((dmrTSCCEnable || p25CtrlEnable || nxdnCtrlEnable) && peerObj["vcChannels"].is<json::array>()) {
                    json::array vcChannels = peerObj["vcChannels"].get<json::array>();

                    struct
                    {
                        bool operator()(json::value a, json::value b) const 
                        {
                            json::object aObj = a.get<json::object>();
                            json::object bObj = b.get<json::object>();

                            uint32_t chNoA = aObj["channelNo"].get<uint32_t>();
                            uint32_t chNoB = bObj["channelNo"].get<uint32_t>();

                            if (chNoA < chNoB)
                                return true;
                            return false;
                        }
                    } channelComparator;

                    // sort channel list
                    std::sort(vcChannels.begin(), vcChannels.end(), channelComparator);

                    for (int32_t i = 0; i < (int32_t)vcChannels.size(); i++) {

                        json::object vcObj = vcChannels[i].get<json::object>();

                        uint8_t vcChannelId = vcObj["channelId"].get<uint8_t>();
                        uint32_t vcChannelNo = vcObj["channelNo"].get<uint32_t>();

                        // skip adding this entry if it matches the primary peer (this indicates a bad configuration)
                        if ((vcChannelId == channelId) && (vcChannelNo == channelNo)) {
                            continue;
                        }

                        uint8_t state = modem::STATE_DMR;
                        if (dmrTSCCEnable) {
                            state = modem::STATE_DMR;
                        }

                        if (p25CtrlEnable) {
                            state = modem::STATE_P25;
                        }

                        if (nxdnCtrlEnable) {
                            state = modem::STATE_NXDN;
                        }

                        vcObj["state"].set<uint8_t>(state);

                        bool _false = false;
                        vcObj["dmrTSCCEnable"].set<bool>(_false);
                        vcObj["dmrCC"].set<bool>(_false);
                        vcObj["p25CtrlEnable"].set<bool>(_false);
                        vcObj["p25CC"].set<bool>(_false);
                        vcObj["nxdnCtrlEnable"].set<bool>(_false);
                        vcObj["nxdnCC"].set<bool>(_false);

                        addNode(peerId, vcObj, i);
                    }
                }
            } else {
                NodeStatusWidget* wdgt = *it;
                json::object peerObj = entry.second;
                updateNode(wdgt, entry.first, peerObj);

                uint32_t peerId = entry.first;
                uint8_t channelId = peerObj["channelId"].get<uint8_t>();
                uint32_t channelNo = peerObj["channelNo"].get<uint32_t>();

                bool dmrTSCCEnable = peerObj["dmrTSCCEnable"].get<bool>();
                bool dmrCC = peerObj["dmrCC"].get<bool>();
                bool p25CtrlEnable = peerObj["p25CtrlEnable"].get<bool>();
                bool p25CC = peerObj["p25CC"].get<bool>();
                bool nxdnCtrlEnable = peerObj["nxdnCtrlEnable"].get<bool>();
                bool nxdnCC = peerObj["nxdnCC"].get<bool>();

                if ((dmrTSCCEnable || p25CtrlEnable || nxdnCtrlEnable) && peerObj["vcChannels"].is<json::array>()) {
                    json::array vcChannels = peerObj["vcChannels"].get<json::array>();

                    struct
                    {
                        bool operator()(json::value a, json::value b) const 
                        {
                            json::object aObj = a.get<json::object>();
                            json::object bObj = b.get<json::object>();

                            uint32_t chNoA = aObj["channelNo"].get<uint32_t>();
                            uint32_t chNoB = bObj["channelNo"].get<uint32_t>();

                            if (chNoA < chNoB)
                                return true;
                            return false;
                        }
                    } channelComparator;

                    // sort channel list
                    std::sort(vcChannels.begin(), vcChannels.end(), channelComparator);

                    for (int32_t i = 0; i < (int32_t)vcChannels.size(); i++) {
                        auto it2 = std::find_if(m_nodes.begin(), m_nodes.end(), [&](NodeStatusWidget* wdgt) {
                            if (wdgt->peerId == peerId && wdgt->uniqueId == i)
                                return true;
                            return false;
                        });

                        if (it2 != m_nodes.end()) {
                            NodeStatusWidget* subWdgt = *it2;
                            json::object vcObj = vcChannels[i].get<json::object>();

                            uint8_t vcChannelId = vcObj["channelId"].get<uint8_t>();
                            uint32_t vcChannelNo = vcObj["channelNo"].get<uint32_t>();

                            // skip adding this entry if it matches the primary peer (this indicates a bad configuration)
                            if ((vcChannelId == channelId) && (vcChannelNo == channelNo)) {
                                continue;
                            }

                            uint8_t state = modem::STATE_DMR;
                            if (dmrTSCCEnable) {
                                state = modem::STATE_DMR;
                            }

                            if (p25CtrlEnable) {
                                state = modem::STATE_P25;
                            }

                            if (nxdnCtrlEnable) {
                                state = modem::STATE_NXDN;
                            }

                            vcObj["state"].set<uint8_t>(state);

                            bool _false = false;
                            vcObj["dmrTSCCEnable"].set<bool>(_false);
                            vcObj["dmrCC"].set<bool>(_false);
                            vcObj["p25CtrlEnable"].set<bool>(_false);
                            vcObj["p25CC"].set<bool>(_false);
                            vcObj["nxdnCtrlEnable"].set<bool>(_false);
                            vcObj["nxdnCC"].set<bool>(_false);

                            updateNode(subWdgt, peerId, vcObj, i);
                        }
                    }
                }
            }
        }

        rootWidget->redraw();
        redraw();
    }

private:
    std::vector<NodeStatusWidget*> m_nodes;

    const int m_defaultOffsX = 2;
    int m_nodeWdgtOffsX = m_defaultOffsX;
    int m_nodeWdgtOffsY = 2;

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FScrollView::initLayout();
    }

    /**
     * @brief 
     */
    void draw() override
    {
        const auto& wc = getColorTheme();
        setColor(wc->label_inactive_fg, wc->dialog_bg);
        setPrintPos(FPoint{1, 1});
        clearArea();

        FScrollView::draw();
    }

    /**
     * @brief 
     * @param peerId 
     * @param peerObj 
     * @param uniqueId 
     */
    void addNode(uint32_t peerId, json::object peerObj, int32_t uniqueId = -1)
    {
        const auto& rootWidget = getRootWidget();

        int maxWidth = 77;
        if (rootWidget) {
            maxWidth = rootWidget->getClientWidth() - 3;
        }

        NodeStatusWidget* wdgt = new NodeStatusWidget(this);

        uint8_t channelId = peerObj["channelId"].get<uint8_t>();
        uint32_t channelNo = peerObj["channelNo"].get<uint32_t>();

        VoiceChData data = VoiceChData(channelId, channelNo, std::string(), 9990U, std::string(), false);

        wdgt->channelId = channelId;
        wdgt->channelNo = channelNo;
        wdgt->chData = data;
        wdgt->peerId = peerId;
        wdgt->peerStatus = peerObj;

        if (uniqueId == -1)
            wdgt->uniqueId = peerId;
        else
            wdgt->uniqueId = uniqueId;

        // update widget status
        try
        {
            wdgt->update();
        }
        catch (std::exception& e) {
            ::LogWarning(LOG_HOST, "PEER %u, failed to update peer status, %s", peerId, e.what());
        }

        // set control position
        if (m_nodeWdgtOffsX + NODE_STATUS_WIDTH > maxWidth) {
            m_nodeWdgtOffsY += NODE_STATUS_HEIGHT + 2;
            m_nodeWdgtOffsX = m_defaultOffsX;
        }

        wdgt->setGeometry(FPoint{m_nodeWdgtOffsX, m_nodeWdgtOffsY}, FSize{NODE_STATUS_WIDTH, NODE_STATUS_HEIGHT});

        m_nodeWdgtOffsX += NODE_STATUS_WIDTH + 2;
        wdgt->redraw();
        
        m_nodes.push_back(wdgt);

        rootWidget->redraw();
        redraw();
    }

    /**
     * @brief 
     * 
     * @param wdgt 
     * @param peerObj 
     * @param uniqueId 
     */
    void updateNode(NodeStatusWidget* wdgt, uint32_t peerId, json::object peerObj, int32_t uniqueId = -1)
    {
        assert(wdgt != nullptr);

        uint8_t channelId = peerObj["channelId"].get<uint8_t>();
        uint32_t channelNo = peerObj["channelNo"].get<uint32_t>();

        VoiceChData data = VoiceChData(channelId, channelNo, std::string(), 9990U, std::string(), false);

        wdgt->channelId = channelId;
        wdgt->channelNo = channelNo;
        wdgt->chData = data;
        wdgt->peerId = peerId;
        wdgt->peerStatus = peerObj;

        if (uniqueId == -1)
            wdgt->uniqueId = peerId;
        else
            wdgt->uniqueId = uniqueId;

        // update widget status
        try
        {
            wdgt->setFailed(false);
            wdgt->update();
        }
        catch (std::exception& e) {
            wdgt->setFailed(true);
            ::LogWarning(LOG_HOST, "PEER %u, failed to update peer status, %s", peerId, e.what());
        }

        wdgt->redraw();
    }
};

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the node status window.
 * @ingroup fneSysView
 */
class HOST_SW_API NodeStatusWnd final : public finalcut::FDialog {
public:
    /**
     * @brief Initializes a new instance of the NodeStatusWnd class.
     * @param widget 
     */
    explicit NodeStatusWnd(FWidget* widget = nullptr) : FDialog{widget}
    {
        m_killed = false;
        Thread::runAsThread(this, threadNodeUpdate);
    }

private:
    bool m_killed;

    ScrollView m_scroll{this};

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FDialog::setText("Peer Watch");
        FDialog::setSize(FSize{80, 25});

        FDialog::setMinimizable(false);
        FDialog::setShadow();

        std::size_t maxWidth, maxHeight;
        const auto& rootWidget = getRootWidget();

        if (rootWidget) {
            maxWidth = rootWidget->getClientWidth();
            maxHeight = rootWidget->getClientHeight();
        }
        else {
            // fallback to xterm default size
            maxWidth = 80;
            maxHeight = 24;
        }

        const int x = 1 + int((maxWidth - getWidth()) / 2);
        const int y = 1 + int((maxHeight - getHeight()) / 3);
        FWindow::setPos(FPoint{x, y}, false);
        FDialog::adjustSize();

        FDialog::setModal();

        initControls();

        FDialog::initLayout();

        rootWidget->redraw(); // bryanb: wtf?
        redraw();

        FWindow::zoomWindow();
    }

    /**
     * @brief Initializes window controls.
     */
    void initControls()
    {
        m_scroll.setGeometry(FPoint{1, 1}, FSize{78, 22});
        m_scroll.setScrollSize(FSize{230, 440});

        m_scroll.update();
    }

    /**
     * @brief Adjusts window size.
     */
    void adjustSize() override
    {
        m_scroll.setGeometry(FPoint{1, 1}, FSize{getWidth() - 2, getHeight() - 3});
    }

    /* Entry point to node update thread. */

    static void* threadNodeUpdate(void* arg)
    {
        thread_t* th = (thread_t*)arg;
        if (th != nullptr) {
    #if defined(_WIN32)
            ::CloseHandle(th->thread);
    #else
            ::pthread_detach(th->thread);
    #endif // defined(_WIN32)

            std::string threadName("sysview:node-status-update");
            NodeStatusWnd* wnd = (NodeStatusWnd*)th->obj;
            if (wnd == nullptr) {
                delete th;
                return nullptr;
            }

            if (wnd->m_killed) {
                delete th;
                return nullptr;
            }

            LogDebug(LOG_HOST, "[ OK ] %s", threadName.c_str());
    #ifdef _GNU_SOURCE
            ::pthread_setname_np(th->thread, threadName.c_str());
    #endif // _GNU_SOURCE

            bool killed = wnd->m_killed;
            while (!killed) {
                if (wnd != nullptr) {
                    if (wnd->isShown()) {
                        wnd->m_scroll.update();
                        wnd->redraw();

                        killed = wnd->m_killed;
                    }

                    Thread::sleep(250U);
                } else {
                    break;
                }
            }

            LogDebug(LOG_HOST, "[STOP] %s", threadName.c_str());
            delete th;
        }

        return nullptr;
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
        if (key == FKey::Escape) {
            this->close();
        }
    }

    /**
     * @brief Event that occurs when the window is closed.
     * @param e Close event.
     */
    void onClose(FCloseEvent* e) override
    {
        m_killed = true;
        Thread::sleep(1250U); // this is a horrible way of making sure the update thread stops...

        hide();
    }
};

#endif // __NODE_STATUS_WND_H__