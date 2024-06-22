// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI peer application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DFSI_PEER_NETWORK_H__)
#define __DFSI_PEER_NETWORK_H__

#include "Defines.h"
#include "host/network/Network.h"

#include <string>
#include <cstdint>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements the core peer networking logic.
    // ---------------------------------------------------------------------------

    class HOST_SW_API DfsiPeerNetwork : public Network {
    public:
        /// <summary>Initializes a new instance of the PeerNetwork class.</summary>
        DfsiPeerNetwork(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
            bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup);

        /// <summary>Writes P25 LDU1 frame data to the network.</summary>
        bool writeP25LDU1(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data, 
            P25DEF::FrameType::E frameType) override;
        /// <summary>Writes P25 LDU2 frame data to the network.</summary>
        bool writeP25LDU2(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data) override;

    protected:
        /// <summary>Writes configuration to the network.</summary>
        bool writeConfig() override;

    private:
        /// <summary>Creates an P25 LDU1 frame message.</summary>
        UInt8Array createP25_LDU1Message_Raw(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
            const uint8_t* data, P25DEF::FrameType::E frameType);
        /// <summary>Creates an P25 LDU2 frame message.</summary>
        UInt8Array createP25_LDU2Message_Raw(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
            const uint8_t* data);
    };
} // namespace network

#endif // __DFSI_PEER_NETWORK_H__