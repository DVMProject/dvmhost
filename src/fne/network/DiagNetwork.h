// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Converged FNE Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Converged FNE Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DIAG_NETWORK_H__)
#define __DIAG_NETWORK_H__

#include "fne/Defines.h"
#include "common/network/BaseNetwork.h"
#include "fne/network/FNENetwork.h"

#include <string>

#include <pthread.h>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

class HOST_SW_API HostFNE;

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements the diagnostic/activity log networking logic.
    // ---------------------------------------------------------------------------

    class HOST_SW_API DiagNetwork : public BaseNetwork {
    public:
        /// <summary>Initializes a new instance of the DiagNetwork class.</summary>
        DiagNetwork(HostFNE* host, FNENetwork* fneNetwork, const std::string& address, uint16_t port);
        /// <summary>Finalizes a instance of the DiagNetwork class.</summary>
        ~DiagNetwork() override;

        /// <summary>Gets the current status of the network.</summary>
        NET_CONN_STATUS getStatus() { return m_status; }

        /// <summary>Sets endpoint preshared encryption key.</summary>
        void setPresharedKey(const uint8_t* presharedKey);

        /// <summary>Process a data frames from the network.</summary>
        void processNetwork();

        /// <summary>Updates the timer by the passed number of milliseconds.</summary>
        void clock(uint32_t ms) override;

        /// <summary>Opens connection to the network.</summary>
        bool open() override;

        /// <summary>Closes connection to the network.</summary>
        void close() override;

    private:
        friend class FNENetwork;
        FNENetwork* m_fneNetwork;
        HostFNE* m_host;

        std::string m_address;
        uint16_t m_port;

        NET_CONN_STATUS m_status;

        /// <summary>Entry point to process a given network packet.</summary>
        static void* threadedNetworkRx(void* arg);
    };
} // namespace network

#endif // __FNE_NETWORK_H__
