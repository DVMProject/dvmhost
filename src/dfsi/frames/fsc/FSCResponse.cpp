// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - DFSI V.24/UDP Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "frames/fsc/FSCResponse.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/Utils.h"
#include "common/Log.h"

#include <cassert>
#include <cstring>

using namespace p25::dfsi;
using namespace p25::dfsi::fsc;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a instance of the FSCResponse class. */

FSCResponse::FSCResponse() :
    m_version(1U)
{
    /* stub */
}

/* Initializes a instance of the FSCResponse class. */

FSCResponse::FSCResponse(uint8_t* data) :
    m_version(1U)
{
    decode(data);
}

/* Decode a FSC message frame. */

bool FSCResponse::decode(const uint8_t* data)
{
    assert(data != nullptr);

    m_version = data[0U];                                       // Response Version

    return true;
}

/* Encode a FSC message frame. */

void FSCResponse::encode(uint8_t* data)
{
    assert(data != nullptr);
    
    data[0U] = m_version;                                       // Response Version
}
