// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
#include "host/Defines.h"
#include "common/edac/AMBEFEC.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/NXDNUtils.h"
#include "common/Log.h"
#include "common/Utils.h"

using namespace edac;
using namespace nxdn;
using namespace nxdn::defines;

#include <catch2/catch_test_macros.hpp>

TEST_CASE("NXDN", "[AMBE FEC Test]") {
    SECTION("NXDN_AMBEFEC_Test") {
        bool failed = false;

        INFO("NXDN AMBE FEC FEC Test");

        uint8_t testData[] = {
            0xCDU, 0xF5U, 0x9DU, 0x5DU, 0xFCU, 0xFAU, 0x0AU, 0x6EU, 0x8AU, 0x23U, 0x56U, 0xE8U,
            0x17U, 0x49U, 0xC6U, 0x58U, 0x89U, 0x30U, 0x1AU, 0xA5U, 0xF5U, 0xACU, 0x5AU, 0x6EU, 0xF8U, 0x09U, 0x3CU, 0x48U,
            0x0FU, 0x4FU, 0xFDU, 0xCFU, 0x80U, 0xD5U, 0x77U, 0x0CU, 0xFEU, 0xE9U, 0x05U, 0xCEU, 0xE6U, 0x20U, 0xDFU, 0xFFU,
            0x18U, 0x9CU, 0x2DU, 0xA9U
        };

        NXDNUtils::scrambler(testData);

        Utils::dump(2U, "NXDN AMBE FEC Test, descrambled test data", testData, NXDN_FRAME_LENGTH_BYTES);

        AMBEFEC fec = AMBEFEC();

        uint32_t errors = 0U;

        errors += fec.measureNXDNBER(testData + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 0U);
        errors += fec.measureNXDNBER(testData + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 9U);
        errors += fec.measureNXDNBER(testData + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 18U);
        errors += fec.measureNXDNBER(testData + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 27U);

        if (errors > 0)
            failed = true;

cleanup:
        REQUIRE(failed==false);
    }
}
