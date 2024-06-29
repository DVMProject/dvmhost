// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2010,2016,2021 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017,2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file Golay24128.h
 * @ingroup edac
 * @file Golay24128.cpp
 * @ingroup edac
 */
#if !defined(__GOLAY24128_H__)
#define __GOLAY24128_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements Golay (23,12,7) and Golay (24,12,8) forward error
     *  correction.
     * @ingroup edac
     */
    class HOST_SW_API Golay24128 {
    public:
        /**
         * @brief Decode Golay (23,12,7) FEC.
         * @param code 
         * @returns uint8_t Number of errors detected.
         */
        static uint32_t decode23127(uint32_t code);
        /**
         * @brief Decode Golay (24,12,8) FEC.
         * @param code 
         * @param out 
         * @returns bool 
         */
        static bool decode24128(uint32_t code, uint32_t& out);
        /**
         * @brief Decode Golay (24,12,8) FEC.
         * @param bytes Golay FEC encoded data byte array.
         * @param out 
         * @returns bool 
         */
        static bool decode24128(uint8_t* bytes, uint32_t& out);
        /**
         * @brief Decode Golay (24,12,8) FEC.
         * @param data Data decoded with Golay FEC.
         * @param raw Raw data to decode.
         * @param msglen Length of data to decode.
         */
        static void decode24128(uint8_t* data, const uint8_t* raw, uint32_t msglen);

        /**
         * @brief Encode Golay (23,12,7) FEC.
         * @param data Data to encode with Golay FEC.
         * @returns uint32_t
         */
        static uint32_t encode23127(uint32_t data);
        /**
         * @brief Encode Golay (24,12,8) FEC.
         * @param data Data to encode with Golay FEC.
         * @returns uint32_t
         */
        static uint32_t encode24128(uint32_t data);
        /**
         * @brief Encode Golay (24,12,8) FEC.
         * @param data Data encoded with Golay FEC.
         * @param raw Raw data to encode.
         * @param msglen Length of data to encode.
         */
        static void encode24128(uint8_t* data, const uint8_t* raw, uint32_t msglen);

    private:
        /**
         * @brief Compute the syndrome corresponding to the given pattern, i.e., the
         * remainder after dividing the pattern (when considering it as the vector
         * representation of a polynomial) by the generator polynomial, GENPOL.
         * In the program this pattern has several meanings: (1) pattern = information
         * bits, when constructing the encoding table; (2) pattern = error pattern,
         * when constructing the decoding table; and (3) pattern = received vector, to
         * obtain its syndrome in decoding.
         */
        static uint32_t getSyndrome23127(uint32_t pattern);
    };
} // namespace edac

#endif // __GOLAY24128_H__
