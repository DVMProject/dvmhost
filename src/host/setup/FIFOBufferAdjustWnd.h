// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__FIFO_BUFFER_ADJUST_WND_H__)
#define __FIFO_BUFFER_ADJUST_WND_H__

#include "common/Thread.h"
#include "setup/HostSetup.h"

#include "setup/CloseWndBase.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the FIFO buffer adjustment window.
// ---------------------------------------------------------------------------

class HOST_SW_API FIFOBufferAdjustWnd final : public CloseWndBase {
public:
    /// <summary>
    /// Initializes a new instance of the FIFOBufferAdjustWnd class.
    /// </summary>
    /// <param name="setup"></param>
    /// <param name="widget"></param>
    explicit FIFOBufferAdjustWnd(HostSetup* setup, FWidget* widget = nullptr) : CloseWndBase{setup, widget}
    {
        /* stub */
    }

private:
    FLabel m_fifoBufferLabel{"FIFO Buffers", this};
    FLabel m_dmrBufferLabel{"DMR Buffer (bytes): ", this};
    FLabel m_p25BufferLabel{"P25 Buffer (bytes): ", this};
    FLabel m_nxdnBufferLabel{"NXDN Buffer (bytes): ", this};

    FSpinBox m_dmrBuffer{this};
    FSpinBox m_p25Buffer{this};
    FSpinBox m_nxdnBuffer{this};

    /// <summary>
    ///
    /// </summary>
    void initLayout() override
    {
        FDialog::setText("FIFO Buffer Adjustment");
        FDialog::setSize(FSize{60, 13});

        m_enableSetButton = true;
        CloseWndBase::initLayout();
    }

    /// <summary>
    ///
    /// </summary>
    void initControls() override
    {
        // symbol levels
        {
            m_fifoBufferLabel.setGeometry(FPoint(2, 1), FSize(20, 2));
            m_fifoBufferLabel.setEmphasis();
            m_fifoBufferLabel.setAlignment(Align::Center);

            m_dmrBufferLabel.setGeometry(FPoint(2, 3), FSize(25, 1));
            m_dmrBuffer.setGeometry(FPoint(28, 3), FSize(10, 1));
            m_dmrBuffer.setRange(DMR_TX_BUFFER_LEN, 65535);
            m_dmrBuffer.setValue(m_setup->m_modem->m_dmrFifoLength);
            m_dmrBuffer.setShadow(false);
            m_dmrBuffer.addCallback("changed", [&]() {
                m_setup->m_modem->m_dmrFifoLength = m_dmrBuffer.getValue();
            });

            m_p25BufferLabel.setGeometry(FPoint(2, 4), FSize(25, 1));
            m_p25Buffer.setGeometry(FPoint(28, 4), FSize(10, 1));
            m_p25Buffer.setRange(P25_TX_BUFFER_LEN, 65535);
            m_p25Buffer.setValue(m_setup->m_modem->m_p25FifoLength);
            m_p25Buffer.setShadow(false);
            m_p25Buffer.addCallback("changed", [&]() {
                m_setup->m_modem->m_p25FifoLength = m_p25Buffer.getValue();
            });

            m_nxdnBufferLabel.setGeometry(FPoint(2, 5), FSize(25, 1));
            m_nxdnBuffer.setGeometry(FPoint(28, 5), FSize(10, 1));
            m_nxdnBuffer.setRange(NXDN_TX_BUFFER_LEN, 65535);
            m_nxdnBuffer.setValue(m_setup->m_modem->m_nxdnFifoLength);
            m_nxdnBuffer.setShadow(false);
            m_nxdnBuffer.addCallback("changed", [&]() {
                m_setup->m_modem->m_nxdnFifoLength = m_nxdnBuffer.getValue();
            });
        }

        m_setButton.addCallback("clicked", [&]() {
            Thread::sleep(2);
            m_setup->writeFifoLength();
        });

        CloseWndBase::initControls();
    }
};

#endif // __FIFO_BUFFER_ADJUST_WND_H__