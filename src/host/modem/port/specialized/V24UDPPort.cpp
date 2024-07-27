// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
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

#define RTP_END_OF_CALL_SEQ 65535

const uint32_t BUFFER_LENGTH = 2000U;

const char* V24_UDP_HARDWARE = "V.24 UDP Modem Controller";
const uint8_t V24_UDP_PROTOCOL_VERSION = 4U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the V24UDPPort class. */

V24UDPPort::V24UDPPort(uint32_t peerId, const std::string& address, uint16_t modemPort, uint16_t controlPort, bool useFSC, bool debug) :
    m_socket(nullptr),
    m_localPort(modemPort),
    m_controlSocket(nullptr),
    m_ctrlFrameQueue(nullptr),
    m_address(address),
    m_addr(),
    m_controlAddr(),
    m_addrLen(0U),
    m_ctrlAddrLen(0U),
    m_buffer(2000U, "UDP Port Ring Buffer"),
    m_reqConnectionTimer(1000U, 30U),
    m_heartbeatTimer(1000U, 5U),
    m_reqConnectionToPeer(true),
    m_establishedConnection(false),
    m_random(),
    m_peerId(peerId),
    m_streamId(0U),
    m_timestamp(INVALID_TS),
    m_pktSeq(0U),
    m_modemState(STATE_P25),
    m_tx(false),
    m_debug(debug)
{
    assert(peerId > 0U);
    assert(!address.empty());
    assert(modemPort > 0U);
    assert(controlPort > 0U);

    if (controlPort > 0U && useFSC) {
        m_controlSocket = new Socket(controlPort);
        m_ctrlFrameQueue = new RawFrameQueue(m_controlSocket, m_debug);

        if (udp::Socket::lookup(address, controlPort, m_controlAddr, m_ctrlAddrLen) != 0)
            m_ctrlAddrLen = 0U;

        if (m_ctrlAddrLen > 0U) {
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

/* Updates the timer by the passed number of milliseconds. */

void V24UDPPort::clock(uint32_t ms)
{
    // if we have a FSC control socket
    if (m_controlSocket != nullptr) {
        if (m_reqConnectionToPeer) {
            if (!m_reqConnectionTimer.isRunning()) {
                // make initial request
                writeConnect();
                m_reqConnectionTimer.start();
            } else {
                m_reqConnectionTimer.clock(ms);
                if (m_reqConnectionTimer.isRunning() && m_reqConnectionTimer.hasExpired()) {
                    // make another request
                    writeConnect();
                }
            }
        }

        if (m_establishedConnection) {
            m_heartbeatTimer.clock(ms);
            if (m_heartbeatTimer.isRunning() && m_heartbeatTimer.hasExpired()) {
                writeHeartbeat();
                m_heartbeatTimer.start();
            }
        }

        processCtrlNetwork();
    }

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
                RTPHeader rtpHeader = RTPHeader();
                rtpHeader.decode(data);

                // ensure payload type is correct
                if (rtpHeader.getPayloadType() != DFSI_RTP_PAYLOAD_TYPE)
                {
                    LogError(LOG_MODEM, "Invalid RTP header received from network");
                    return;
                }

                // copy message
                uint32_t messageLength = ret - RTP_HEADER_LENGTH_BYTES;
                UInt8Array __message = std::make_unique<uint8_t[]>(messageLength);
                uint8_t* message = __message.get();
                ::memset(message, 0x00U, messageLength);

                ::memcpy(message, data + RTP_HEADER_LENGTH_BYTES, messageLength);

                if (udp::Socket::match(addr, m_addr)) {
                    UInt8Array __reply = std::make_unique<uint8_t[]>(messageLength + 4U);
                    uint8_t* reply = __reply.get();

                    reply[0U] = DVM_SHORT_FRAME_START;
                    reply[1U] = messageLength & 0xFFU;
                    reply[2U] = CMD_P25_DATA;

                    reply[3U] = 0x00U;

                    ::memcpy(reply + 4U, message, messageLength);

                    m_buffer.addData(reply, messageLength + 4U);
                }
                else {
                    std::string addrStr = udp::Socket::address(addr);
                    LogWarning(LOG_HOST, "SECURITY: Remote modem mode encountered invalid IP address; %s", addrStr.c_str());
                }
            }
        }
    }
}

/* Resets the RTP packet sequence and stream ID. */

void V24UDPPort::reset()
{
    m_pktSeq = 0U;
    m_timestamp = INVALID_TS;
    m_streamId = createStreamId();
}

/* Opens a connection to the port. */

bool V24UDPPort::open()
{
    if (m_addrLen == 0U && m_ctrlAddrLen == 0U) {
        LogError(LOG_NET, "Unable to resolve the address of the modem");
        return false;
    }

    if (m_controlSocket != nullptr) {
        return m_controlSocket->open(m_controlAddr);
    } else {
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
            uint8_t* message = generateMessage(buffer + 3U, length - 3U, m_streamId, m_peerId, m_pktSeq, &messageLen);

            bool written = m_socket->write(message, messageLen, m_addr, m_addrLen);
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

/* Closes the connection to the port. */

void V24UDPPort::close()
{
    if (m_controlSocket != nullptr)
        m_controlSocket->close();
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
        req->address = address;
        req->addrLen = addrLen;

        req->length = length;
        req->buffer = new uint8_t[length];
        ::memcpy(req->buffer, buffer.get(), length);

        if (!Thread::runAsThread(this, threadedCtrlNetworkRx, req)) {
            delete[] req->buffer;
            delete req;
            return;
        }
    }
}

/* Process a data frames from the network. */

void* V24UDPPort::threadedCtrlNetworkRx(void* arg)
{
    V24PacketRequest* req = (V24PacketRequest*)arg;
    if (req != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(req->thread);
#else
        ::pthread_detach(req->thread);
#endif // defined(_WIN32)

        V24UDPPort* network = static_cast<V24UDPPort*>(req->obj);
        if (network == nullptr) {
            delete req;
            return nullptr;
        }

        if (req->length > 0) {
            if (network->m_reqConnectionToPeer) {
                // FSC_CONNECT response -- is ... strange
                if (req->buffer[0] == 1U) {
                    network->m_reqConnectionToPeer = false;
                    network->m_reqConnectionTimer.stop();
                    network->m_establishedConnection = true;

                    FSCConnectResponse resp = FSCConnectResponse(req->buffer);
                    uint16_t vcBasePort = resp.getVCBasePort();

                    network->m_localPort = vcBasePort;
                    network->createVCPort(vcBasePort);
                    network->m_heartbeatTimer.start();

                    uint8_t buffer[FSCConnectResponse::LENGTH];
                    ::memset(buffer, 0x00U, FSCConnectResponse::LENGTH);

                    resp.setVCBasePort(network->m_localPort);
                    resp.encode(buffer);

                    network->m_ctrlFrameQueue->write(buffer, FSCConnectResponse::LENGTH, network->m_controlAddr, network->m_ctrlAddrLen);
                }
            }
            else
            {
                std::unique_ptr<FSCMessage> message = FSCMessage::createMessage(req->buffer);
                if (message != nullptr) {
                    switch (message->getMessageId())
                    {
                        case FSCMessageType::FSC_ACK:
                            {
                                FSCACK* ackMessage = static_cast<FSCACK*>(message.get());
                                switch (ackMessage->getResponseCode())
                                {
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
                                            if (ackMessage->getAckMessageId() == FSCMessageType::FSC_DISCONNECT) {
                                                network->m_reqConnectionTimer.stop();
                                                network->m_reqConnectionToPeer = false;
                                                network->m_establishedConnection = false;
                                                network->m_heartbeatTimer.stop();
                                            }
                                        }
                                        break;

                                    default:
                                        LogError(LOG_MODEM, "V.24 UDP, unknown ACK opcode, ackMessageId = $%02X", ackMessage->getAckMessageId());
                                        break;
                                }
                            }
                            break;

                        case FSCMessageType::FSC_CONNECT:
                            {
                                network->createVCPort(network->m_localPort);
                                network->m_heartbeatTimer.start();

                                uint8_t buffer[FSCConnectResponse::LENGTH];
                                ::memset(buffer, 0x00U, FSCConnectResponse::LENGTH);

                                FSCConnectResponse resp = FSCConnectResponse(req->buffer);
                                resp.setVCBasePort(network->m_localPort);
                                resp.encode(buffer);

                                network->m_ctrlFrameQueue->write(buffer, FSCConnectResponse::LENGTH, network->m_controlAddr, network->m_ctrlAddrLen);
                            }
                            break;

                        case FSCMessageType::FSC_DISCONNECT:
                            {
                                network->m_reqConnectionTimer.stop();
                                network->m_reqConnectionToPeer = false;
                                network->m_establishedConnection = false;
                                network->m_heartbeatTimer.stop();
                            }
                            break;

                        case FSCMessageType::FSC_HEARTBEAT:
                            {
                                if (network->m_establishedConnection) {
                                    network->writeHeartbeat();
                                }
                            }
                            break;

                        default:
                            break;
                    }
                }
            }
        }

        if (req->buffer != nullptr)
            delete[] req->buffer;
        delete req;
    }

    return nullptr;
}

/* Internal helper to setup the voice channel port. */

void V24UDPPort::createVCPort(uint16_t port)
{
    m_socket = new Socket(port);

    if (udp::Socket::lookup(m_address, port, m_addr, m_addrLen) != 0)
        m_addrLen = 0U;

    if (m_addrLen > 0U) {
        std::string addrStr = udp::Socket::address(m_addr);
        LogWarning(LOG_HOST, "SECURITY: Remote modem expects V.24 voice channel IP address; %s for remote modem control", addrStr.c_str());
    }
}

/* Internal helper to write a FSC connect packet. */

void V24UDPPort::writeConnect()
{
    FSCConnect connect = FSCConnect();
    connect.setFSHeartbeatPeriod(5U);   // hardcoded?
    connect.setHostHeartbeatPeriod(5U); // hardcoded?
    connect.setVCBasePort(m_localPort);
    connect.setVCSSRC(m_peerId);

    uint8_t buffer[FSCConnect::LENGTH];
    ::memset(buffer, 0x00U, FSCConnect::LENGTH);

    connect.encode(buffer);

    m_ctrlFrameQueue->write(buffer, FSCConnect::LENGTH, m_controlAddr, m_ctrlAddrLen);
}

/* Internal helper to write a FSC heartbeat packet. */

void V24UDPPort::writeHeartbeat()
{
    uint8_t buffer[FSCHeartbeat::LENGTH];
    ::memset(buffer, 0x00U, FSCHeartbeat::LENGTH);

    FSCHeartbeat hb = FSCHeartbeat();
    hb.encode(buffer);

    m_ctrlFrameQueue->write(buffer, FSCHeartbeat::LENGTH, m_controlAddr, m_ctrlAddrLen);
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
            LogDebug(LOG_NET, "V24UDPPort::generateMessage() RTP streamId = %u, previous TS = %u, TS = %u, rtpSeq = %u", streamId, m_timestamp, timestamp, rtpSeq);
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
            LogDebug(LOG_NET, "V24UDPPort::generateMessage() RTP streamId = %u, initial TS = %u, rtpSeq = %u", streamId, header.getTimestamp(), rtpSeq);
        m_timestamp = header.getTimestamp();
    }

    if (streamId != 0U && rtpSeq == RTP_END_OF_CALL_SEQ) {
        m_timestamp = INVALID_TS;
        if (m_debug)
            LogDebug(LOG_NET, "V24UDPPort::generateMessage() RTP streamId = %u, rtpSeq = %u", streamId, rtpSeq);
    }

    ::memcpy(buffer + RTP_HEADER_LENGTH_BYTES, message, length);

    if (m_debug)
        Utils::dump(1U, "V24UDPPort::generateMessage() Buffered Message", buffer, bufferLen);

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

    reply[4U] = uint8_t(m_modemState);

    reply[5U] = m_tx ? 0x01U : 0x00U;

    reply[6U] = 0U;

    reply[7U] = 0U;
    reply[8U] = 0U;

    reply[9U] = 0U;

    reply[10U] = 5U; // always report 5 P25 frames worth of space

    reply[11U] = 0U;

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

    m_buffer.addData(reply, 5U);
}