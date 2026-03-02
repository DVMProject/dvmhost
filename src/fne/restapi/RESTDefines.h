// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
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
#include "host/restapi/RESTDefines.h"

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define FNE_GET_PEER_QUERY              "/peer/query"
#define FNE_GET_PEER_COUNT              "/peer/count"
#define FNE_PUT_PEER_RESET              "/peer/reset"
#define FNE_PUT_PEER_RESET_CONN         "/peer/connreset"

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
#define FNE_PUT_PEER_NAK_PEERID         "/peer/nak/byPeerId"
#define FNE_PUT_PEER_NAK_ADDRESS        "/peer/nak/byAddress"

#define FNE_GET_ADJ_MAP_LIST            "/adjmap/list"
#define FNE_PUT_ADJ_MAP_ADD             "/adjmap/add"
#define FNE_PUT_ADJ_MAP_DELETE          "/adjmap/delete"
#define FNE_GET_ADJ_MAP_COMMIT          "/adjmap/commit"

#define FNE_GET_FORCE_UPDATE            "/force-update"

#define FNE_GET_RELOAD_TGS              "/reload-tgs"
#define FNE_GET_RELOAD_RIDS             "/reload-rids"
#define FNE_GET_RELOAD_PEERLIST         "/reload-peers"
#define FNE_GET_RELOAD_CRYPTO           "/reload-crypto"

#define FNE_GET_STATS                   "/stats"
#define FNE_GET_RESET_TOTAL_CALLS       "/stat-reset-total-calls"
#define FNE_GET_RESET_ACTIVE_CALLS      "/stat-reset-active-calls"
#define FNE_GET_RESET_CALL_COLLISIONS   "/stat-reset-call-collisions"
#define FNE_GET_AFF_LIST                "/report-affiliations"

#define FNE_GET_SPANNING_TREE           "/spanning-tree"

#endif // __FNE_REST_DEFINES_H__
