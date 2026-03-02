// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "host/Defines.h"
#include "common/p25/P25Defines.h"
#include "common/p25/data/Assembler.h"
#include "common/Log.h"
#include "common/Utils.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::data;

#include <catch2/catch_test_macros.hpp>
#include <stdlib.h>
#include <time.h>

TEST_CASE("P25 PDU Unconfirmed Test", "[p25][pdu_unconfirmed]") {
    bool failed = false;

    INFO("P25 PDU Unconfirmed Test");

    srand((unsigned int)time(NULL));

    g_logDisplayLevel = 1U;

    // test PDU data
    uint32_t testLength = 120U;
    uint8_t testPDUSource[] =
    {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x20, 0x54, 0x45, 0x53, 0x54, 0x54, 0x45, 0x53, 0x54, 0x54, 0x45, 0x53, 0x54, 0x20, 0x20,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
        0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11,
        0x20, 0x54, 0x45, 0x53, 0x54, 0x54, 0x45, 0x53, 0x54, 0x54, 0x45, 0x53, 0x54, 0x20, 0x20,
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
        0x2F, 0x2E, 0x2D, 0x2C, 0x2B, 0x2A, 0x29, 0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21
    };

    data::Assembler::setVerbose(true);
    data::Assembler::setDumpPDUData(true);

    Assembler assembler = Assembler();

    Utils::dump(2U, "P25_PDU_Unconfirmed_Test, Test Source", testPDUSource, 120U);

    DataHeader dataHeader = DataHeader();
    dataHeader.setFormat(PDUFormatType::UNCONFIRMED);
    dataHeader.setMFId(MFG_STANDARD);
    dataHeader.setAckNeeded(false);
    dataHeader.setOutbound(true);
    dataHeader.setSAP(PDUSAP::USER_DATA);
    dataHeader.setLLId(0x12345U);
    dataHeader.setFullMessage(true);
    dataHeader.setBlocksToFollow(1U);

    dataHeader.calculateLength(testLength);

    /*
    ** self-sanity check the assembler chain
    */

    uint32_t bitLength = 0U;
    UInt8Array ret = assembler.assemble(dataHeader, false, false, testPDUSource, &bitLength);

    LogInfoEx("T", "P25_PDU_Confirmed_Large_Test, Assembled Bit Length = %u (%u)", bitLength, bitLength / 8);
    Utils::dump(2U, "P25_PDU_Unconfirmed_Test, Assembled PDU", ret.get(), bitLength / 8);

    if (ret == nullptr)
        failed = true;

    if (!failed) {
        uint8_t buffer[P25_PDU_FRAME_LENGTH_BYTES];
        ::memset(buffer, 0x00U, P25_PDU_FRAME_LENGTH_BYTES);

        // for the purposes of our test we strip the pad bit length from the bit length
        bitLength -= dataHeader.getPadLength() * 8U;

        uint32_t blockCnt = 0U;
        for (uint32_t i = P25_PREAMBLE_LENGTH_BITS; i < bitLength; i += P25_PDU_FEC_LENGTH_BITS) {
            ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
            Utils::getBitRange(ret.get(), buffer, i, P25_PDU_FEC_LENGTH_BITS);

            bool ret = false;
            if (blockCnt == 0U)
                ret = assembler.disassemble(buffer, P25_PDU_FEC_LENGTH_BYTES, true);
            else
                ret = assembler.disassemble(buffer, P25_PDU_FEC_LENGTH_BYTES);
            if (!ret) {
                failed = true;
                ::LogError("T", "P25_PDU_Unconfirmed_Test, PDU Disassemble, block %u", blockCnt);
            }

            blockCnt++;
        }

        if (assembler.getComplete()) {
            uint8_t pduUserData2[P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
            uint32_t pduUserDataLength = assembler.getUserDataLength();
            assembler.getUserData(pduUserData2);

            for (uint32_t i = 0; i < pduUserDataLength - 4U; i++) {
                if (pduUserData2[i] != testPDUSource[i]) {
                    ::LogError("T", "P25_PDU_Unconfirmed_Test, INVALID AT IDX %d", i);
                    failed = true;
                }
            }
        }
    }

    REQUIRE(failed==false);
}
