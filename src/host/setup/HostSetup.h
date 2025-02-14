// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017,2018 Andy Uribe, CA6JAU
 *  Copyright (C) 2021-2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup setup Host Setup
 * @brief Implementation for the interactive host setup TUI.
 * @ingroup host
 * 
 * @file HostSetup.h
 * @ingroup setup
 * @file HostSetup.cpp
 * @ingroup setup
 */
#if !defined(__HOST_SETUP_H__)
#define __HOST_SETUP_H__

#include "Defines.h"
#include "common/edac/AMBEFEC.h"
#include "common/lookups/IdenTableLookup.h"
#include "common/yaml/Yaml.h"
#include "common/RingBuffer.h"
#include "common/StopWatch.h"
#include "modem/Modem.h"
#include "Host.h"

#include <string>

#if !defined(CATCH2_TEST_COMPILATION)

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define DMR_CAL_STR         "[Tx] DMR 1200 Hz Tone Mode (2.75Khz Deviation)"
#define P25_CAL_STR         "[Tx] P25 1200 Hz Tone Mode (2.83Khz Deviation)"
#define DMR_LF_CAL_STR      "[Tx] DMR Low Frequency Mode (80 Hz square wave)"
#define DMR_CAL_1K_STR      "[Tx] DMR BS 1031 Hz Test Pattern (TS2 CC1 ID1 TG9)"
#define DMR_DMO_CAL_1K_STR  "[Tx] DMR MS 1031 Hz Test Pattern (TS2 CC1 ID1 TG9)"
#define P25_CAL_1K_STR      "[Tx] P25 1011 Hz Test Pattern (NAC293 ID1 TG1)"
#define P25_TDU_TEST_STR    "[Tx] P25 TDU Data Test Stream"
#define NXDN_CAL_1K_STR     "[Tx] NXDN 1031 Hz Test Pattern (RAN1 ID1 TG1)"
#define DMR_FEC_STR         "[Rx] DMR MS FEC BER Test Mode"
#define DMR_FEC_1K_STR      "[Rx] DMR MS 1031 Hz Test Pattern (CC1 ID1 TG9)"
#define P25_FEC_STR         "[Rx] P25 FEC BER Test Mode"
#define P25_FEC_1K_STR      "[Rx] P25 1011 Hz Test Pattern (NAC293 ID1 TG1)"
#define NXDN_FEC_STR        "[Rx] NXDN FEC BER Test Mode"
#define RSSI_CAL_STR        "RSSI Calibration Mode"

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

#if defined(ENABLE_SETUP_TUI)
class HOST_SW_API SetupApplication;
class HOST_SW_API SetupMainWnd;

class HOST_SW_API AdjustWndBase;
class HOST_SW_API CloseWndBase;
class HOST_SW_API LevelAdjustWnd;
class HOST_SW_API SymbLevelAdjustWnd;
class HOST_SW_API HSBandwidthAdjustWnd;
class HOST_SW_API HSGainAdjustWnd;
class HOST_SW_API FIFOBufferAdjustWnd;

class HOST_SW_API LoggingAndDataSetWnd;
class HOST_SW_API SystemConfigSetWnd;
class HOST_SW_API SiteParamSetWnd;
class HOST_SW_API ChannelConfigSetWnd;
#endif // defined(ENABLE_SETUP_TUI)

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements an interactive session to setup the DVM.
 * @ingroup setup
 */
class HOST_SW_API HostSetup {
public:
    /**
     * @brief Initializes a new instance of the HostSetup class.
     * @param confFile Full-path to the configuration file.
     */
    HostSetup(const std::string& confFile);
    /**
     * @brief Finalizes a instance of the HostSetup class.
     */
    virtual ~HostSetup();

#if defined(ENABLE_SETUP_TUI)
    /**
     * @brief Executes the processing loop.
     * @param argc main() argc. 
     * @param argv main() argv.
     * @returns int Exit code.
     */
    virtual int run(int argc, char** argv);
#else
    /**
     * @brief Executes the processing loop.
     * @param argc main() argc. 
     * @param argv main() argv.
     * @returns int Exit code.
     */
    virtual int run(int argc, char **argv) = 0;
#endif // defined(ENABLE_SETUP_TUI)

protected:
#if defined(ENABLE_SETUP_TUI)
    friend class SetupApplication;
    friend class SetupMainWnd;

    friend class AdjustWndBase;
    friend class CloseWndBase;
    friend class LevelAdjustWnd;
    friend class SymbLevelAdjustWnd;
    friend class HSBandwidthAdjustWnd;
    friend class HSGainAdjustWnd;
    friend class FIFOBufferAdjustWnd;

    friend class LoggingAndDataSetWnd;
    friend class SystemConfigSetWnd;
    friend class SiteParamSetWnd;
    friend class ChannelConfigSetWnd;

    SetupMainWnd* m_setupWnd;
#endif // defined(ENABLE_SETUP_TUI)
    const std::string &m_confFile;
    yaml::Node m_conf;

    StopWatch m_stopWatch;
    modem::Modem *m_modem;

    RingBuffer<uint8_t> m_queue;

    edac::AMBEFEC m_fec;
    bool m_transmit;

    bool m_duplex;
    bool m_startupDuplex;

    bool m_dmrEnabled;
    bool m_dmrRx1K;
    bool m_p25Enabled;
    bool m_p25Rx1K;
    bool m_p25TduTest;
    bool m_nxdnEnabled;

    bool m_isHotspot;

    bool m_isConnected;
    bool m_debug;

    uint8_t m_mode;
    std::string m_modeStr;

    uint32_t m_rxFrequency;         // hotspot modem - Rx Frequency
    uint32_t m_rxAdjustedFreq;
    uint32_t m_txFrequency;         // hotspot modem - Tx Frequency
    uint32_t m_txAdjustedFreq;
    uint8_t m_channelId;
    uint32_t m_channelNo;

    lookups::IdenTableLookup* m_idenTable;

    uint32_t m_berFrames;
    uint32_t m_berBits;
    uint32_t m_berErrs;
    uint32_t m_berUndecodableLC;
    uint32_t m_berUncorrectable;

    uint32_t m_timeout;
    uint32_t m_timer;

    bool m_updateConfigFromModem;
    bool m_hasFetchedStatus;
    bool m_requestedStatus;

    bool m_reqBootload;

    /**
     * @brief Modem port open callback.
     * @param modem Instance of the Modem class.
     * @returns bool True, if the modem is opened, otherwise false.
     */
    bool portModemOpen(modem::Modem* modem);
    /**
     * @brief Modem port close callback.
     * @param modem Instance of the Modem class.
     * @returns bool True, if the modem is closed, otherwise false.
     */
    bool portModemClose(modem::Modem* modem);
    /**
     * @brief Modem clock callback.
     * @param modem Instance of the Modem class.
     * @param ms 
     * @param rspType Modem message response type.
     * @param rspDblLen Flag indicating whether or not this message is a double length message.
     * @param[in] buffer Buffer containing modem message.
     * @param len Length of buffer.
     * @returns bool True, if the modem response was handled, otherwise false.
     */
    bool portModemHandler(modem::Modem* modem, uint32_t ms, modem::RESP_TYPE_DVM rspType, bool rspDblLen, const uint8_t* buffer, uint16_t len);

    /**
     * @brief Helper to save configuration.
     */
    void saveConfig();
    /**
     * @brief Helper to calculate the Rx/Tx frequencies.
     * @param consoleDisplay Flag indicating output should goto the console log.
     * @param txFrequency Transmit frequency to use (this will auto calculate the Rx frequency from Tx frequency).
     * @returns bool True, if Rx/Tx frequencies are calculated, otherwise false.
     */
    bool calculateRxTxFreq(bool consoleDisplay = false, uint32_t txFrequency = 0U);
    /**
     * @brief Helper to log the system configuration parameters.
     */
    void displayConfigParams();
    /**
     * @brief Initializes the modem DSP.
     * @param consoleDisplay Flag indicating output should goto the console log.
     * @returns bool True, if modem is initialized, otherwise false.
     */
    bool createModem(bool consoleDisplay = false);
    /**
     * @brief Helper to toggle modem transmit mode.
     * @returns bool True, if setting was applied, otherwise false.
     */
    bool setTransmit();

    /**
     * @brief Helper to update BER display window.
     * @param ber Bit Error Rate.
     */
    void updateTUIBER(float ber);
    /**
     * @brief Process DMR Rx BER.
     * @param[in] buffer DMR data to measure for BER.
     * @param seq Sequence Number.
     */
    void processDMRBER(const uint8_t* buffer, uint8_t seq);
    /**
     * @brief Process DMR Tx 1011hz BER.
     * @param[in] buffer DMR data to measure for BER.
     * @param seq Sequence Number.
     */
    void processDMR1KBER(const uint8_t* buffer, uint8_t seq);
    /**
     * @brief Process P25 Rx BER.
     * @param[in] buffer P25 data to measure for BER.
     */
    void processP25BER(const uint8_t* buffer);
    /**
     * @brief Process P25 Tx 1011hz BER.
     * @param[in] buffer P25 data to measure for BER.
     */
    void processP251KBER(const uint8_t* buffer);
    /**
     * @brief Process NXDN Rx BER.
     * @param[in] buffer NXDN data to measure for BER.
     */
    void processNXDNBER(const uint8_t* buffer);

    /**
     * @brief Write configuration to the modem DSP.
     * @returns bool True, if configuration is written, otherwise false.
     */
    bool writeConfig();
    /**
     * @brief Write configuration to the modem DSP.
     * @param modeOverride Forced modem mode override.
     * @returns bool True, if configuration is written, otherwise false.
     */
    bool writeConfig(uint8_t modeOverride);
    /**
     * @brief Write RF parameters to the air interface modem.
     * @returns bool True, if parameters are written, otherwise false.
     */
    bool writeRFParams();
    /**
     * @brief Write symbol level adjustments to the modem DSP.
     * @returns bool True, if adjustments are written, otherwise false.
     */
    bool writeSymbolAdjust();
    /**
     * @brief Write transmit FIFO buffer lengths.
     * @returns bool True, if FIFO buffer lengths are written, otherwise false.
     */
    bool writeFifoLength();
    /**
     * @brief Helper to sleep the calibration thread.
     */
    void sleep(uint32_t ms);

    /**
     * @brief Read the configuration area on the air interface modem.
     * @returns bool True, if modem flash was read, otherwise false.
     */
    bool readFlash();
    /**
     * @brief Process the configuration data from the air interface modem.
     * @param[in] buffer Buffer containing configuration data from modem.
     */
    void processFlashConfig(const uint8_t *buffer);
    /**
     * @brief Erase the configuration area on the air interface modem.
     * @returns bool True, if modem coonfiguration area was erased, otherwise false.
     */
    bool eraseFlash();
    /**
     * @brief Write the configuration area on the air interface modem.
     * @returns bool True, if modem coonfiguration area was written, otherwise false.
     */
    bool writeFlash();

    /**
     * @brief 
     */
    void writeBootload();

    /**
     * @brief Helper to clock the calibration BER timer.
     */
    void timerClock();
    /**
     * @brief Helper to start the calibration BER timer.
     */
    void timerStart();
    /**
     * @brief Helper to stop the calibration BER timer.
     */
    void timerStop();

    /**
     * @brief Retrieve the current status from the air interface modem.
     */
    void getStatus();
#if defined(ENABLE_SETUP_TUI)
    /**
     * @brief Prints the current status.
     */
    virtual void printStatus();
#else
    /**
     * @brief Prints the current status.
     */
    virtual void printStatus() = 0;
#endif // defined(ENABLE_SETUP_TUI)

    /**
     * @brief Add data frame to the data ring buffer.
     * @param[in] data Data to add to ring buffer.
     * @param length Length of data.
     * @param maxFrameSize 
     */
    void addFrame(const uint8_t* data, uint32_t length, uint32_t maxFrameSize);

    /**
     * @brief Counts the total number of bit errors between bytes.
     * @param a Byte 1.
     * @param b Byte 2.
     * @returns uint8_t Number of bit errors between bytes.
     */
    uint8_t countErrs(uint8_t a, uint8_t b);
};

#endif // !defined(CATCH2_TEST_COMPILATION)

#endif // __HOST_SETUP_H__
