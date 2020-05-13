/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2011-2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2018 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__MODEM_H__)
#define __MODEM_H__

#include "Defines.h"
#include "modem/SerialController.h"
#include "RingBuffer.h"
#include "Timer.h"

#include <string>

#define MODEM_VERSION_STR "%.*s, Modem protocol: %u"
#define NULL_MODEM "null"

namespace modem
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    enum RESP_TYPE_DVM {
        RTM_OK,
        RTM_TIMEOUT,
        RTM_ERROR
    };

    enum DVM_STATE {
        STATE_IDLE = 0U,
        // DMR
        STATE_DMR = 1U,
        // Project 25
        STATE_P25 = 2U,

        // CW
        STATE_CW = 10U,

        // Symbol Tests
        STATE_DMR_LEVELA = 70U,
        STATE_DMR_LEVELB = 71U,
        STATE_DMR_LEVELC = 72U,
        STATE_DMR_LEVELD = 73U,
        STATE_P25_LEVELA = 74U,
        STATE_P25_LEVELB = 75U,
        STATE_P25_LEVELC = 77U,
        STATE_P25_LEVELD = 78U,

        // Calibration States
        STATE_P25_CAL_1K = 92U,

        STATE_DMR_DMO_CAL_1K = 93U,
        STATE_DMR_CAL_1K = 94U,

        STATE_LF_CAL = 95U,

        STATE_RSSI_CAL = 96U,

        STATE_P25_CAL = 97U,
        STATE_DMR_CAL = 98U
    };

    enum DVM_COMMANDS {
        CMD_GET_VERSION = 0x00U,
        CMD_GET_STATUS = 0x01U,
        CMD_SET_CONFIG = 0x02U,
        CMD_SET_MODE = 0x03U,

        CMD_SET_RXLEVEL = 0x05U,

        CMD_CAL_DATA = 0x08U,
        CMD_RSSI_DATA = 0x09U,

        CMD_SEND_CWID = 0x0AU,

        CMD_DMR_DATA1 = 0x18U,
        CMD_DMR_LOST1 = 0x19U,
        CMD_DMR_DATA2 = 0x1AU,
        CMD_DMR_LOST2 = 0x1BU,
        CMD_DMR_SHORTLC = 0x1CU,
        CMD_DMR_START = 0x1DU,
        CMD_DMR_ABORT = 0x1EU,

        CMD_P25_DATA = 0x31U,
        CMD_P25_LOST = 0x32U,
        CMD_P25_CLEAR = 0x33U,

        CMD_ACK = 0x70U,
        CMD_NAK = 0x7FU,

        CMD_SERIAL = 0x80U,

        CMD_DEBUG1 = 0xF1U,
        CMD_DEBUG2 = 0xF2U,
        CMD_DEBUG3 = 0xF3U,
        CMD_DEBUG4 = 0xF4U,
        CMD_DEBUG5 = 0xF5U,
    };

    enum CMD_REASON_CODE {
        RSN_OK = 0U,
        RSN_NAK = 1U,

        RSN_ILLEGAL_LENGTH = 2U,
        RSN_INVALID_REQUEST = 4U,
        RSN_RINGBUFF_FULL = 8U,

        RSN_INVALID_FDMA_PREAMBLE = 10U,
        RSN_INVALID_MODE = 11U,

        RSN_INVALID_DMR_CC = 12U,
        RSN_INVALID_DMR_SLOT = 13U,
        RSN_INVALID_DMR_START = 14U,
        RSN_INVALID_DMR_RX_DELAY = 15U,

        RSN_DMR_DISABLED = 63U,
        RSN_P25_DISABLED = 64U,
    };

    const uint8_t DVM_FRAME_START = 0xFEU;

    const uint8_t MAX_FDMA_PREAMBLE = 255U;

    const uint32_t MAX_RESPONSES = 30U;
    const uint32_t BUFFER_LENGTH = 2000U;

    const uint32_t MAX_ADC_OVERFLOW = 128U;
    const uint32_t MAX_DAC_OVERFLOW = 128U;

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements the core interface to the modem hardware.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Modem {
    public:
        /// <summary>Initializes a new instance of the Modem class.</summary>
        Modem(const std::string& port, bool duplex, bool rxInvert, bool txInvert, bool pttInvert, bool dcBlocker,
            bool cosLockout, uint8_t fdmaPreamble, uint8_t dmrRxDelay, bool disableOFlowReset, bool trace, bool debug);
        /// <summary>Finalizes a instance of the Modem class.</summary>
        virtual ~Modem();

        /// <summary>Sets the modem DSP RF DC offset parameters.</summary>
        virtual void setDCOffsetParams(int txDCOffset, int rxDCOffset);
        /// <summary>Sets the modem DSP enabled modes.</summary>
        virtual void setModeParams(bool dmrEnabled, bool p25Enabled);
        /// <summary>Sets the modem DSP RF deviation levels.</summary>
        virtual void setLevels(float rxLevel, float cwIdTXLevel, float dmrTXLevel, float p25TXLevel);
        /// <summary>Sets the modem DSP DMR color code.</summary>
        virtual void setDMRParams(uint32_t colorCode);
        /// <summary>Sets the modem DSP RF receive deviation levels.</summary>
        virtual void setRXLevel(float rxLevel);

        /// <summary>Opens connection to the modem DSP.</summary>
        virtual bool open();

        /// <summary>Updates the modem by the passed number of milliseconds.</summary>
        virtual void clock(uint32_t ms);

        /// <summary>Closes connection to the modem DSP.</summary>
        virtual void close();

        /// <summary>Reads DMR Slot 1 frame data from the DMR Slot 1 ring buffer.</summary>
        virtual uint32_t readDMRData1(uint8_t* data);
        /// <summary>Reads DMR Slot 2 frame data from the DMR Slot 1 ring buffer.</summary>
        virtual uint32_t readDMRData2(uint8_t* data);
        /// <summary>Reads P25 frame data from the P25 ring buffer.</summary>
        virtual uint32_t readP25Data(uint8_t* data);

        /// <summary>Helper to test if the DMR Slot 1 ring buffer has free space.</summary>
        virtual bool hasDMRSpace1() const;
        /// <summary>Helper to test if the DMR Slot 2 ring buffer has free space.</summary>
        virtual bool hasDMRSpace2() const;
        /// <summary>Helper to test if the P25 ring buffer has free space.</summary>
        virtual bool hasP25Space() const;

        /// <summary>Flag indicating whether or not the modem DSP is transmitting.</summary>
        virtual bool hasTX() const;
        /// <summary>Flag indicating whether or not the modem DSP has carrier detect.</summary>
        virtual bool hasCD() const;

        /// <summary>Flag indicating whether or not the modem DSP is currently locked out.</summary>
        virtual bool hasLockout() const;
        /// <summary>Flag indicating whether or not the modem DSP is currently in an error condition.</summary>
        virtual bool hasError() const;

        /// <summary>Clears any buffered DMR Slot 1 frame data to be sent to the modem DSP.</summary>
        void clearDMRData1();
        /// <summary>Clears any buffered DMR Slot 2 frame data to be sent to the modem DSP.</summary>
        void clearDMRData2();
        /// <summary>Clears any buffered P25 frame data to be sent to the modem DSP.</summary>
        void clearP25Data();

        /// <summary>Internal helper to inject DMR Slot 1 frame data as if it came from the modem DSP.</summary>
        void injectDMRData1(const uint8_t* data, uint32_t length);
        /// <summary>Internal helper to inject DMR Slot 2 frame data as if it came from the modem DSP.</summary>
        void injectDMRData2(const uint8_t* data, uint32_t length);
        /// <summary>Internal helper to inject P25 frame data as if it came from the modem DSP.</summary>
        void injectP25Data(const uint8_t* data, uint32_t length);

        /// <summary>Writes DMR Slot 1 frame data to the DMR Slot 1 ring buffer.</summary>
        virtual bool writeDMRData1(const uint8_t* data, uint32_t length);
        /// <summary>Writes DMR Slot 2 frame data to the DMR Slot 2 ring buffer.</summary>
        virtual bool writeDMRData2(const uint8_t* data, uint32_t length);
        /// <summary>Writes P25 frame data to the P25 ring buffer.</summary>
        virtual bool writeP25Data(const uint8_t* data, uint32_t length);

        /// <summary>Triggers the start of DMR transmit.</summary>
        virtual bool writeDMRStart(bool tx);
        /// <summary>Writes a DMR short LC to the modem DSP.</summary>
        virtual bool writeDMRShortLC(const uint8_t* lc);
        /// <summary>Writes a DMR abort message for the given slot to the modem DSP.</summary>
        virtual bool writeDMRAbort(uint32_t slotNo);

        /// <summary>Sets the current operating mode for the modem DSP.</summary>
        virtual bool setMode(DVM_STATE state);

        /// <summary>Transmits the given string as CW morse.</summary>
        virtual bool sendCWId(const std::string& callsign);

        /// <summary>Helper to create an instance of the Modem class.</summary>
        static Modem* createModem(const std::string& port, bool duplex, bool rxInvert, bool txInvert, bool pttInvert, bool dcBlocker,
            bool cosLockout, uint8_t fdmaPreamble, uint8_t dmrRxDelay, bool disableOFlowReset, bool trace, bool debug);

    private:
        std::string m_port;

        uint32_t m_dmrColorCode;

        bool m_duplex;

        bool m_rxInvert;
        bool m_txInvert;
        bool m_pttInvert;

        bool m_dcBlocker;
        bool m_cosLockout;

        uint8_t m_fdmaPreamble;
        uint8_t m_dmrRxDelay;

        float m_rxLevel;
        float m_cwIdTXLevel;
        float m_dmrTXLevel;
        float m_p25TXLevel;

        bool m_disableOFlowReset;
        bool m_trace;
        bool m_debug;

        bool m_dmrEnabled;
        bool m_p25Enabled;
        int m_rxDCOffset;
        int m_txDCOffset;

        uint32_t m_adcOverFlowCount;
        uint32_t m_dacOverFlowCount;

        CSerialController m_serial;
        uint8_t* m_buffer;
        uint32_t m_length;
        uint32_t m_offset;

        RingBuffer<uint8_t> m_rxDMRData1;
        RingBuffer<uint8_t> m_rxDMRData2;
        RingBuffer<uint8_t> m_txDMRData1;
        RingBuffer<uint8_t> m_txDMRData2;
        RingBuffer<uint8_t> m_rxP25Data;
        RingBuffer<uint8_t> m_txP25Data;

        Timer m_statusTimer;
        Timer m_inactivityTimer;
        Timer m_playoutTimer;

        uint32_t m_dmrSpace1;
        uint32_t m_dmrSpace2;
        uint32_t m_p25Space;

        bool m_tx;
        bool m_cd;
        bool m_lockout;
        bool m_error;

        /// <summary>Retrieve the modem DSP version.</summary>
        bool getFirmwareVersion();
        /// <summary>Retrieve the current status from the modem DSP.</summary>
        bool getStatus();
        /// <summary>Write configuration to the modem DSP.</summary>
        bool writeConfig();

        /// <summary></summary>
        void printDebug();

        /// <summary></summary>
        RESP_TYPE_DVM getResponse();
    };
} // namespace modem

#endif // __MODEM_H__
