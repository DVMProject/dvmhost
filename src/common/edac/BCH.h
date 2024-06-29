// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
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
    class HOST_SW_API BCH {
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
         * @brief Encodes input data with BCH.
         * @param data Data to encode with BCH.
         */
        void encode(uint8_t* data);

    private:
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
