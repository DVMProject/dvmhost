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
#if !defined(__QR1676_H__)
#define __QR1676_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements Quadratic residue (16,7,6) forward error correction.
    // ---------------------------------------------------------------------------

    class HOST_SW_API QR1676 {
    public:
        /// <summary>Decode QR (16,7,6) FEC.</summary>
        static uint8_t decode(const uint8_t* data);
        /// <summary>Encode QR (16,7,6) FEC.</summary>
        static void encode(uint8_t* data);

    private:
        /// <summary></summary>
        static uint32_t getSyndrome1576(uint32_t pattern);
    };
} // namespace edac

#endif // __QR1676_H__
