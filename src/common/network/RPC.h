// SPDX-License-Identifier: GPL-2.0-only
/**
 * Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
*  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
*
*/
/**
 * @file RPC.h
 * @ingroup network_core
 * @file RPC.cpp
 * @ingroup network_core
 */
#if !defined(__RPC_H__)
#define __RPC_H__

#include "common/Defines.h"
#include "common/network/RawFrameQueue.h"
#include "common/network/json/json.h"
#include "common/network/udp/Socket.h"

#include <string>
#include <cstdint>
#include <functional>
#include <unordered_map>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Macros
    // ---------------------------------------------------------------------------

    #define RPC_FUNC_BIND(funcAddr, classInstance) std::bind(&funcAddr, classInstance, std::placeholders::_1,  std::placeholders::_2)

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the Remote Procedure Call networking logic.
     * @ingroup network_core
     */
    class HOST_SW_API RPC {
    public:
        typedef std::function<void(json::object& request, json::object& reply)> RPCType;

        /**
         * @brief Status/Response Codes
         */
        enum StatusType {
            OK = 200,                       //! OK 200

            BAD_REQUEST = 400,              //! Bad Request 400
            INVALID_ARGS = 401,             //! Invalid Arguments 401
            UNHANDLED_REQUEST = 402,        //! Unhandled Request 402
        } status;

        /**
         * @brief Initializes a new instance of the RPC class.
         * @param address Network Hostname/IP address to connect to.
         * @param port Network port number.
         * @param localPort 
         * @param peerId Unique ID on the network.
         * @param password Network authentication password.
         * @param debug Flag indicating whether network debug is enabled.
         */
        RPC(const std::string& address, uint16_t port, uint16_t localPort, const std::string& password, bool debug);
        /**
         * @brief Finalizes a instance of the RPC class.
         */
        ~RPC();

        /**
         * @brief Updates the timer by the passed number of milliseconds.
         * @param ms Number of milliseconds.
         */
        void clock(uint32_t ms);

        /**
         * @brief Writes an RPC request to the network.
         * @param request JSON content body for request.
         * @param reply Reply handler.
         * @param address IP address to write data to.
         * @param port Port number to write data to. 
         * @returns bool True, if message was written, otherwise false.
         */
        bool req(uint16_t func, const json::object& request, RPCType reply, std::string address, uint16_t port);
        /**
         * @brief Writes an RPC request to the network.
         * @param request JSON content body for request.
         * @param reply Reply handler.
         * @param address IP address to write data to.
         * @param addrLen 
         * @returns bool True, if message was written, otherwise false.
         */
        bool req(uint16_t func, const json::object& request, RPCType reply, sockaddr_storage& address, uint32_t addrLen);

        /**
         * @brief Helper to generate a default response error payload.
         * @param reply JSON reply.
         * @param message Textual error message to send.
         * @param status Status to send.
         */
        void defaultResponse(json::object &reply, std::string message, StatusType status = StatusType::BAD_REQUEST);

        /**
         * @brief Opens connection to the network.
         * @returns bool True, if networking has started, otherwise false.
         */
        bool open();

        /**
         * @brief Closes connection to the network.
         */
        void close();

        /**
         * @brief Helper to register an RPC handler.
         * @param func Function opcode.
         * @param handler Function handler.
         */
        void registerHandler(uint16_t func, RPCType handler) { m_handlers[func] = handler; }
        /**
         * @brief Helper to unregister an RPC handler.
         * @param func Function opcode.
         */
        void unregisterHandler(uint16_t func) { m_handlers.erase(func); }

    private:
        std::string m_address;
        uint16_t m_port;

        bool m_debug;

        udp::Socket* m_socket;
        RawFrameQueue* m_frameQueue;

        std::string m_password;

        std::map<uint16_t, RPCType> m_handlers;

        /**
         * @brief Writes an RPC reply to the network.
         * @param request JSON content body for reply.
         * @param address IP address to write data to.
         * @param addrLen 
         * @returns bool True, if message was written, otherwise false.
         */
        bool reply(uint16_t func, json::object& request, sockaddr_storage& address, uint32_t addrLen);

        /**
         * @brief Default status response handler.
         * @param request JSON request.
         * @param reply JSON response.
         */
        void defaultHandler(json::object& request, json::object& reply);
    };
} // namespace network

#endif // __RPC_H__
