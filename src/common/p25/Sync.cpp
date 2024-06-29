// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/Sync.h"

using namespace p25;
using namespace p25::defines;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/* Helper to append P25 sync bytes to the passed buffer. */

void Sync::addP25Sync(uint8_t* data)
{
    assert(data != nullptr);

    ::memcpy(data, P25_SYNC_BYTES, P25_SYNC_LENGTH_BYTES);
}
