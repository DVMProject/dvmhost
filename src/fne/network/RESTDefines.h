// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup fne_rest FNE REST API
 * @brief Implementation for the FNE REST API.
 * @ingroup fne
 * 
 * @file RESTDefines.h
 * @ingroup fne_rest
 */
#if !defined(__FNE_REST_DEFINES_H__)
#define __FNE_REST_DEFINES_H__

#include "fne/Defines.h"
#include "host/network/RESTDefines.h"

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define FNE_GET_PEER_QUERY              "/peer/query"
#define FNE_GET_PEER_COUNT              "/peer/count"
#define FNE_PUT_PEER_RESET              "/peer/reset"

#define FNE_GET_RID_QUERY               "/rid/query"
#define FNE_PUT_RID_ADD                 "/rid/add"
#define FNE_PUT_RID_DELETE              "/rid/delete"
#define FNE_GET_RID_COMMIT              "/rid/commit"

#define FNE_GET_TGID_QUERY              "/tg/query"
#define FNE_PUT_TGID_ADD                "/tg/add"
#define FNE_PUT_TGID_DELETE             "/tg/delete"
#define FNE_GET_TGID_COMMIT             "/tg/commit"

#define FNE_GET_PEER_LIST               "/peer/list"
#define FNE_PUT_PEER_ADD                "/peer/add"
#define FNE_PUT_PEER_DELETE             "/peer/delete"
#define FNE_GET_PEER_COMMIT             "/peer/commit"

#define FNE_GET_FORCE_UPDATE            "/force-update"

#define FNE_GET_RELOAD_TGS              "/reload-tgs"
#define FNE_GET_RELOAD_RIDS             "/reload-rids"

#define FNE_GET_AFF_LIST                "/report-affiliations"

#endif // __FNE_REST_DEFINES_H__
