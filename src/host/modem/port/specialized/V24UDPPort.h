// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * @package DVM / Modem Host Software
 * @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
 * @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file V24UDPPort.h
 * @ingroup port
 * @file V24UDPPort.cpp
 * @ingroup port
 */
#if !defined(__V24_UDP_PORT_H__)
#define __V24_UDP_PORT_H__

#include "Defines.h"
#include "common/network/udp/Socket.h"
#include "common/network/RawFrameQueue.h"
#include "common/network/RTPHeader.h"
#include "common/RingBuffer.h"
#include "common/Timer.h"
#include "common/ThreadPool.h"
#include "modem/port/IModemPort.h"

#include <string>
#include <random>
#include <mutex>

namespace modem
{
    namespace port
    {
        namespace specialized
        {
            // ---------------------------------------------------------------------------
            //  Structure Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Represents the data required for a V.24 network packet handler thread.
             * @ingroup fne_network
             */
            struct V24PacketRequest : thread_t {
                sockaddr_storage address;               //! IP Address and Port. 
                uint32_t addrLen;                       //!
                network::frame::RTPHeader rtpHeader;    //! RTP Header
                int length = 0U;                        //! Length of raw data buffer
                uint8_t *buffer;                        //! Raw data buffer
            };

            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief This class implements low-level routines to communicate over UDP for V.24.
             * @ingroup port
             */
            class HOST_SW_API V24UDPPort : public IModemPort {
            public:
                /**
                 * @brief Initializes a new instance of the V24UDPPort class.
                 * @param peerId Unique ID of this modem on the network.
                 * @param address Hostname/IP address to connect to.
                 * @param modemPort Port number.
                 * @param controlPort Control Port number.
                 * @param controlLocalPort Local listening control port number.
                 * @param useFSC Flag indicating whether or not FSC handshakes are used to setup communications.
                 * @param fscInitiator Flag indicating whether or not the FSC handshake should be initiated when the port is opened.
                 * @param debug Flag indicating whether network debug is enabled.
                 */
                V24UDPPort(uint32_t peerId, const std::string& modemAddress, uint16_t modemPort, uint16_t controlPort = 0U, 
                    uint16_t controlLocalPort = 0U, bool useFSC = false, bool fscInitiator = false, bool debug = false);
                /**
                 * @brief Finalizes a instance of the V24UDPPort class.
                 */
                ~V24UDPPort() override;

                /**
                 * @brief Helper to set and configure the heartbeat interval for FSC connections.
                 */
                void setHeartbeatInterval(uint32_t interval);

                /**
                 * @brief Updates the timer by the passed number of milliseconds.
                 * @param ms Number of milliseconds.
                 */
                void clock(uint32_t ms);

                /**
                 * @brief Resets the RTP packet sequence and stream ID.
                 */
                void reset();

                /**
                 * @brief Opens a connection to the FSC port.
                 * @returns bool True, if connection is opened, otherwise false.
                 */
                bool openFSC();
                /**
                 * @brief Opens a connection to the port.
                 * @returns bool True, if connection is opened, otherwise false.
                 */
                bool open() override;

                /**
                 * @brief Reads data from the port.
                 * @param[out] buffer Buffer to read data from the port to.
                 * @param length Length of data to read from the port.
                 * @returns int Actual length of data read from serial port.
                 */
                int read(uint8_t* buffer, uint32_t length) override;
                /**
                 * @brief Writes data to the port.
                 * @param[in] buffer Buffer containing data to write to port.
                 * @param length Length of data to write to port.
                 * @returns int Actual length of data written to the port.
                 */
                int write(const uint8_t* buffer, uint32_t length) override;

                /**
                 * @brief Closes the connection to the FSC port.
                 */
                void closeFSC();
                /**
                 * @brief Closes the connection to the port.
                 */
                void close() override;

            private:
                network::udp::Socket* m_socket;
                uint16_t m_localPort;

                network::udp::Socket* m_controlSocket;
                network::RawFrameQueue* m_ctrlFrameQueue;

                std::string m_address;
                sockaddr_storage m_addr;
                sockaddr_storage m_controlAddr;
                uint32_t m_addrLen;
                uint32_t m_ctrlAddrLen;

                uint16_t m_ctrlLocalPort;

                sockaddr_storage m_remoteCtrlAddr;
                uint32_t m_remoteCtrlAddrLen;
                sockaddr_storage m_remoteRTPAddr;
                uint32_t m_remoteRTPAddrLen;

                RingBuffer<uint8_t> m_buffer;

                bool m_fscInitiator;

                Timer m_timeoutTimer;

                Timer m_reqConnectionTimer;
                uint32_t m_heartbeatInterval;
                Timer m_heartbeatTimer;

                std::mt19937 m_random;

                uint32_t m_peerId;

                uint32_t m_streamId;
                uint32_t m_timestamp;
                uint16_t m_pktSeq;

                enum CS_STATE : uint8_t {
                    CS_NOT_CONNECTED = 0,
                    CS_CONNECTING = 1,
                    CS_CONNECTED = 2
                };
                CS_STATE m_fscState;

                uint8_t m_modemState;
                bool m_tx;

                ThreadPool m_ctrlThreadPool;
                ThreadPool m_vcThreadPool;

                bool m_debug;

                static std::mutex m_bufferMutex;

                /**
                 * @brief Process FSC control frames from the network.
                 */
                void processCtrlNetwork();

                /**
                 * @brief Entry point to process a given network packet.
                 * @param arg Instance of the V24PacketRequest structure.
                 */
                static void taskCtrlNetworkRx(V24PacketRequest* req);

                /**
                 * @brief Process voice conveyance frames from the network.
                 */
                void processVCNetwork();

                /**
                 * @brief Entry point to process a given network packet.
                 * @param req Instance of the V24PacketRequest structure.
                 */
                static void taskVCNetworkRx(V24PacketRequest* req);

                /**
                 * @brief Internal helper to setup the local voice channel port.
                 * @param port Port number.
                 */
                void createVCPort(uint16_t port);
                /**
                 * @brief Internal helper to setup the remote voice channel port.
                 * @param address IP Address or Hostname.
                 * @param port Port number.
                 */
                void createRemoteVCPort(std::string address, uint16_t port);
                /**
                 * @brief Internal helper to write a FSC connect packet.
                 */
                void writeConnect();
                /**
                 * @brief Internal helper to write a FSC heartbeat packet.
                 */
                void writeHeartbeat();

                /**
                 * @brief Helper to update the RTP packet sequence.
                 * @param reset Flag indicating the current RTP packet sequence value should be reset.
                 * @returns uint16_t RTP packet sequence.
                 */
                uint16_t pktSeq(bool reset = false);

                /**
                 * @brief Generates a new stream ID.
                 * @returns uint32_t New stream ID.
                 */
                uint32_t createStreamId() { std::uniform_int_distribution<uint32_t> dist(0x00000001, 0xfffffffe); return dist(m_random); }

                /**
                 * @brief Generate RTP message.
                 * @param[in] message Message buffer to frame and queue.
                 * @param length Length of message.
                 * @param streamId Message stream ID.
                 * @param ssrc RTP SSRC ID.
                 * @param rtpSeq RTP Sequence.
                 * @param[out] outBufferLen Length of buffer generated.
                 * @returns uint8_t* Buffer containing RTP message.
                 */
                uint8_t* generateMessage(const uint8_t* message, uint32_t length, uint32_t streamId,
                    uint32_t ssrc, uint16_t rtpSeq, uint32_t* outBufferLen);

                /**
                 * @brief Helper to return a faked modem version.
                 */
                void getVersion();
                /**
                 * @brief Helper to return a faked modem status.
                 */
                void getStatus();
                /**
                 * @brief Helper to write a faked modem acknowledge.
                 * @param type  
                 */
                void writeAck(uint8_t type);
                /**
                 * @brief Helper to write a faked modem negative acknowledge.
                 * @param opcode 
                 * @param err 
                 */
                void writeNAK(uint8_t opcode, uint8_t err);
            };
        } // namespace specialized
    } // namespace port
} // namespace modem

#endif // __V24_UDP_PORT_H__
