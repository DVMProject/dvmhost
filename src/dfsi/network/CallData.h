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
*   Borrowed from work by Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DFSI_CALL_DATA_H__)
#define __DFSI_CALL_DATA_H__

// DVM Includes
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
#include "frames/Frames.h"
#include "host/modem/Modem.h"
#include "host/modem/port/IModemPort.h"
#include "host/modem/port/UARTPort.h"

// CPP includes
#include <random>

namespace network 
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //     Represents an on-going call.
    // ---------------------------------------------------------------------------

    class HOST_SW_API VoiceCallData {
    public:
        /// <summary>Initializes a new instance of the VoiceCallData class.</summary>
        VoiceCallData();
        /// <summary>Initializes a new instance of the VoiceCallData class.</summary>
        ~VoiceCallData();

        /// <summary>Reset call data to defaults.</summary>
        void resetCallData();
        /// <summary>Generate a new stream ID for a call.</summary>
        void newStreamId();

        // Call Data
        uint32_t srcId;
        uint32_t dstId;

        uint8_t lco;
        uint8_t mfId;
        uint8_t serviceOptions;

        uint8_t lsd1;
        uint8_t lsd2;

        uint8_t* mi;
        uint8_t algoId;
        uint32_t kId;

        uint8_t* VHDR1;
        uint8_t* VHDR2;

        uint8_t* netLDU1;
        uint8_t* netLDU2;

        uint32_t seqNo;
        uint8_t n;

        uint32_t streamId;
    
    private:
        // Used for stream ID generation
        std::mt19937 random;

    };
}

#endif // __DFSI_CALL_DATA_H__