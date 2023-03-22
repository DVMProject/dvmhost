/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2016 by Simon Rune G7RZU
*   Copyright (C) 2016,2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2017,2019 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*/
#if !defined(__DMR_ACL__ACCESS_CONTROL_H__)
#define __DMR_ACL__ACCESS_CONTROL_H__

#include "Defines.h"
#include "lookups/RadioIdLookup.h"
#include "lookups/TalkgroupIdLookup.h"

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
            static void init(RadioIdLookup* ridLookup, TalkgroupIdLookup* tidLookup);

            /// <summary>Helper to validate a source radio ID.</summary>
            static bool validateSrcId(uint32_t id);
            /// <summary>Helper to validate a talkgroup ID.</summary>
            static bool validateTGId(uint32_t slotNo, uint32_t id);

        private:
            static RadioIdLookup* m_ridLookup;
            static TalkgroupIdLookup* m_tidLookup;
        };
    } // namespace acl
} // namespace dmr

#endif // __DMR_ACL__ACCESS_CONTROL_H__
