// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/network/RTPHeader.h"
#include "common/p25/dfsi/frames/Frames.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "modem/port/specialized/V24UDPPort.h"
#include "modem/Modem.h"

using namespace p25::dfsi::defines;
using namespace p25::dfsi::frames;
using namespace p25::dfsi::frames::fsc;
using namespace modem::port;
using namespace modem::port::specialized;
using namespace network;
using namespace network::frame;
using namespace network::udp;

#include <cstring>
#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define MAX_THREAD_CNT 8U
#define RTP_END_OF_CALL_SEQ 65535

const uint32_t BUFFER_LENGTH = 2000U;

const char* V24_UDP_HARDWARE = "V.24 UDP Modem Controller";
const uint8_t V24_UDP_PROTOCOL_VERSION = 4U;

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex V24UDPPort::m_bufferMutex;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the V24UDPPort class. */

V24UDPPort::V24UDPPort(uint32_t peerId, const std::string& address, uint16_t modemPort, uint16_t controlPort, 
    uint16_t controlLocalPort, bool useFSC, bool fscInitiator, bool debug) :
    m_socket(nullptr),
    m_localPort(modemPort),
    m_controlSocket(nullptr),
    m_ctrlFrameQueue(nullptr),
    m_address(address),
    m_addr(),
    m_controlAddr(),
    m_addrLen(0U),
    m_ctrlAddrLen(0U),
    m_ctrlLocalPort(controlLocalPort),
    m_remoteCtrlAddr(),
    m_remoteCtrlAddrLen(0U),
    m_remoteRTPAddr(),
    m_remoteRTPAddrLen(0U),
    m_buffer(2000U, "UDP Port Ring Buffer"),
    m_fscInitiator(fscInitiator),
    m_timeoutTimer(1000U, 45U),
    m_reqConnectionTimer(1000U, 30U),
    m_heartbeatInterval(5U),
    m_heartbeatTimer(1000U, 5U),
    m_random(),
    m_peerId(peerId),
    m_streamId(0U),
    m_timestamp(INVALID_TS),
    m_pktSeq(0U),
    m_fscState(CS_NOT_CONNECTED),
    m_modemState(STATE_P25),
    m_tx(false),
    m_ctrlThreadPool(MAX_THREAD_CNT, "v24cc"),
    m_vcThreadPool(MAX_THREAD_CNT, "v24vc"),
    m_debug(debug)
{
    assert(peerId > 0U);
    assert(!address.empty());
    assert(modemPort > 0U);

    if (controlPort > 0U && useFSC) {
        if (controlLocalPort == 0U)
            controlLocalPort = controlPort;

        m_controlSocket = new Socket(controlLocalPort);
        m_ctrlFrameQueue = new RawFrameQueue(m_controlSocket, m_debug);

        if (udp::Socket::lookup(address, controlPort, m_controlAddr, m_ctrlAddrLen) != 0)
            m_ctrlAddrLen = 0U;

        if (m_ctrlAddrLen > 0U) {
            m_remoteCtrlAddr = m_controlAddr;
            m_remoteCtrlAddrLen = m_remoteCtrlAddrLen;

            std::string ctrlAddrStr = udp::Socket::address(m_controlAddr);
            LogWarning(LOG_HOST, "SECURITY: Remote modem expects V.24 control channel IP address; %s for remote modem control", ctrlAddrStr.c_str());
        }
    } else {
        createVCPort(modemPort);
    }

    std::random_device rd;
    std::mt19937 mt(rd());
    m_random = mt;

    m_streamId = createStreamId();
}

/* Finalizes a instance of the V24UDPPort class. */

V24UDPPort::~V24UDPPort()
{
    if (m_ctrlFrameQueue != nullptr)
        delete m_ctrlFrameQueue;
    if (m_controlSocket != nullptr)
        delete m_controlSocket;

    if (m_socket != nullptr)
        delete m_socket;
}

/* Helper to set and configure the heartbeat interval for FSC connections. */

void V24UDPPort::setHeartbeatInterval(uint32_t interval)
{
    if (interval < 5U)
        interval = 5U;
    if (interval > 30U)
        interval = 30U;

    m_heartbeatInterval = interval;
    m_heartbeatTimer = Timer(1000U, interval);
}

/* Updates the timer by the passed number of milliseconds. */

void V24UDPPort::clock(uint32_t ms)
{
    // if we have a FSC control socket
    if (m_controlSocket != nullptr) {
        if ((m_fscState == CS_NOT_CONNECTED) && m_fscInitiator) {
            if (!m_reqConnectionTimer.isRunning()) {
                // make initial request
                writeConnect();
                m_reqConnectionTimer.start();
            } else {
                m_reqConnectionTimer.clock(ms);
                if (m_reqConnectionTimer.isRunning() && m_reqConnectionTimer.hasExpired()) {
                    // make another request
                    writeConnect();
                    m_reqConnectionTimer.start();
                }
            }
        }

        if (m_fscState == CS_CONNECTED) {
            m_heartbeatTimer.clock(ms);
            if (m_heartbeatTimer.isRunning() && m_heartbeatTimer.hasExpired()) {
                writeHeartbeat();
                m_heartbeatTimer.start();
            }
        }

        m_timeoutTimer.clock(ms);
        if (m_timeoutTimer.isRunning() && m_timeoutTimer.hasExpired()) {
            LogError(LOG_NET, "V.24 UDP, DFSI connection to the remote endpoint has timed out, disconnected");
            m_fscState = CS_NOT_CONNECTED;
            if (!m_fscInitiator)
                m_reqConnectionTimer.stop();
            else
                m_reqConnectionTimer.start();
            m_heartbeatTimer.stop();
            m_timeoutTimer.stop();
        }

        processCtrlNetwork();
    }

    processVCNetwork();
}

/* Resets the RTP packet sequence and stream ID. */

void V24UDPPort::reset()
{
    m_pktSeq = 0U;
    m_timestamp = INVALID_TS;
    m_streamId = createStreamId();
}

/* Opens a connection to the FSC port. */

bool V24UDPPort::openFSC()
{
    if (m_ctrlAddrLen == 0U) {
        LogError(LOG_NET, "Unable to resolve the address of the modem");
        return false;
    }

    m_ctrlThreadPool.start();
    m_vcThreadPool.start();

    if (m_controlSocket != nullptr) {
        return m_controlSocket->open(m_controlAddr);
    }

    return false;
}

/* Opens a connection to the port. */

bool V24UDPPort::open()
{
    if (m_controlSocket != nullptr) {
        return true; // FSC mode always returns that the port was opened
    } else {
        m_vcThreadPool.start();

        if (m_addrLen == 0U) {
            LogError(LOG_NET, "Unable to resolve the address of the modem");
            return false;
        }

        if (m_socket != nullptr) {
            return m_socket->open(m_addr);
        }
    }

    return false;
}

/* Reads data from the port. */

int V24UDPPort::read(uint8_t* buffer, uint32_t length)
{
    assert(buffer != nullptr);
    assert(length > 0U);

    std::lock_guard<std::mutex> lock(m_bufferMutex);

    // Get required data from the ring buffer
    uint32_t avail = m_buffer.dataSize();
    if (avail < length)
        length = avail;

    if (length > 0U)
        m_buffer.get(buffer, length);

    return int(length);
}

/* Writes data to the port. */

int V24UDPPort::write(const uint8_t* buffer, uint32_t length)
{
    assert(buffer != nullptr);
    assert(length > 0U);

    switch (buffer[2U]) {
    case CMD_GET_VERSION:
        getVersion();
        return int(length);
    case CMD_GET_STATUS:
        getStatus();
        return int(length);
    case CMD_SET_CONFIG:
    case CMD_SET_MODE:
        writeAck(buffer[2U]);
        return int(length);
    case CMD_P25_DATA:
    {
        if (m_socket != nullptr) {
            uint32_t messageLen = 0U;
            uint8_t* message = generateMessage(buffer + 4U, length - 4U, m_streamId, m_peerId, m_pktSeq, &messageLen);

            if (m_debug)
                Utils::dump(1U, "!!! Tx Outgoing DFSI UDP", buffer + 4U, length - 4U);

            bool written = m_socket->write(message, messageLen, m_remoteRTPAddr, m_remoteRTPAddrLen);
            if (written)
                return length;
        } else {
            writeNAK(CMD_P25_DATA, RSN_INVALID_REQUEST);
            return int(length);
        }
    }
    break;
    case CMD_FLSH_READ:
        writeNAK(CMD_FLSH_READ, RSN_NO_INTERNAL_FLASH);
        return int(length);
    default:
        return int(length);
    }

    return -1;
}

/* Closes the connection to the FSC port. */

void V24UDPPort::closeFSC()
{
    if (m_controlSocket != nullptr) {
        if (m_fscState == CS_CONNECTED) {
            LogMessage(LOG_MODEM, "V.24 UDP, Closing DFSI FSC Connection, vcBasePort = %u", m_localPort);

            FSCDisconnect discoMessage = FSCDisconnect();

            uint8_t buffer[FSCDisconnect::LENGTH];
            ::memset(buffer, 0x00U, FSCDisconnect::LENGTH);
            discoMessage.encode(buffer);

            m_ctrlFrameQueue->write(buffer, FSCDisconnect::LENGTH, m_controlAddr, m_ctrlAddrLen);

            Thread::sleep(500U);
        }

        m_ctrlThreadPool.stop();
        m_ctrlThreadPool.wait();

        m_vcThreadPool.stop();
        m_vcThreadPool.wait();

        m_controlSocket->close();
    }
}

/* Closes the connection to the port. */

void V24UDPPort::close()
{
    m_vcThreadPool.stop();
    m_vcThreadPool.wait();

    if (m_socket != nullptr)
        m_socket->close();
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Process FSC control frames from the network. */

void V24UDPPort::processCtrlNetwork()
{
    sockaddr_storage address;
    uint32_t addrLen;
    int length = 0U;

    // read message
    UInt8Array buffer = m_ctrlFrameQueue->read(length, address, addrLen);
    if (length > 0) {
        if (m_debug)
            Utils::dump(1U, "FSC Control Network Message", buffer.get(), length);

        V24PacketRequest* req = new V24PacketRequest();
        req->obj = this;

        req->address = address;
        req->addrLen = addrLen;

        req->length = length;
        req->buffer = new uint8_t[length];
        ::memcpy(req->buffer, buffer.get(), length);

        // enqueue the task
        if (!m_ctrlThreadPool.enqueue(new_pooltask(taskCtrlNetworkRx, req))) {
            LogError(LOG_NET, "Failed to task enqueue control network packet request, %s:%u", 
                udp::Socket::address(address).c_str(), udp::Socket::port(address));
            if (req != nullptr) {
                if (req->buffer != nullptr)
                    delete[] req->buffer;
                delete req;
            }
        }
    }
}

/* Process a data frames from the network. */

void V24UDPPort::taskCtrlNetworkRx(void* arg)
{
    V24PacketRequest* req = (V24PacketRequest*)arg;
    if (req != nullptr) {
        V24UDPPort* network = static_cast<V24UDPPort*>(req->obj);
        if (network == nullptr) {
            delete req;
            return;
        }

        if (req->length > 0) {
            std::unique_ptr<FSCMessage> message = FSCMessage::createMessage(req->buffer);
            if (message != nullptr) {
                switch (message->getMessageId()) {
                    case FSCMessageType::FSC_ACK:
                    {
                        FSCACK* ackMessage = static_cast<FSCACK*>(message.get());
                        if (network->m_debug)
                            LogDebug(LOG_MODEM, "V.24 UDP, ACK, ackMessageId = $%02X, ackResponseCode = $%02X, respLength = %u", ackMessage->getAckMessageId(), ackMessage->getResponseCode(), ackMessage->getResponseLength());

                        switch (ackMessage->getResponseCode()) {
                            case FSCAckResponseCode::CONTROL_NAK:
                            case FSCAckResponseCode::CONTROL_NAK_CONNECTED:
                            case FSCAckResponseCode::CONTROL_NAK_M_UNSUPP:
                            case FSCAckResponseCode::CONTROL_NAK_V_UNSUPP:
                            case FSCAckResponseCode::CONTROL_NAK_F_UNSUPP:
                            case FSCAckResponseCode::CONTROL_NAK_PARMS:
                            case FSCAckResponseCode::CONTROL_NAK_BUSY:
                                LogError(LOG_MODEM, "V.24 UDP, ACK, ackMessageId = $%02X, ackResponseCode = $%02X", ackMessage->getAckMessageId(), ackMessage->getResponseCode());
                                break;

                            case FSCAckResponseCode::CONTROL_ACK:
                            {
                                switch (ackMessage->getAckMessageId()) {
                                    case FSCMessageType::FSC_CONNECT:
                                    {
                                        uint16_t vcBasePort = __GET_UINT16B(ackMessage->responseData, 1U);

                                        if (network->m_socket != nullptr) {
                                            network->m_socket->close();
                                            delete network->m_socket;
                                            network->m_socket = nullptr;
                                        }

                                        uint16_t remoteCtrlPort = Socket::port(req->address);
                                        network->m_remoteCtrlAddr = req->address;
                                        network->m_remoteCtrlAddrLen = req->addrLen;

                                        // setup local RTP VC port (where we receive traffic)
                                        network->createVCPort(network->m_localPort);
                                        network->m_socket->open(network->m_addr);

                                        // setup remote RTP VC port (where we send traffic to)
                                        std::string remoteAddress = Socket::address(req->address);
                                        network->createRemoteVCPort(remoteAddress, vcBasePort);

                                        network->m_fscState = CS_CONNECTED;
                                        network->m_reqConnectionTimer.stop();
                                        network->m_heartbeatTimer.start();
                                        network->m_timeoutTimer.start();

                                        LogMessage(LOG_MODEM, "V.24 UDP, Established DFSI FSC Connection, ctrlRemotePort = %u, vcLocalPort = %u, vcRemotePort = %u", remoteCtrlPort, network->m_localPort, vcBasePort);
                                    }
                                    break;

                                    case FSCMessageType::FSC_DISCONNECT:
                                    {
                                        if (network->m_socket != nullptr) {
                                            network->m_socket->close();
                                            delete network->m_socket;
                                            network->m_socket = nullptr;
                                        }

                                        network->m_fscState = CS_NOT_CONNECTED;
                                        if (!network->m_fscInitiator)
                                            network->m_reqConnectionTimer.stop();
                                        else
                                            network->m_reqConnectionTimer.start();
                                        network->m_heartbeatTimer.stop();
                                        network->m_timeoutTimer.stop();
                                    }
                                    break;

                                    default:
                                        break;
                                }
                            }
                            break;

                            default:
                                LogError(LOG_MODEM, "V.24 UDP, unknown ACK opcode, ackMessageId = $%02X", ackMessage->getAckMessageId());
                                break;
                        }

                        if (ackMessage->getResponseLength() > 0U && ackMessage->responseData != nullptr) {
                            delete[] ackMessage->responseData; // FSCACK doesn't clean itself up...
                        }
                    }
                    break;

                    case FSCMessageType::FSC_CONNECT:
                    {
                        FSCConnect* connMessage = static_cast<FSCConnect*>(message.get());
                        FSCACK ackResp = FSCACK();
                        ackResp.setCorrelationTag(connMessage->getCorrelationTag());
                        ackResp.setAckMessageId(FSCMessageType::FSC_CONNECT);
                        ackResp.setResponseCode(FSCAckResponseCode::CONTROL_ACK);
                        ackResp.setAckCorrelationTag(connMessage->getCorrelationTag());

                        if (connMessage->getVersion() != 1U) {
                            ackResp.setResponseCode(FSCAckResponseCode::CONTROL_NAK_V_UNSUPP);

                            uint8_t buffer[FSCACK::LENGTH];
                            ::memset(buffer, 0x00U, FSCACK::LENGTH);
                            ackResp.encode(buffer);

                            network->m_ctrlFrameQueue->write(buffer, FSCACK::LENGTH, req->address, req->addrLen);
                            break;
                        }

                        if (network->m_socket != nullptr) {
                            network->m_socket->close();
                            delete network->m_socket;
                            network->m_socket = nullptr;
                        }

                        uint16_t vcBasePort = connMessage->getVCBasePort();
                        network->m_heartbeatInterval = connMessage->getHostHeartbeatPeriod();
                        if (network->m_heartbeatInterval > 30U)
                            network->m_heartbeatInterval = 30U;

                        uint16_t remoteCtrlPort = Socket::port(req->address);
                        network->m_remoteCtrlAddr = req->address;
                        network->m_remoteCtrlAddrLen = req->addrLen;
    
                        LogMessage(LOG_MODEM, "V.24 UDP, Incoming DFSI FSC Connection, ctrlRemotePort = %u, vcLocalPort = %u, vcRemotePort = %u, hostHBInterval = %u", remoteCtrlPort, network->m_localPort, vcBasePort, connMessage->getHostHeartbeatPeriod());

                        // setup local RTP VC port (where we receive traffic)
                        network->createVCPort(network->m_localPort);
                        network->m_socket->open(network->m_addr);

                        // setup remote RTP VC port (where we send traffic to)
                        std::string remoteAddress = Socket::address(req->address);
                        network->createRemoteVCPort(remoteAddress, vcBasePort);

                        network->m_fscState = CS_CONNECTED;
                        network->m_reqConnectionTimer.stop();

                        if (connMessage->getHostHeartbeatPeriod() > 30U)
                            LogWarning(LOG_MODEM, "V.24 UDP, DFSI FSC Connection, requested heartbeat of %u, reduce to 30 seconds or less", connMessage->getHostHeartbeatPeriod());

                        network->m_heartbeatTimer = Timer(1000U, network->m_heartbeatInterval);
                        network->m_heartbeatTimer.start();
                        network->m_timeoutTimer.start();

                        // construct connect ACK response data
                        uint8_t respData[3U];
                        ::memset(respData, 0x00U, 3U);

                        respData[0U] = 1U; // Version 1
                        __SET_UINT16B(network->m_localPort, respData, 1U);

                        // pack ack
                        ackResp.setResponseLength(3U);
                        ackResp.responseData = respData;

                        uint8_t buffer[FSCACK::LENGTH + 3U];
                        ::memset(buffer, 0x00U, FSCACK::LENGTH + 3U);
                        ackResp.encode(buffer);

                        if (network->m_ctrlFrameQueue->write(buffer, FSCACK::LENGTH + 3U, req->address, req->addrLen))
                            LogMessage(LOG_MODEM, "V.24 UDP, Established DFSI FSC Connection, ctrlRemotePort = %u, vcLocalPort = %u, vcRemotePort = %u", remoteCtrlPort, network->m_localPort, vcBasePort);
                    }
                    break;

                    case FSCMessageType::FSC_SEL_CHAN:
                    {
                        FSCACK ackResp = FSCACK();
                        ackResp.setCorrelationTag(message->getCorrelationTag());
                        ackResp.setAckMessageId(FSCMessageType::FSC_SEL_CHAN);
                        ackResp.setResponseCode(FSCAckResponseCode::CONTROL_ACK);
                        ackResp.setAckCorrelationTag(message->getCorrelationTag());
                        ackResp.setResponseLength(0U);

                        uint8_t buffer[FSCACK::LENGTH];
                        ::memset(buffer, 0x00U, FSCACK::LENGTH);
                        ackResp.encode(buffer);

                        network->m_ctrlFrameQueue->write(buffer, FSCACK::LENGTH, req->address, req->addrLen);
                    }
                    break;

                    case FSCMessageType::FSC_REPORT_SEL_MODES:
                    {
                        FSCACK ackResp = FSCACK();
                        ackResp.setCorrelationTag(message->getCorrelationTag());
                        ackResp.setAckMessageId(FSCMessageType::FSC_REPORT_SEL_MODES);
                        ackResp.setResponseCode(FSCAckResponseCode::CONTROL_ACK);
                        ackResp.setAckCorrelationTag(message->getCorrelationTag());

                        // construct connect ACK response data
                        uint8_t respData[5U];
                        ::memset(respData, 0x00U, 5U);

                        // bryanb: because DVM is essentially a repeater -- we hardcode these values
                        respData[0U] = 1U; // Version 1
                        respData[1U] = 1U; // Repeat Inbound Audio
                        respData[2U] = 0U; // Rx Channel Selection
                        respData[3U] = 0U; // Tx Channel Selection
                        respData[4U] = 1U; // Monitor Mode

                        // pack ack
                        ackResp.setResponseLength(5U);
                        ackResp.responseData = respData;

                        uint8_t buffer[FSCACK::LENGTH + 5U];
                        ::memset(buffer, 0x00U, FSCACK::LENGTH + 5U);
                        ackResp.encode(buffer);

                        network->m_ctrlFrameQueue->write(buffer, FSCACK::LENGTH + 5U, req->address, req->addrLen);
                    }
                    break;

                    case FSCMessageType::FSC_DISCONNECT:
                    {
                        LogMessage(LOG_MODEM, "V.24 UDP, DFSI FSC Disconnect, vcBasePort = %u", network->m_localPort);

                        if (network->m_socket != nullptr) {
                            network->m_socket->close();
                            delete network->m_socket;
                            network->m_socket = nullptr;
                        }

                        network->m_fscState = CS_NOT_CONNECTED;
                        network->m_reqConnectionTimer.stop();
                        network->m_heartbeatTimer.stop();
                        network->m_timeoutTimer.stop();
                    }
                    break;

                    case FSCMessageType::FSC_HEARTBEAT:
                        network->m_timeoutTimer.start();
                        break;

                    default:
                        break;
                }
            }
        }

        if (req->buffer != nullptr)
            delete[] req->buffer;
        delete req;
    }
}

/* Process voice conveyance frames from the network. */

void V24UDPPort::processVCNetwork()
{
    // if we have a RTP voice socket
    if (m_socket != nullptr) {
        uint8_t data[BUFFER_LENGTH];
        ::memset(data, 0x00U, BUFFER_LENGTH);

        sockaddr_storage addr;
        uint32_t addrLen;
        int ret = m_socket->read(data, BUFFER_LENGTH, addr, addrLen);
        if (ret != 0) {
            // An error occurred on the socket
            if (ret < 0)
                return;

            // Add new data to the ring buffer
            if (ret > 0) {
                if (m_debug)
                    Utils::dump("!!! Rx Incoming DFSI UDP", data, ret);

                V24PacketRequest* req = new V24PacketRequest();
                req->obj = this;

                req->address = addr;
                req->addrLen = addrLen;

                req->rtpHeader = RTPHeader();
                req->rtpHeader.decode(data);

                // ensure payload type is correct
                if (req->rtpHeader.getPayloadType() != DFSI_RTP_PAYLOAD_TYPE) {
                    LogError(LOG_MODEM, "Invalid RTP header received from network");
                    delete req;
                    return;
                }

                // copy message
                req->length = ret - RTP_HEADER_LENGTH_BYTES;
                req->buffer = new uint8_t[req->length];
                ::memset(req->buffer, 0x00U, req->length);

                ::memcpy(req->buffer, data + RTP_HEADER_LENGTH_BYTES, req->length);

                // enqueue the task
                if (!m_vcThreadPool.enqueue(new_pooltask(taskVCNetworkRx, req))) {
                    LogError(LOG_NET, "Failed to task enqueue voice network packet request, %s:%u", 
                        udp::Socket::address(addr).c_str(), udp::Socket::port(addr));
                    if (req != nullptr) {
                        if (req->buffer != nullptr)
                            delete[] req->buffer;
                        delete req;
                    }
                }
            }
        }
    }
}

/* Process a data frames from the network. */

void V24UDPPort::taskVCNetworkRx(void* arg)
{
    V24PacketRequest* req = (V24PacketRequest*)arg;
    if (req != nullptr) {
        V24UDPPort* network = static_cast<V24UDPPort*>(req->obj);
        if (network == nullptr) {
            delete req;
            return;
        }

        if (req->length > 0) {
            if (udp::Socket::match(req->address, network->m_remoteRTPAddr)) {
                UInt8Array __reply = std::make_unique<uint8_t[]>(req->length + 4U);
                uint8_t* reply = __reply.get();

                reply[0U] = DVM_SHORT_FRAME_START;
                reply[1U] = (req->length + 4U) & 0xFFU;
                reply[2U] = CMD_P25_DATA;

                reply[3U] = 0x00U;

                ::memcpy(reply + 4U, req->buffer, req->length);

                std::lock_guard<std::mutex> lock(m_bufferMutex);
                network->m_buffer.addData(reply, req->length + 4U);
            }
            else {
                std::string addrStr = udp::Socket::address(req->address);
                LogWarning(LOG_HOST, "SECURITY: Remote modem mode encountered invalid IP address; %s", addrStr.c_str());
            }
        }

        if (req->buffer != nullptr)
            delete[] req->buffer;
        delete req;
    }
}

/* Internal helper to setup the local voice channel port. */

void V24UDPPort::createVCPort(uint16_t port)
{
    m_socket = new Socket(port);

    if (udp::Socket::lookup(m_address, port, m_addr, m_addrLen) != 0)
        m_addrLen = 0U;

    if (m_addrLen > 0U) {
        std::string addrStr = udp::Socket::address(m_addr);
        LogWarning(LOG_HOST, "SECURITY: Remote modem expects V.24 voice channel IP address; %s:%u for Rx traffic", addrStr.c_str(), port);
    }
}

/* Internal helper to setup the remote voice channel port. */

void V24UDPPort::createRemoteVCPort(std::string address, uint16_t port)
{
    if (udp::Socket::lookup(address, port, m_remoteRTPAddr, m_remoteRTPAddrLen) != 0)
    m_remoteRTPAddrLen = 0U;

    if (m_remoteRTPAddrLen > 0U) {
        std::string addrStr = udp::Socket::address(m_remoteRTPAddr);
        LogWarning(LOG_HOST, "SECURITY: Remote modem expects V.24 voice channel IP address; %s:%u for Tx traffic", addrStr.c_str(), port);
    }
}

/* Internal helper to write a FSC connect packet. */

void V24UDPPort::writeConnect()
{
    LogMessage(LOG_MODEM, "V.24 UDP, Attempting DFSI FSC Connection, peerId = %u, vcBasePort = %u", m_peerId, m_localPort);

    FSCConnect connect = FSCConnect();
    connect.setFSHeartbeatPeriod(m_heartbeatInterval);
    connect.setHostHeartbeatPeriod(m_heartbeatInterval);
    connect.setVCBasePort(m_localPort);
    connect.setVCSSRC(m_peerId);

    uint8_t buffer[FSCConnect::LENGTH];
    ::memset(buffer, 0x00U, FSCConnect::LENGTH);

    connect.encode(buffer);

    m_fscState = CS_CONNECTING;

    m_ctrlFrameQueue->write(buffer, FSCConnect::LENGTH, m_controlAddr, m_ctrlAddrLen);
}

/* Internal helper to write a FSC heartbeat packet. */

void V24UDPPort::writeHeartbeat()
{
    uint8_t buffer[FSCHeartbeat::LENGTH];
    ::memset(buffer, 0x00U, FSCHeartbeat::LENGTH);

    FSCHeartbeat hb = FSCHeartbeat();
    hb.encode(buffer);

    m_ctrlFrameQueue->write(buffer, FSCHeartbeat::LENGTH, m_remoteCtrlAddr, m_remoteCtrlAddrLen);
}

/* Generate RTP message. */

uint8_t* V24UDPPort::generateMessage(const uint8_t* message, uint32_t length, uint32_t streamId,
    uint32_t ssrc, uint16_t rtpSeq, uint32_t* outBufferLen)
{
    assert(message != nullptr);
    assert(length > 0U);

    uint32_t timestamp = m_timestamp;
    if (timestamp != INVALID_TS) {
        timestamp += (RTP_GENERIC_CLOCK_RATE / 133);
        if (m_debug)
            LogDebugEx(LOG_NET, "V24UDPPort::generateMessage()", "RTP streamId = %u, previous TS = %u, TS = %u, rtpSeq = %u", streamId, m_timestamp, timestamp, rtpSeq);
        m_timestamp = timestamp;
    }

    uint32_t bufferLen = RTP_HEADER_LENGTH_BYTES + length;
    uint8_t* buffer = new uint8_t[bufferLen];
    ::memset(buffer, 0x00U, bufferLen);

    RTPHeader header = RTPHeader();
    header.setExtension(false);

    header.setPayloadType(DFSI_RTP_PAYLOAD_TYPE);
    header.setTimestamp(timestamp);
    header.setSequence(rtpSeq);
    header.setSSRC(ssrc);

    header.encode(buffer);

    if (streamId != 0U && timestamp == INVALID_TS && rtpSeq != RTP_END_OF_CALL_SEQ) {
        if (m_debug)
            LogDebugEx(LOG_NET, "V24UDPPort::generateMessage()", "RTP streamId = %u, initial TS = %u, rtpSeq = %u", streamId, header.getTimestamp(), rtpSeq);
        m_timestamp = header.getTimestamp();
    }

    if (streamId != 0U && rtpSeq == RTP_END_OF_CALL_SEQ) {
        m_timestamp = INVALID_TS;
        if (m_debug)
            LogDebugEx(LOG_NET, "V24UDPPort::generateMessage()", "RTP streamId = %u, rtpSeq = %u", streamId, rtpSeq);
    }

    ::memcpy(buffer + RTP_HEADER_LENGTH_BYTES, message, length);

    if (m_debug)
        Utils::dump(1U, "[V24UDPPort::generateMessage()] Buffered Message", buffer, bufferLen);

    if (outBufferLen != nullptr) {
        *outBufferLen = bufferLen;
    }

    return buffer;
}

/* Helper to return a faked modem version. */

void V24UDPPort::getVersion()
{
    uint8_t reply[200U];

    reply[0U] = DVM_SHORT_FRAME_START;
    reply[1U] = 0U;
    reply[2U] = CMD_GET_VERSION;

    reply[3U] = V24_UDP_PROTOCOL_VERSION;
    reply[4U] = 15U;

    // Reserve 16 bytes for the UDID
    ::memset(reply + 5U, 0x00U, 16U);

    uint8_t count = 21U;
    for (uint8_t i = 0U; V24_UDP_HARDWARE[i] != 0x00U; i++, count++)
        reply[count] = V24_UDP_HARDWARE[i];

    reply[1U] = count;

    std::lock_guard<std::mutex> lock(m_bufferMutex);
    m_buffer.addData(reply, count);
}

/* Helper to return a faked modem status. */

void V24UDPPort::getStatus()
{
    uint8_t reply[15U];

    // Send all sorts of interesting internal values
    reply[0U] = DVM_SHORT_FRAME_START;
    reply[1U] = 12U;
    reply[2U] = CMD_GET_STATUS;

    reply[3U] = 0x00U;
    reply[3U] |= 0x08U; // P25 enable flag
    if (m_fscState == CS_CONNECTED)
        reply[3U] |= 0x40U; // V.24 connected flag

    reply[4U] = uint8_t(m_modemState);

    reply[5U] = m_tx ? 0x01U : 0x00U;

    reply[6U] = 0U;

    reply[7U] = 0U;
    reply[8U] = 0U;

    reply[9U] = 0U;

    reply[10U] = 5U; // always report 5 P25 frames worth of space

    reply[11U] = 0U;

    std::lock_guard<std::mutex> lock(m_bufferMutex);
    m_buffer.addData(reply, 12U);
}

/* Helper to write a faked modem acknowledge. */

void V24UDPPort::writeAck(uint8_t type)
{
    uint8_t reply[4U];

    reply[0U] = DVM_SHORT_FRAME_START;
    reply[1U] = 4U;
    reply[2U] = CMD_ACK;
    reply[3U] = type;

    std::lock_guard<std::mutex> lock(m_bufferMutex);
    m_buffer.addData(reply, 4U);
}

/* Helper to write a faked modem negative acknowledge. */

void V24UDPPort::writeNAK(uint8_t opcode, uint8_t err)
{
    uint8_t reply[5U];

    reply[0U] = DVM_SHORT_FRAME_START;
    reply[1U] = 5U;
    reply[2U] = CMD_NAK;
    reply[3U] = opcode;
    reply[4U] = err;

    std::lock_guard<std::mutex> lock(m_bufferMutex);
    m_buffer.addData(reply, 5U);
}