// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file BCH.h
 * @ingroup edac
 * @file BCH.cpp
 * @ingroup edac
 */
#if !defined(__BCH_H__)
#define __BCH_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements Bose/Chaudhuri/Hocquenghem codes for protecting P25 NID
     *  data.
     * @ingroup edac
     */
    class DVM_COMMON_API BCH {
    public:
        /**
         * @brief Initializes a new instance of the BCH class.
         */
        BCH();
        /**
         * @brief Finalizes a instance of the BCH class.
         */
        ~BCH();

        /**
         * @brief Encodes input data with BCH(63,16,23).
         * @param[in,out] data Eight-byte buffer. The first 16 bits are
         *  encoded into the following 47 bits; bit 64 is not modified.
         */
        void encode(uint8_t* data);

        /**
         * @brief Decodes and corrects a BCH(63,16,23).
         * @param[in,out] data Eight-byte buffer. Only the first 63 bits
         *  participate in BCH decoding; bit 64 is not modified.
         * @param[out] errors Number of corrected bits in the BCH codeword.
         * @returns True if the codeword was valid or corrected, otherwise false.
         */
        bool decode(uint8_t* data, uint32_t& errors);

    private:
        /**
         * @brief Multiplies two elements of GF(2^6).
         * @param[in] a Left field element.
         * @param[in] b Right field element.
         * @returns Product in GF(2^6).
         */
        static uint8_t multiply(uint8_t a, uint8_t b);
        /**
         * @brief Raises the primitive field element to an integer exponent.
         * @param[in] exponent Exponent, reduced modulo 63.
         * @returns Field element corresponding to alpha raised to the exponent.
         */
        static uint8_t alphaPower(int exponent);
        /**
         * @brief Computes the multiplicative inverse of a field element.
         * @param[in] value Nonzero element of GF(2^6).
         * @returns Multiplicative inverse of value.
         */
        static uint8_t inverse(uint8_t value);

        /**
         * @brief Compute redundancy bb[], the coefficients of b(x). The redundancy polynomial b(x) is the 
         *  remainder after dividing x^(length-k)*data(x) by the generator polynomial g(x).
         * @param data 
         * @param bb 
         */
        void encode(const int* data, int* bb);
    };
} // namespace edac

#endif // __BCH_H__
