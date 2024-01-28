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
#if !defined(__RS129_H__)
#define __RS129_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements Reed-Solomon (12,9) forward error correction.
    // ---------------------------------------------------------------------------

    class HOST_SW_API RS129 {
    public:
        /// <summary>Check RS (12,9) FEC.</summary>
        static bool check(const uint8_t* in);

        /// <summary>Encode RS (12,9) FEC.</summary>
        static void encode(const uint8_t* msg, uint32_t nbytes, uint8_t* parity);

    private:
        /// <summary></summary>
        static uint8_t gmult(uint8_t a, uint8_t b);
    };
} // namespace edac

#endif // __RS129_H__
