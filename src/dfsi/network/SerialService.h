// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI peer application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*
*/

#if !defined(__SERIAL_SERVICE_H__)
#define __SERIAL_SERVICE_H__

// DVM Includes
#include "Defines.h"
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
#include "host/modem/Modem.h"
#include "host/modem/port/IModemPort.h"
#include "host/modem/port/UARTPort.h"

// System Includes
#include <string>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <random>
#include <pthread.h>

using namespace modem;
using namespace p25;

namespace network
{
    // ---------------------------------------------------------------------------
    // Class Declaration
    //      Serial V24 service
    // ---------------------------------------------------------------------------

    class HOST_SW_API SerialService {
    public:
        SerialService(const std::string& portName, uint32_t baudrate, DfsiPeerNetwork* network, uint32_t p25TxQueueSize, uint32_t p25RxQueueSize, bool debug, bool trace);

        ~SerialService();

        void clock(uint32_t ms);

        bool open();

        void close();

        void processP25(UInt8Array p25Buffer, uint32_t length);

    private:
        std::string m_portName;
        uint32_t m_baudrate;

        port::IModemPort* m_port;

        bool m_debug;
        bool m_trace;

        DfsiPeerNetwork* m_network;

        uint8_t* m_lastIMBE;

        std::unordered_map<uint32_t, uint32_t> m_streamTimestamps;

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

        RESP_TYPE_DVM readSerial();
        int writeSerial(const uint8_t* data, uint32_t length);

        uint32_t readP25Frame(uint8_t* data);
        void writeP25Frame(const uint8_t* data, uint32_t length);

        /// <summary>Helper to insert IMBE silence frames for missing audio.</summary>
        void insertMissingAudio(uint8_t* data, uint32_t& lost);

        void printDebug(const uint8_t* buffer, uint16_t length);
    };
   
} // namespace network

#endif // __SERIAL_SERVICE_H__