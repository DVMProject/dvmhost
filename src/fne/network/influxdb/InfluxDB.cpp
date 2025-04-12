// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "fne/network/influxdb/InfluxDB.h"

#if defined(_WIN32)
#include <ws2tcpip.h>
#include <Winsock2.h>
#endif

using namespace network::influxdb;

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

uint32_t detail::TSCaller::m_currThreadCnt = 0U;
