// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES FROM THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 */

#include "common/network/Network.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/lc/RTCH.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>

using namespace network;
using namespace nxdn;
using namespace nxdn::defines;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Test network class for NXDN frame serialization.
 */
class NXDNFrameTestNetwork final : public Network {
public:
    /**
     * @brief Initializes a new instance of the NXDNFrameTestNetwork class.
     */
    NXDNFrameTestNetwork() :
        Network("127.0.0.1", 1U, 0U, 1U, "test", true, false,
            false, false, true, false, true, true, false, false, false, false)
    {
        /* stub */
    }

    /**
     * @brief Creates an NXDN FNE message containing a single CAI frame.
     * @param length The length of the created message.
     * @param control The NXDN RTCH control information.
     * @param frame The CAI frame data.
     * @param frameLength The length of the CAI frame data.
     * @returns UInt8Array The serialized NXDN FNE message.
     */
    UInt8Array create(uint32_t& length, const lc::RTCH& control,
        const uint8_t* frame, uint32_t frameLength)
    {
        return createNXDN_Message(length, control, frame, frameLength);
    }
};

TEST_CASE("NXDN FNE message carries exactly one CAI frame", "[nxdn][network]")
{
    NXDNFrameTestNetwork network;
    lc::RTCH control;
    control.setMessageType(MessageType::RTCH_VCALL);
    control.setSrcId(1201U);
    control.setDstId(2201U);

    uint8_t frame[NXDN_FRAME_LENGTH_BYTES];
    for (uint32_t i = 0U; i < sizeof(frame); i++)
        frame[i] = (uint8_t)i;

    uint32_t messageLength = 0U;
    UInt8Array message = network.create(messageLength, control, frame, sizeof(frame));

    REQUIRE(message != nullptr);
    REQUIRE(messageLength == NXDN_PACKET_LENGTH + PACKET_PAD);
    REQUIRE(::memcmp(message.get(), TAG_NXDN_DATA, 4U) == 0);
    REQUIRE(message[23U] == NXDN_FRAME_LENGTH_BYTES);
    REQUIRE(::memcmp(message.get() + MSG_HDR_SIZE, frame, sizeof(frame)) == 0);
}
