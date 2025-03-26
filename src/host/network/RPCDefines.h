// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup host_rpc Host RPC
 * @brief Implementation for the host RPC.
 * @ingroup host
 * 
 * @file RPCDefines.h
 * @ingroup host_prc
 */
#if !defined(__RPC_DEFINES_H__)
#define __RPC_DEFINES_H__

#include "Defines.h"

/**
 * @addtogroup host_rpc
 * @{
 */

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define RPC_REGISTER_CC_VC          0x1000U

#define RPC_RELEASE_P25_TG          0x0101U
#define RPC_RELEASE_DMR_TG          0x0102U
#define RPC_RELEASE_NXDN_TG         0x0103U
#define RPC_TOUCH_P25_TG            0x0201U
#define RPC_TOUCH_DMR_TG            0x0202U
#define RPC_TOUCH_NXDN_TG           0x0203U
#define RPC_PERMIT_P25_TG           0x0001U
#define RPC_PERMIT_DMR_TG           0x0002U
#define RPC_PERMIT_NXDN_TG          0x0003U

#define RPC_DMR_TSCC_PAYLOAD_ACT    0x0010U

/** @} */

#endif // __RPC_DEFINES_H__
