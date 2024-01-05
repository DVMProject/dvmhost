/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2016 by Simon Rune G7RZU
*   Copyright (C) 2016,2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2017,2019,2022 Bryan Biedenkapp N2PLL
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
#include "Defines.h"
#include "nxdn/acl/AccessControl.h"
#include "Log.h"

using namespace nxdn::acl;

#include <algorithm>
#include <vector>
#include <cstring>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

RadioIdLookup* AccessControl::m_ridLookup;
TalkgroupRulesLookup* AccessControl::m_tidLookup;

/// <summary>
/// Initializes the P25 access control.
/// </summary>
/// <param name="ridLookup">Instance of the RadioIdLookup class.</param>
/// <param name="tidLookup">Instance of the TalkgroupRulesLookup class.</param>
void AccessControl::init(RadioIdLookup* ridLookup, TalkgroupRulesLookup* tidLookup)
{
    m_ridLookup = ridLookup;
    m_tidLookup = tidLookup;
}

/// <summary>
/// Helper to validate a source radio ID.
/// </summary>
/// <param name="id">Source Radio ID.</param>
/// <returns>True, if source radio ID is valid, otherwise false.</returns>
bool AccessControl::validateSrcId(uint32_t id)
{
    // check if RID ACLs are enabled
    if (m_ridLookup->getACL() == false) {
        RadioId rid = m_ridLookup->find(id);
        if (!rid.radioDefault() && !rid.radioEnabled()) {
            return false;
        }

        return true;
    }

    // lookup RID and perform test for validity
    RadioId rid = m_ridLookup->find(id);
    if (!rid.radioEnabled())
        return false;

    return true;
}

/// <summary>
/// Helper to validate a talkgroup ID.
/// </summary>
/// <param name="slotNo">DMR slot number.</param>
/// <param name="id">Talkgroup ID.</param>
/// <returns>True, if talkgroup ID is valid, otherwise false.</returns>
bool AccessControl::validateTGId(uint32_t id)
{
    // TG0 is never valid
    if (id == 0U)
        return false;

    // check if TID ACLs are enabled
    if (m_tidLookup->getACL() == false) {
        return true;
    }

    // lookup TID and perform test for validity
    TalkgroupRuleGroupVoice tid = m_tidLookup->find(id);
    if (tid.isInvalid())
        return false;

    if (!tid.config().active())
        return false;

    return true;
}
