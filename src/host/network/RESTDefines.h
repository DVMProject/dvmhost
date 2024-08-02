// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup host_rest Host REST API
 * @brief Implementation for the host REST API.
 * @ingroup host
 * 
 * @file RESTDefines.h
 * @ingroup host_rest
 */
#if !defined(__REST_DEFINES_H__)
#define __REST_DEFINES_H__

#include "Defines.h"

/**
 * @addtogroup host_rest
 * @{
 */

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define DVM_REST_RAND_MAX 0xfffffffffffffffe

#define PUT_AUTHENTICATE                "/auth"

#define GET_VERSION                     "/version"
#define GET_STATUS                      "/status"
#define GET_VOICE_CH                    "/voice-ch"

#define PUT_MDM_MODE                    "/mdm/mode"
#define MODE_OPT_IDLE                   "idle"
#define MODE_OPT_LCKOUT                 "lockout"
#define MODE_OPT_FDMR                   "dmr"
#define MODE_OPT_FP25                   "p25"
#define MODE_OPT_FNXDN                  "nxdn"

#define RID_CMD_P25_SET_MFID            "p25-setmfid"
#define RID_CMD_PAGE                    "page"
#define RID_CMD_CHECK                   "check"
#define RID_CMD_INHIBIT                 "inhibit"
#define RID_CMD_UNINHIBIT               "uninhibit"
#define RID_CMD_DYN_REGRP               "dyn-regrp"
#define RID_CMD_DYN_REGRP_CANCEL        "dyn-regrp-cancel"
#define RID_CMD_DYN_REGRP_LOCK          "dyn-regrp-lock"
#define RID_CMD_DYN_REGRP_UNLOCK        "dyn-regrp-unlock"
#define RID_CMD_GAQ                     "group-aff-req"
#define RID_CMD_UREG                    "unit-reg"
#define RID_CMD_EMERG                   "emerg"

#define PUT_MDM_KILL                    "/mdm/kill"

#define PUT_SET_SUPERVISOR              "/set-supervisor"
#define PUT_PERMIT_TG                   "/permit-tg"
#define PUT_GRANT_TG                    "/grant-tg"
#define GET_RELEASE_GRNTS               "/release-grants"
#define GET_RELEASE_AFFS                "/release-affs"

#define PUT_REGISTER_CC_VC              "/register-cc-vc"
#define PUT_RELEASE_TG                  "/release-tg-grant"
#define PUT_TOUCH_TG                    "/touch-tg-grant"

#define GET_RID_WHITELIST_BASE          "/rid-whitelist/"
#define GET_RID_WHITELIST               GET_RID_WHITELIST_BASE"(\\d+)"
#define GET_RID_BLACKLIST_BASE          "/rid-blacklist/"
#define GET_RID_BLACKLIST               GET_RID_BLACKLIST_BASE"(\\d+)"

#define GET_DMR_BEACON                  "/dmr/beacon"
#define GET_DMR_DEBUG_BASE              "/dmr/debug/"
#define GET_DMR_DEBUG                   GET_DMR_DEBUG_BASE"(\\d+)/(\\d+)"
#define GET_DMR_DUMP_CSBK_BASE          "/dmr/dump-csbk/"
#define GET_DMR_DUMP_CSBK               GET_DMR_DUMP_CSBK_BASE"(\\d+)"
#define PUT_DMR_RID                     "/dmr/rid"
#define GET_DMR_CC_DEDICATED            "/dmr/cc-enable"
#define GET_DMR_CC_BCAST                "/dmr/cc-broadcast"
#define PUT_DMR_TSCC_PAYLOAD_ACT        "/dmr/payload-activate"
#define GET_DMR_AFFILIATIONS            "/dmr/report-affiliations"

#define GET_P25_CC                      "/p25/cc"
#define GET_P25_CC_FALLBACK_BASE        "/p25/cc-fallback/"
#define GET_P25_CC_FALLBACK             GET_P25_CC_FALLBACK_BASE"(\\d+)"
#define GET_P25_DEBUG_BASE              "/p25/debug/"
#define GET_P25_DEBUG                   GET_P25_DEBUG_BASE"(\\d+)/(\\d+)"
#define GET_P25_DUMP_TSBK_BASE          "/p25/dump-tsbk/"
#define GET_P25_DUMP_TSBK               GET_P25_DUMP_TSBK_BASE"(\\d+)"
#define PUT_P25_RID                     "/p25/rid"
#define GET_P25_CC_DEDICATED            "/p25/cc-enable"
#define GET_P25_CC_BCAST                "/p25/cc-broadcast"
#define PUT_P25_RAW_TSBK                "/p25/raw-tsbk"
#define GET_P25_AFFILIATIONS            "/p25/report-affiliations"

#define GET_NXDN_CC                     "/nxdn/cc"
#define GET_NXDN_DEBUG_BASE             "/nxdn/debug/"
#define GET_NXDN_DEBUG                  GET_NXDN_DEBUG_BASE"(\\d+)/(\\d+)"
#define GET_NXDN_DUMP_RCCH_BASE         "/nxdn/dump-rcch/"
#define GET_NXDN_DUMP_RCCH              GET_NXDN_DUMP_RCCH_BASE"(\\d+)"
#define GET_NXDN_CC_DEDICATED           "/nxdn/cc-enable"
#define GET_NXDN_AFFILIATIONS           "/nxdn/report-affiliations"

/** @} */

#endif // __REST_DEFINES_H__
