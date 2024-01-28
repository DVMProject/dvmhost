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
#if !defined(__P25_LC_TSBK__OSP_SCCB_H__)
#define  __P25_LC_TSBK__OSP_SCCB_H__

#include "common/Defines.h"
#include "common/p25/lc/TSBK.h"

namespace p25
{
    namespace lc
    {
        namespace tsbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Implements SCCB - Secondary Control Channel Broadcast.
            // ---------------------------------------------------------------------------

            class HOST_SW_API OSP_SCCB : public TSBK {
            public:
                /// <summary>Initializes a new instance of the OSP_SCCB class.</summary>
                OSP_SCCB();

                /// <summary>Decode a trunking signalling block.</summary>
                bool decode(const uint8_t* data, bool rawTSBK = false) override;
                /// <summary>Encode a trunking signalling block.</summary>
                void encode(uint8_t* data, bool rawTSBK = false, bool noTrellis = false) override;

                /// <summary>Returns a string that represents the current TSBK.</summary>
                std::string toString(bool isp = false) override;

            public:
                /// <summary>SCCB channel ID 1.</summary>
                __PROPERTY(uint8_t, sccbChannelId1, SCCBChnId1);
                /// <summary>SCCB channel ID 2.</summary>
                __PROPERTY(uint8_t, sccbChannelId2, SCCBChnId2);

                __COPY(OSP_SCCB);
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__OSP_SCCB_H__
