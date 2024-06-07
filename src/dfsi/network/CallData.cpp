// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI peer application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*
*   Borrowed from work by Bryan Biedenkapp, N2PLL
*
*/

#include "CallData.h"

using namespace network;
using namespace modem;
using namespace p25;
using namespace dfsi;

VoiceCallData::VoiceCallData() :
    srcId(0U),
    dstId(0U),
    lco(0U),
    mfId(P25_MFG_STANDARD),
    serviceOptions(0U),
    lsd1(0U),
    lsd2(0U),
    mi(),
    algoId(P25_ALGO_UNENCRYPT),
    kId(0U),
    VHDR1(),
    VHDR2(),
    netLDU1(),
    netLDU2(),
    seqNo(0U),
    n(0U),
    streamId(0U)
{
    mi = new uint8_t[P25_MI_LENGTH_BYTES];
    VHDR1 = new uint8_t[MotVoiceHeader1::HCW_LENGTH];
    VHDR2 = new uint8_t[MotVoiceHeader2::HCW_LENGTH];
    netLDU1 = new uint8_t[9U * 25U];
    netLDU2 = new uint8_t[9U * 25U];

    ::memset(netLDU1, 0x00U, 9U * 25U);
    ::memset(netLDU2, 0x00U, 9U * 25U);
}

VoiceCallData::~VoiceCallData() {
    delete[] mi;
    delete[] VHDR1;
    delete[] VHDR2;
    delete[] netLDU1;
    delete[] netLDU2;
}

void VoiceCallData::resetCallData() {
    srcId = 0U;
    dstId = 0U;
    lco = 0U;
    mfId = P25_MFG_STANDARD;
    serviceOptions = 0U;
    lsd1 = 0U;
    lsd2 = 0U;

    ::memset(mi, 0x00U, P25_MI_LENGTH_BYTES);

    algoId = P25_ALGO_UNENCRYPT;
    kId = 0U;

    ::memset(VHDR1, 0x00U, MotVoiceHeader1::HCW_LENGTH);
    ::memset(VHDR2, 0x00U, MotVoiceHeader2::HCW_LENGTH);

    ::memset(netLDU1, 0x00U, 9U * 25U);
    ::memset(netLDU2, 0x00U, 9U * 25U);

    n = 0U;
    seqNo = 0U;
    streamId = 0U;
}

void VoiceCallData::newStreamId() {
    std::uniform_int_distribution<uint32_t> dist(DVM_RAND_MIN, DVM_RAND_MAX);
    streamId = dist(random);
}