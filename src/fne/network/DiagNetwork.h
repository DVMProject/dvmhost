// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file DiagNetwork.h
 * @ingroup fne_network
 * @file DiagNetwork.cpp
 * @ingroup fne_network
 */
#if !defined(__DIAG_NETWORK_H__)
#define __DIAG_NETWORK_H__

#include "fne/Defines.h"
#include "common/network/BaseNetwork.h"
#include "fne/network/FNENetwork.h"

#include <string>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

class HOST_SW_API HostFNE;

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the diagnostic/activity log networking logic.
     * @ingroup fne_network
     */
    class HOST_SW_API DiagNetwork : public BaseNetwork {
    public:
        /**
         * @brief Initializes a new instance of the DiagNetwork class.
         * @param host Instance of the HostFNE class.
         * @param network Instance of the FNENetwork class.
         * @param address Network Hostname/IP address to listen on.
         * @param port Network port number.
         */
        DiagNetwork(HostFNE* host, FNENetwork* fneNetwork, const std::string& address, uint16_t port);
        /**
         * @brief Finalizes a instance of the DiagNetwork class.
         */
        ~DiagNetwork() override;

        /**
         * @brief Gets the current status of the network.
         * @returns NET_CONN_STATUS Current network status.
         */
        NET_CONN_STATUS getStatus() { return m_status; }

        /**
         * @brief Sets endpoint preshared encryption key.
         * @param presharedKey Encryption preshared key for networking.
         */
        void setPresharedKey(const uint8_t* presharedKey);

        /**
         * @brief Process a data frames from the network.
         */
        void processNetwork();

        /**
         * @brief Updates the timer by the passed number of milliseconds.
         * @param ms Number of milliseconds.
         */
        void clock(uint32_t ms) override;

        /**
         * @brief Opens connection to the network.
         * @returns bool True, if networking has started, otherwise false.
         */
        bool open() override;

        /**
         * @brief Closes connection to the network.
         */
        void close() override;

    private:
        friend class FNENetwork;
        FNENetwork* m_fneNetwork;
        HostFNE* m_host;

        std::string m_address;
        uint16_t m_port;

        NET_CONN_STATUS m_status;

        /**
         * @brief Entry point to process a given network packet.
         * @param arg Instance of the NetPacketRequest structure.
         * @returns void* (Ignore)
         */
        static void* threadedNetworkRx(void* arg);
    };
} // namespace network

#endif // __FNE_NETWORK_H__
