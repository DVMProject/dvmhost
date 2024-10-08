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
 * @file TransmitWndBase.h
 * @ingroup fneSysView
 */
#if !defined(__TRANSMIT_WND_BASE_H__)
#define __TRANSMIT_WND_BASE_H__

#include "common/lookups/AffiliationLookup.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/lc/CSBK.h"
#include "common/dmr/lc/csbk/CSBKFactory.h"
#include "common/dmr/SlotType.h"
#include "common/dmr/Sync.h"
#include "common/p25/P25Defines.h"
#include "common/p25/lc/LC.h"
#include "common/p25/lc/TSBK.h"
#include "common/p25/lc/tsbk/TSBKFactory.h"
#include "common/p25/Sync.h"
#include "host/modem/Modem.h"
#include "SysViewMain.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the base class for transmit windows.
 * @ingroup fneSysView
 */
class HOST_SW_API TransmitWndBase : public finalcut::FDialog {
public:
    /**
     * @brief Initializes a new instance of the TransmitWndBase class.
     * @param widget 
     */
    explicit TransmitWndBase(FWidget* widget = nullptr) : FDialog{widget}
    {
        /* stub */
    }

protected:
    uint8_t m_mode = modem::STATE_DMR;

    /**
     * @brief Initializes the window layout.
     */
    void initLayout() override
    {
        FDialog::setMinimizable(true);
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
    }

    /**
     * @brief Initializes window controls.
     */
    virtual void initControls()
    {
        resizeControls();

        m_dmrSlotLabel.setGeometry(FPoint(2, 4), FSize(10, 1));
        m_dmrSlot.setGeometry(FPoint(18, 4), FSize(5, 1));
        m_dmrSlot.setRange(1, 2);
        m_dmrSlot.setValue(1);
        m_dmrSlot.setShadow(false);
    

        m_digModeGroup.setGeometry(FPoint(2, 1), FSize(56, 2));
        m_modeDMR.setPos(FPoint(1, 1));
        m_modeDMR.addCallback("toggled", [&]() {
            if (m_modeDMR.isChecked()) {
                m_mode = modem::STATE_DMR;
                m_dmrSlot.setEnable(true);
                redraw();
            }
        });

        m_modeP25.setPos(FPoint(13, 1));
        m_modeP25.addCallback("toggled", [&]() {
            if (m_modeP25.isChecked()) {
                m_mode = modem::STATE_P25;
                m_dmrSlot.setEnable(false);
                redraw();
            }
        });
/*
        m_modeNXDN.setPos(FPoint(22, 1));
        m_modeNXDN.addCallback("toggled", [&]() {
            if (m_modeNXDN.isChecked()) {
                m_mode = modem::STATE_NXDN;
                m_dmrSlot.setEnable(false);
                redraw();
            }
        });
*/

        focusFirstChild();
    }

    /**
     * @brief
     */
    void resizeControls()
    {
        // transmit button and close button logic
        m_txButton.setGeometry(FPoint(3, int(getHeight()) - 6), FSize(10, 3));
        m_txButton.addCallback("clicked", [&]() { setTransmit(); });

        m_closeButton.setGeometry(FPoint(17, int(getHeight()) - 6), FSize(9, 3));
        m_closeButton.addCallback("clicked", [&]() { hide(); });
    }

    /**
     * @brief Adjusts window size.
     */
    void adjustSize() override
    {
        FDialog::adjustSize();
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
        else if (key == FKey::F12) {
            setTransmit();
        }
    }

    /**
     * @brief Event that occurs when the window is closed.
     * @param e Close Event
     */
    void onClose(FCloseEvent* e) override
    {
        hide();
    }

protected:
    /**
     * @brief Helper to write a DMR extended function packet.
     * @param slot DMR slot number.
     * @param func 
     * @param arg 
     * @param dstId 
     */
    void writeDMR_Ext_Func(uint8_t slot, uint32_t func, uint32_t arg, uint32_t dstId)
    {
        using namespace dmr;
        using namespace dmr::defines;
        using namespace dmr::lc::csbk;

        std::unique_ptr<CSBK_EXT_FNCT> csbk = std::make_unique<CSBK_EXT_FNCT>();
        csbk->setGI(false);
        csbk->setExtendedFunction(func);
        csbk->setSrcId(arg);
        csbk->setDstId(dstId);

        LogMessage(LOG_RF, "DMR Slot %u, CSBK, %s, op = $%02X, arg = %u, tgt = %u",
            slot, csbk->toString().c_str(), func, arg, dstId);

        write_CSBK(slot, csbk.get());
    }

    /**
     * @brief Helper to write a network CSBK.
     * @param slot DMR slot number.
     * @param csbk Instance of dmr::lc::CSBK.
     */
    void write_CSBK(uint8_t slot, dmr::lc::CSBK* csbk)
    {
        using namespace dmr;
        using namespace dmr::defines;

        uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
        ::memset(data + 2U, 0x00U, DMR_FRAME_LENGTH_BYTES);

        SlotType slotType;
        slotType.setColorCode(0U);
        slotType.setDataType(DataType::CSBK);

        // Regenerate the CSBK data
        csbk->encode(data + 2U);

        // Regenerate the Slot Type
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, true);

        data::NetData dmrData;
        dmrData.setSlotNo(slot);
        dmrData.setDataType(DataType::CSBK);
        dmrData.setSrcId(csbk->getSrcId());
        dmrData.setDstId(csbk->getDstId());
        dmrData.setFLCO(csbk->getGI() ? FLCO::GROUP : FLCO::PRIVATE);
        dmrData.setN(0U);
        dmrData.setSeqNo(0U);
        dmrData.setBER(0U);
        dmrData.setRSSI(0U);

        dmrData.setData(data + 2U);

        getNetwork()->writeDMR(dmrData);
    }

    /**
     * @brief Helper to write a P25 extended function packet.
     * @param func 
     * @param arg 
     * @param dstId 
     */
    void writeP25_Ext_Func(uint32_t func, uint32_t arg, uint32_t dstId)
    {
        using namespace p25;
        using namespace p25::defines;
        using namespace p25::lc::tsbk;

        std::unique_ptr<IOSP_EXT_FNCT> iosp = std::make_unique<IOSP_EXT_FNCT>();
        iosp->setExtendedFunction(func);
        iosp->setSrcId(arg);
        iosp->setDstId(dstId);

        // class $02 is Motorola -- set the MFID properly
        if ((func >> 8) == 0x02U) {
            iosp->setMFId(MFG_MOT);
        }

        LogMessage(LOG_RF, P25_TSDU_STR ", %s, mfId = $%02X, op = $%02X, arg = %u, tgt = %u",
            iosp->toString().c_str(), iosp->getMFId(), iosp->getExtendedFunction(), iosp->getSrcId(), iosp->getDstId());

        write_TSDU(iosp.get());
    }

    /**
     * @brief Helper to write a network TSDU.
     * @param tsbk Instance of p25::lc::TSBK.
     */
    void write_TSDU(p25::lc::TSBK* tsbk)
    {
        using namespace p25;
        using namespace p25::defines;

        uint8_t data[P25_TSDU_FRAME_LENGTH_BYTES];
        ::memset(data, 0x00U, P25_TSDU_FRAME_LENGTH_BYTES);

        // Generate Sync
        Sync::addP25Sync(data);

        // network bursts have no NID

        // Generate TSBK block
        tsbk->setLastBlock(true); // always set last block -- this a Single Block TSDU
        tsbk->encode(data);

        // Add busy bits
        P25Utils::addStatusBits(data, P25_TSDU_FRAME_LENGTH_BYTES, false);

        // Set first busy bits to 1,1
        P25Utils::setStatusBits(data, P25_SS0_START, true, true);

        LogDebug(LOG_RF, P25_TSDU_STR ", lco = $%02X, mfId = $%02X, lastBlock = %u, AIV = %u, EX = %u, srcId = %u, dstId = %u, sysId = $%03X, netId = $%05X",
            tsbk->getLCO(), tsbk->getMFId(), tsbk->getLastBlock(), tsbk->getAIV(), tsbk->getEX(), tsbk->getSrcId(), tsbk->getDstId(),
            tsbk->getSysId(), tsbk->getNetId());

        Utils::dump(1U, "!!! *TSDU (SBF) TSBK Block Data", data + P25_PREAMBLE_LENGTH_BYTES, P25_TSBK_FEC_LENGTH_BYTES);

        lc::LC lc = lc::LC();
        lc.setLCO(tsbk->getLCO());
        lc.setMFId(tsbk->getMFId());
        lc.setSrcId(tsbk->getSrcId());
        lc.setDstId(tsbk->getDstId());

        getNetwork()->writeP25TSDU(lc, data);
    }

    /**
     * @brief Helper to transmit.
     */
    virtual void setTransmit()
    {
        /* stub */
    }

    FButton m_txButton{"Transmit", this};
    FButton m_closeButton{"Close", this};

    FButtonGroup m_digModeGroup{"Digital Mode", this};
    FRadioButton m_modeDMR{"DMR", &m_digModeGroup};
    FRadioButton m_modeP25{"P25", &m_digModeGroup};
//    FRadioButton m_modeNXDN{"NXDN", &m_digModeGroup};

    FLabel m_dmrSlotLabel{"DMR Slot: ", this};
    FSpinBox m_dmrSlot{this};
};

#endif // __TRANSMIT_WND_BASE_H__