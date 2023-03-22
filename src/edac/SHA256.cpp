/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2005, 2006, 2008 Free Software Foundation, Inc.
*   Copyright (C) 2011,2015 by Jonathan Naylor G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "edac/SHA256.h"

using namespace edac;

#include <cstdio>
#include <cstring>
#include <cassert>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

#ifdef WORDS_BIGENDIAN
# define SWAP(n) (n)
#else
# define SWAP(n) \
    (((n) << 24) | (((n) & 0xff00) << 8) | (((n) >> 8) & 0xff00) | ((n) >> 24))
#endif

#define BLOCKSIZE 4096
#if BLOCKSIZE % 64 != 0
# error "invalid BLOCKSIZE"
#endif

// Round functions.
#define F2(A,B,C) ( ( A & B ) | ( C & ( A | B ) ) )
#define F1(E,F,G) ( G ^ ( E & ( F ^ G ) ) )

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

/* 
 * This array contains the bytes used to pad the buffer to the next 64-byte
 * boundary.  
 */
static const uint8_t fillbuf[64] = { 0x80, 0 /* , 0, 0, ...  */ };

/*
 * SHA256 round constants
 */
#define K(I) roundConstants[I]
static const uint32_t roundConstants[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
    0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
    0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
    0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
    0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL,
};

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/*
 * Copy the value from v into the memory location pointed to by *cp,
 * If your architecture allows unaligned access this is equivalent to
 * (uint32_t *) cp = v
 */
static inline void set_uint32(uint8_t* cp, uint32_t v)
{
    assert(cp != nullptr);
    ::memcpy(cp, &v, sizeof v);
}

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the SHA256 class.
/// </summary>
/// <remarks>
/// Takes a pointer to a 256 bit block of data (eight 32 bit ints) and
/// intializes it to the start constants of the SHA256 algorithm.  This
/// must be called before using hash in the call to sha256_hash
/// </remarks>
SHA256::SHA256() :
    m_state(nullptr),
    m_total(nullptr),
    m_buflen(0U),
    m_buffer(nullptr)
{
    m_state = new uint32_t[8U];
    m_total = new uint32_t[2U];
    m_buffer = new uint32_t[32U];

    init();
}

/// <summary>
/// Finalizes a instance of the SHA256 class.
/// </summary>
SHA256::~SHA256()
{
    delete[] m_state;
    delete[] m_total;
    delete[] m_buffer;
}

/// <summary>
/// Starting with the result of former calls of this function (or the initialization
/// function update the context for the next LEN bytes starting at BUFFER. It is
/// necessary that LEN is a multiple of 64!!!
/// </summary>
/// <remarks>
/// Process LEN bytes of BUFFER, accumulating context into CTX.
/// It is assumed that LEN % 64 == 0.
/// Most of this code comes from GnuPG's cipher/sha1.c.
/// </remarks>
/// <param name="buffer"></param>
/// <param name="len"></param>
void SHA256::processBlock(const uint8_t* buffer, uint32_t len)
{
    assert(buffer != nullptr);

    const uint32_t *words = (uint32_t *)buffer;
    uint32_t nwords = len / sizeof(uint32_t);
    const uint32_t *endp = words + nwords;
    uint32_t x[16];
    uint32_t a = m_state[0];
    uint32_t b = m_state[1];
    uint32_t c = m_state[2];
    uint32_t d = m_state[3];
    uint32_t e = m_state[4];
    uint32_t f = m_state[5];
    uint32_t g = m_state[6];
    uint32_t h = m_state[7];

    // First increment the byte count.  FIPS PUB 180-2 specifies the possible
    // length of the file up to 2^64 bits.  Here we only compute the
    // number of bytes.  Do a double word increment.
    m_total[0] += len;
    if (m_total[0] < len)
        ++m_total[1];

#define rol(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define S0(x) (rol(x, 25) ^ rol(x, 14) ^ (x >> 3))
#define S1(x) (rol(x, 15) ^ rol(x, 13) ^ (x >> 10))
#define SS0(x) (rol(x, 30) ^ rol(x, 19) ^ rol(x, 10))
#define SS1(x) (rol(x, 26) ^ rol(x, 21) ^ rol(x, 7))

#define M(I) (tm = S1(x[(I - 2) & 0x0f]) + x[(I - 7) & 0x0f] + S0(x[(I - 15) & 0x0f]) + x[I & 0x0f], x[I & 0x0f] = tm)

#define R(A,B,C,D,E,F,G,H,K,M)  do { t0 = SS0(A) + F2(A,B,C);            \
                                 t1 = H + SS1(E) + F1(E,F,G) + K + M;    \
                                 D += t1;  H = t0 + t1;                  \
                                } while(0)

    while (words < endp) {
        uint32_t tm;
        uint32_t t0, t1;
        // FIXME: see sha1.c for a better implementation.
        for (uint32_t t = 0U; t < 16U; t++) {
            x[t] = SWAP(*words);
            words++;
        }

        R(a, b, c, d, e, f, g, h, K(0), x[0]);
        R(h, a, b, c, d, e, f, g, K(1), x[1]);
        R(g, h, a, b, c, d, e, f, K(2), x[2]);
        R(f, g, h, a, b, c, d, e, K(3), x[3]);
        R(e, f, g, h, a, b, c, d, K(4), x[4]);
        R(d, e, f, g, h, a, b, c, K(5), x[5]);
        R(c, d, e, f, g, h, a, b, K(6), x[6]);
        R(b, c, d, e, f, g, h, a, K(7), x[7]);
        R(a, b, c, d, e, f, g, h, K(8), x[8]);
        R(h, a, b, c, d, e, f, g, K(9), x[9]);
        R(g, h, a, b, c, d, e, f, K(10), x[10]);
        R(f, g, h, a, b, c, d, e, K(11), x[11]);
        R(e, f, g, h, a, b, c, d, K(12), x[12]);
        R(d, e, f, g, h, a, b, c, K(13), x[13]);
        R(c, d, e, f, g, h, a, b, K(14), x[14]);
        R(b, c, d, e, f, g, h, a, K(15), x[15]);
        R(a, b, c, d, e, f, g, h, K(16), M(16));
        R(h, a, b, c, d, e, f, g, K(17), M(17));
        R(g, h, a, b, c, d, e, f, K(18), M(18));
        R(f, g, h, a, b, c, d, e, K(19), M(19));
        R(e, f, g, h, a, b, c, d, K(20), M(20));
        R(d, e, f, g, h, a, b, c, K(21), M(21));
        R(c, d, e, f, g, h, a, b, K(22), M(22));
        R(b, c, d, e, f, g, h, a, K(23), M(23));
        R(a, b, c, d, e, f, g, h, K(24), M(24));
        R(h, a, b, c, d, e, f, g, K(25), M(25));
        R(g, h, a, b, c, d, e, f, K(26), M(26));
        R(f, g, h, a, b, c, d, e, K(27), M(27));
        R(e, f, g, h, a, b, c, d, K(28), M(28));
        R(d, e, f, g, h, a, b, c, K(29), M(29));
        R(c, d, e, f, g, h, a, b, K(30), M(30));
        R(b, c, d, e, f, g, h, a, K(31), M(31));
        R(a, b, c, d, e, f, g, h, K(32), M(32));
        R(h, a, b, c, d, e, f, g, K(33), M(33));
        R(g, h, a, b, c, d, e, f, K(34), M(34));
        R(f, g, h, a, b, c, d, e, K(35), M(35));
        R(e, f, g, h, a, b, c, d, K(36), M(36));
        R(d, e, f, g, h, a, b, c, K(37), M(37));
        R(c, d, e, f, g, h, a, b, K(38), M(38));
        R(b, c, d, e, f, g, h, a, K(39), M(39));
        R(a, b, c, d, e, f, g, h, K(40), M(40));
        R(h, a, b, c, d, e, f, g, K(41), M(41));
        R(g, h, a, b, c, d, e, f, K(42), M(42));
        R(f, g, h, a, b, c, d, e, K(43), M(43));
        R(e, f, g, h, a, b, c, d, K(44), M(44));
        R(d, e, f, g, h, a, b, c, K(45), M(45));
        R(c, d, e, f, g, h, a, b, K(46), M(46));
        R(b, c, d, e, f, g, h, a, K(47), M(47));
        R(a, b, c, d, e, f, g, h, K(48), M(48));
        R(h, a, b, c, d, e, f, g, K(49), M(49));
        R(g, h, a, b, c, d, e, f, K(50), M(50));
        R(f, g, h, a, b, c, d, e, K(51), M(51));
        R(e, f, g, h, a, b, c, d, K(52), M(52));
        R(d, e, f, g, h, a, b, c, K(53), M(53));
        R(c, d, e, f, g, h, a, b, K(54), M(54));
        R(b, c, d, e, f, g, h, a, K(55), M(55));
        R(a, b, c, d, e, f, g, h, K(56), M(56));
        R(h, a, b, c, d, e, f, g, K(57), M(57));
        R(g, h, a, b, c, d, e, f, K(58), M(58));
        R(f, g, h, a, b, c, d, e, K(59), M(59));
        R(e, f, g, h, a, b, c, d, K(60), M(60));
        R(d, e, f, g, h, a, b, c, K(61), M(61));
        R(c, d, e, f, g, h, a, b, K(62), M(62));
        R(b, c, d, e, f, g, h, a, K(63), M(63));

        a = m_state[0] += a;
        b = m_state[1] += b;
        c = m_state[2] += c;
        d = m_state[3] += d;
        e = m_state[4] += e;
        f = m_state[5] += f;
        g = m_state[6] += g;
        h = m_state[7] += h;
    }
}

/// <summary>
/// Starting with the result of former calls of this function (or the initialization
/// function update the context for the next LEN bytes starting at BUFFER. It is NOT
/// required that LEN is a multiple of 64.
/// </summary>
/// <param name="buffer"></param>
/// <param name="len"></param>
void SHA256::processBytes(const uint8_t* buffer, uint32_t len)
{
    assert(buffer != nullptr);

    // When we already have some bits in our internal buffer concatenate
    // both inputs first.
    if (m_buflen != 0U) {
        uint32_t left_over = m_buflen;
        uint32_t add = 128U - left_over > len ? len : 128U - left_over;

        ::memcpy(&((char*)m_buffer)[left_over], buffer, add);
        m_buflen += add;

        if (m_buflen > 64U) {
            processBlock((uint8_t*)m_buffer, m_buflen & ~63U);

            m_buflen &= 63U;

            // The regions in the following copy operation cannot overlap.
            ::memcpy(m_buffer, &((char*)m_buffer)[(left_over + add) & ~63U], m_buflen);
        }

        buffer += add;
        len -= add;
    }

    // Process available complete blocks.
    if (len >= 64U) {
        processBlock(buffer, len & ~63U);
        buffer += (len & ~63U);
        len &= 63U;
    }

    // Move remaining bytes in internal buffer.
    if (len > 0U) {
        uint32_t left_over = m_buflen;

        ::memcpy(&((char*)m_buffer)[left_over], buffer, len);
        left_over += len;

        if (left_over >= 64U) {
            processBlock((uint8_t*)m_buffer, 64U);
            left_over -= 64U;
            ::memcpy(m_buffer, &m_buffer[16], left_over);
        }

        m_buflen = left_over;
    }
}

/// <summary>
/// Process the remaining bytes in the bufferand put result from context
/// in first 32 bytes following buffer. The result is always in little
/// endian byte order, so that a byte - wise output yields to the wanted
/// ASCII representation of the message digest.
/// </summary>
/// <param name="buffer"></param>
/// <returns></returns>
uint8_t* SHA256::finish(uint8_t* buffer)
{
    assert(buffer != nullptr);

    conclude();

    return read(buffer);
}

/// <summary>
/// Put result from context in first 32 bytes following buffer. The result is
/// always in little endian byte order, so that a byte - wise output yields
/// to the wanted ASCII representation of the message digest.
/// </summary>
/// <param name="buffer"></param>
/// <returns></returns>
uint8_t* SHA256::read(uint8_t* buffer)
{
    assert(buffer != nullptr);

    for (uint32_t i = 0U; i < 8U; i++)
        set_uint32(buffer + i * sizeof(m_state[0]), SWAP(m_state[i]));

    return buffer;
}

/// <summary>
/// Compute SHA256 message digest for the length bytes beginning at buffer. The
/// result is always in little endian byte order, so that a byte-wise
/// output yields to the wanted ASCII representation of the message
/// digest.
/// </summary>
/// <param name="buffer"></param>
/// <param name="len"></param>
/// <param name="resblock"></param>
/// <returns></returns>
uint8_t* SHA256::buffer(const uint8_t* buffer, uint32_t len, uint8_t* resblock)
{
    assert(buffer != nullptr);
    assert(resblock != nullptr);

    // Initialize the computation context.
    init();

    // Process whole buffer but last len % 64 bytes.
    processBytes(buffer, len);

    // Put result in desired memory area.
    return finish(resblock);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
void SHA256::init()
{
    m_state[0] = 0x6a09e667UL;
    m_state[1] = 0xbb67ae85UL;
    m_state[2] = 0x3c6ef372UL;
    m_state[3] = 0xa54ff53aUL;
    m_state[4] = 0x510e527fUL;
    m_state[5] = 0x9b05688cUL;
    m_state[6] = 0x1f83d9abUL;
    m_state[7] = 0x5be0cd19UL;

    m_total[0] = m_total[1] = 0;
    m_buflen = 0;
}

/// <summary>
/// Process the remaining bytes in the internal buffer and the usual
/// prolog according to the standard and write the result to the buffer.
/// </summary>
void SHA256::conclude()
{
    // Take yet unprocessed bytes into account.
    uint32_t bytes = m_buflen;
    uint32_t size = (bytes < 56) ? 64 / 4 : 64 * 2 / 4;

    // Now count remaining bytes.
    m_total[0] += bytes;
    if (m_total[0] < bytes)
        ++m_total[1];

    // Put the 64-bit file length in *bits* at the end of the buffer.
    // Use set_uint32 rather than a simple assignment, to avoid risk of
    // unaligned access.
    set_uint32((uint8_t*)& m_buffer[size - 2], SWAP((m_total[1] << 3) | (m_total[0] >> 29)));
    set_uint32((uint8_t*)& m_buffer[size - 1], SWAP(m_total[0] << 3));

    ::memcpy(&((char*)m_buffer)[bytes], fillbuf, (size - 2) * 4 - bytes);

    // Process last bytes.
    processBlock((uint8_t*)m_buffer, size * 4);
}
