// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__P25_LC__AMBT_H__)
#define  __P25_LC__AMBT_H__

#include "common/Defines.h"
#include "common/p25/lc/TSBK.h"
#include "common/p25/data/DataHeader.h"
#include "common/p25/data/DataBlock.h"

namespace p25
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents link control data for Alternate Trunking packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API AMBT : public TSBK {
        public:
            /// <summary>Initializes a new instance of the AMBT class.</summary>
            AMBT();

            /// <summary>Decode a alternate trunking signalling block.</summary>
            virtual bool decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks) = 0;
            /// <summary>Encode a alternate trunking signalling block.</summary>
            virtual void encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData) = 0;

            /// <summary>Decode a trunking signalling block.</summary>
            bool decode(const uint8_t* data, bool rawTSBK = false);
            /// <summary>Encode a trunking signalling block.</summary>
            void encode(uint8_t* data, bool rawTSBK = false, bool noTrellis = false);

        protected:
            /// <summary>Internal helper to convert AMBT bytes to a 64-bit long value.</summary>
            static ulong64_t toValue(const data::DataHeader& dataHeader, const uint8_t* pduUserData);

            /// <summary>Internal helper to decode a trunking signalling block.</summary>
            bool decode(const data::DataHeader& dataHeader, const data::DataBlock* blocks, uint8_t* pduUserData);
            /// <summary>Internal helper to encode a trunking signalling block.</summary>
            void encode(data::DataHeader& dataHeader, uint8_t* pduUserData);
        };
    } // namespace lc
} // namespace p25

#endif // __P25_LC__AMBT_H__
