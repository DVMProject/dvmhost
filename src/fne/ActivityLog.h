// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Converged FNE Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Converged FNE Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__ACTIVITY_LOG_H__)
#define __ACTIVITY_LOG_H__

#include "Defines.h"

#include <string>

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/// <summary>Initializes the activity log.</summary>
extern HOST_SW_API bool ActivityLogInitialise(const std::string& filePath, const std::string& fileRoot);
/// <summary>Finalizes the activity log.</summary>
extern HOST_SW_API void ActivityLogFinalise();
/// <summary>Writes a new entry to the activity log.</summary>
extern HOST_SW_API void ActivityLog(const char* msg, ...);

#endif // __ACTIVITY_LOG_H__
