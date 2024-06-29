// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *
 */
/**
 * @file Hamming.h
 * @ingroup edac
 * @file Hamming.cpp
 * @ingroup edac
 */
#if !defined(__HAMMING_H__)
#define __HAMMING_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements Hamming (15,11,3), (13,9,3), (10,6,3), (16,11,4) and
     *  (17, 12, 3) forward error correction.
     * @ingroup edac
     */
    class HOST_SW_API Hamming {
    public:
        /**
         * @brief Decode Hamming (15,11,3).
         * @param d Boolean bit array.
         * @returns bool True, if bit errors are detected, otherwise false.
         */
        static bool decode15113_1(bool* d);
        /**
         * @brief Encode Hamming (15,11,3).
         * @param d Boolean bit array.
         */
        static void encode15113_1(bool* d);

        /**
         * @brief Decode Hamming (15,11,3).
         * @param d Boolean bit array.
         * @returns bool True, if bit errors are detected, otherwise false.
         */
        static bool decode15113_2(bool* d);
        /**
         * @brief Encode Hamming (15,11,3).
         * @param d Boolean bit array.
         */
        static void encode15113_2(bool* d);

        /**
         * @brief Decode Hamming (13,9,3).
         * @param d Boolean bit array.
         * @returns bool True, if bit errors are detected, otherwise false.
         */
        static bool decode1393(bool* d);
        /**
         * @brief Encode Hamming (13,9,3).
         * @param d Boolean bit array.
         */
        static void encode1393(bool* d);

        /**
         * @brief Decode Hamming (10,6,3).
         * @param d Boolean bit array.
         * @returns bool True, if bit errors are detected, otherwise false.
         */
        static bool decode1063(bool* d);
        /**
         * @brief Encode Hamming (10,6,3).
         * @param d Boolean bit array.
         */
        static void encode1063(bool* d);

        /**
         * @brief Decode Hamming (16,11,4).
         * @param d Boolean bit array.
         * @returns bool True, if bit errors are detected, otherwise false.
         */
        static bool decode16114(bool* d);
        /**
         * @brief Encode Hamming (16,11,4).
         * @param d Boolean bit array.
         */
        static void encode16114(bool* d);

        /**
         * @brief Decode Hamming (17,12,3).
         * @param d Boolean bit array.
         * @returns bool True, if bit errors are detected, otherwise false.
         */
        static bool decode17123(bool* d);
        /**
         * @brief Encode Hamming (17,12,3).
         * @param d Boolean bit array.
         */
        static void encode17123(bool* d);
    };
} // namespace edac

#endif // __HAMMING_H__
