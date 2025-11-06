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
#include "common/concurrent/unordered_map.h"
#include "common/p25/P25Defines.h"
#include "common/p25/Crypto.h"
#include "common/network/udp/Socket.h"
#include "common/network/RawFrameQueue.h"
#include "network/FNENetwork.h"
#include "network/callhandler/packetdata/P25PacketData.h"

namespace network
{
    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents the data required for a OTAR network packet handler thread.
     * @ingroup fne_network
     */
    struct OTARPacketRequest : thread_t {
        sockaddr_storage address;               //!< IP Address and Port. 
        uint32_t addrLen;                       //!< 
        int length = 0U;                        //!< Length of raw data buffer
        uint8_t *buffer;                        //!< Raw data buffer
    };

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
         * @param debug Flag indicating whether debug is enabled.
         * @param verbose Flag indicating whether verbose logging is enabled.
         */
        P25OTARService(FNENetwork* network, network::callhandler::packetdata::P25PacketData* packetData, bool debug, bool verbose);
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

        /**
         * @brief Opens a connection to the OTAR port.
         * @param address Hostname/IP address to listen on.
         * @param port Port number.
         * @returns bool True, if connection is opened, otherwise false.
         */
        bool open(const std::string& address, uint16_t port);

        /**
         * @brief Closes the connection to the OTAR port.
         */
        void close();

    private:
        network::udp::Socket* m_socket;
        network::RawFrameQueue* m_frameQueue;

        ThreadPool m_threadPool;

        FNENetwork* m_network;
        network::callhandler::packetdata::P25PacketData* m_packetData;

        concurrent::unordered_map<uint32_t, uint16_t> m_rsiMessageNumber;

        bool m_allowNoUKEKRekey;

        bool m_debug;
        bool m_verbose;

        /**
         * @brief Entry point to process a given network packet.
         * @param arg Instance of the OTARPacketRequest structure.
         */
        static void taskNetworkRx(OTARPacketRequest* req);

        /**
         * @brief Encrypt/decrypt KMM frame.
         * @param[in] algoId Algorithm ID.
         * @param[in] kid Key ID.
         * @param mi Message Indicator.
         * @param[in] buffer KMM frame buffer.
         * @param len Length of KMM frame buffer.
         * @param encrypt True to encrypt, false to decrypt.
         * @returns UInt8Array Buffer containing the encrypted/decrypted KMM frame.
         */
        UInt8Array cryptKMM(uint8_t algoId, uint16_t kid, uint8_t* mi, const uint8_t* buffer, uint32_t len, bool encrypt = false);

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
         * @brief Helper used to return a Rekey-Command KMM to the calling SU.
         * @param llId Logical Link Address.
         * @param kmmRSI KMM Radio Set Identifier.
         * @param flags Hello KMM flags.
         * @param[out] payloadSize Size of the returned KMM payload.
         * @returns UInt8Array Buffer containing the processed KMM frame (if any).
         */
        UInt8Array write_KMM_Rekey_Command(uint32_t llId, uint32_t kmmRSI, uint8_t flags, uint32_t* payloadSize);

        /**
         * @brief Helper used to return a Registration-Command KMM to the calling SU.
         * @param llId Logical Link Address.
         * @param kmmRSI KMM Radio Set Identifier.
         * @param[out] payloadSize Size of the returned KMM payload.
         * @returns UInt8Array Buffer containing the processed KMM frame (if any).
         */
        UInt8Array write_KMM_Reg_Command(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize);

        /**
         * @brief Helper used to return a Deregistration-Response KMM to the calling SU.
         * @param llId Logical Link Address.
         * @param kmmRSI KMM Radio Set Identifier.
         * @param[out] payloadSize Size of the returned KMM payload.
         * @returns UInt8Array Buffer containing the processed KMM frame (if any).
         */
        UInt8Array write_KMM_Dereg_Response(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize);

        /**
         * @brief Helper used to return a No-Service KMM to the calling SU.
         * @param llId Logical Link Address.
         * @param kmmRSI KMM Radio Set Identifier.
         * @param[out] payloadSize Size of the returned KMM payload.
         * @returns UInt8Array Buffer containing the processed KMM frame (if any).
         */
        UInt8Array write_KMM_NoService(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize);

        /**
         * @brief Helper used to return a Zeroize KMM to the calling SU.
         * @param llId Logical Link Address.
         * @param kmmRSI KMM Radio Set Identifier.
         * @param[out] payloadSize Size of the returned KMM payload.
         * @returns UInt8Array Buffer containing the processed KMM frame (if any).
         */
        UInt8Array write_KMM_Zeroize(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize);

        /**
         * @brief Helper used to log a KMM response.
         * @param llId Logical Link Address.
         * @param kmmString 
         * @param status Status.
         */
        void logResponseStatus(uint32_t llId, std::string kmmString, uint8_t status);
    };
} // namespace network

#endif // __P25_OTAR_SERVICE_H__
