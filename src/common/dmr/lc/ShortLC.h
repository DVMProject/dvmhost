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
#if !defined(__DMR_LC__SHORT_LC_H__)
#define __DMR_LC__SHORT_LC_H__

#include "common/Defines.h"

namespace dmr
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents short DMR link control.
        // ---------------------------------------------------------------------------

        class HOST_SW_API ShortLC {
        public:
            /// <summary>Initializes a new instance of the ShortLC class.</summary>
            ShortLC();
            /// <summary>Finalizes a instance of the ShortLC class.</summary>
            ~ShortLC();

            /// <summary>Decode DMR short-link control data.</summary>
            bool decode(const uint8_t* in, uint8_t* out);
            /// <summary>Encode DMR short-link control data.</summary>
            void encode(const uint8_t* in, uint8_t* out);

        private:
            bool* m_rawData;
            bool* m_deInterData;

            /// <summary></summary>
            void decodeExtractBinary(const uint8_t* in);
            /// <summary></summary>
            void decodeDeInterleave();
            /// <summary></summary>
            bool decodeErrorCheck();
            /// <summary></summary>
            void decodeExtractData(uint8_t* data) const;

            /// <summary></summary>
            void encodeExtractData(const uint8_t* in) const;
            /// <summary></summary>
            void encodeErrorCheck();
            /// <summary></summary>
            void encodeInterleave();
            /// <summary></summary>
            void encodeExtractBinary(uint8_t* data);
        };
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC__SHORT_LC_H__
