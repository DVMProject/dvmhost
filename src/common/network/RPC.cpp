// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/edac/CRC.h"
#include "common/edac/SHA256.h"
#include "common/network/RPCHeader.h"
#include "common/network/json/json.h"
#include "common/Log.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "network/RPC.h"

using namespace network;
using namespace network::frame;

#include <cstdio>
#include <cassert>
#include <cmath>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

#define REPLY_WAIT 200 // 200ms

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the RPC class. */

RPC::RPC(const std::string& address, uint16_t port, uint16_t localPort, const std::string& password, bool debug) :
    m_address(address),
    m_port(port),
    m_debug(debug),
    m_socket(nullptr),
    m_frameQueue(nullptr),
    m_password(password),
    m_handlers(),
    m_handlerReplied()
{
    assert(!address.empty());
    assert(port > 0U);
    assert(!password.empty());

    m_socket = new udp::Socket(address, port);
    m_frameQueue = new RawFrameQueue(m_socket, debug);
}

/* Finalizes a instance of the RPC class. */

RPC::~RPC()
{
    if (m_frameQueue != nullptr) {
        delete m_frameQueue;
    }

    if (m_socket != nullptr) {
        delete m_socket;
    }
}

/* Updates the timer by the passed number of milliseconds. */

void RPC::clock(uint32_t ms)
{
    sockaddr_storage address;
    uint32_t addrLen;

    frame::RPCHeader rpcHeader;
    int length = 0U;

    // read message
    UInt8Array buffer = m_frameQueue->read(length, address, addrLen);
    uint8_t* raw = buffer.get();
    if (length > 0) {
        if (length < RPC_HEADER_LENGTH_BYTES) {
            LogError(LOG_NET, "RPC::clock(), message received from network is malformed! %u bytes != %u bytes", 
                RPC_HEADER_LENGTH_BYTES, length);
            return;
        }

        // decode RTP header
        if (!rpcHeader.decode(buffer.get())) {
            LogError(LOG_NET, "RPC::clock(), invalid RPC packet received from network");
            return;
        }

        if (m_debug) {
            LogDebugEx(LOG_NET, "RPC::clock()", "received RPC, %s:%u, func = $%04X, messageLength = %u", 
                udp::Socket::address(address).c_str(), udp::Socket::port(address), rpcHeader.getFunction(), rpcHeader.getMessageLength());
        }

        // copy message
        uint32_t messageLength = rpcHeader.getMessageLength();
        UInt8Array message = std::unique_ptr<uint8_t[]>(new uint8_t[messageLength]);
        ::memcpy(message.get(), raw + RPC_HEADER_LENGTH_BYTES, messageLength);

        uint16_t calc = edac::CRC::createCRC16(message.get(), messageLength * 8U);
        if (m_debug) {
            LogDebugEx(LOG_NET, "RPC::clock()", "RPC, calc = $%04X, crc = $%04X", calc, rpcHeader.getCRC());
        }

        if (calc != rpcHeader.getCRC()) {
            LogError(LOG_NET, "RPC::clock(), failed CRC CCITT-162 check");
            return;
        }

        // parse JSON body
        std::string content = std::string((char*)message.get());

        json::value v;
        std::string err = json::parse(v, content);
        if (!err.empty()) {
            LogError(LOG_NET, "RPC::clock(), invalid RPC JSON payload, %s", err.c_str());
            return;
        }

        // ensure parsed JSON is an object
        if (!v.is<json::object>()) {
            LogError(LOG_NET, "RPC::clock(), invalid RPC JSON payload, request was not a JSON object");
            return;
        }

        json::object request = v.get<json::object>();
        json::object response;

        // find RPC function callback
        if (m_handlers.find(rpcHeader.getFunction()) != m_handlers.end()) {
            bool isReply = (rpcHeader.getFunction() & RPC_REPLY_FUNC) == RPC_REPLY_FUNC;
            if (isReply) {
                m_handlerReplied[rpcHeader.getFunction()] = true;
            }

            m_handlers[rpcHeader.getFunction()](request, response);

            // remove the reply handler (these should be temporary)
            if (isReply) {
                m_handlers.erase(rpcHeader.getFunction());
            } else {
                reply(rpcHeader.getFunction(), response, address, addrLen);
            }
        } else {
            bool isReply = (rpcHeader.getFunction() & RPC_REPLY_FUNC) == RPC_REPLY_FUNC;
            if (isReply) {
                m_handlerReplied[rpcHeader.getFunction()] = true;

                if (!request["status"].is<int>()) {
                    ::LogError(LOG_NET, "RPC %s:%u, invalid RPC response", udp::Socket::address(address).c_str(), udp::Socket::port(address));
                    return;
                }

                int status = request["status"].get<int>();
                if (status != network::RPC::OK) {
                    if (request["message"].is<std::string>()) {
                        std::string retMsg = request["message"].get<std::string>();
                        ::LogError(LOG_NET, "RPC %s:%u failed, %s", udp::Socket::address(address).c_str(), udp::Socket::port(address), retMsg.c_str());
                    }
                }
            }
            else
                LogWarning(LOG_NET, "RPC::clock(), ignoring unhandled function, func = $%04X, reply = %u", rpcHeader.getFunction() & 0x3FFFU, isReply);
        }
    }
}

/* Writes an RPC request to the network. */

bool RPC::req(uint16_t func, const json::object& request, RPCType reply, std::string address, uint16_t port,
    bool blocking)
{
    sockaddr_storage addr;
    uint32_t addrLen = 0U;
    if (udp::Socket::lookup(address, port, addr, addrLen) == 0) {
        return req(func, request, reply, addr, addrLen, blocking);
    }

    return false;
}

/* Writes an RPC request to the network. */

bool RPC::req(uint16_t func, const json::object& request, RPCType reply, sockaddr_storage& address, uint32_t addrLen,
    bool blocking)
{
    json::value v = json::value(request);
    std::string json = v.serialize();

    if (m_debug) {
        LogDebugEx(LOG_NET, "RPC::req()", "sending RPC, %s:%u, func = $%04X, messageLength = %u", 
            udp::Socket::address(address).c_str(), udp::Socket::port(address), func, json.length() + 1U);
    }

    // make sure we're not trying to send an RPC request to ourselves
    if (m_address == udp::Socket::address(address) && m_port == udp::Socket::port(address)) {
        LogError(LOG_NET, "RPC, cowardly refusing to send RPC to ourselves");
        return false;
    }

    // generate RPC header
    RPCHeader header = RPCHeader();
    header.setFunction(func & 0x3FFFU);
    header.setMessageLength(json.length() + 1U);

    // generate message
    CharArray __message = std::make_unique<char[]>(json.length() + 1U);
    char* message = __message.get();

    ::snprintf(message, json.length() + 1U, "%s", json.c_str());

    uint16_t crc = edac::CRC::createCRC16((uint8_t*)message, (json.length() + 1U) * 8U);
    if (m_debug) {
        LogDebugEx(LOG_NET, "RPC::req()", "RPC, crc = $%04X", crc);
    }

    header.setCRC(crc);

    // generate RPC message
    UInt8Array __buffer = std::make_unique<uint8_t[]>(json.length() + 9U);
    uint8_t* buffer = __buffer.get();

    header.encode(buffer);
    ::memcpy(buffer + 8U, message, json.length() + 1U);

    // install reply handler
    if (reply != nullptr) {
        m_handlers[func | RPC_REPLY_FUNC] = reply;
        m_handlerReplied[func | RPC_REPLY_FUNC] = false;
    }

    bool ret = m_frameQueue->write(buffer, json.length() + 9U, address, addrLen);
    if (!ret)
        return false;
    else {
        // are we blocking return until a reply is received?
        if (blocking) {
            // we only block for up to 200ms -- after we we treat the call as failed and return
            int timeout = REPLY_WAIT;
            while (timeout > 0) {
                if (m_handlerReplied.find(func | RPC_REPLY_FUNC) != m_handlerReplied.end()) {
                    if (m_handlerReplied[func | RPC_REPLY_FUNC]) {
                        m_handlerReplied[func | RPC_REPLY_FUNC] = false;
                        break;
                    }
                }

                if (m_debug)
                    LogDebugEx(LOG_HOST, "RPC::req()", "blocking = %u, to = %d", blocking, timeout);

                timeout--;
                Thread::sleep(1);
            }

            if (timeout == 0) {
                return false;
            }

            return true;
        }
        else {
            return true;
        }
    }

    return false;
}

/* Helper to generate a default response error payload. */

void RPC::defaultResponse(json::object& reply, std::string message, StatusType status)
{
    reply = json::object();

    int s = (int)status;
    reply["status"].set<int>(s);
    reply["message"].set<std::string>(message);
}

/* Opens connection to the network. */

bool RPC::open()
{
    if (m_debug)
        LogMessage(LOG_NET, "Opening RPC network");

    // generate AES256 key
    size_t size = m_password.size();

    uint8_t* in = new uint8_t[size];
    for (size_t i = 0U; i < size; i++)
        in[i] = m_password.at(i);

    uint8_t passwordHash[32U];
    ::memset(passwordHash, 0x00U, 32U);

    edac::SHA256 sha256;
    sha256.buffer(in, (uint32_t)(size), passwordHash);

    delete[] in;

    m_socket->setPresharedKey(passwordHash);

    return m_socket->open();
}

/* Closes connection to the network. */

void RPC::close()
{
    if (m_debug)
        LogMessage(LOG_NET, "Closing RPC network");

    m_socket->close();
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------


/* Writes an RPC reply to the network. */

bool RPC::reply(uint16_t func, json::object& reply, sockaddr_storage& address, uint32_t addrLen)
{
    json::value v = json::value(reply);
    std::string json = v.serialize();

    // generate RPC header
    RPCHeader header = RPCHeader();
    header.setFunction(func | RPC_REPLY_FUNC);
    header.setMessageLength(json.length() + 1U);

    // generate message
    CharArray __message = std::make_unique<char[]>(json.length() + 1U);
    char* message = __message.get();

    ::snprintf(message, json.length() + 1U, "%s", json.c_str());

    uint16_t crc = edac::CRC::createCRC16((uint8_t*)message, (json.length() + 1U) * 8U);
    header.setCRC(crc);

    // generate RPC message
    UInt8Array __buffer = std::make_unique<uint8_t[]>(json.length() + 9U);
    uint8_t* buffer = __buffer.get();

    header.encode(buffer);
    ::memcpy(buffer + 8U, message, json.length() + 1U);

    return m_frameQueue->write(buffer, json.length() + 9U, address, addrLen);
}

/* Default status response handler. */

void RPC::defaultHandler(json::object& request, json::object& reply)
{
    reply = json::object();

    int s = (int)StatusType::UNHANDLED_REQUEST;
    reply["status"].set<int>(s);
    reply["message"].set<std::string>("unhandled request");
}
