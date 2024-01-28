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
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__P25_DATA__LOW_SPEED_DATA_H__)
#define __P25_DATA__LOW_SPEED_DATA_H__

#include "common/Defines.h"

namespace p25
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents embedded low speed data in P25 LDUs.
        // ---------------------------------------------------------------------------

        class HOST_SW_API LowSpeedData {
        public:
            /// <summary>Initializes a new instance of the LowSpeedData class.</summary>
            LowSpeedData();
            /// <summary>Finalizes a new instance of the LowSpeedData class.</summary>
            ~LowSpeedData();

            /// <summary>Equals operator.</summary>
            LowSpeedData& operator=(const LowSpeedData& data);

            /// <summary>Decodes embedded low speed data.</summary>
            void process(uint8_t* data);
            /// <summary>Encode embedded low speed data.</summary>
            void encode(uint8_t* data) const;

        public:
            /// <summary>Low speed data 1 value.</summary>
            __PROPERTY(uint8_t, lsd1, LSD1);
            /// <summary>Low speed data 2 value.</summary>
            __PROPERTY(uint8_t, lsd2, LSD2);

        private:
            /// <summary></summary>
            uint8_t encode(const uint8_t in) const;
        };
    } // namespace data
} // namespace p25

#endif // __P25_DATA__LOW_SPEED_DATA_H__
