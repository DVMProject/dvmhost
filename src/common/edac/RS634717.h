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
*   Copyright (C) 2016 Jonathan Naylor, G4KLX
*   Copyright (C) 2017,2023 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__RS634717_H__)
#define __RS634717_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements Reed-Solomon (63,47,17). Which is also used to implement
    //      Reed-Solomon (24,12,13), (24,16,9) and (36,20,17) forward
    //      error correction.
    // ---------------------------------------------------------------------------

    class HOST_SW_API RS634717 {
    public:
        /// <summary>Initializes a new instance of the RS634717 class.</summary>
        RS634717();
        /// <summary>Finalizes a instance of the RS634717 class.</summary>
        ~RS634717();

        /// <summary>Decode RS (24,12,13) FEC.</summary>
        bool decode241213(uint8_t* data);
        /// <summary>Encode RS (24,12,13) FEC.</summary>
        void encode241213(uint8_t* data);

        /// <summary>Decode RS (24,16,9) FEC.</summary>
        bool decode24169(uint8_t* data);
        /// <summary>Encode RS (24,16,9) FEC.</summary>
        void encode24169(uint8_t* data);

        /// <summary>Decode RS (36,20,17) FEC.</summary>
        bool decode362017(uint8_t* data);
        /// <summary>Encode RS (36,20,17) FEC.</summary>
        void encode362017(uint8_t* data);

    private:
        /// <summary></summary>
        uint8_t gf6Mult(uint8_t a, uint8_t b) const;
    };
} // namespace edac

#endif // __RS634717_H__
