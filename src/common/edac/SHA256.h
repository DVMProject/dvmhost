// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2005,2006,2008,2009 Free Software Foundation, Inc.
 *  Copyright (C) 2011,2015,2016 Jonathan Naylor, G4KLX
 *
 */
/**
 * @file SHA256.h
 * @ingroup edac
 * @file SHA256.cpp
 * @ingroup edac
 */
#if !defined(__SHA256_H__)
#define __SHA256_H__

#include "common/Defines.h"

#include <cstdint>

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    enum {
        SHA256_DIGEST_SIZE = 256 / 8
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements SHA-256 hashing.
     * @ingroup edac
     */
    class HOST_SW_API SHA256 {
    public:
        /**
         * @brief Initializes a new instance of the SHA256 class.'
         * 
         * Takes a pointer to a 256 bit block of data (eight 32 bit ints) and
         * initializes it to the start constants of the SHA256 algorithm. This
         * must be called before using hash in the call to sha256_hash
         */
        SHA256();
        /**
         * @brief Finalizes a instance of the SHA256 class.
         */
        ~SHA256();

        /**
         * @brief Starting with the result of former calls of this function (or the initialization
         *  function update the context for the next LEN bytes starting at BUFFER. It is
         *  necessary that LEN is a multiple of 64!!!
         * 
         * Process LEN bytes of BUFFER, accumulating context into CTX.
         * It is assumed that LEN % 64 == 0.
         * Most of this code comes from GnuPG's cipher/sha1.c.
         * 
         * @param[in] buffer Buffer to process.
         * @param len Length of buffer.
         */
        void processBlock(const uint8_t* buffer, uint32_t len);

        /**
         * @brief Starting with the result of former calls of this function (or the initialization
         *  function update the context for the next LEN bytes starting at BUFFER. It is NOT
         *  required that LEN is a multiple of 64.
         * @param[in] buffer Buffer to process.
         * @param len Length of buffer.
         */
        void processBytes(const uint8_t* buffer, uint32_t len);

        /**
         * @brief Process the remaining bytes in the buffer and put result from context
         *  in first 32 bytes following buffer. The result is always in little
         *  endian byte order, so that a byte - wise output yields to the wanted
         *  ASCII representation of the message digest.
         * @param buffer Buffer to process.
         */
        uint8_t* finish(uint8_t* buffer);

        /**
         * @brief Put result from context in first 32 bytes following buffer. The result is
         *  always in little endian byte order, so that a byte - wise output yields
         *  to the wanted ASCII representation of the message digest.
         * @param buffer Buffer to process.
         */
        uint8_t* read(uint8_t* buffer);

        /**
         * @brief Compute SHA256 message digest for the length bytes beginning at buffer. The
         *  result is always in little endian byte order, so that a byte-wise
         *  output yields to the wanted ASCII representation of the message
         *  digest.
         * @param[in] buffer Buffer to process
         * @param len Length of buffer.
         * @param[out] resblock Resulting SHA256 hash.
         */
        uint8_t* buffer(const uint8_t* buffer, uint32_t len, uint8_t* resblock);

    private:
        uint32_t* m_state;
        uint32_t* m_total;
        uint32_t m_buflen;
        uint32_t* m_buffer;

        /**
         * @brief Initialize SHA256 machine states.
         */
        void init();
        /**
         * @brief Process the remaining bytes in the internal buffer and the usual
         *  prolog according to the standard and write the result to the buffer.
         */
        void conclude();
    };
} // namespace edac

#endif // __SHA256_H__
