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
 * @file CallData.h
 * @ingroup dfsi_network
 * @file CallData.cpp
 * @ingroup dfsi_network
 */
#if !defined(__DFSI_CALL_DATA_H__)
#define __DFSI_CALL_DATA_H__

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

#include <random>

namespace network 
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents an on-going call.
     * @ingroup dfsi_network
     */
    class HOST_SW_API VoiceCallData {
    public:
        /**
         * @brief Initializes a new instance of the VoiceCallData class.
         */
        VoiceCallData();
        /**
         * @brief Initializes a new instance of the VoiceCallData class.
         */
        ~VoiceCallData();

        /**
         * @brief Reset call data to defaults.
         */
        void resetCallData();
        /**
         * @brief Generate a new stream ID for a call.
         */
        void newStreamId();

        /** @name Call Data */
        /** 
         * @brief Source Radio ID.
         */
        uint32_t srcId;
        /** 
         * @brief Destination ID.
         */
        uint32_t dstId;

        /**
         * @brief Link Control Opcode.
         */
        uint8_t lco;
        /**
         * @brief Manufacturer ID.
         */
        uint8_t mfId;
        /**
         * @brief Call Service Options.
         */
        uint8_t serviceOptions;

        /**
         * @brief Low Speed Data 1.
         */
        uint8_t lsd1;
        /**
         * @brief Low Speed Data 2.
         */
        uint8_t lsd2;

        /**
         * @brief Encryption Message Indicator.
         */
        uint8_t* mi;
        /**
         * @brief Encryption Algorithm ID.
         */
        uint8_t algoId;
        /**
         * @brief Encryption Key ID.
         */
        uint32_t kId;

        /**
         * @brief Voice Header 1.
         */
        uint8_t* VHDR1;
        /**
         * @brief Voice Header 2.
         */
        uint8_t* VHDR2;

        /**
         * @brief FNE Network LDU1 Buffer.
         */
        uint8_t* netLDU1;
        /**
         * @brief FNE Network LDU2 Buffer.
         */
        uint8_t* netLDU2;

        /**
         * @brief Sequence Number.
         */
        uint32_t seqNo;
        /**
         * @brief 
         */
        uint8_t n;

        /**
         * @brief Stream ID.
         */
        uint32_t streamId;
        /** @} */
    
    private:
        // Used for stream ID generation
        std::mt19937 random;

    };
}

#endif // __DFSI_CALL_DATA_H__