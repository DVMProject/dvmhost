// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015 Jonathan Naylor, G4KLX
 *
 */
/**
 * @file Golay2087.h
 * @ingroup edac
 * @file Golay2087.cpp
 * @ingroup edac
 */
#if !defined(__GOLAY2087_H__)
#define __GOLAY2087_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements Golay (20,8,7) forward error correction.
     * @ingroup edac
     */
    class HOST_SW_API Golay2087 {
    public:
        /**
         * @brief Decode Golay (20,8,7) FEC.
         * @param bytes Golay FEC encoded data byte array.
         * @returns uint8_t 
         */
        static uint8_t decode(const uint8_t* data);
        /**
         * @brief Encode Golay (20,8,7) FEC.
         * @param data Data to encode with Golay FEC.
         */
        static void encode(uint8_t* data);

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
        static uint32_t getSyndrome1987(uint32_t pattern);
    };
} // namespace edac

#endif // __GOLAY2087_H__
