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
*   Copyright (C) 2016 Jonathan Naylor, G4KLX
*   Copyright (C) 2017,2023 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "edac/RS634717.h"
#include "edac/rs/RS.h"
#include "Log.h"
#include "Utils.h"

using namespace edac;

#include <cassert>
#include <vector>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t ENCODE_MATRIX[12U][24U] = {
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 062, 044, 003, 025, 014, 016, 027, 003, 053, 004, 036, 047 },
    { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 011, 012, 011, 011, 016, 064, 067, 055, 001, 076, 026, 073 },
    { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 003, 001, 005, 075, 014, 006, 020, 044, 066, 006, 070, 066 },
    { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 021, 070, 027, 045, 016, 067, 023, 064, 073, 033, 044, 021 },
    { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 030, 022, 003, 075, 015, 015, 033, 015, 051, 003, 053, 050 },
    { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 001, 041, 027, 056, 076, 064, 021, 053, 004, 025, 001, 012 },
    { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 061, 076, 021, 055, 076, 001, 063, 035, 030, 013, 064, 070 },
    { 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 024, 022, 071, 056, 021, 035, 073, 042, 057, 074, 043, 076 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 072, 042, 005, 020, 043, 047, 033, 056, 001, 016, 013, 076 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 072, 014, 065, 054, 035, 025, 041, 016, 015, 040, 071, 026 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 073, 065, 036, 061, 042, 022, 017, 004, 044, 020, 025, 005 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 071, 005, 055, 003, 071, 034, 060, 011, 074, 002, 041, 050 } };

const uint8_t ENCODE_MATRIX_24169[16U][24U] = {
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 051, 045, 067, 015, 064, 067, 052, 012 },
    { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 057, 025, 063, 073, 071, 022, 040, 015 },
    { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 005, 001, 031, 004, 016, 054, 025, 076 },
    { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 073, 007, 047, 014, 041, 077, 047, 011 },
    { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 075, 015, 051, 051, 017, 067, 017, 057 },
    { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 020, 032, 014, 042, 075, 042, 070, 054 },
    { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 002, 075, 043, 005, 001, 040, 012, 064 },
    { 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 024, 074, 015, 072, 024, 026, 074, 061 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 042, 064, 007, 022, 061, 020, 040, 065 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 032, 032, 055, 041, 057, 066, 021, 077 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 065, 036, 025, 007, 050, 016, 040, 051 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 064, 006, 054, 032, 076, 046, 014, 036 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 062, 063, 074, 070, 005, 027, 037, 046 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 055, 043, 034, 071, 057, 076, 050, 064 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 024, 023, 023, 005, 050, 070, 042, 023 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 067, 075, 045, 060, 057, 024, 006, 026 } };

const uint8_t ENCODE_MATRIX_362017[20U][36U] = {
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 074, 037, 034, 006, 002, 007, 044, 064, 026, 014, 026, 044, 054, 013, 077, 005 },
    { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 004, 017, 050, 024, 011, 005, 030, 057, 033, 003, 002, 002, 015, 016, 025, 026 },
    { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 007, 023, 037, 046, 056, 075, 043, 045, 055, 021, 050, 031, 045, 027, 071, 062 },
    { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 026, 005, 007, 063, 063, 027, 063, 040, 006, 004, 040, 045, 047, 030, 075, 007 },
    { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 023, 073, 073, 041, 072, 034, 021, 051, 067, 016, 031, 074, 011, 021, 012, 021 },
    { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 024, 051, 025, 023, 022, 041, 074, 066, 074, 065, 070, 036, 067, 045, 064, 001 },
    { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 052, 033, 014, 002, 020, 006, 014, 025, 052, 023, 035, 074, 075, 075, 043, 027 },
    { 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 055, 062, 056, 025, 073, 060, 015, 030, 013, 017, 020, 002, 070, 055, 014, 047 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 054, 051, 032, 065, 077, 012, 054, 013, 035, 032, 056, 012, 075, 001, 072, 063 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 074, 041, 030, 041, 043, 022, 051, 006, 064, 033, 003, 047, 027, 012, 055, 047 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 054, 070, 011, 003, 013, 022, 016, 057, 003, 045, 072, 031, 030, 056, 035, 022 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 051, 007, 072, 030, 065, 054, 006, 021, 036, 063, 050, 061, 064, 052, 001, 060 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 001, 065, 032, 070, 013, 044, 073, 024, 012, 052, 021, 055, 012, 035, 014, 072 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 011, 070, 005, 010, 065, 024, 015, 077, 022, 024, 024, 074, 007, 044, 007, 046 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 006, 002, 065, 011, 041, 020, 045, 042, 046, 054, 035, 012, 040, 064, 065, 033 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 034, 031, 001, 015, 044, 064, 016, 024, 052, 016, 006, 062, 020, 013, 055, 057 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 063, 043, 025, 044, 077, 063, 017, 017, 064, 014, 040, 074, 031, 072, 054, 006 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 071, 021, 070, 044, 056, 004, 030, 074, 004, 023, 071, 070, 063, 045, 056, 043 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 002, 001, 053, 074, 002, 014, 052, 074, 012, 057, 024, 063, 015, 042, 052, 033 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 034, 035, 002, 023, 021, 027, 022, 033, 064, 042, 005, 073, 051, 046, 073, 060 } };

/// <summary>Define a reed-solomon codec.</summary>
/// <param name="TYPE">Data type primitive</param>
/// <param name="SYMBOLS">Total number of symbols; must be a power of 2 minus 1, eg 2^8-1 == 255</param>
/// <param name="PAYLOAD">The maximum number of non-parity symbols, eg 253 ==> 2 parity symbols</param>
/// <param name="POLY">A primitive polynomial appropriate to the SYMBOLS size</param>
/// <param name="FCR">The first consecutive root of the Reed-Solomon generator polynomial</param>
/// <param name="PRIM">The primitive root of the generator polynomial</param>
#define __RS(TYPE, SYMBOLS, PAYLOAD, POLY, FCR, PRIM)                           \
            edac::rs::reed_solomon<TYPE,                                        \
            edac::rs::log_<(SYMBOLS) + 1>::value,                               \
            (SYMBOLS) - (PAYLOAD), FCR, PRIM,                                   \
            edac::rs::gfpoly<edac::rs::log_<(SYMBOLS) + 1>::value, POLY>>

/// <summary>Define a 63-symbol reed-solomon codec.</summary>
/// <param name="PAYLOAD">The maximum number of non-parity symbols, eg 253 ==> 2 parity symbols</param>
#define __RS_63(PAYLOAD) __RS(uint8_t, 63, PAYLOAD, 0x43, 1, 1)

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

class RS6347 : public __RS_63(47) {
public:
    RS6347() : __RS_63(47)() { /* stub */ }
};
RS6347 rs634717;    // 16 bit / 8 bit corrections max / 5 bytes total

class RS6351 : public __RS_63(51) {
public:
    RS6351() : __RS_63(51)() { /* stub */ }
};
RS6351 rs241213;    // 12 bit / 6 bit corrections max / 3 bytes total

class RS6355 : public __RS_63(55) {
public:
    RS6355() : __RS_63(55)() { /* stub */ }
};
RS6355 rs24169;     // 8 bit / 4 bit corrections max / 2 bytes total

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the RS634717 class.
/// </summary>
RS634717::RS634717() = default;

/// <summary>
/// Finalizes a instance of the RS634717 class.
/// </summary>
RS634717::~RS634717() = default;

/// <summary>
/// Decode RS (24,12,13) FEC.
/// </summary>
/// <param name="data">Reed-Solomon FEC encoded data to decode.</param>
/// <returns>True, if data was decoded, otherwise false.</returns>
bool RS634717::decode241213(uint8_t* data)
{
    assert(data != nullptr);

    std::vector<uint8_t> codeword(63, 0);

    uint32_t offset = 0U;
    for (uint32_t i = 0U; i < 24U; i++, offset += 6)
        codeword[39 + i] = Utils::bin2Hex(data, offset);

    int ec = rs241213.decode(codeword);
#if DEBUG_RS
    LogDebug(LOG_HOST, "RS634717::decode241213(), errors = %d", ec);
#endif
    offset = 0U;
    for (uint32_t i = 0U; i < 12U; i++, offset += 6)
        Utils::hex2Bin(codeword[39 + i], data, offset);

    if ((ec == -1) || (ec >= 6)) {
        return false;
    }

    return true;
}

/// <summary>
/// Encode RS (24,12,13) FEC.
/// </summary>
/// <param name="data">Raw data to encode with Reed-Solomon FEC.</param>
void RS634717::encode241213(uint8_t* data)
{
    assert(data != nullptr);

    uint8_t codeword[24U];

    for (uint32_t i = 0U; i < 24U; i++) {
        codeword[i] = 0x00U;

        uint32_t offset = 0U;
        for (uint32_t j = 0U; j < 12U; j++, offset += 6U) {
            uint8_t hexbit = Utils::bin2Hex(data, offset);
            codeword[i] ^= gf6Mult(hexbit, ENCODE_MATRIX[j][i]);
        }
    }

    uint32_t offset = 0U;
    for (uint32_t i = 0U; i < 24U; i++, offset += 6U)
        Utils::hex2Bin(codeword[i], data, offset);
}

/// <summary>
/// Decode RS (24,16,9) FEC.
/// </summary>
/// <param name="data">Reed-Solomon FEC encoded data to decode.</param>
/// <returns>True, if data was decoded, otherwise false.</returns>
bool RS634717::decode24169(uint8_t* data)
{
    assert(data != nullptr);

    std::vector<uint8_t> codeword(63, 0);

    uint32_t offset = 0U;
    for (uint32_t i = 0U; i < 24U; i++, offset += 6)
        codeword[39 + i] = Utils::bin2Hex(data, offset);

    int ec = rs24169.decode(codeword);
#if DEBUG_RS
    LogDebug(LOG_HOST, "RS634717::decode24169(), errors = %d\n", ec);
#endif
    offset = 0U;
    for (uint32_t i = 0U; i < 16U; i++, offset += 6)
        Utils::hex2Bin(codeword[39 + i], data, offset);

    if ((ec == -1) || (ec >= 4)) {
        return false;
    }

    return true;
}

/// <summary>
/// Encode RS (24,16,9) FEC.
/// </summary>
/// <param name="data">Raw data to encode with Reed-Solomon FEC.</param>
void RS634717::encode24169(uint8_t* data)
{
    assert(data != nullptr);

    uint8_t codeword[24U];

    for (uint32_t i = 0U; i < 24U; i++) {
        codeword[i] = 0x00U;

        uint32_t offset = 0U;
        for (uint32_t j = 0U; j < 16U; j++, offset += 6U) {
            uint8_t hexbit = Utils::bin2Hex(data, offset);
            codeword[i] ^= gf6Mult(hexbit, ENCODE_MATRIX_24169[j][i]);
        }
    }

    uint32_t offset = 0U;
    for (uint32_t i = 0U; i < 24U; i++, offset += 6U)
        Utils::hex2Bin(codeword[i], data, offset);
}

/// <summary>
/// Decode RS (36,20,17) FEC.
/// </summary>
/// <param name="data">Reed-Solomon FEC encoded data to decode.</param>
/// <returns>True, if data was decoded, otherwise false.</returns>
bool RS634717::decode362017(uint8_t* data)
{
    assert(data != nullptr);

    std::vector<uint8_t> codeword(63, 0);

    uint32_t offset = 0U;
    for (uint32_t i = 0U; i < 36U; i++, offset += 6)
        codeword[27 + i] = Utils::bin2Hex(data, offset);

    int ec = rs634717.decode(codeword);
#if DEBUG_RS
    LogDebug(LOG_HOST, "RS634717::decode362017(), errors = %d\n", ec);
#endif
    offset = 0U;
    for (uint32_t i = 0U; i < 20U; i++, offset += 6)
        Utils::hex2Bin(codeword[27 + i], data, offset);

    if ((ec == -1) || (ec >= 8)) {
        return false;
    }

    return true;
}

/// <summary>
/// Encode RS (36,20,17) FEC.
/// </summary>
/// <param name="data">Raw data to encode with Reed-Solomon FEC.</param>
void RS634717::encode362017(uint8_t* data)
{
    assert(data != nullptr);

    uint8_t codeword[36U];

    for (uint32_t i = 0U; i < 36U; i++) {
        codeword[i] = 0x00U;

        uint32_t offset = 0U;
        for (uint32_t j = 0U; j < 20U; j++, offset += 6U) {
            uint8_t hexbit = Utils::bin2Hex(data, offset);
            codeword[i] ^= gf6Mult(hexbit, ENCODE_MATRIX_362017[j][i]);
        }
    }

    uint32_t offset = 0U;
    for (uint32_t i = 0U; i < 36U; i++, offset += 6U)
        Utils::hex2Bin(codeword[i], data, offset);
}

// ---------------------------------------------------------------------------
//  Private Static Class Members
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <remarks>GF(2 ^ 6) multiply (for Reed-Solomon encoder).</remarks>
/// <param name="a"></param>
/// <param name="b"></param>
/// <returns></returns>
uint8_t RS634717::gf6Mult(uint8_t a, uint8_t b) const
{
    uint8_t p = 0x00U;

    for (uint32_t i = 0U; i < 6U; i++) {
        if ((b & 0x01U) == 0x01U)
            p ^= a;

        a <<= 1;

        if ((a & 0x40U) == 0x40U)
            a ^= 0x43U;            // primitive polynomial : x ^ 6 + x + 1

        b >>= 1;
    }

    return p;
}
