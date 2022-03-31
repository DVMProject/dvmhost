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
*   Copyright (C) 2011-2021 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2021 by Bryan Biedenkapp N2PLL
*   Copyright (C) 2021 by Nat Moore <https://github.com/jelimoore>
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
#include "modem/port/IModemPort.h"
#include "RingBuffer.h"
#include "Timer.h"

#include <string>
#include <functional>

#define MODEM_VERSION_STR "%.*s, Modem protocol: %u"
#define MODEM_UNSUPPORTED_STR "Modem protocol: %u, unsupported! Stopping."
#define NULL_MODEM "null"

#define MODEM_OC_PORT_HANDLER bool(modem::Modem* modem)
#define MODEM_OC_PORT_HANDLER_BIND(funcAddr, classInstance) std::bind(&funcAddr, classInstance, std::placeholders::_1)
#define MODEM_RESP_HANDLER bool(modem::Modem* modem, uint32_t ms, modem::RESP_TYPE_DVM rspType, bool rspDblLen, const uint8_t* buffer, uint16_t len)
#define MODEM_RESP_HANDLER_BIND(funcAddr, classInstance) std::bind(&funcAddr, classInstance, std::placeholders::_1, std::placeholders::_2, \
    std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6)

const uint8_t PROTOCOL_VERSION = 2U;

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------
class HOST_SW_API HostCal;

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

        // Calibration States
        STATE_P25_LF_CAL = 91U,
        STATE_P25_CAL_1K = 92U,

        STATE_DMR_DMO_CAL_1K = 93U,
        STATE_DMR_CAL_1K = 94U,
        STATE_DMR_LF_CAL = 95U,

        STATE_RSSI_CAL = 96U,

        STATE_P25_CAL = 97U,
        STATE_DMR_CAL = 98U
    };

    enum DVM_COMMANDS {
        CMD_GET_VERSION = 0x00U,
        CMD_GET_STATUS = 0x01U,
        CMD_SET_CONFIG = 0x02U,
        CMD_SET_MODE = 0x03U,

        CMD_SET_SYMLVLADJ = 0x04U,
        CMD_SET_RXLEVEL = 0x05U,
        CMD_SET_RFPARAMS = 0x06U,

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

        CMD_FLSH_READ = 0xE0U,
        CMD_FLSH_WRITE = 0xE1U,

        CMD_DEBUG1 = 0xF1U,
        CMD_DEBUG2 = 0xF2U,
        CMD_DEBUG3 = 0xF3U,
        CMD_DEBUG4 = 0xF4U,
        CMD_DEBUG5 = 0xF5U,
        CMD_DEBUG_DUMP = 0xFAU,
    };

    enum CMD_TAGS {
        TAG_HEADER = 0x00U,

        TAG_DATA = 0x01U,

        TAG_LOST = 0x02U,
        TAG_EOT = 0x03U,
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

        RSN_INVALID_P25_CORR_COUNT = 16U,

        RSN_NO_INTERNAL_FLASH = 20U,
        RSN_FAILED_ERASE_FLASH = 21U,
        RSN_FAILED_WRITE_FLASH = 22U,
        RSN_FLASH_WRITE_TOO_BIG = 23U,

        RSN_HS_NO_DUAL_MODE = 32U,

        RSN_DMR_DISABLED = 63U,
        RSN_P25_DISABLED = 64U,
    };

    enum RESP_STATE {
        RESP_START,
        RESP_LENGTH1,
        RESP_LENGTH2,
        RESP_TYPE,
        RESP_DATA
    };

    enum ADF_GAIN_MODE {
        ADF_GAIN_AUTO = 0U,
        ADF_GAIN_AUTO_LIN = 1U,
        ADF_GAIN_LOW = 2U,
        ADF_GAIN_HIGH = 3U
    };

    const uint8_t DVM_FRAME_START = 0xFEU;

    const uint8_t DVM_CONF_AREA_VER = 0x01U;
    const uint8_t DVM_CONF_AREA_LEN = 246U;

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
        Modem(port::IModemPort* port, bool duplex, bool rxInvert, bool txInvert, bool pttInvert, bool dcBlocker, bool cosLockout,
            uint8_t fdmaPreamble, uint8_t dmrRxDelay, uint8_t p25CorrCount, uint8_t packetPlayoutTime, bool disableOFlowReset,
            bool ignoreModemConfigArea, bool trace, bool debug);
        /// <summary>Finalizes a instance of the Modem class.</summary>
        ~Modem();

        /// <summary>Sets the RF DC offset parameters.</summary>
        void setDCOffsetParams(int txDCOffset, int rxDCOffset);
        /// <summary>Sets the enabled modes.</summary>
        void setModeParams(bool dmrEnabled, bool p25Enabled);
        /// <summary>Sets the RF deviation levels.</summary>
        void setLevels(float rxLevel, float cwIdTXLevel, float dmrTXLevel, float p25TXLevel);
        /// <summary>Sets the symbol adjustment levels.</summary>
        void setSymbolAdjust(int dmrSymLevel3Adj, int dmrSymLevel1Adj, int p25SymLevel3Adj, int p25SymLevel1Adj);
        /// <summary>Sets the RF parameters.</summary>
        void setRFParams(uint32_t rxFreq, uint32_t txFreq, int rxTuning, int txTuning, uint8_t rfPower, int8_t dmrDiscBWAdj, int8_t p25DiscBWAdj,
            int8_t dmrPostBWAdj, int8_t p25PostBWAdj, ADF_GAIN_MODE gainMode);
        /// <summary>Sets the DMR color code.</summary>
        void setDMRColorCode(uint32_t colorCode);
        /// <summary>Sets the P25 NAC.</summary>
        void setP25NAC(uint32_t nac);
        /// <summary>Sets the P25 DFSI data mode.</summary>
        void setP25DFSI(bool dfsi);
        /// <summary>Sets the RF receive deviation levels.</summary>
        void setRXLevel(float rxLevel);

        /// <summary>Sets a custom modem response handler.</summary>
        /// <remarks>If the response handler returns true, processing will stop, otherwise it will continue.</remarks>
        void setResponseHandler(std::function<MODEM_RESP_HANDLER> handler);
        /// <summary>Sets a custom modem open port handler.</summary>
        /// <remarks>If the open handler is set, it is the responsibility of the handler to complete air interface 
        /// initialization (i.e. write configuration, etc).</remarks>
        void setOpenHandler(std::function<MODEM_OC_PORT_HANDLER> handler);
        /// <summary>Sets a custom modem close port handler.</summary>
        void setCloseHandler(std::function<MODEM_OC_PORT_HANDLER> handler);

        /// <summary>Opens connection to the air interface modem.</summary>
        bool open();

        /// <summary>Updates the modem by the passed number of milliseconds.</summary>
        void clock(uint32_t ms);

        /// <summary>Closes connection to the air interface modem.</summary>
        void close();

        /// <summary>Reads DMR Slot 1 frame data from the DMR Slot 1 ring buffer.</summary>
        uint32_t readDMRData1(uint8_t* data);
        /// <summary>Reads DMR Slot 2 frame data from the DMR Slot 1 ring buffer.</summary>
        uint32_t readDMRData2(uint8_t* data);
        /// <summary>Reads P25 frame data from the P25 ring buffer.</summary>
        uint32_t readP25Data(uint8_t* data);

        /// <summary>Helper to test if the DMR Slot 1 ring buffer has free space.</summary>
        bool hasDMRSpace1() const;
        /// <summary>Helper to test if the DMR Slot 2 ring buffer has free space.</summary>
        bool hasDMRSpace2() const;
        /// <summary>Helper to test if the P25 ring buffer has free space.</summary>
        bool hasP25Space() const;
        
        /// <summary>Helper to test if the modem is a hotspot.</summary>
        bool isHotspot() const;
        
        /// <summary>Helper to test if the modem is in P25 DFSI data mode.</summary>
        bool isP25DFSI() const;

        /// <summary>Flag indicating whether or not the air interface modem is transmitting.</summary>
        bool hasTX() const;
        /// <summary>Flag indicating whether or not the air interface modem has carrier detect.</summary>
        bool hasCD() const;

        /// <summary>Flag indicating whether or not the air interface modem is currently locked out.</summary>
        bool hasLockout() const;
        /// <summary>Flag indicating whether or not the air interface modem is currently in an error condition.</summary>
        bool hasError() const;

        /// <summary>Clears any buffered DMR Slot 1 frame data to be sent to the air interface modem.</summary>
        void clearDMRData1();
        /// <summary>Clears any buffered DMR Slot 2 frame data to be sent to the air interface modem.</summary>
        void clearDMRData2();
        /// <summary>Clears any buffered P25 frame data to be sent to the air interface modem.</summary>
        void clearP25Data();

        /// <summary>Internal helper to inject DMR Slot 1 frame data as if it came from the air interface modem.</summary>
        void injectDMRData1(const uint8_t* data, uint32_t length);
        /// <summary>Internal helper to inject DMR Slot 2 frame data as if it came from the air interface modem.</summary>
        void injectDMRData2(const uint8_t* data, uint32_t length);
        /// <summary>Internal helper to inject P25 frame data as if it came from the air interface modem.</summary>
        void injectP25Data(const uint8_t* data, uint32_t length);

        /// <summary>Writes DMR Slot 1 frame data to the DMR Slot 1 ring buffer.</summary>
        bool writeDMRData1(const uint8_t* data, uint32_t length);
        /// <summary>Writes DMR Slot 2 frame data to the DMR Slot 2 ring buffer.</summary>
        bool writeDMRData2(const uint8_t* data, uint32_t length);
        /// <summary>Writes P25 frame data to the P25 ring buffer.</summary>
        bool writeP25Data(const uint8_t* data, uint32_t length);

        /// <summary>Triggers the start of DMR transmit.</summary>
        bool writeDMRStart(bool tx);
        /// <summary>Writes a DMR short LC to the air interface modem.</summary>
        bool writeDMRShortLC(const uint8_t* lc);
        /// <summary>Writes a DMR abort message for the given slot to the air interface modem.</summary>
        bool writeDMRAbort(uint32_t slotNo);

        /// <summary>Writes raw data to the air interface modem.</summary>
        int write(const uint8_t* data, uint32_t length);

        /// <summary>Gets the current operating state for the air interface modem.</summary>
        DVM_STATE getState() const;
        /// <summary>Sets the current operating state for the air interface modem.</summary>
        bool setState(DVM_STATE state);

        /// <summary>Transmits the given string as CW morse.</summary>
        bool sendCWId(const std::string& callsign);

    private:
        friend class ::HostCal;

        port::IModemPort* m_port;

        uint32_t m_dmrColorCode;
        uint32_t m_p25NAC;

        bool m_duplex;

        bool m_rxInvert;                // dedicated modem - Rx signal inversion
        bool m_txInvert;                // dedicated modem - Tx signal inversion
        bool m_pttInvert;               // dedicated modem - PTT signal inversion

        bool m_dcBlocker;               // dedicated modem - DC blocker
        bool m_cosLockout;              // dedicated modem - COS lockout

        uint8_t m_fdmaPreamble;
        uint8_t m_dmrRxDelay;
        uint8_t m_p25CorrCount;

        float m_rxLevel;                // dedicated/hotspot modem - Rx modulation level
        float m_cwIdTXLevel;            // dedicated/hotspot modem - CW ID Tx modulation level
        float m_dmrTXLevel;             // dedicated/hotspot modem - DMR Tx modulation level
        float m_p25TXLevel;             // dedicated/hotspot modem - P25 Tx modulation level

        bool m_disableOFlowReset;

        bool m_dmrEnabled;
        bool m_p25Enabled;
        int m_rxDCOffset;               // dedicated modem - Rx signal DC offset
        int m_txDCOffset;               // dedicated modem - Tx signal DC offset

        bool m_isHotspot;

        uint32_t m_rxFrequency;         // hotspot modem - Rx Frequency
        int m_rxTuning;                 // hotspot modem - Rx Frequency Offset
        uint32_t m_txFrequency;         // hotspot modem - Tx Frequency
        int m_txTuning;                 // hotspot modem - Tx Frequency Offset
        uint8_t m_rfPower;              // hotspot modem - RF power

        int8_t m_dmrDiscBWAdj;          // hotspot modem - DMR discriminator BW adjustment    
        int8_t m_p25DiscBWAdj;          // hotspot modem - P25 discriminator BW adjustment
        int8_t m_dmrPostBWAdj;          // hotspot modem - DMR post demod BW adjustment
        int8_t m_p25PostBWAdj;          // hotspot modem - P25 post demod BW adjustment

        ADF_GAIN_MODE m_adfGainMode;    // hotspot modem - ADF7021 Rx gain

        int m_dmrSymLevel3Adj;          // dedicated modem - +3/-3 DMR symbol adjustment
        int m_dmrSymLevel1Adj;          // dedicated modem - +1/-1 DMR symbol adjustment
        int m_p25SymLevel3Adj;          // dedicated modem - +3/-3 P25 symbol adjustment
        int m_p25SymLevel1Adj;          // dedicated modem - +1/-1 P25 symbol adjustment

        uint32_t m_adcOverFlowCount;    // dedicated modem - ADC overflow count
        uint32_t m_dacOverFlowCount;    // dedicated modem - DAC overflow count

        DVM_STATE m_modemState;

        uint8_t* m_buffer;
        uint16_t m_length;
        uint16_t m_rspOffset;
        RESP_STATE m_rspState;
        bool m_rspDoubleLength;
        DVM_COMMANDS m_rspType;

        std::function<MODEM_OC_PORT_HANDLER> m_openPortHandler;
        std::function<MODEM_OC_PORT_HANDLER> m_closePortHandler;
        std::function<MODEM_RESP_HANDLER> m_rspHandler;

        RingBuffer<uint8_t> m_rxDMRData1;
        RingBuffer<uint8_t> m_rxDMRData2;
        RingBuffer<uint8_t> m_txDMRData1;
        RingBuffer<uint8_t> m_txDMRData2;
        RingBuffer<uint8_t> m_rxP25Data;
        RingBuffer<uint8_t> m_txP25Data;

        bool m_useDFSI;

        Timer m_statusTimer;
        Timer m_inactivityTimer;

        uint32_t m_dmrSpace1;
        uint32_t m_dmrSpace2;
        uint32_t m_p25Space;

        bool m_tx;
        bool m_cd;
        bool m_lockout;
        bool m_error;

        bool m_ignoreModemConfigArea;
        bool m_flashDisabled;

        /// <summary>Retrieve the air interface modem version.</summary>
        bool getFirmwareVersion();
        /// <summary>Retrieve the current status from the air interface modem.</summary>
        bool getStatus();
        /// <summary>Write configuration to the air interface modem.</summary>
        bool writeConfig();
        /// <summary>Write symbol level adjustments to the air interface modem.</summary>
        bool writeSymbolAdjust();
        /// <summary>Write RF parameters to the air interface modem.</summary>
        bool writeRFParams();

        /// <summary>Retrieve the data from the configuration area on the air interface modem.</summary>
        bool readFlash();
        /// <summary>Process the configuration data from the air interface modem.</summary>
        void processFlashConfig(const uint8_t *buffer);

        /// <summary>Print debug air interface messages to the host log.</summary>
        void printDebug(const uint8_t* buffer, uint16_t len);

        /// <summary>Helper to get the raw response packet from modem.</summary>
        RESP_TYPE_DVM getResponse();

    public:
        /// <summary>Flag indicating if modem trace is enabled.</summary>
        __READONLY_PROPERTY(bool, trace, Trace);
        /// <summary>Flag indicating if modem debugging is enabled.</summary>
        __READONLY_PROPERTY(bool, debug, Debug);

        /// <summary>Modem packet playout timer.</summary>
        __READONLY_PROPERTY(Timer, playoutTimer, PlayoutTimer);
    };
} // namespace modem

#endif // __MODEM_H__
