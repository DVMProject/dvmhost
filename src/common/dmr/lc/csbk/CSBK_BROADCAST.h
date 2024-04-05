// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022-2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DMR_LC_CSBK__CSBK_BROADCAST_H__)
#define  __DMR_LC_CSBK__CSBK_BROADCAST_H__

#include "common/Defines.h"
#include "common/dmr/lc/CSBK.h"

namespace dmr
{
    namespace lc
    {
        namespace csbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Implements BCAST - Announcement PDUs
            // ---------------------------------------------------------------------------

            class HOST_SW_API CSBK_BROADCAST : public CSBK {
            public:
                /// <summary>Initializes a new instance of the CSBK_BROADCAST class.</summary>
                CSBK_BROADCAST();

                /// <summary>Decode a control signalling block.</summary>
                bool decode(const uint8_t* data) override;
                /// <summary>Encode a control signalling block.</summary>
                void encode(uint8_t* data) override;

                /// <summary>Returns a string that represents the current CSBK.</summary>
                std::string toString() override;

            public:
                /// <summary>Broadcast Announcment Type.</summary>
                __PROPERTY(uint8_t, anncType, AnncType);
                /// <summary>Broadcast Hibernation Flag.</summary>
                __PROPERTY(bool, hibernating, Hibernating);

                /// <summary>Broadcast Announce/Withdraw Channel 1 Flag.</summary>
                __PROPERTY(bool, annWdCh1, AnnWdCh1);
                /// <summary>Broadcast Announce/Withdraw Channel 2 Flag.</summary>
                __PROPERTY(bool, annWdCh2, AnnWdCh2);

                /// <summary>Require Registration.</summary>
                __PROPERTY(bool, requireReg, RequireReg);
                /// <summary>System Identity.</summary>
                __PROPERTY(uint32_t, systemId, SystemId);

                /// <summary>Backoff Number.</summary>
                __PROPERTY(uint8_t, backoffNo, BackoffNo);

                __COPY(CSBK_BROADCAST);
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC_CSBK__CSBK_BROADCAST_H__
