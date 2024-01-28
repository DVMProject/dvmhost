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
*   Copyright (C) 2010,2016,2021 Jonathan Naylor, G4KLX
*   Copyright (C) 2017,2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__GOLAY24128_H__)
#define __GOLAY24128_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements Golay (23,12,7) and Golay (24,12,8) forward error
    //      correction.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Golay24128 {
    public:
        /// <summary>Decode Golay (23,12,7) FEC.</summary>
        static uint32_t decode23127(uint32_t code);
        /// <summary>Decode Golay (24,12,8) FEC.</summary>
        static bool decode24128(uint32_t code, uint32_t& out);
        /// <summary>Decode Golay (24,12,8) FEC.</summary>
        static bool decode24128(uint8_t* bytes, uint32_t& out);
        /// <summary>Decode Golay (24,12,8) FEC.</summary>
        static void decode24128(uint8_t* data, const uint8_t* raw, uint32_t msglen);

        /// <summary>Encode Golay (23,12,7) FEC.</summary>
        static uint32_t encode23127(uint32_t data);
        /// <summary>Encode Golay (24,12,8) FEC.</summary>
        static uint32_t encode24128(uint32_t data);
        /// <summary>Encode Golay (24,12,8) FEC.</summary>
        static void encode24128(uint8_t* data, const uint8_t* raw, uint32_t msglen);

    private:
        /// <summary></summary>
        static uint32_t getSyndrome23127(uint32_t pattern);
    };
} // namespace edac

#endif // __GOLAY24128_H__
