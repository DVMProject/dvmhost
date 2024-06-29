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
 * @file QR1676.h
 * @ingroup edac
 * @file QR1676.cpp
 * @ingroup edac
 */
#if !defined(__QR1676_H__)
#define __QR1676_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements Quadratic residue (16,7,6) forward error correction.
     * @ingroup edac
     */
    class HOST_SW_API QR1676 {
    public:
        /**
         * @brief Decode QR (16,7,6) FEC.
         * @param data QR (16,7,6) FEC encoded data to decode.
         * @returns uint8_t Number of errors detected.
         */
        static uint8_t decode(const uint8_t* data);
        /**
         * @brief Encode QR (16,7,6) FEC.
         * @param data Raw data to encode with QR (16,7,6) FEC.
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
        static uint32_t getSyndrome1576(uint32_t pattern);
    };
} // namespace edac

#endif // __QR1676_H__
