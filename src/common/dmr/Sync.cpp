// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *
 */
#include "Defines.h"
#include "dmr/DMRDefines.h"
#include "dmr/Sync.h"

using namespace dmr;
using namespace dmr::defines;

#include <cassert>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/* Helper to append DMR data sync bytes to the passed buffer. */
void Sync::addDMRDataSync(uint8_t* data, bool duplex)
{
    assert(data != nullptr);

    if (duplex) {
        for (uint32_t i = 0U; i < 7U; i++)
            data[i + 13U] = (data[i + 13U] & ~SYNC_MASK[i]) | BS_SOURCED_DATA_SYNC[i];
    }
    else {
        for (uint32_t i = 0U; i < 7U; i++)
            data[i + 13U] = (data[i + 13U] & ~SYNC_MASK[i]) | MS_SOURCED_DATA_SYNC[i];
    }
}

/* Helper to append DMR voice sync bytes to the passed buffer. */
void Sync::addDMRAudioSync(uint8_t* data, bool duplex)
{
    assert(data != nullptr);

    if (duplex) {
        for (uint32_t i = 0U; i < 7U; i++)
            data[i + 13U] = (data[i + 13U] & ~SYNC_MASK[i]) | BS_SOURCED_AUDIO_SYNC[i];
    }
    else {
        for (uint32_t i = 0U; i < 7U; i++)
            data[i + 13U] = (data[i + 13U] & ~SYNC_MASK[i]) | MS_SOURCED_AUDIO_SYNC[i];
    }
}
