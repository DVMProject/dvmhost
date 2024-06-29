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
/**
 * @file Data.h
 * @ingroup dmr
 * @file Data.cpp
 * @ingroup dmr
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
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements radio and talkgroup ID access control checking.
         * @ingroup dmr
         */
        class HOST_SW_API AccessControl {
        public:
            /**
             * @brief Initializes the DMR access control.
             * @param ridLookup Instance of the RadioIdLooup class.
             * @param tidLookup Instance of the TalkgroupRulesLooup class.
             */
            static void init(RadioIdLookup* ridLookup, TalkgroupRulesLookup* tidLookup);

            /**
             * @brief Helper to validate a source radio ID.
             * @param id Source Radio ID (RID).
             * @returns bool True, if source radio ID is valid, otherwise false.
             */
            static bool validateSrcId(uint32_t id);
            /**
             * @brief Helper to validate a talkgroup ID.
             * @param slotNo DMR slot number.
             * @param id Talkgroup ID (TGID).
             * @returns bool True, if talkgroup ID is valid, otherwise false.
             */
            static bool validateTGId(uint32_t slotNo, uint32_t id);

            /**
             * @brief Helper to determine if a talkgroup ID is non-preferred.
             * @param slotNo DMR slot number.
             * @param id Talkgroup ID (TGID).
             * @returns bool True, if talkgroup ID is non-preferred, otherwise false.
             */
            static bool tgidNonPreferred(uint32_t slotNo, uint32_t id);

        private:
            static RadioIdLookup* m_ridLookup;
            static TalkgroupRulesLookup* m_tidLookup;
        };
    } // namespace acl
} // namespace dmr

#endif // __DMR_ACL__ACCESS_CONTROL_H__
