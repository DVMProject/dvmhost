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
 * @file RS129.h
 * @ingroup edac
 * @file RS129.cpp
 * @ingroup edac
 */
#if !defined(__RS129_H__)
#define __RS129_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements Reed-Solomon (12,9) forward error correction.
     * @ingroup edac
     */
    class HOST_SW_API RS129 {
    public:
        /**
         * @brief Check RS (12,9) FEC.
         * @param[in] in Buffer to check with RS (12,9).
         * @returns bool True, if parity is valid, otherwise false.
         */
        static bool check(const uint8_t* in);

        /**
         * @brief Encode RS (12,9) FEC.
         * Simulate a LFSR with generator polynomial for n byte RS code.
         * Pass in a pointer to the data array, and amount of data.
         * 
         * The parity bytes are deposited into parity.
         * @param[in] msg Buffer to protect.
         * @param nbytes Length of buffer.
         * @param[out] parity RS (12,9) parity.
         */
        static void encode(const uint8_t* msg, uint32_t nbytes, uint8_t* parity);

    private:
        /**
         * @brief 
         */
        static uint8_t gmult(uint8_t a, uint8_t b);
    };
} // namespace edac

#endif // __RS129_H__
