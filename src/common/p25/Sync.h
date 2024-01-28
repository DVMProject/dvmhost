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
*   Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
*
*/
#if !defined(__P25_SYNC_H__)
#define __P25_SYNC_H__

#include "common/Defines.h"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Helper class for generating P25 sync data.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Sync {
    public:
        /// <summary>Helper to append P25 sync bytes to the passed buffer.</summary>
        static void addP25Sync(uint8_t* data);
    };
} // namespace p25

#endif // __P25_SYNC_H__
