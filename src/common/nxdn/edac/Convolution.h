// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2018,2021 Jonathan Naylor, G4KLX
 *  Copyright (C) 2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup nxdn_edac Error Detection & Correction
 * @brief Implementation for the NXDN EDAC routines.
 * @ingroup nxdn
 * 
 * @file Convolution.h
 * @ingroup nxdn_edac
 * @file Convolution.cpp
 * @ingroup nxdn_edac
 */
#if !defined(__NXDN_EDAC__CONVOLUTION_H__)
#define  __NXDN_EDAC__CONVOLUTION_H__

#include "common/Defines.h"

#include <cstdint>

namespace nxdn
{
    namespace edac
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements NXDN frame convolution processing.
         * @ingroup nxdn_edac
         */
        class HOST_SW_API Convolution {
        public:
            /**
             * @brief Initializes a new instance of the Convolution class.
             */
            Convolution();
            /**
             * @brief Finalizes a instance of the Convolution class.
             */
            ~Convolution();

            /**
             * @brief Starts convolution processing.
             */
            void start();
            /**
             * @brief 
             * @param out 
             * @param nBits 
             * @returns uint32_t 
             */
            uint32_t chainback(uint8_t* out, uint32_t nBits);

            /**
             * @brief 
             * @param s0 
             * @param s1 
             * @returns bool
             */
            bool decode(uint8_t s0, uint8_t s1);
            /**
             * @brief 
             * @param[in] in 
             * @param[out] out 
             * @param nBits 
             */
            void encode(const uint8_t* in, uint8_t* out, uint32_t nBits) const;

        private:
            uint16_t* m_metrics1;
            uint16_t* m_metrics2;

            uint16_t* m_oldMetrics;
            uint16_t* m_newMetrics;

            uint64_t* m_decisions;

            uint64_t* m_dp;
        };
    } // namespace edac
} // namespace nxdn

#endif // __NXDN_EDAC__CONVOLUTION_H__
