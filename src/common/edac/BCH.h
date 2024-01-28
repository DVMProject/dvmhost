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
*
*/
#if !defined(__BCH_H__)
#define __BCH_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements Bose/Chaudhuri/Hocquenghem codes for protecting P25 NID
    //      data.
    // ---------------------------------------------------------------------------

    class HOST_SW_API BCH {
    public:
        /// <summary>Initializes a new instance of the BCH class.</summary>
        BCH();
        /// <summary>Finalizes a instance of the BCH class.</summary>
        ~BCH();

        /// <summary>Encodes input data with BCH.</summary>
        void encode(uint8_t* data);

    private:
        /// <summary></summary>
        void encode(const int* data, int* bb);
    };
} // namespace edac

#endif // __BCH_H__
