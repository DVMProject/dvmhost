// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Simon Rune, G7RZU
 *  Copyright (C) 2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017,2019,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "dmr/acl/AccessControl.h"

using namespace dmr::acl;

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

RadioIdLookup* AccessControl::m_ridLookup;
TalkgroupRulesLookup* AccessControl::m_tidLookup;

/* Initializes the DMR access control. */
void AccessControl::init(RadioIdLookup* ridLookup, TalkgroupRulesLookup* tidLookup)
{
    m_ridLookup = ridLookup;
    m_tidLookup = tidLookup;
}

/* Helper to validate a source radio ID. */
bool AccessControl::validateSrcId(uint32_t id)
{
    // check if RID ACLs are enabled
    if (!m_ridLookup->getACL()) {
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

/* Helper to validate a talkgroup ID. */
bool AccessControl::validateTGId(uint32_t slotNo, uint32_t id)
{
    // TG0 is never valid
    if (id == 0U)
        return false;

    // check if TID ACLs are enabled
    if (!m_tidLookup->getACL()) {
        return true;
    }

    // lookup TID and perform test for validity
    TalkgroupRuleGroupVoice tid = m_tidLookup->find(id);
    if (tid.isInvalid())
        return false;

    if (!tid.config().active())
        return false;

    if (slotNo != 0) {
        if (tid.source().tgSlot() != slotNo)
            return false;

        return true;
    } else {
        return true;
    }
}

/* Helper to determine if a talkgroup ID is non-preferred. */
bool AccessControl::tgidNonPreferred(uint32_t slotNo, uint32_t id)
{
    // TG0 is never valid
    if (id == 0U)
        return false;

    // check if TID ACLs are enabled
    if (!m_tidLookup->getACL()) {
        return false;
    }

    // lookup TID and perform test for validity
    TalkgroupRuleGroupVoice tid = m_tidLookup->find(id);
    if (tid.config().nonPreferred())
        return true;

    return false;
}
