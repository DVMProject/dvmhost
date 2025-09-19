// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 * 
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file P25OTARService.h
 * @ingroup fne_network
 * @file P25OTARService.cpp
 * @ingroup fne_network
 */
#if !defined(__P25_OTAR_SERVICE_H__)
#define __P25_OTAR_SERVICE_H__

#include "fne/Defines.h"
#include "common/p25/P25Defines.h"
#include "common/p25/Crypto.h"
#include "network/FNENetwork.h"
#include "network/callhandler/packetdata/P25PacketData.h"

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the P25 OTAR service.
     * @ingroup fne_network
     */
    class HOST_SW_API P25OTARService {
    public:
        /**
         * @brief Initializes a new instance of the P25OTARService class.
         * @param network Instance of the FNENetwork class.
         * @param packetData Instance of the P25PacketData class.
         * @param debug Flag indicating whether network debug is enabled.
         */
        P25OTARService(FNENetwork* network, network::callhandler::packetdata::P25PacketData* packetData, bool debug);
        /**
         * @brief Finalizes a instance of the P25OTARService class.
         */
        ~P25OTARService();

        /**
         * @brief Helper used to process KMM frames from PDU data.
         * @param[in] data Network data buffer.
         * @param len Length of data.
         * @param encrypted Flag indicating whether or not the KMM frame is encrypted.
         * @param llId Logical Link ID.
         * @param n Send Sequence Number.
         * @param encrypted Flag indicating whether or not the KMM frame is encrypted.
         * @returns bool True, if KMM processed, otherwise false.
         */
        bool processDLD(const uint8_t* data, uint32_t len, uint32_t llId, uint8_t n, bool encrypted);

        /**
         * @brief Updates the timer by the passed number of milliseconds.
         * @param ms Number of milliseconds.
         */
        void clock(uint32_t ms);

    private:
        FNENetwork* m_network;
        network::callhandler::packetdata::P25PacketData* m_packetData;

        p25::crypto::P25Crypto* m_crypto;

        bool m_debug;

        /**
         * @brief Helper used to process KMM frames.
         * @param[in] data Network data buffer.
         * @param len Length of data.
         * @param encrypted Flag indicating whether or not the KMM frame is encrypted.
         * @param llId Logical Link ID.
         * @param[out] payloadSize Size of the returned KMM payload.
         * @returns UInt8Array Buffer containing the processed KMM frame (if any).
         */
        UInt8Array processKMM(const uint8_t* data, uint32_t len, uint32_t llId, bool encrypted, uint32_t* payloadSize);

        /**
         * @brief Helper used to return a No-Service KMM to the calling SU.
         * @param llId Logical Link Address.
         * @param kmmRSI KMM Radio Set Identifier.
         * @param[out] payloadSize Size of the returned KMM payload.
         * @returns UInt8Array Buffer containing the processed KMM frame (if any).
         */
        UInt8Array write_KMM_NoService(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize);
    };
} // namespace network

#endif // __P25_OTAR_SERVICE_H__
