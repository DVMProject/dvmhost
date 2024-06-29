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
 * @file BPTC19696.h
 * @ingroup edac
 * @file BPTC19696.cpp
 * @ingroup edac
 */
#if !defined(__BPTC19696_H__)
#define __BPTC19696_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements Block Product Turbo Code (196,96) FEC.
     * @ingroup edac
     */
    class HOST_SW_API BPTC19696 {
    public:
        /**
         * @brief Initializes a new instance of the BPTC19696 class.
         */
        BPTC19696();
        /**
         * @brief Finalizes a instance of the BPTC19696 class.
         */
        ~BPTC19696();

        /**
         * @brief Decode BPTC (196,96) FEC.
         * @param[in] in Input data to decode.
         * @param[out] out Decoded data.
         */
        void decode(const uint8_t* in, uint8_t* out);
        /**
         * @brief Encode BPTC (196,96) FEC.
         * @param[in] in Input data to encode.
         * @param[out] out Encoded data.
         */
        void encode(const uint8_t* in, uint8_t* out);

    private:
        bool* m_rawData;
        bool* m_deInterData;

        /**
         * @brief 
         * @param in
         */
        void decodeExtractBinary(const uint8_t* in);
        /**
         * @brief 
         */
        void decodeErrorCheck();
        /**
         * @brief 
         */
        void decodeDeInterleave();
        /**
         * @brief 
         * @param data 
         */
        void decodeExtractData(uint8_t* data) const;

        /**
         * @brief 
         */
        void encodeExtractData(const uint8_t* in) const;
        /**
         * @brief 
         */
        void encodeInterleave();
        /**
         * @brief 
         */
        void encodeErrorCheck();
        /**
         * @brief 
         * @param data 
         */
        void encodeExtractBinary(uint8_t* data);
    };
} // namespace edac

#endif // __BPTC19696_H__
