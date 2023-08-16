/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2021-2023 by Bryan Biedenkapp N2PLL
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
#if !defined(__HOST_SETUP_H__)
#define __HOST_SETUP_H__

#include "Defines.h"
#include "edac/AMBEFEC.h"
#include "modem/Modem.h"
#include "host/Host.h"
#include "lookups/IdenTableLookup.h"
#include "yaml/Yaml.h"
#include "RingBuffer.h"
#include "StopWatch.h"

#include <string>

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
//      This class implements an interactive session to setup the DVM.
// ---------------------------------------------------------------------------

class HOST_SW_API HostSetup {
public:
    /// <summary>Initializes a new instance of the HostSetup class.</summary>
    HostSetup(const std::string& confFile);
    /// <summary>Finalizes a instance of the HostSetup class.</summary>
    virtual ~HostSetup();

#if defined(ENABLE_SETUP_TUI)
    /// <summary>Executes the processing loop.</summary>
    virtual int run(int argc, char** argv);
#else
    /// <summary>Executes the processing loop.</summary>
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

    /// <summary>Modem port open callback.</summary>
    bool portModemOpen(modem::Modem* modem);
    /// <summary>Modem port close callback.</summary>
    bool portModemClose(modem::Modem* modem);
    /// <summary>Modem clock callback.</summary>
    bool portModemHandler(modem::Modem* modem, uint32_t ms, modem::RESP_TYPE_DVM rspType, bool rspDblLen, const uint8_t* buffer, uint16_t len);

    /// <summary>Helper to save configuration.</summary>
    void saveConfig();
    /// <summary>Helper to calculate the Rx/Tx frequencies.</summary>
    bool calculateRxTxFreq(bool consoleDisplay = false);
    /// <summary>Helper to log the system configuration parameters.</summary>
    void displayConfigParams();
    /// <summary>Initializes the modem DSP.</summary>
    bool createModem(bool consoleDisplay = false);
    /// <summary>Helper to toggle modem transmit mode.</summary>
    bool setTransmit();

    /// <summary>Helper to update BER display window.</summary>
    void updateTUIBER(float ber);
    /// <summary>Process DMR Rx BER.</summary>
    void processDMRBER(const uint8_t* buffer, uint8_t seq);
    /// <summary>Process DMR Tx 1011hz BER.</summary>
    void processDMR1KBER(const uint8_t* buffer, uint8_t seq);
    /// <summary>Process P25 Rx BER.</summary>
    void processP25BER(const uint8_t* buffer);
    /// <summary>Process P25 Tx 1011hz BER.</summary>
    void processP251KBER(const uint8_t* buffer);
    /// <summary>Process NXDN Rx BER.</summary>
    void processNXDNBER(const uint8_t* buffer);

    /// <summary>Write configuration to the modem DSP.</summary>
    bool writeConfig();
    /// <summary>Write configuration to the modem DSP.</summary>
    bool writeConfig(uint8_t modeOverride);
    /// <summary>Write RF parameters to the air interface modem.</summary>
    bool writeRFParams();
    /// <summary>Write symbol level adjustments to the modem DSP.</summary>
    bool writeSymbolAdjust();
    /// <summary>Write transmit FIFO buffer lengths.</summary>
    bool writeFifoLength();
    /// <summary>Helper to sleep the calibration thread.</summary>
    void sleep(uint32_t ms);

    /// <summary>Read the configuration area on the air interface modem.</summary>
    bool readFlash();
    /// <summary>Process the configuration data from the air interface modem.</summary>
    void processFlashConfig(const uint8_t *buffer);
    /// <summary>Erase the configuration area on the air interface modem.</summary>
    bool eraseFlash();
    /// <summary>Write the configuration area on the air interface modem.</summary>
    bool writeFlash();

    /// <summary>Helper to clock the calibration BER timer.</summary>
    void timerClock();
    /// <summary>Helper to start the calibration BER timer.</summary>
    void timerStart();
    /// <summary>Helper to stop the calibration BER timer.</summary>
    void timerStop();

    /// <summary>Retrieve the current status from the air interface modem.</summary>
    void getStatus();
#if defined(ENABLE_SETUP_TUI)
    /// <summary>Prints the current status.</summary>
    virtual void printStatus();
#else
    /// <summary>Prints the current status.</summary>
    virtual void printStatus() = 0;
#endif // defined(ENABLE_SETUP_TUI)

    /// <summary>Add data frame to the data ring buffer.</summary>
    void addFrame(const uint8_t* data, uint32_t length, uint32_t maxFrameSize);

    /// <summary>Counts the total number of bit errors between bytes.</summary>
    uint8_t countErrs(uint8_t a, uint8_t b);
};

#endif // __HOST_SETUP_H__
