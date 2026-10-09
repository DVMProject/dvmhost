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

#if defined(CATCH2_TEST_COMPILATION)
class FNETestHooks;
#endif

#include "fne/Defines.h"
#include "common/concurrent/unordered_map.h"
#include "common/p25/P25Defines.h"
#include "common/p25/Crypto.h"
#include "common/p25/kmm/KMMFrame.h"
#include "common/p25/kmm/KeysetItem.h"
#include "common/network/udp/Socket.h"
#include "common/network/RawFrameQueue.h"
#include "network/TrafficNetwork.h"
#include "network/callhandler/packetdata/P25PacketData.h"

#include <condition_variable>
#include <array>
#include <mutex>
#include <unordered_map>

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
#if defined(CATCH2_TEST_COMPILATION)
        friend class ::FNETestHooks;
#endif
    public:
        /**
         * @brief Initializes a new instance of the P25OTARService class.
         * @param network Instance of the TrafficNetwork class.
         * @param packetData Instance of the P25PacketData class.
         * @param debug Flag indicating whether debug is enabled.
         * @param verbose Flag indicating whether verbose logging is enabled.
         */
        P25OTARService(TrafficNetwork* network, network::callhandler::packetdata::P25PacketData* packetData, bool debug, bool verbose);
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
        bool processDLD(const uint8_t* data, uint32_t len, uint32_t llId, uint8_t n, bool encrypted,
            uint8_t algoId = P25DEF::ALGO_UNENCRYPT, uint16_t kid = 0U, const uint8_t* mi = nullptr);

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

        /** 
         * @brief Supplies a TEK returned asynchronously by an upstream master. 
         * @param key The TEK key item.
         * @param algId The algorithm identifier for the TEK.
         * @param keyLength The length of the TEK key in bytes.
         * 
         */
        void cacheUpstreamTEK(const p25::kmm::KeyItem& key, uint8_t algId, uint8_t keyLength);
        /** 
         * @brief Supplies a UKEK returned asynchronously by an upstream master. 
         * @param rsi The Radio Subscriber Identifier associated with the UKEK.
         * @param key The UKEK key item.
         * @param algId The algorithm identifier for the UKEK.
         * @param keyLength The length of the UKEK key in bytes.
         */
        void cacheUpstreamUKEK(uint32_t rsi, const p25::kmm::KeyItem& key, uint8_t algId, uint8_t keyLength);

    private:
        /**
         * @brief Context for KMM message authentication.
         */
        struct KMMAuthContext {
            bool authenticated = false;                     //!< Indicates if the KMM message is authenticated.
            bool hasMessageNumber = false;                  //!< Indicates if the KMM message has a message number.
            uint16_t messageNumber = 0U;                    //!< Message number of the KMM message.
            uint8_t algorithmId = P25DEF::ALGO_UNENCRYPT;   //!< Algorithm ID used for the KMM message.
            uint16_t keyId = 0U;                            //!< Key ID used for the KMM message.
            uint16_t format = P25DEF::KMM_MAC_FORMAT_CBC;   //!< MAC format used for the KMM message.
        };

        /**
         * @brief Represents an upstream key received from the master.
         */
        struct UpstreamKey {
            p25::kmm::KeyItem key;                          //!< The key item for the upstream key.
            uint8_t algorithmId = P25DEF::ALGO_UNENCRYPT;   //!< Algorithm ID for the upstream key.
            uint8_t keyLength = 0U;                         //!< Length of the upstream key in bytes.
            uint64_t receivedAt = 0U;                       //!< Timestamp when the upstream key was received.
        };

        /**
         * @brief Represents the state of a warm start transaction.
         */
        enum class WarmStartState : uint8_t 
        {
            /**
             * @brief Waiting for the warm start acknowledgment from the master.
             */
            WAIT_WARM_ACK,
            /**
             * @brief Waiting for the rekey acknowledgment from the master.
             */
            WAIT_REKEY_ACK
        };

        /**
         * @brief Represents a warm start transaction.
         */
        struct WarmStartTransaction {
            WarmStartState state = WarmStartState::WAIT_WARM_ACK; //!< Current state of the warm start transaction.
            uint32_t llId = 0U;                             //!< Logical link ID associated with the warm start transaction.
            uint32_t rsi = 0U;                              //!< Subscriber RSI associated with the warm start transaction.
            uint16_t temporaryKId = 0U;                     //!< Temporary key ID used in the warm start transaction.
            uint16_t warmStartMN = 0U;                      //!< Message number for the warm start transaction.
            uint64_t deadline = 0U;                         //!< Deadline for the warm start transaction.
            bool dataLinkIndependent = false;               //!< Indicates if the warm start transaction is data link independent.
            bool hasDLIEndpoint = false;                    //!< Indicates if the warm start transaction has a data link independent endpoint.
            sockaddr_storage dliAddress{};                  //!< Address of the data link independent endpoint.
            uint32_t dliAddressLength = 0U;                 //!< Length of the data link independent endpoint address.
            std::array<uint8_t, P25DEF::MAX_ENC_KEY_LENGTH_BYTES> temporaryTEK{}; //!< Temporary TEK used in the warm start transaction.
            std::vector<uint16_t> permittedFinalKIds;       //!< Permitted final key IDs for the warm start transaction.
            std::vector<uint16_t> pendingRekeyMNs;          //!< Pending rekey message numbers for the warm start transaction.
        };

        network::udp::Socket* m_socket;
        network::RawFrameQueue* m_frameQueue;

        ThreadPool m_threadPool;

        TrafficNetwork* m_network;
        network::callhandler::packetdata::P25PacketData* m_packetData;

        concurrent::unordered_map<uint32_t, uint16_t> m_rsiMessageNumber;
        concurrent::unordered_map<uint32_t, uint16_t> m_rsiInboundMessageNumber;
        concurrent::unordered_map<uint32_t, uint64_t> m_rsiInboundFingerprint;
        concurrent::unordered_map<uint32_t, bool> m_dliRegistered;

        mutable std::mutex m_outboundMessageNumberMutex;

        bool m_allowNoUKEKRekey;

        bool m_debug;
        bool m_verbose;

        mutable std::mutex m_upstreamKeyMutex;
        mutable std::condition_variable m_upstreamKeyReady;
        mutable std::unordered_map<uint32_t, UpstreamKey> m_upstreamTEKs;
        mutable std::unordered_map<uint32_t, UpstreamKey> m_upstreamUKEKs;
        mutable std::mutex m_warmStartMutex;
        mutable std::unordered_map<uint32_t, WarmStartTransaction> m_warmStartTransactions;

        /** 
         * @brief Atomically reserves one or more outbound KMM message numbers. 
         * @param[in] rsi Radio Subscriber Identifier (RSI) for which to reserve message numbers.
         * @param[in] count Number of outbound message numbers to reserve.
         * @returns The first reserved outbound message number.
         * 
         */
        uint16_t reserveOutboundMessageNumbers(uint32_t rsi, uint16_t count = 1U);

        /**
         * @brief Entry point to process a given network packet.
         * @param arg Instance of the OTARPacketRequest structure.
         */
        static void taskNetworkRx(OTARPacketRequest* req);

        /**
         * @brief Helper used to process KMM frames.
         * @param[in] data Network data buffer.
         * @param len Length of data.
         * @param encrypted Flag indicating whether or not the KMM frame is encrypted.
         * @param llId Logical Link ID.
         * @param[out] payloadSize Size of the returned KMM payload.
         * @param algoId Algorithm ID.
         * @param kid Key ID.
         * @param mi Message Indicator.
         * @param dataLinkIndependent Flag indicating whether the KMM frame is data-link independent.
         * @param additionalResponses Optional container for additional KMM responses.
         * @returns UInt8Array Buffer containing the processed KMM frame (if any).
         */
        UInt8Array processKMM(const uint8_t* data, uint32_t len, uint32_t llId, bool encrypted, uint32_t* payloadSize,
            uint8_t algoId = P25DEF::ALGO_UNENCRYPT, uint16_t kid = 0U, const uint8_t* mi = nullptr,
            bool dataLinkIndependent = false, std::vector<std::vector<uint8_t>>* additionalResponses = nullptr);

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
         * @brief Resolves a TEK locally, then from an upstream replica master. 
         * @param[in] kid Key ID.
         * @param[in] algorithmId Algorithm ID.
         * @param[in] requestingRSI Requesting RSI (default is 0U).
         * @return Resolved TEK as an EKCKeyItem.
         * 
         */
        EKCKeyItem resolveTEK(uint16_t kid, uint8_t algorithmId, uint32_t requestingRSI = 0U) const;
        /** 
         * @brief Resolves a UKEK locally, then from an upstream replica master. 
         * @param[in] rsi Requesting RSI (default is 0U).
         * @return Resolved UKEK as an EKCKeyItem.
         */
        EKCKeyItem resolveUKEK(uint32_t rsi) const;

        /**
         * @brief Resolves a warm start TEK locally, then from an upstream replica master.
         * @param[in] kid Key ID.
         * @param[in] rsi Requesting RSI (default is 0U).
         * @return Resolved warm start TEK as an EKCKeyItem.
         */
        EKCKeyItem resolveWarmStartTEK(uint16_t kid, uint32_t rsi = 0U) const;
        /**
         * @brief Erases a warm start transaction for the given requesting RSI.
         * @param[in] rsi Requesting RSI.
         */
        void eraseWarmStart(uint32_t rsi);

        /**
         * @brief Resolves the required outer-encryption context for a locally generated KMM response.
         * @param[in] data Encoded plaintext KMM response.
         * @param len Length of the encoded response.
         * @param[in,out] encrypted Whether outer encryption is already in use or is required.
         * @param[in,out] algoId Outer-encryption algorithm ID.
         * @param[in,out] kid Outer-encryption key ID.
         * @return True when the response has a valid usable security context.
         */
        bool resolveResponseSecurity(const uint8_t* data, uint32_t len, bool& encrypted,
            uint8_t& algoId, uint16_t& kid) const;

        /**
         * @brief Helper used to return a Rekey-Command KMM to the calling SU.
         * @param llId Logical Link Address.
         * @param kmmRSI KMM Radio Set Identifier.
         * @param flags Hello KMM flags.
         * @param[out] payloadSize Size of the returned KMM payload.
         * @param auth Authentication context containing MAC/MN fields to mirror.
         * @param additionalResponses Optional container for additional KMM responses.
         * @returns UInt8Array Buffer containing the processed KMM frame (if any).
         */
        UInt8Array write_KMM_Rekey_Command(uint32_t llId, uint32_t kmmRSI, uint8_t flags, uint32_t* payloadSize,
            const KMMAuthContext& auth, std::vector<std::vector<uint8_t>>* additionalResponses = nullptr,
            bool useWarmStartTEK = false);

        /**
         * @brief Builds and records an RK3 Warm-Start transaction.
         * @param llId Logical Link Address.
         * @param kmmRSI KMM Radio Set Identifier.
         * @param[out] payloadSize Size of the returned KMM payload.
         * @param dataLinkIndependent Flag indicating whether the KMM frame is data-link independent.
         * @returns UInt8Array Buffer containing the processed KMM frame (if any).
         */
        UInt8Array write_KMM_WarmStart_Command(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize,
            bool dataLinkIndependent = false);

        /**
         * @brief Builds an authenticated Changeover-Command KMM for an SU.
         * @param llId Logical Link Address used to track the outbound message number.
         * @param kmmRSI Destination KMM Radio Set Identifier.
         * @param supersededKeysetId Keyset ID being superseded.
         * @param activeKeysetId Keyset ID becoming active.
         * @param[out] payloadSize Size of the returned KMM payload.
         * @returns Encoded KMM Changeover Command, or nullptr if no authorized MAC TEK is available.
         */
        UInt8Array write_KMM_Changeover_Command(uint32_t llId, uint32_t kmmRSI,
            uint8_t supersededKeysetId, uint8_t activeKeysetId, uint32_t* payloadSize);

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
         * @param auth Authentication context containing MAC/MN fields to mirror.
         * @returns UInt8Array Buffer containing the processed KMM frame (if any).
         */
        UInt8Array write_KMM_Dereg_Response(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize,
            const KMMAuthContext& auth);

        /**
         * @brief Helper used to return a No-Service KMM to the calling SU.
         * @param llId Logical Link Address.
         * @param kmmRSI KMM Radio Set Identifier.
         * @param[out] payloadSize Size of the returned KMM payload.
         * @param auth Authentication context containing MAC/MN fields to mirror.
         * @returns UInt8Array Buffer containing the processed KMM frame (if any).
         */
        UInt8Array write_KMM_NoService(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize,
            const KMMAuthContext& auth);

        /**
         * @brief Builds an authenticated KMM Negative-Acknowledgment.
         * @param kmmRSI Destination KMM Radio Set Identifier.
         * @param messageId Message ID being rejected.
         * @param messageNumber Message Number being rejected, or zero if absent.
         * @param status AACA-D Table 56 status.
         * @param[out] payloadSize Size of the returned KMM payload.
         * @param auth Authentication context for the secured response.
         * @returns Encoded KMM NACK, or nullptr when no authenticated response can be made.
         */
        UInt8Array write_KMM_NegativeAck(uint32_t kmmRSI, uint8_t messageId, uint16_t messageNumber,
            uint8_t status, uint32_t* payloadSize, const KMMAuthContext& auth);

        /** 
         * @brief Encodes a response and mirrors an authenticated request's MAC/MN fields. 
         * @param frame KMM frame to encode.
         * @param[out] payloadSize Size of the returned KMM payload.
         * @param auth Authentication context containing MAC/MN fields to mirror.
         */
        UInt8Array encode_KMM_Response(p25::kmm::KMMFrame& frame, uint32_t* payloadSize,
            const KMMAuthContext& auth);

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
