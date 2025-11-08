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

TEST_CASE("PDU_Confirmed_ConvReg_Test", "[P25 PDU Confirmed Conv Reg Test]") {
    SECTION("P25_PDU_Confirmed_ConvReg_Test") {
        bool failed = false;

        INFO("P25 PDU Confirmed Conv Reg Test");

        srand((unsigned int)time(NULL));

        g_logDisplayLevel = 1U;

        // data block
        uint8_t dataBlock[] = 
        {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC4, 0x1C,
            0x2A, 0x6E, 0x12, 0x2A, 0x20, 0x67, 0x0F, 0x79, 0x29, 0x2C, 0x70, 0x9E, 0x0B, 0x32, 0x21, 0x23,
            0x3D, 0x22, 0xED, 0x8C, 0x29, 0x26, 0x50,
            
            0x26, 0xE0, 0xB2, 0x22, 0x22, 0xB0, 0x72, 0x20, 0xE2, 0x22, 0x22, 0x59, 0x11, 0xE3, 0x92, 0x22, 
            0x22, 0x92, 0x73, 0x21, 0x52, 0x22, 0x22, 0x1F, 0x30
        };

        // expected PDU user data
        uint8_t expectedUserData[] =
        {
            0x00, 0x54, 0x36, 0x9F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC9, 0x9D, 0x42, 0x56
        };

        data::Assembler::setVerbose(true);
        data::Assembler::setDumpPDUData(true);

        Assembler assembler = Assembler();

        uint8_t pduUserData[P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES];
        ::memset(pduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES);

        /*
        ** self-sanity check the assembler chain
        */

        DataHeader rspHeader = DataHeader();
        rspHeader.setFormat(PDUFormatType::CONFIRMED);
        rspHeader.setMFId(assembler.dataHeader.getMFId());
        rspHeader.setAckNeeded(true);
        rspHeader.setOutbound(true);
        rspHeader.setSAP(PDUSAP::CONV_DATA_REG);
        rspHeader.setSynchronize(true);
        rspHeader.setLLId(0x12345U);
        rspHeader.setBlocksToFollow(1U);

        uint32_t regType = PDURegType::ACCEPT;
        uint32_t llId = 0x12345U;
        uint32_t ipAddr = 0x7F000001;

        pduUserData[0U] = ((regType & 0x0FU) << 4);                                 // Registration Type & Options
        pduUserData[1U] = (llId >> 16) & 0xFFU;                                     // Logical Link ID
        pduUserData[2U] = (llId >> 8) & 0xFFU;
        pduUserData[3U] = (llId >> 0) & 0xFFU;
        if (regType == PDURegType::ACCEPT) {
            pduUserData[8U] = (ipAddr >> 24) & 0xFFU;                               // IP Address
            pduUserData[9U] = (ipAddr >> 16) & 0xFFU;
            pduUserData[10U] = (ipAddr >> 8) & 0xFFU;
            pduUserData[11U] = (ipAddr >> 0) & 0xFFU;
        }

        Utils::dump(2U, "P25, PDU Registration Response", pduUserData, 12U);

        rspHeader.calculateLength(12U);
        uint32_t bitLength = 0U;
        UInt8Array ret = assembler.assemble(rspHeader, false, false, pduUserData, &bitLength);

        LogInfoEx("T", "P25_PDU_Confirmed_Large_Test, Assembled Bit Length = %u (%u)", bitLength, bitLength / 8);
        Utils::dump(2U, "P25_PDU_Confirmed_Test, Assembled PDU", ret.get(), bitLength / 8);

        if (ret == nullptr)
            failed = true;

        if (!failed) {
            uint8_t buffer[P25_PDU_FRAME_LENGTH_BYTES];
            ::memset(buffer, 0x00U, P25_PDU_FRAME_LENGTH_BYTES);

            uint32_t blockCnt = 0U;
            for (uint32_t i = P25_PREAMBLE_LENGTH_BITS; i < bitLength; i += P25_PDU_FEC_LENGTH_BITS) {
                ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
                Utils::getBitRange(ret.get(), buffer, i, P25_PDU_FEC_LENGTH_BITS);

                Utils::dump(2U, "P25_PDU_Confirmed_Test, Block", buffer, P25_PDU_FEC_LENGTH_BYTES);

                bool ret = false;
                if (blockCnt == 0U)
                    ret = assembler.disassemble(buffer, P25_PDU_FEC_LENGTH_BYTES, true);
                else
                    ret = assembler.disassemble(buffer, P25_PDU_FEC_LENGTH_BYTES);
                if (!ret) {
                    failed = true;
                    ::LogError("T", "P25_PDU_Confirmed_Test, PDU Disassemble, block %u", blockCnt);
                }

                blockCnt++;
            }

            if (assembler.getComplete()) {
                uint8_t pduUserData2[P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
                uint32_t pduUserDataLength = assembler.getUserDataLength() - 4U;
                assembler.getUserData(pduUserData2);

                for (uint32_t i = 0; i < pduUserDataLength; i++) {
                    if (pduUserData2[i] != pduUserData[i]) {
                        ::LogError("T", "P25_PDU_Confirmed_Test, INVALID AT IDX %d", i);
                        failed = true;
                    }
                }
            }
        }

        /*
        ** test disassembly against the static test data block
        */

       if (!failed) {
            uint8_t buffer[P25_PDU_FRAME_LENGTH_BYTES];
            ::memset(buffer, 0x00U, P25_PDU_FRAME_LENGTH_BYTES);

            uint32_t blockCnt = 0U;
            for (uint32_t i = P25_PREAMBLE_LENGTH_BYTES; i < 64U; i += P25_PDU_FEC_LENGTH_BYTES) {
                LogInfoEx("T", "i = %u", i);

                ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
                ::memcpy(buffer, dataBlock + i, P25_PDU_FEC_LENGTH_BYTES);

                Utils::dump(2U, "P25_PDU_Confirmed_Test, Block", buffer, P25_PDU_FEC_LENGTH_BYTES);

                bool ret = false;
                if (blockCnt == 0U)
                    ret = assembler.disassemble(buffer, P25_PDU_FEC_LENGTH_BYTES, true);
                else
                    ret = assembler.disassemble(buffer, P25_PDU_FEC_LENGTH_BYTES);
                if (!ret) {
                    failed = true;
                    ::LogError("T", "P25_PDU_Confirmed_Test, PDU Disassemble, block %u", blockCnt);
                }

                blockCnt++;
            }

            if (assembler.getComplete()) {
                uint8_t pduUserData2[P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
                uint32_t pduUserDataLength = assembler.getUserDataLength() - 4U;
                assembler.getUserData(pduUserData2);

                for (uint32_t i = 0; i < pduUserDataLength; i++) {
                    if (pduUserData2[i] != expectedUserData[i]) {
                        ::LogError("T", "P25_PDU_Confirmed_Test, INVALID AT IDX %d", i);
                        failed = true;
                    }
                }
            }
        }

        REQUIRE(failed==false);
    }
}
