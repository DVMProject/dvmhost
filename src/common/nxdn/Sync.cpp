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
*   Copyright (C) 2020 Jonathan Naylor, G4KLX
*
*/
#include "Defines.h"
#include "nxdn/NXDNDefines.h"
#include "nxdn/Sync.h"

using namespace nxdn;

#include <cassert>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Helper to append NXDN sync bytes to the passed buffer.
/// </summary>
/// <param name="data"></param>
void Sync::addNXDNSync(uint8_t* data)
{
    assert(data != nullptr);

    for (uint32_t i = 0U; i < NXDN_FSW_BYTES_LENGTH; i++)
        data[i] = (data[i] & ~NXDN_FSW_BYTES_MASK[i]) | NXDN_FSW_BYTES[i];
}
