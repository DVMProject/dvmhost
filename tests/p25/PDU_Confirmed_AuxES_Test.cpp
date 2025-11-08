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

TEST_CASE("PDU_Confirmed_AuxES_Test", "[P25 PDU Confirmed Aux ES Test]") {
    SECTION("P25_PDU_Confirmed_AuxES_Test") {
        bool failed = false;

        INFO("P25 PDU Confirmed Aux ES Test");

        srand((unsigned int)time(NULL));

        g_logDisplayLevel = 1U;

        // test PDU data
        uint32_t testLength = 30U;
        uint8_t testPDUSource[] =
        {
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        };

        uint8_t encryptMI[] =
        {
            0x70, 0x30, 0xF1, 0xF7, 0x65, 0x69, 0x26, 0x67
        };

        data::Assembler::setVerbose(true);
        data::Assembler::setDumpPDUData(true);

        Assembler assembler = Assembler();

        Utils::dump(2U, "P25_PDU_Confirmed_AuxES_Test, Test Source", testPDUSource, 30U);

        DataHeader dataHeader = DataHeader();
        dataHeader.setFormat(PDUFormatType::CONFIRMED);
        dataHeader.setMFId(MFG_STANDARD);
        dataHeader.setAckNeeded(true);
        dataHeader.setOutbound(true);
        dataHeader.setSAP(PDUSAP::ENC_USER_DATA);
        dataHeader.setLLId(0x12345U);
        dataHeader.setFullMessage(true);
        dataHeader.setBlocksToFollow(1U);

        dataHeader.setEXSAP(PDUSAP::USER_DATA);

        dataHeader.setMI(encryptMI);
        dataHeader.setAlgId(ALGO_AES_256);
        dataHeader.setKId(0x2F62U);

        dataHeader.calculateLength(testLength);

        /*
        ** self-sanity check the assembler chain
        */

        uint32_t bitLength = 0U;
        UInt8Array ret = assembler.assemble(dataHeader, false, true, testPDUSource, &bitLength);

        LogInfoEx("T", "P25_PDU_Confirmed_AuxES_Test, Assembled Bit Length = %u (%u)", bitLength, bitLength / 8);
        Utils::dump(2U, "P25_PDU_Confirmed_AuxES_Test, Assembled PDU", ret.get(), bitLength / 8);

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

                LogInfoEx("T", "P25_PDU_Confirmed_AuxES_Test, i = %u", i);
                Utils::dump(2U, "buffer", buffer, P25_PDU_FEC_LENGTH_BYTES);

                bool ret = false;
                if (blockCnt == 0U)
                    ret = assembler.disassemble(buffer, P25_PDU_FEC_LENGTH_BYTES, true);
                else
                    ret = assembler.disassemble(buffer, P25_PDU_FEC_LENGTH_BYTES);
                if (!ret) {
                    failed = true;
                    ::LogError("T", "P25_PDU_Confirmed_AuxES_Test, PDU Disassemble, block %u", blockCnt);
                }

                blockCnt++;
            }

            if (assembler.getComplete()) {
                uint8_t pduUserData2[P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
                uint32_t pduUserDataLength = assembler.getUserDataLength();
                assembler.getUserData(pduUserData2);

                for (uint32_t i = 0; i < pduUserDataLength - 4U; i++) {
                    if (pduUserData2[i] != testPDUSource[i]) {
                        ::LogError("T", "P25_PDU_Confirmed_AuxES_Test, INVALID AT IDX %d", i);
                        failed = true;
                    }
                }
            }
        }

        REQUIRE(failed==false);
    }
}
