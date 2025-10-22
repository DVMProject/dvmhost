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
 * @file HAParameters.h
 * @ingroup fne_network
 */
#if !defined(__HA_PARAMETERS_H__)
#define __HA_PARAMETERS_H__

#include "fne/Defines.h"

namespace network
{
    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents high availiability parameter data.
     * @ingroup fne_network
     */
    struct HAParameters {
    public:
        /**
         * @brief Initializes a new instance of the HAParameters class
         */
        HAParameters() :
            peerId(0U),
            masterIP(0U),
            masterPort(TRAFFIC_DEFAULT_PORT)
        {
            /* stub **/
        }

        /**
         * @brief Initializes a new instance of the HAParameters class
         * @param peerId Peer ID.
         * @param ipAddr Master IP Address.
         * @param port Master Port.
         */
        HAParameters(uint32_t peerId, uint32_t ipAddr, uint16_t port) :
            peerId(peerId),
            masterIP(ipAddr),
            masterPort(port)
        {
            /* stub */
        }

        /**
         * @brief Equals operator. Copies this HAParameters to another HAParameters.
         * @param data Instance of HAParameters to copy.
         */
        HAParameters& operator=(const HAParameters& data)
        {
            if (this != &data) {
                peerId = data.peerId;
                masterIP = data.masterIP;
                masterPort = data.masterPort;
            }

            return *this;
        }

    public:
        uint32_t peerId;        //!< Peer ID.
        
        uint32_t masterIP;      //!< Master IP Address.
        uint16_t masterPort;    //!< Master Port.
    };
} // namespace network

#endif // __HA_PARAMETERS_H__
