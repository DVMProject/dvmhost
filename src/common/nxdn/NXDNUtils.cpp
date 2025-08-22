// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2020 Jonathan Naylor, G4KLX
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "nxdn/NXDNDefines.h"
#include "nxdn/NXDNUtils.h"
#include "Utils.h"

using namespace nxdn;
using namespace nxdn::defines;

#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t SCRAMBLER[] = {
    0x00U, 0x00U, 0x00U, 0x82U, 0xA0U, 0x88U, 0x8AU, 0x00U, 0xA2U, 0xA8U, 0x82U, 0x8AU, 0x82U, 0x02U,
    0x20U, 0x08U, 0x8AU, 0x20U, 0xAAU, 0xA2U, 0x82U, 0x08U, 0x22U, 0x8AU, 0xAAU, 0x08U, 0x28U, 0x88U,
    0x28U, 0x28U, 0x00U, 0x0AU, 0x02U, 0x82U, 0x20U, 0x28U, 0x82U, 0x2AU, 0xAAU, 0x20U, 0x22U, 0x80U,
    0xA8U, 0x8AU, 0x08U, 0xA0U, 0xAAU, 0x02U };

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/* Helper to scramble the NXDN frame data. */

void NXDNUtils::scrambler(uint8_t* data)
{
    assert(data != nullptr);

    for (uint32_t i = 0U; i < NXDN_FRAME_LENGTH_BYTES; i++)
        data[i] ^= SCRAMBLER[i];
}

/* Helper to add the post field bits on NXDN frame data. */

void NXDNUtils::addPostBits(uint8_t* data)
{
    assert(data != nullptr);

    // post field
    for (uint32_t i = 0U; i < NXDN_CAC_E_POST_FIELD_BITS; i++) {
        uint32_t n = i + NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_CAC_FEC_LENGTH_BITS + NXDN_CAC_E_POST_FIELD_BITS;
        bool b = READ_BIT(NXDN_PREAMBLE, i);
        WRITE_BIT(data, n, b);
    }
}

/* Helper to convert a cause code to a string. */

std::string NXDNUtils::causeToString(uint8_t cause)
{
    switch (cause) {
    case CauseResponse::RSRC_NOT_AVAIL_NETWORK:
        return std::string("RSRC_NOT_AVAIL_NETWORK (Resource Not Available - Network)");
    case CauseResponse::RSRC_NOT_AVAIL_TEMP:
        return std::string("RSRC_NOT_AVAIL_TEMP (Resource Not Available - Temporary)");
    case CauseResponse::RSRC_NOT_AVAIL_QUEUED:
        return std::string("RSRC_NOT_AVAIL_QUEUED (Resource Not Available - Queued)");
    case CauseResponse::SVC_UNAVAILABLE:
        return std::string("SVC_UNAVAILABLE (Service Unavailable)");
    case CauseResponse::PROC_ERROR:
        return std::string("PROC_ERROR (Procedure Error - Lack of packet data)");
    case CauseResponse::PROC_ERROR_UNDEF:
        return std::string("PROC_ERROR_UNDEF (Procedure Error - Invalid packet data)");

    case CauseResponse::VD_GRP_NOT_PERM:
        return std::string("VD_GRP_NOT_PERM (Voice Group Not Permitted)");
    case CauseResponse::VD_REQ_UNIT_NOT_PERM:
        return std::string("VD_REQ_UNIT_NOT_PERM (Voice Requesting Unit Not Permitted)");
    case CauseResponse::VD_TGT_UNIT_NOT_PERM:
        return std::string("VD_TGT_UNIT_NOT_PERM (Voice Target Unit Not Permitted)");
    case CauseResponse::VD_REQ_UNIT_NOT_REG:
        return std::string("VD_REQ_UNIT_NOT_REG (Voice Requesting Unit Not Registered)");
    case CauseResponse::VD_QUE_CHN_RESOURCE_NOT_AVAIL:
        return std::string("VD_QUE_CHN_RESOURCE_NOT_AVAIL (Voice Channel Resources Unavailable)");
    case CauseResponse::VD_QUE_TGT_UNIT_BUSY:
        return std::string("VD_QUE_TGT_UNIT_BUSY (Voice Target Unit Busy)");
    case CauseResponse::VD_QUE_GRP_BUSY:
        return std::string("VD_QUE_GRP_BUSY (Voice Group Busy)");

    case CauseResponse::DISC_USER:
        return std::string("DISC_USER (Disconnect by User)");
    case CauseResponse::DISC_OTHER:
        return std::string("DISC_OTHER (Other Disconnect)");

    default:
        return std::string();
    }
}
