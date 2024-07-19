// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - DFSI V.24/UDP Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 * 
 */
#include "CallData.h"

using namespace network;
using namespace modem;
using namespace p25;
using namespace p25::defines;
using namespace p25::dfsi::frames;
using namespace dfsi;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the VoiceCallData class. */

VoiceCallData::VoiceCallData() :
    srcId(0U),
    dstId(0U),
    lco(0U),
    mfId(MFG_STANDARD),
    serviceOptions(0U),
    lsd1(0U),
    lsd2(0U),
    mi(),
    algoId(ALGO_UNENCRYPT),
    kId(0U),
    VHDR1(),
    VHDR2(),
    netLDU1(),
    netLDU2(),
    seqNo(0U),
    n(0U),
    streamId(0U)
{
    mi = new uint8_t[MI_LENGTH_BYTES];
    VHDR1 = new uint8_t[MotVoiceHeader1::HCW_LENGTH];
    VHDR2 = new uint8_t[MotVoiceHeader2::HCW_LENGTH];
    netLDU1 = new uint8_t[9U * 25U];
    netLDU2 = new uint8_t[9U * 25U];

    ::memset(netLDU1, 0x00U, 9U * 25U);
    ::memset(netLDU2, 0x00U, 9U * 25U);
}

/* Finalizes a instance of the VoiceCallData class. */

VoiceCallData::~VoiceCallData() 
{
    delete[] mi;
    delete[] VHDR1;
    delete[] VHDR2;
    delete[] netLDU1;
    delete[] netLDU2;
}

/* Reset call data to defaults. */

void VoiceCallData::resetCallData() 
{
    srcId = 0U;
    dstId = 0U;
    lco = 0U;
    mfId = MFG_STANDARD;
    serviceOptions = 0U;
    lsd1 = 0U;
    lsd2 = 0U;

    ::memset(mi, 0x00U, MI_LENGTH_BYTES);

    algoId = ALGO_UNENCRYPT;
    kId = 0U;

    ::memset(VHDR1, 0x00U, MotVoiceHeader1::HCW_LENGTH);
    ::memset(VHDR2, 0x00U, MotVoiceHeader2::HCW_LENGTH);

    ::memset(netLDU1, 0x00U, 9U * 25U);
    ::memset(netLDU2, 0x00U, 9U * 25U);

    n = 0U;
    seqNo = 0U;
    streamId = 0U;
}

/* Generate a new stream ID for a call. */

void VoiceCallData::newStreamId() 
{
    std::uniform_int_distribution<uint32_t> dist(DVM_RAND_MIN, DVM_RAND_MAX);
    streamId = dist(random);
}