// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - TSBK/CSBK Decoder
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file Decode.h
 * @ingroup tsbkdecode
 * @file Decode.cpp
 * @ingroup tsbkdecode
 */
#if !defined(__TSBKDECODE_DECODE_H__)
#define __TSBKDECODE_DECODE_H__

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/**
 * @brief Decodes a raw block or frame and prints its fields.
 * @param bytes Raw block or frame bytes.
 * @param csbk Flag indicating a DMR CSBK rather than a P25 TSBK.
 * @param frame Flag indicating a complete frame rather than a raw block.
 * @param isp Flag indicating inbound P25 subscriber signalling.
 * @returns bool True if decoded successfully, otherwise false for invalid input,
 * unknown opcodes, or unimplemented decoders. CRC failures warn and decoding continues.
 */
bool decodeBlock(std::vector<uint8_t> bytes, bool csbk, bool frame, bool isp);

#endif // __TSBKDECODE_DECODE_H__
