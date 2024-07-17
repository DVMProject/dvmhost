// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - DFSI V.24/UDP Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *
 */
/**
 * @defgroup dfsi_network DFSI Networking/Serial Communications
 * @brief Implementation for the DFSI networking and serial communications.
 * @ingroup dfsi
 *
 * @file SerialService.h
 * @ingroup dfsi_network
 * @file SerialService.cpp
 * @ingroup dfsi_network
 */
#if !defined(__SERIAL_SERVICE_H__)
#define __SERIAL_SERVICE_H__

#include "Defines.h"
#include "common/edac/RS634717.h"
#include "common/network/RawFrameQueue.h"
#include "common/p25/data/LowSpeedData.h"
#include "common/p25/P25Defines.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/p25/dfsi/LC.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "common/yaml/Yaml.h"
#include "common/RingBuffer.h"
#include "network/DfsiPeerNetwork.h"
#include "network/CallData.h"
#include "frames/Frames.h"
#include "host/modem/Modem.h"
#include "host/modem/port/IModemPort.h"
#include "host/modem/port/UARTPort.h"
#include "host/modem/port/ModemNullPort.h"

#include <string>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <random>
#include <pthread.h>

using namespace modem;
using namespace p25;
using namespace dfsi;

namespace network
{
    /**
     * @brief DFSI serial tx flags used to determine proper jitter handling of data in ringbuffer.
     * @ingroup dfsi_network
     */
    enum SERIAL_TX_TYPE {
        NONIMBE,
        IMBE
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the serial V.24 communications service.
     * @ingroup dfsi_network
     */
    class HOST_SW_API SerialService {
    public:
        /**
         * @brief Initializes an instance of the SerialService class.
         * @param portType Serial port type.
         * @param portName Serial port device name.
         * @param baudrate Baud rate.
         * @param rtrt Flag indicating whether or not RT/RT is enabled.
         * @param diu Flag indicating whether or not V.24 communications are to a DIU.
         * @param jitter 
         * @param network Instance of the DfsiPeerNetwork class.
         * @param p25TxQueueSize Ringbuffer size for the P25 transmit queue.
         * @param p25RxQueueSize Ringbuffer size for the P24 receive queue.
         * @param callTimeout 
         * @param debug Flag indicating whether verbose debug logging is enabled.
         * @param trace Flag indicating whether verbose trace logging is enabled.
         */
        SerialService(std::string& portType, const std::string& portName, uint32_t baudrate, bool rtrt, bool diu, uint16_t jitter, DfsiPeerNetwork* network, uint32_t p25TxQueueSize, uint32_t p25RxQueueSize, uint16_t callTimeout, bool debug, bool trace);
        /**
         * @brief Finalizes an instance of the SerialService class.
         */
        ~SerialService();

        /**
         * @brief Updates the serial interface by the passed number of milliseconds.
         * @param ms Number of milliseconds.
         */
        void clock(uint32_t ms);

        /**
         * @brief Opens connection to the serial interface.
         * @returns bool True, if serial interface has started, otherwise false.
         */
        bool open();

        /**
         * @brief Closes connection to the serial interface.
         */
        void close();

        /**
         * @brief Process data frames from the network.
         * @param p25Buffer Buffer containing the network frame.
         * @param length Length of the buffer.
         */
        void processP25FromNet(UInt8Array p25Buffer, uint32_t length);

        /**
         * @brief Process data frames from the V.24 serial interface.
         * 
         *  This function pieces together LDU1/LDU2 messages from individual DFSI frames received over the serial port. It's 
         *  called multiple times before an LDU is sent, and each time adds more data pieces to the LDUs.
         */
        void processP25ToNet();

        /**
         * @brief Send the CMD_GET_VERSION to the connected V24 board to get firmware/UDID info
        */
        void getBoardInfo();

    private:
        std::string m_portName;
        uint32_t m_baudrate;
        bool m_rtrt;
        bool m_diu;

        port::IModemPort* m_port;
        uint16_t m_jitter;

        bool m_debug;
        bool m_trace;

        DfsiPeerNetwork* m_network;

        uint8_t* m_lastIMBE;

        // Tables to keep track of calls
        std::unordered_map<uint32_t, uint64_t> m_lastHeard;
        std::unordered_map<uint32_t, uint32_t> m_sequences;

        uint8_t* m_msgBuffer;
        RESP_STATE m_msgState;
        uint16_t m_msgLength;
        uint16_t m_msgOffset;
        DVM_COMMANDS m_msgType;
        bool m_msgDoubleLength;

        uint32_t m_netFrames;
        uint32_t m_netLost;

        RingBuffer<uint8_t> m_rxP25Queue;
        RingBuffer<uint8_t> m_txP25Queue;

        // Storage for V24 TX jitter buffer metering
        uint64_t m_lastP25Tx;

        edac::RS634717 m_rs;

        // Counter for assembling a full LDU from individual frames in the RX queue
        uint8_t m_rxP25LDUCounter;

        // "Mutex" flags to indicate if calls to/from the FNE are already in progress
        bool m_netCallInProgress;
        bool m_lclCallInProgress;

        // Time in ms to wait before considering a call in progress as "over" in case we miss the TDUs
        uint16_t m_callTimeout;

        // Storage for handling local/net call timeouts (for callInProgress mutexes)
        uint64_t m_lastNetFrame;
        uint64_t m_lastLclFrame;

        // Control and LSD objects for current RX P25 data being built to send to the net
        lc::LC* m_rxVoiceControl;
        data::LowSpeedData* m_rxVoiceLsd;

        // Call Data object for current RX P25 call (VHDR, LDU, etc)
        VoiceCallData* m_rxVoiceCallData;

        // The last LDU1 LC sent to the net, used to keep track of current call src/dst IDs, etc
        p25::lc::LC* m_rxLastLDU1;

        // Functions called by clock() to read/write from/to the serial port
        /**
         * @brief Read a data message from the serial interface.
         * @return RESP_TYPE_DVM 
         */
        RESP_TYPE_DVM readSerial();
        /**
         * @brief Helper to write data from the P25 Tx queue to the serial interface.
         * 
         * Very similar to the readP25Frame function. 
         * 
         * Note: the length encoded at the start does not include the length, tag, or timestamp bytes.
         * 
         * @return int Actual number of bytes written to the serial interface.
         */
        int writeSerial();

        /**
         * @brief Gets a frame of P25 data from the RX queue.
         * @param data Buffer containing P25 data.
         * @return uint32_t Size of the P25 data buffer, including leading data tag.
         */
        uint32_t readP25Frame(uint8_t* data);
        /**
         * @brief Process a P25 LDU and add to the TX queue, timed appropriately.
         * @param duid DUID.
         * @param lc Instance of the dfsi::LC class.
         * @param ldu Buffer containing LDU data.
         */
        void writeP25Frame(P25DEF::DUID::E duid, dfsi::LC& lc, uint8_t* ldu);

        /**
         * @brief Send a start of stream sequence (HDU, etc) to the connected serial V24 device.
         * @param lc Instance of the p25::LC class.
         */
        void startOfStream(const LC& lc);
        /**
         * @brief Send an end of stream sequence (TDU, etc) to the connected serial V24 device.
         */
        void endOfStream();

        /**
         * @brief Helper to add a V.24 dataframe to the P25 Tx queue with the proper timestamp and formatting.
         * @param data Buffer containing V.24 data frame to send.
         * @param len Length of buffer.
         * @param msgType Type of message to send (used for proper jitter clocking).
         */
        void addTxToQueue(uint8_t* data, uint16_t length, SERIAL_TX_TYPE msgType);

        /**
         * @brief Helper to insert IMBE silence frames for missing audio.
         * @param data Buffer containing frame data.
         * @param[out] lost Count of lost IMBE frames.
         */
        void insertMissingAudio(uint8_t* data, uint32_t& lost);

        /**
         * @brief Resets the current rx call data (stream ID, TGIDs, etc)
        */
        void rxResetCallData();

        /**
         * @brief 
         * @param buffer 
         * @param length 
         */
        void printDebug(const uint8_t* buffer, uint16_t length);
    };
} // namespace network

#endif // __SERIAL_SERVICE_H__