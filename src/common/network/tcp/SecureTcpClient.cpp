// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#include "common/Defines.h"
#include "common/network/tcp/SecureTcpClient.h"

#if defined(ENABLE_TCP_SSL)

using namespace network::tcp;

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::string SecureTcpClient::m_sslHostname = std::string();

#endif // ENABLE_TCP_SSL