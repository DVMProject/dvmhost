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
*   Copyright (C) 2015 Jonathan Naylor, G4KLX
*
*/
#if !defined(__GOLAY2087_H__)
#define __GOLAY2087_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements Golay (20,8,7) forward error correction.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Golay2087 {
    public:
        /// <summary>Decode Golay (20,8,7) FEC.</summary>
        static uint8_t decode(const uint8_t* data);
        /// <summary>Encode Golay (20,8,7) FEC.</summary>
        static void encode(uint8_t* data);

    private:
        /// <summary></summary>
        static uint32_t getSyndrome1987(uint32_t pattern);
    };
} // namespace edac

#endif // __GOLAY2087_H__
