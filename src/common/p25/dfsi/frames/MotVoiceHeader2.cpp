// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/dfsi/frames/MotVoiceHeader2.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/Utils.h"
#include "common/Log.h"

#include <cassert>
#include <cstring>

using namespace p25;
using namespace p25::dfsi;
using namespace p25::dfsi::defines;
using namespace p25::dfsi::frames;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a instance of the MotVoiceHeader2 class. */

MotVoiceHeader2::MotVoiceHeader2() :
    header(nullptr),
    m_source(SourceFlag::QUANTAR)
{
    header = new uint8_t[HCW_LENGTH];
    ::memset(header, 0x00U, HCW_LENGTH);
}

/* Initializes a instance of the MotVoiceHeader2 class. */

MotVoiceHeader2::MotVoiceHeader2(uint8_t* data) :
    header(nullptr),
    m_source(SourceFlag::QUANTAR)
{
    decode(data);
}

/* Finalizes a instance of the MotVoiceHeader2 class. */

MotVoiceHeader2::~MotVoiceHeader2()
{
    if (header != nullptr)
        delete[] header;
}

/* Decode a voice header 2 frame. */

bool MotVoiceHeader2::decode(const uint8_t* data)
{
    assert(data != nullptr);

    m_source = (SourceFlag::E)data[21];

    if (header != nullptr) {
        delete[] header;
    }

    header = new uint8_t[HCW_LENGTH];
    ::memset(header, 0x00U, HCW_LENGTH);
    ::memcpy(header, data + 1U, HCW_LENGTH);

    return true;
}

/* Encode a voice header 2 frame. */

void MotVoiceHeader2::encode(uint8_t* data)
{
    assert(data != nullptr);

    data[0U] = DFSIFrameType::MOT_VHDR_2;

    if (header != nullptr) {
        ::memcpy(data + 1U, header, HCW_LENGTH);
    }

    data[LENGTH - 1U] = (uint8_t)m_source;
}
