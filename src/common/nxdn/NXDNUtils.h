// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2020 Jonathan Naylor, G4KLX
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__NXDN_UTILS_H__)
#define __NXDN_UTILS_H__

#include "common/Defines.h"

namespace nxdn
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements various helper functions for scrambling NXDN data.
    // ---------------------------------------------------------------------------

    class HOST_SW_API NXDNUtils {
    public:
        /// <summary>Helper to scramble the NXDN frame data.</summary>
        static void scrambler(uint8_t* data);
        /// <summary>Helper to add the post field bits on NXDN frame data.</summary>
        static void addPostBits(uint8_t* data);
    };
} // namespace nxdn

#endif // __NXDN_UTILS_H__
