// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2011-2021 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
 *  Copyright (C) 2021 Nat Moore
 *
 */
/**
 * @defgroup modem Modem Interface
 * @brief Implementation for the modem interface.
 * @ingroup host
 * 
 * @defgroup port Modem Ports
 * @brief Implementation for various modem ports.
 * @ingroup modem
 * 
 * @file Modem.h
 * @ingroup modem
 * @file Modem.cpp
 * @ingroup modem
 */
#if !defined(__MODEM_H__)
#define __MODEM_H__

#include "Defines.h"
#include "common/RingBuffer.h"
#include "common/Timer.h"
#include "modem/port/IModemPort.h"
#include "network/RESTAPI.h"

#include <string>
#include <functional>
#include <mutex>

/**
 * @addtogroup modem
 * @{
 */

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define MODEM_VERSION_STR "%.*s, Modem protocol: %u"
#define MODEM_UNSUPPORTED_STR "Modem protocol: %u, unsupported! Stopping."
#define NULL_MODEM "null"

// 505 = DMR_FRAME_LENGTH_BYTES * 15 + 10 (BUFFER_LEN = DMR_FRAME_LENGTH_BYTES * NO_OF_FRAMES + 10)
#define DMR_TX_BUFFER_LEN 505U  // 15 frames + pad
// 522 = P25_PDU_FRAME_LENGTH_BYTES + 10 (BUFFER_LEN = P25_PDU_FRAME_LENGTH_BYTES + 10)
#define P25_TX_BUFFER_LEN 522U  // 1 PDU frame + pad
// 538 = NXDN_FRAME_LENGTH_BYTES * 11 + 10 (BUFFER_LEN = NXDN_FRAME_LENGTH_BYTES * NO_OF_FRAMES)
#define NXDN_TX_BUFFER_LEN 538U // 11 frames + pad

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

#define MODEM_OC_PORT_HANDLER bool(modem::Modem* modem)
#define MODEM_OC_PORT_HANDLER_BIND(funcAddr, classInstance) std::bind(&funcAddr, classInstance, std::placeholders::_1)
#define MODEM_RESP_HANDLER bool(modem::Modem* modem, uint32_t ms, modem::RESP_TYPE_DVM rspType, bool rspDblLen, const uint8_t* buffer, uint16_t len)
#define MODEM_RESP_HANDLER_BIND(funcAddr, classInstance) std::bind(&funcAddr, classInstance, std::placeholders::_1, std::placeholders::_2, \
    std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6)

/** @} */

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

class HOST_SW_API Host;
class HOST_SW_API HostCal;
class HOST_SW_API RESTAPI;

class HOST_SW_API HostSetup;
#if defined(ENABLE_SETUP_TUI)        
class HOST_SW_API SetupApplication;
class HOST_SW_API SetupMainWnd;

class HOST_SW_API LevelAdjustWnd;
class HOST_SW_API SymbLevelAdjustWnd;
class HOST_SW_API HSBandwidthAdjustWnd;
class HOST_SW_API HSGainAdjustWnd;
class HOST_SW_API FIFOBufferAdjustWnd;
#endif // defined(ENABLE_SETUP_TUI)

namespace modem
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    /**
     * @addtogroup modem
     * @{
     */

    /**
     * @brief Modem response types.
     */
    enum RESP_TYPE_DVM {
        RTM_OK,                             //! OK
        RTM_TIMEOUT,                        //! Timeout
        RTM_ERROR                           //! Error
    };

    /**
     * @brief Modem operation states.
     */
    enum DVM_STATE {
        STATE_IDLE = 0U,                    //! Idle
        // DMR
        STATE_DMR = 1U,                     //! DMR
        // Project 25
        STATE_P25 = 2U,                     //! Project 25
        // NXDN
        STATE_NXDN = 3U,                    //! NXDN

        // CW
        STATE_CW = 10U,                     //! Continuous Wave

        // Calibration States
        STATE_P25_CAL_1K = 92U,             //! Project 25 Calibration 1K

        STATE_DMR_DMO_CAL_1K = 93U,         //! DMR DMO Calibration 1K
        STATE_DMR_CAL_1K = 94U,             //! DMR Calibration 1K
        STATE_DMR_LF_CAL = 95U,             //! DMR Low Frequency Calibration

        STATE_RSSI_CAL = 96U,               //! RSSI Calibration

        STATE_P25_CAL = 97U,                //! Project 25 Calibration
        STATE_DMR_CAL = 98U,                //! DMR Calibration
        STATE_NXDN_CAL = 99U                //! NXDN Calibration
    };

    /**
     * @brief Modem commands.
     */
    enum DVM_COMMANDS {
        CMD_GET_VERSION = 0x00U,            //! Get Modem Version
        CMD_GET_STATUS = 0x01U,             //! Get Modem Status
        CMD_SET_CONFIG = 0x02U,             //! Set Modem Configuration
        CMD_SET_MODE = 0x03U,               //! Set Modem Mode

        CMD_SET_SYMLVLADJ = 0x04U,          //! Set Symbol Level Adjustments
        CMD_SET_RXLEVEL = 0x05U,            //! Set Rx Level
        CMD_SET_RFPARAMS = 0x06U,           //! (Hotspot) Set RF Parameters

        CMD_CAL_DATA = 0x08U,               //! Calibration Data
        CMD_RSSI_DATA = 0x09U,              //! RSSI Data

        CMD_SEND_CWID = 0x0AU,              //! Send Continous Wave ID (Morse)

        CMD_SET_BUFFERS = 0x0FU,            //! Set FIFO Buffer Lengths

        CMD_DMR_DATA1 = 0x18U,              //! DMR Data Slot 1
        CMD_DMR_LOST1 = 0x19U,              //! DMR Data Lost Slot 1
        CMD_DMR_DATA2 = 0x1AU,              //! DMR Data Slot 2
        CMD_DMR_LOST2 = 0x1BU,              //! DMR Data Lost Slot 2
        CMD_DMR_SHORTLC = 0x1CU,            //! DMR Short Link Control
        CMD_DMR_START = 0x1DU,              //! DMR Start Transmit
        CMD_DMR_ABORT = 0x1EU,              //! DMR Abort
        CMD_DMR_CACH_AT_CTRL = 0x1FU,       //! DMR Set CACH AT Control
        CMD_DMR_CLEAR1 = 0x20U,             //! DMR Clear Slot 1 Buffer
        CMD_DMR_CLEAR2 = 0x21U,             //! DMR Clear Slot 2 Buffer

        CMD_P25_DATA = 0x31U,               //! Project 25 Data
        CMD_P25_LOST = 0x32U,               //! Project 25 Data Lost
        CMD_P25_CLEAR = 0x33U,              //! Project 25 Clear Buffer

        CMD_NXDN_DATA = 0x41U,              //! NXDN Data
        CMD_NXDN_LOST = 0x42U,              //! NXDN Data Lost
        CMD_NXDN_CLEAR = 0x43U,             //! NXDN Clear Buffer

        CMD_ACK = 0x70U,                    //! Command ACK
        CMD_NAK = 0x7FU,                    //! Command NACK

        CMD_FLSH_READ = 0xE0U,              //! Read Flash Partition
        CMD_FLSH_WRITE = 0xE1U,             //! Write Flash Partition

        CMD_RESET_MCU = 0xEAU,              //! Soft Reboot MCU

        CMD_DEBUG1 = 0xF1U,                 //!
        CMD_DEBUG2 = 0xF2U,                 //!
        CMD_DEBUG3 = 0xF3U,                 //!
        CMD_DEBUG4 = 0xF4U,                 //!
        CMD_DEBUG5 = 0xF5U,                 //!
        CMD_DEBUG_DUMP = 0xFAU,             //!
    };

    /**
     * @brief Modem command tags.
     */
    enum CMD_TAGS {
        TAG_HEADER = 0x00U,                 //! Header

        TAG_DATA = 0x01U,                   //! Data

        TAG_LOST = 0x02U,                   //! Lost Data
        TAG_EOT = 0x03U,                    //! End of Transmission
    };

    /**
     * @brief Modem response reason codes.
     */
    enum CMD_REASON_CODE {
        RSN_OK = 0U,                        //! OK
        RSN_NAK = 1U,                       //! Negative Acknowledge

        RSN_ILLEGAL_LENGTH = 2U,            //! Illegal Length
        RSN_INVALID_REQUEST = 4U,           //! Invalid Request
        RSN_RINGBUFF_FULL = 8U,             //! Ring Buffer Full

        RSN_INVALID_FDMA_PREAMBLE = 10U,    //! Invalid FDMA Preamble Length
        RSN_INVALID_MODE = 11U,             //! Invalid Mode

        RSN_INVALID_DMR_CC = 12U,           //! Invalid DMR CC
        RSN_INVALID_DMR_SLOT = 13U,         //! Invalid DMR Slot
        RSN_INVALID_DMR_START = 14U,        //! Invaild DMR Start Transmit
        RSN_INVALID_DMR_RX_DELAY = 15U,     //! Invalid DMR Rx Delay

        RSN_INVALID_P25_CORR_COUNT = 16U,   //! Invalid P25 Correlation Count

        RSN_NO_INTERNAL_FLASH = 20U,        //! No Internal Flash
        RSN_FAILED_ERASE_FLASH = 21U,       //! Failed to erase flash partition
        RSN_FAILED_WRITE_FLASH = 22U,       //! Failed to write flash partition
        RSN_FLASH_WRITE_TOO_BIG = 23U,      //! Data to large for flash partition

        RSN_HS_NO_DUAL_MODE = 32U,          //! (Hotspot) No Dual Mode Operation

        RSN_DMR_DISABLED = 63U,             //! DMR Disabled
        RSN_P25_DISABLED = 64U,             //! Project 25 Disabled
        RSN_NXDN_DISABLED = 65U             //! NXDN Disabled
    };

    /**
     * @brief Modem response state machine.
     */
    enum RESP_STATE {
        RESP_START,                         //! Start Handling Frame
        RESP_LENGTH1,                       //! Frame Length 1
        RESP_LENGTH2,                       //! Frame Length 2
        RESP_TYPE,                          //! Frame Type
        RESP_DATA                           //! Frame Data
    };

    /**
     * @brief Hotspot gain modes.
     */
    enum ADF_GAIN_MODE {
        ADF_GAIN_AUTO = 0U,                 //! Automatic
        ADF_GAIN_AUTO_LIN = 1U,             //! Automatic (Linear)
        ADF_GAIN_LOW = 2U,                  //! Low
        ADF_GAIN_HIGH = 3U                  //! High
    };

    const uint8_t DVM_SHORT_FRAME_START = 0xFEU;
    const uint8_t DVM_LONG_FRAME_START = 0xFDU;

    const uint8_t DVM_CONF_AREA_VER = 0x02U;
    const uint8_t DVM_CONF_AREA_LEN = 246U;

    const uint8_t MAX_FDMA_PREAMBLE = 255U;

    const uint32_t MAX_RESPONSES = 30U;
    const uint32_t BUFFER_LENGTH = 2000U;

    const uint32_t MAX_ADC_OVERFLOW = 128U;
    const uint32_t MAX_DAC_OVERFLOW = 128U;

    const uint32_t MODEM_POLL_TIME = 125U;
    
    /** @} */

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the core interface to the modem hardware.
     * @ingroup modem
     */
    class HOST_SW_API Modem {
    public:
        /**
         * @brief Initializes a new instance of the Modem class.
         * @param port Port the air interface modem is connected to.
         * @param duplex Flag indicating the modem is operating in duplex mode.
         * @param rxInvert Flag indicating the Rx polarity should be inverted.
         * @param txInvert Flag indicating the Tx polarity should be inverted.
         * @param pttInvert Flag indicating the PTT polarity should be inverted.
         * @param dcBlocker Flag indicating whether the DSP DC-level blocking should be enabled.
         * @param cosLockout Flag indicating whether the COS signal should be used to lockout the modem.
         * @param fdmaPreamble Count of FDMA preambles to transmit before data. (P25/DMR DMO)
         * @param dmrRxDelay Compensate for delay in receiver audio chain in ms. Usually DSP based.
         * @param p25CorrCount P25 Correlation Countdown.
         * @param dmrQueueSize Modem DMR frame buffer queue size (bytes).
         * @param p25QueueSize Modem P25 frame buffer queue size (bytes).
         * @param nxdnQueueSize Modem NXDN frame buffer queue size (bytes).
         * @param disableOFlowReset Flag indicating whether the ADC/DAC overflow reset logic is disabled.
         * @param ignoreModemConfigArea Flag indicating whether the modem configuration area is ignored.
         * @param dumpModemStatus Flag indicating whether the modem status is dumped to the log.
         * @param trace Flag indicating whether air interface modem trace is enabled.
         * @param debug Flag indicating whether air interface modem debug is enabled.
         */
        Modem(port::IModemPort* port, bool duplex, bool rxInvert, bool txInvert, bool pttInvert, bool dcBlocker, bool cosLockout,
            uint8_t fdmaPreamble, uint8_t dmrRxDelay, uint8_t p25CorrCount, uint32_t dmrQueueSize, uint32_t p25QueueSize, uint32_t nxdnQueueSize,
            bool disableOFlowReset, bool ignoreModemConfigArea, bool dumpModemStatus, bool trace, bool debug);
        /**
         * @brief Finalizes a instance of the Modem class.
         */
        virtual ~Modem();

        /**
         * @brief Sets the RF DC offset parameters.
         * @param txDCOffset Tx DC offset parameter.
         * @param rxDCOffset Rx DC offset parameter.
         */
        void setDCOffsetParams(int txDCOffset, int rxDCOffset);
        /**
         * @brief Sets the enabled modes.
         * @param dmrEnabled Flag indicating DMR processing is enabled.
         * @param p25Enabled Flag indicating P25 processing is enabled.
         * @param nxdnEnabled Flag indicating NXDN processing is enabled.
         */
        void setModeParams(bool dmrEnabled, bool p25Enabled, bool nxdnEnabled);
        /**
         * @brief Sets the RF deviation levels.
         * @param rxLevel Rx Level.
         * @param cwIdTXLevel CWID Transmit Level.
         * @param dmrTXLevel DMR Transmit Level.
         * @param p25TXLevel P25 Transmit Level.
         * @param nxdnTXLevel NXDN Transmit Level.
         */
        void setLevels(float rxLevel, float cwIdTXLevel, float dmrTXLevel, float p25TXLevel, float nxdnTXLevel);
        /**
         * @brief Sets the symbol adjustment levels.
         * @param dmrSymLevel3Adj DMR +3/-3 Symbol Level Adjust.
         * @param dmrSymLevel1Adj DMR +1/-1 Symbol Level Adjust.
         * @param p25SymLevel3Adj P25 +3/-3 Symbol Level Adjust.
         * @param p25SymLevel1Adj P25 +1/-1 Symbol Level Adjust.
         * @param nxdnSymLevel3Adj NXDN +3/-3 Symbol Level Adjust.
         * @param nxdnSymLevel1Adj NXDN +1/-1 Symbol Level Adjust.
         */
        void setSymbolAdjust(int dmrSymLevel3Adj, int dmrSymLevel1Adj, int p25SymLevel3Adj, int p25SymLevel1Adj, int nxdnSymLevel3Adj, int ndxnSymLevel1Adj);
        /**
         * @brief Sets the RF parameters.
         * @param rxFreq Receive Frequency (hz).
         * @param txFreq Transmit Frequency (hz).
         * @param rfPower RF Power Level.
         * @param dmrDiscBWAdj DMR Discriminator Bandwidth Adjust.
         * @param p25DiscBWAdj P25 Discriminator Bandwidth Adjust.
         * @param nxdnDiscBWAdj NXDN Discriminator Bandwidth Adjust.
         * @param dmrPostBWAdj DMR Post Bandwidth Adjust.
         * @param p25PostBWAdj P25 Post Bandwidth Adjust.
         * @param nxdnPostBWAdj NXDN Post Bandwidth Adjust.
         * @param gainMode Gain Mode.
         * @param afcEnable Flag indicating the Automatic Frequency Correction is enabled.
         * @param afcKI 
         * @param afcKP 
         * @param afcRange 
         */
        void setRFParams(uint32_t rxFreq, uint32_t txFreq, int rxTuning, int txTuning, uint8_t rfPower, int8_t dmrDiscBWAdj, int8_t p25DiscBWAdj, int8_t nxdnDiscBWAdj,
            int8_t dmrPostBWAdj, int8_t p25PostBWAdj, int8_t nxdnPostBWAdj, ADF_GAIN_MODE gainMode, bool afcEnable, uint8_t afcKI, uint8_t afcKP, uint8_t afcRange);
        /**
         * @brief Sets the softpot parameters.
         * @param rxCoarse Rx Coarse Level.
         * @param rxFine Rx Fine Level.
         * @param txCoarse Tx Coarse Level.
         * @param txFine Tx Fine Level.
         * @param rssiCoarse RSSI Coarse Level.
         * @param rssiFine RSSI Fine Level.
         */
        void setSoftPot(uint8_t rxCoarse, uint8_t rxFine, uint8_t txCoarse, uint8_t txFine, uint8_t rssiCoarse, uint8_t rssiFine);
        /**
         * @brief Sets the DMR color code.
         * @param colorCode Color code.
         */
        void setDMRColorCode(uint32_t colorCode);
        /**
         * @brief Sets the P25 NAC.
         * @param nac NAC.
         */
        virtual void setP25NAC(uint32_t nac);
        /**
         * @brief Sets the RF receive deviation levels.
         * @param rxLevel Rx Level.
         */
        void setRXLevel(float rxLevel);
        /**
         * @brief Sets the modem transmit FIFO buffer lengths.
         * @param dmrLength DMR FIFO Buffer Length.
         * @param p25Length P25 FIFO Buffer Length.
         * @param nxdnLength NXDN FIFO Buffer Length.
         */
        void setFifoLength(uint16_t dmrLength, uint16_t p25Length, uint16_t nxdnLength);

        /**
         * @brief Sets a custom modem response handler.
         * If the response handler returns true, processing will stop, otherwise it will continue.
         * @param handler Function callback to handle modem responses.
         */
        void setResponseHandler(std::function<MODEM_RESP_HANDLER> handler);
        /**
         * @brief Sets a custom modem open port handler.
         * @param handler Function callback to handle modem open.
         */
        void setOpenHandler(std::function<MODEM_OC_PORT_HANDLER> handler);
        /**
         * @brief Sets a custom modem close port handler.
         * @param handler Function callback to handle modem close.
         */
        void setCloseHandler(std::function<MODEM_OC_PORT_HANDLER> handler);

        /**
         * @brief Opens connection to the air interface modem.
         * @returns bool True, if connection to modem is made, otherwise false.
         */
        virtual bool open();

        /**
         * @brief Updates the modem by the passed number of milliseconds.
         * @param ms Number of milliseconds.
         */
        virtual void clock(uint32_t ms);

        /**
         * @brief Closes connection to the air interface modem.
         */
        virtual void close();

        /**
         * @brief Get the frame data length for the next frame in the DMR Slot 1 ring buffer.
         * @returns uint32_t Length of frame data retrieved.
         */
        uint32_t peekDMRFrame1Length();
        /**
         * @brief Reads DMR Slot 1 frame data from the DMR Slot 1 ring buffer.
         * @param[out] data Buffer to write frame data to.
         * @returns uint32_t Length of data read from ring buffer.
         */
        uint32_t readDMRFrame1(uint8_t* data);
        /**
         * @brief Get the frame data length for the next frame in the DMR Slot 2 ring buffer.
         * @returns uint32_t Length of frame data retrieved.
         */
        uint32_t peekDMRFrame2Length();
        /**
         * @brief Reads DMR Slot 2 frame data from the DMR Slot 1 ring buffer.
         * @param[out] data Buffer to write frame data to.
         * @returns uint32_t Length of data read from ring buffer.
         */
        uint32_t readDMRFrame2(uint8_t* data);
        /**
         * @brief Get the frame data length for the next frame in the P25 ring buffer.
         * @returns uint32_t Length of frame data retrieved.
         */
        uint32_t peekP25FrameLength();
        /**
         * @brief Reads P25 frame data from the P25 ring buffer.
         * @param[out] data Buffer to write frame data to.
         * @returns uint32_t Length of data read from ring buffer.
         */
        uint32_t readP25Frame(uint8_t* data);
        /**
         * @brief Get the frame data length for the next frame in the NXDN ring buffer.
         * @returns uint32_t Length of frame data retrieved.
         */
        uint32_t peekNXDNFrameLength();
        /**
         * @brief Reads NXDN frame data from the NXDN ring buffer.
         * @param[out] data Buffer to write frame data to.
         * @returns uint32_t Length of data read from ring buffer.
         */
        uint32_t readNXDNFrame(uint8_t* data);

        /**
         * @brief Helper to test if the DMR Slot 1 ring buffer has free space.
         * @returns bool True, if the DMR Slot 1 ring buffer has free space, otherwise false.
         */
        bool hasDMRSpace1() const;
        /**
         * @brief Helper to return the currently reported available DMR Slot 1 ring buffer free space.
         * @return uint32_t Size in bytes available for the DMR Slot 1 ring buffer.
         */
        uint32_t getDMRSpace1() const { return m_dmrSpace1; }
        /**
         * @brief Helper to test if the DMR Slot 2 ring buffer has free space.
         * @returns bool True, if the DMR Slot 2 ring buffer has free space, otherwise false.
         */
        bool hasDMRSpace2() const;
        /**
         * @brief Helper to return the currently reported available DMR Slot 2 ring buffer free space.
         * @return uint32_t Size in bytes available for the DMR Slot 2 ring buffer.
         */
        uint32_t getDMRSpace2() const { return m_dmrSpace2; }
        /**
         * @brief Helper to test if the P25 ring buffer has free space.
         * @returns bool True, if the P25 ring buffer has free space, otherwise false.
         */
        virtual bool hasP25Space(uint32_t length) const;
        /**
         * @brief Helper to return the currently reported available P25 ring buffer free space.
         * @return uint32_t Size in bytes available for the P25 ring buffer.
         */
        uint32_t getP25Space() const { return m_p25Space; }
        /**
         * @brief Helper to test if the NXDN ring buffer has free space.
         * @returns bool True, if the NXDN ring buffer has free space, otherwise false.
         */
        bool hasNXDNSpace() const;
        /**
         * @brief Helper to return the currently reported available NXDN ring buffer free space.
         * @return uint32_t Size in bytes available for the NXDN ring buffer.
         */
        uint32_t getNXDNSpace() const { return m_nxdnSpace; }

        /**
         * @brief Helper to test if the modem is a hotspot.
         * @returns bool True, if the modem is a hotspot, otherwise false.
         */
        bool isHotspot() const;

        /**
         * @brief Flag indicating whether or not the air interface modem is transmitting.
         * @returns bool True, if air interface modem is transmitting, otherwise false.
         */
        bool hasTX() const;
        /**
         * @brief Flag indicating whether or not the air interface modem has carrier detect.
         * @returns bool True, if air interface modem has carrier detect, otherwise false.
         */
        bool hasCD() const;

        /**
         * @brief Flag indicating whether or not the air interface modem is currently locked out.
         * @returns bool True, if air interface modem is currently locked out, otherwise false.
         */
        bool hasLockout() const;
        /**
         * @brief Flag indicating whether or not the air interface modem is currently in an error condition.
         * @returns bool True, if the air interface modem is current in an error condition, otherwise false.
         */
        bool hasError() const;

        /**
         * @brief Flag indicating whether or not the air interface modem has sent the initial modem status.
         * @returns bool True, if the air interface modem has sent the initial status, otherwise false.
         */
        bool gotModemStatus() const;

        /**
         * @brief Clears any buffered DMR Slot 1 frame data to be sent to the air interface modem.
         */
        void clearDMRFrame1();
        /**
         * @brief Clears any buffered DMR Slot 2 frame data to be sent to the air interface modem.
         */
        void clearDMRFrame2();
        /**
         * @brief Clears any buffered P25 frame data to be sent to the air interface modem.
         */
        void clearP25Frame();
        /**
         * @brief Clears any buffered NXDN frame data to be sent to the air interface modem.
         */
        void clearNXDNFrame();

        /**
         * @brief Internal helper to inject DMR Slot 1 frame data as if it came from the air interface modem.
         * @param[in] data Data to write to ring buffer.
         * @param length Length of data to write.
         */
        void injectDMRFrame1(const uint8_t* data, uint32_t length);
        /**
         * @brief Internal helper to inject DMR Slot 2 frame data as if it came from the air interface modem.
         * @param[in] data Data to write to ring buffer.
         * @param length Length of data to write.
         */
        void injectDMRFrame2(const uint8_t* data, uint32_t length);
        /**
         * @brief Internal helper to inject P25 frame data as if it came from the air interface modem.
         * @param[in] data Data to write to ring buffer.
         * @param length Length of data to write.
         */
        void injectP25Frame(const uint8_t* data, uint32_t length);
        /**
         * @brief Internal helper to inject NXDN frame data as if it came from the air interface modem.
         * @param[in] data Data to write to ring buffer.
         * @param length Length of data to write.
         */
        void injectNXDNFrame(const uint8_t* data, uint32_t length);

        /**
         * @brief Writes DMR Slot 1 frame data to the DMR Slot 1 ring buffer.
         * @param[in] data Data to write to ring buffer.
         * @param length Length of data to write.
         * @returns bool True, if data is written, otherwise false.
         */
        bool writeDMRFrame1(const uint8_t* data, uint32_t length);
        /**
         * @brief Writes DMR Slot 2 frame data to the DMR Slot 2 ring buffer.
         * @param[in] data Data to write to ring buffer.
         * @param length Length of data to write.
         * @returns bool True, if data is written, otherwise false.
         */
        bool writeDMRFrame2(const uint8_t* data, uint32_t length);
        /**
         * @brief Writes P25 frame data to the P25 ring buffer.
         * @param[in] data Data to write to ring buffer.
         * @param length Length of data to write.
         * @returns bool True, if data is written, otherwise false.
         */
        bool writeP25Frame(const uint8_t* data, uint32_t length);
        /**
         * @brief Writes NXDN frame data to the NXDN ring buffer.
         * @param[in] data Data to write to ring buffer.
         * @param length Length of data to write.
         * @returns bool True, if data is written, otherwise false.
         */
        bool writeNXDNFrame(const uint8_t* data, uint32_t length);

        /**
         * @brief Triggers the start of DMR transmit.
         * @param tx Flag indicating whether or not to start transmitting.
         * @returns bool True, if DMR transmit started, otherwise false.
         */
        bool writeDMRStart(bool tx);
        /**
         * @brief Writes a DMR short LC to the air interface modem.
         * @param lc Buffer contianing Short LC to write.
         * @returns bool True, if DMR LC is written, otherwise false.
         */
        bool writeDMRShortLC(const uint8_t* lc);
        /**
         * @brief Writes a DMR abort message for the given slot to the air interface modem.
         * @param slotNo DMR slot to write abort for.
         * @returns bool True, if DMR abort is written, otherwise false.
         */
        bool writeDMRAbort(uint32_t slotNo);
        /**
         * @brief Sets the ignore flags for setting the CACH Access Type bit on the air interface modem.
         * @param slotNo DMR slot to set ignore CACH AT flag for.
         * @returns bool True, if set flag is written, otherwise false.
         */
        bool setDMRIgnoreCACH_AT(uint8_t slotNo);

        /**
         * @brief Writes raw data to the air interface modem.
         * @param data Data to write to modem.
         * @param length Length of data to write.
         * @returns int Actual length of data written.
         */
        virtual int write(const uint8_t* data, uint32_t length);

        /**
         * @brief Gets the current operating state for the air interface modem.
         * @returns DVM_STATE Current operating state of modem.
         */
        DVM_STATE getState() const;
        /**
         * @brief Sets the current operating state for the air interface modem.
         * @param state Operating state of modem.
         * @returns bool True, if state set, otherwise false.
         */
        bool setState(DVM_STATE state);

        /**
         * @brief Transmits the given string as CW morse.
         * @param callsign Callsign to transmit as CW morse.
         * @returns bool True, if callsign written, otherwise false.
         */
        bool sendCWId(const std::string& callsign);

        /**
         * @brief Returns the protocol version of the connected modem.
         * @returns uint8_t Protocol version of modem.
         */
        uint8_t getVersion() const;

    protected:
        friend class ::Host;
        friend class ::HostCal;
        friend class ::RESTAPI;

        friend class ::HostSetup;
#if defined(ENABLE_SETUP_TUI)        
        friend class ::SetupApplication;
        friend class ::SetupMainWnd;

        friend class ::LevelAdjustWnd;
        friend class ::SymbLevelAdjustWnd;
        friend class ::HSBandwidthAdjustWnd;
        friend class ::HSGainAdjustWnd;
        friend class ::FIFOBufferAdjustWnd;
#endif // defined(ENABLE_SETUP_TUI)

        port::IModemPort* m_port;

        uint8_t m_protoVer;

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
        float m_nxdnTXLevel;            // dedicated/hotspot modem - P25 Tx modulation level

        bool m_disableOFlowReset;

        bool m_dmrEnabled;
        bool m_p25Enabled;
        bool m_nxdnEnabled;
        int m_rxDCOffset;               // dedicated modem - Rx signal DC offset
        int m_txDCOffset;               // dedicated modem - Tx signal DC offset

        bool m_isHotspot;
        bool m_forceHotspot;

        uint32_t m_rxFrequency;         // hotspot modem - Rx Frequency
        int m_rxTuning;                 // hotspot modem - Rx Frequency Offset
        uint32_t m_txFrequency;         // hotspot modem - Tx Frequency
        int m_txTuning;                 // hotspot modem - Tx Frequency Offset
        uint8_t m_rfPower;              // hotspot modem - RF power

        int8_t m_dmrDiscBWAdj;          // hotspot modem - DMR discriminator BW adjustment
        int8_t m_p25DiscBWAdj;          // hotspot modem - P25 discriminator BW adjustment
        int8_t m_nxdnDiscBWAdj;         // hotspot modem - NXDN discriminator BW adjustment
        int8_t m_dmrPostBWAdj;          // hotspot modem - DMR post demod BW adjustment
        int8_t m_p25PostBWAdj;          // hotspot modem - P25 post demod BW adjustment
        int8_t m_nxdnPostBWAdj;         // hotspot modem - NXDN post demod BW adjustment

        ADF_GAIN_MODE m_adfGainMode;    // hotspot modem - ADF7021 Rx gain

        bool m_afcEnable;               // hotspot modem - ADF7021 AFC enable
        uint8_t m_afcKI;                // hotspot modem - AFC KI (affects AFC settling time)
        uint8_t m_afcKP;                // hotspot modem - AFC KP (affects AFC settling time)
        uint8_t m_afcRange;             // hotspot modem - AFC Maximum Range (m_afcRange * 500hz)

        int m_dmrSymLevel3Adj;          // dedicated modem - +3/-3 DMR symbol adjustment
        int m_dmrSymLevel1Adj;          // dedicated modem - +1/-1 DMR symbol adjustment
        int m_p25SymLevel3Adj;          // dedicated modem - +3/-3 P25 symbol adjustment
        int m_p25SymLevel1Adj;          // dedicated modem - +1/-1 P25 symbol adjustment
        int m_nxdnSymLevel3Adj;         // dedicated modem - +3/-3 NXDN symbol adjustment
        int m_nxdnSymLevel1Adj;         // dedicated modem - +1/-1 NXDN symbol adjustment

        uint8_t m_rxCoarsePot;          // dedicated modem - with softpot
        uint8_t m_rxFinePot;            // dedicated modem - with softpot
        uint8_t m_txCoarsePot;          // dedicated modem - with softpot
        uint8_t m_txFinePot;            // dedicated modem - with softpot
        uint8_t m_rssiCoarsePot;        // dedicated modem - with softpot
        uint8_t m_rssiFinePot;          // dedicated modem - with softpot

        uint16_t m_dmrFifoLength;
        uint16_t m_p25FifoLength;
        uint16_t m_nxdnFifoLength;

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

        RingBuffer<uint8_t> m_rxDMRQueue1;
        RingBuffer<uint8_t> m_rxDMRQueue2;
        RingBuffer<uint8_t> m_rxP25Queue;
        RingBuffer<uint8_t> m_rxNXDNQueue;

        Timer m_statusTimer;
        Timer m_inactivityTimer;

        uint32_t m_dmrSpace1;
        uint32_t m_dmrSpace2;
        uint32_t m_p25Space;
        uint32_t m_nxdnSpace;

        bool m_tx;
        bool m_cd;
        bool m_lockout;
        bool m_error;

        std::mutex m_dmr1ReadLock;
        std::mutex m_dmr2ReadLock;
        std::mutex m_p25ReadLock;
        std::mutex m_nxdnReadLock;

        bool m_ignoreModemConfigArea;
        bool m_flashDisabled;

        bool m_gotModemStatus;

        bool m_dumpModemStatus;

        /**
         * @brief Internal helper to warm reset the connection to the modem.
         */
        void reset();

        /**
         * @brief Retrieve the air interface modem version.
         * @returns bool True, if modem version was received, otherwise false.
         */
        bool getFirmwareVersion();
        /**
         * @brief Retrieve the current status from the air interface modem.
         * @returns bool True, if modem status was received, otherwise false.
         */
        bool getStatus();
        /**
         * @brief Write configuration to the air interface modem.
         * @returns bool True, if configuration was written to modem, otherwise false.
         */
        bool writeConfig();
        /**
         * @brief Write symbol level adjustments to the air interface modem.
         * @returns bool True, if symbol adjustments were written to modem, otherwise false.
         */
        bool writeSymbolAdjust();
        /**
         * @brief Write RF parameters to the air interface modem.
         * @returns bool True, if RF parameters were written to modem, otherwise false.
         */
        bool writeRFParams();

        /**
         * @brief Retrieve the data from the configuration area on the air interface modem.
         * @returns bool True, if configuration area on modem was read, otherwise false.
         */
        bool readFlash();
        /**
         * @brief Process the configuration data from the air interface modem.
         * @param[in] buffer Buffer containing the configuration data from the modem.
         */
        void processFlashConfig(const uint8_t *buffer);

        /**
         * @brief Print debug air interface messages to the host log.
         * @param[in] buffer Buffer containing debug messages.
         * @param len Length of buffer.
         */
        void printDebug(const uint8_t* buffer, uint16_t len);

        /**
         * @brief Helper to get the raw response packet from modem.
         * @returns RESP_TYPE_DVM Response type from modem.
         */
        RESP_TYPE_DVM getResponse();

        /**
         * @brief Helper to convert a serial opcode to a string.
         * @param opcode Modem command.
         * @returns std::string String representation of modem opcode.
         */
        std::string cmdToString(uint8_t opcode);
        /**
         * @brief Helper to convert a serial reason code to a string.
         * @param opcode Modem Response reason.
         * @returns std::string String representation of modem response reason.
         */
        std::string rsnToString(uint8_t reason);

    public:
        /**
         * @brief Flag indicating if modem trace is enabled.
         */
        __PROTECTED_READONLY_PROPERTY(bool, trace, Trace);
        /**
         * @brief Flag indicating if modem debugging is enabled.
         */
        __PROTECTED_READONLY_PROPERTY(bool, debug, Debug);
    };
} // namespace modem

#endif // __MODEM_H__
