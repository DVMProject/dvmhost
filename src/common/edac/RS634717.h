// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017,2023,2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file RS634717.h
 * @ingroup edac
 * @file RS634717.cpp
 * @ingroup edac
 */
#if !defined(__RS634717_H__)
#define __RS634717_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements Reed-Solomon (63,47,17). Which is also used to implement
     *  Reed-Solomon (24,12,13), (24,16,9), (36,20,17), (52,30,23), (46,26,21),
     *  (45,26,20). (44,16,29) forward error correction.
     * @ingroup edac
     */
    class HOST_SW_API RS634717 {
    public:
        /**
         * @brief Initializes a new instance of the RS634717 class.
         */
        RS634717();
        /**
         * @brief Finalizes a instance of the RS634717 class.
         */
        ~RS634717();

        /** Project 25 Phase I Reed-Solomon (TIA-102.BAAA-B Section 4.9) */

        /**
         * @brief Decode RS (24,12,13) FEC.
         * @param data Reed-Solomon FEC encoded data to decode.
         * @returns bool True, if data was decoded, otherwise false.
         */
        bool decode241213(uint8_t* data);
        /**
         * @brief Encode RS (24,12,13) FEC.
         * @param data Raw data to encode with Reed-Solomon FEC.
         */
        void encode241213(uint8_t* data);

        /**
         * @brief Decode RS (24,16,9) FEC.
         * @param data Reed-Solomon FEC encoded data to decode.
         * @returns bool True, if data was decoded, otherwise false.
         */
        bool decode24169(uint8_t* data);
        /**
         * @brief Encode RS (24,16,9) FEC.
         * @param data Raw data to encode with Reed-Solomon FEC.
         */
        void encode24169(uint8_t* data);

        /**
         * @brief Decode RS (36,20,17) FEC.
         * @param data Reed-Solomon FEC encoded data to decode.
         * @returns bool True, if data was decoded, otherwise false.
         */
        bool decode362017(uint8_t* data);
        /**
         * @brief Encode RS (36,20,17) FEC.
         * @param data Raw data to encode with Reed-Solomon FEC.
         */
        void encode362017(uint8_t* data);

        /** Project 25 Phase II Reed-Solomon (TIA-102.BBAC-A Section 5.6) */

        /**
         * @brief Decode RS (52,30,23) FEC.
         * @param data Reed-Solomon FEC encoded data to decode.
         * @returns bool True, if data was decoded, otherwise false.
         */
        bool decode523023(uint8_t* data);
        /**
         * @brief Encode RS (52,30,23) FEC.
         * @param data Raw data to encode with Reed-Solomon FEC.
         */
        void encode523023(uint8_t* data);

        /**
         * @brief Decode RS (46,26,21) FEC.
         * @param data Reed-Solomon FEC encoded data to decode.
         * @returns bool True, if data was decoded, otherwise false.
         */
        bool decode462621(uint8_t* data);
        /**
         * @brief Encode RS (46,26,21) FEC.
         * @param data Raw data to encode with Reed-Solomon FEC.
         */
        void encode462621(uint8_t* data);

        /**
         * @brief Decode RS (45,26,20) FEC.
         * @param data Reed-Solomon FEC encoded data to decode.
         * @returns bool True, if data was decoded, otherwise false.
         */
        bool decode452620(uint8_t* data);
        /**
         * @brief Encode RS (45,26,20) FEC.
         * @param data Raw data to encode with Reed-Solomon FEC.
         */
        void encode452620(uint8_t* data);

        /**
         * @brief Decode RS (44,16,29) FEC.
         * @param data Reed-Solomon FEC encoded data to decode.
         * @returns bool True, if data was decoded, otherwise false.
         */
        bool decode441629(uint8_t* data);
        /**
         * @brief Encode RS (44,16,29) FEC.
         * @param data Raw data to encode with Reed-Solomon FEC.
         */
        void encode441629(uint8_t* data);

    private:
        /**
         * @brief GF(2 ^ 6) multiply (for Reed-Solomon encoder).
         * @param a 
         * @param b 
         * @returns uint8_t 
         */
        uint8_t gf6Mult(uint8_t a, uint8_t b) const;
    };
} // namespace edac

#endif // __RS634717_H__
