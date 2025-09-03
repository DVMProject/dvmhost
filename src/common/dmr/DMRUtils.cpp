// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "dmr/DMRDefines.h"
#include "dmr/DMRUtils.h"
#include "Utils.h"

using namespace dmr;
using namespace dmr::defines;

#include <cassert>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/* Helper to convert a denial reason code to a string. */

std::string DMRUtils::rsnToString(uint8_t reason)
{
    switch (reason) {
    case ReasonCode::TS_ACK_RSN_MSG:
        return std::string("TS_ACK_RSN_MSG (Message Accepted)");
    case ReasonCode::TS_ACK_RSN_REG:
        return std::string("TS_ACK_RSN_REG (Registration Accepted)");
    case ReasonCode::TS_ACK_RSN_AUTH_RESP:
        return std::string("TS_ACK_RSN_AUTH_RESP (Authentication Challenge Response)");
    case ReasonCode::TS_ACK_RSN_REG_SUB_ATTACH:
        return std::string("TS_ACK_RSN_REG_SUB_ATTACH (Registration Response with subscription)");
    case ReasonCode::MS_ACK_RSN_MSG:
        return std::string("MS_ACK_RSN_MSG (Message Accepted)");
    case ReasonCode::MS_ACK_RSN_AUTH_RESP:
        return std::string("MS_ACK_RSN_AUTH_RESP (Authentication Challenge Response)");

    case ReasonCode::TS_DENY_RSN_SYS_UNSUPPORTED_SVC:
        return std::string("TS_DENY_RSN_SYS_UNSUPPORTED_SVC (System Unsupported Service)");
    case ReasonCode::TS_DENY_RSN_PERM_USER_REFUSED:
        return std::string("TS_DENY_RSN_PERM_USER_REFUSED (User Permenantly Refused)");
    case ReasonCode::TS_DENY_RSN_TEMP_USER_REFUSED:
        return std::string("TS_DENY_RSN_TEMP_USER_REFUSED (User Temporarily Refused)");
    case ReasonCode::TS_DENY_RSN_TRSN_SYS_REFUSED:
        return std::string("TS_DENY_RSN_TRSN_SYS_REFUSED (System Refused)");
    case ReasonCode::TS_DENY_RSN_TGT_NOT_REG:
        return std::string("TS_DENY_RSN_TGT_NOT_REG (Target Not Registered)");
    case ReasonCode::TS_DENY_RSN_TGT_UNAVAILABLE:
        return std::string("TS_DENY_RSN_TGT_UNAVAILABLE (Target Unavailable)");
    case ReasonCode::TS_DENY_RSN_SYS_BUSY:
        return std::string("TS_DENY_RSN_SYS_BUSY (System Busy)");
    case ReasonCode::TS_DENY_RSN_SYS_NOT_READY:
        return std::string("TS_DENY_RSN_SYS_NOT_READY (System Not Ready)");
    case ReasonCode::TS_DENY_RSN_CALL_CNCL_REFUSED:
        return std::string("TS_DENY_RSN_CALL_CNCL_REFUSED (Call Cancel Refused)");
    case ReasonCode::TS_DENY_RSN_REG_REFUSED:
        return std::string("TS_DENY_RSN_REG_REFUSED (Registration Refused)");
    case ReasonCode::TS_DENY_RSN_REG_DENIED:
        return std::string("TS_DENY_RSN_REG_DENIED (Registration Denied)");
    case ReasonCode::TS_DENY_RSN_MS_NOT_REG:
        return std::string("TS_DENY_RSN_MS_NOT_REG (MS Not Registered)");
    case ReasonCode::TS_DENY_RSN_TGT_BUSY:
        return std::string("TS_DENY_RSN_TGT_BUSY (Target Busy)");
    case ReasonCode::TS_DENY_RSN_TGT_GROUP_NOT_VALID:
        return std::string("TS_DENY_RSN_TGT_GROUP_NOT_VALID (Group Not Valid)");

    case ReasonCode::TS_QUEUED_RSN_NO_RESOURCE:
        return std::string("TS_QUEUED_RSN_NO_RESOURCE (No Resources Available)");
    case ReasonCode::TS_QUEUED_RSN_SYS_BUSY:
        return std::string("TS_QUEUED_RSN_SYS_BUSY (System Busy)");

    case ReasonCode::TS_WAIT_RSN:
        return std::string("TS_WAIT_RSN (Wait)");

    case ReasonCode::MS_DENY_RSN_UNSUPPORTED_SVC:
        return std::string("MS_DENY_RSN_UNSUPPORTED_SVC (Service Unsupported)");

    default:
        return std::string();
    }
}
