// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2020 Jonathan Naylor, G4KLX
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "nxdn/NXDNDefines.h"
#include "nxdn/Sync.h"

using namespace nxdn;
using namespace nxdn::defines;

#include <cassert>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/* Helper to append NXDN sync bytes to the passed buffer. */
void Sync::addNXDNSync(uint8_t* data)
{
    assert(data != nullptr);

    for (uint32_t i = 0U; i < NXDN_FSW_BYTES_LENGTH; i++)
        data[i] = (data[i] & ~NXDN_FSW_BYTES_MASK[i]) | NXDN_FSW_BYTES[i];
}
