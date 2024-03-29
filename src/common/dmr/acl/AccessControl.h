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
*   Copyright (C) 2016 Simon Rune, G7RZU
*   Copyright (C) 2016,2017 Jonathan Naylor, G4KLX
*   Copyright (C) 2017,2019,2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DMR_ACL__ACCESS_CONTROL_H__)
#define __DMR_ACL__ACCESS_CONTROL_H__

#include "common/Defines.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"

namespace dmr
{
    namespace acl
    {
        using namespace ::lookups;

        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      This class implements radio and talkgroup ID access control checking.
        // ---------------------------------------------------------------------------

        class HOST_SW_API AccessControl {
        public:
            /// <summary>Initializes the DMR access control.</summary>
            static void init(RadioIdLookup* ridLookup, TalkgroupRulesLookup* tidLookup);

            /// <summary>Helper to validate a source radio ID.</summary>
            static bool validateSrcId(uint32_t id);
            /// <summary>Helper to validate a talkgroup ID.</summary>
            static bool validateTGId(uint32_t slotNo, uint32_t id);

            /// <summary>Helper to determine if a talkgroup ID is non-preferred.</summary>
            static bool tgidNonPreferred(uint32_t slotNo, uint32_t id);

        private:
            static RadioIdLookup* m_ridLookup;
            static TalkgroupRulesLookup* m_tidLookup;
        };
    } // namespace acl
} // namespace dmr

#endif // __DMR_ACL__ACCESS_CONTROL_H__
