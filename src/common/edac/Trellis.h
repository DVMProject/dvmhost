// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016,2018 Jonathan Naylor, G4KLX
 *  Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file Trellis.h
 * @ingroup edac
 * @file Trellis.cpp
 * @ingroup edac
 */
#if !defined(__EDAC__TRELLIS_H__)
#define __EDAC__TRELLIS_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements 1/2 rate and 3/4 rate Trellis for DMR/P25.
     * @ingroup edac
     */
    class HOST_SW_API Trellis {
    public:
        /**
         * @brief Initializes a new instance of the Trellis class.
         */
        Trellis();
        /**
         * @brief Finalizes a instance of the Trellis class.
         */
        ~Trellis();

        /**
         * @brief Decodes 3/4 rate Trellis.
         * @param[in] data Trellis symbol bytes.
         * @param[out] payload Output bytes.
         * @param skipSymbols Flag indicating symbols should be skipped (this is used for DMR).
         * @returns bool True, if Trellis decoded, otherwise false.
         */
        bool decode34(const uint8_t* data, uint8_t* payload, bool skipSymbols = false);
        /**
         * @brief Encodes 3/4 rate Trellis.
         * @param[in] payload Input bytes.
         * @param[out] data Trellis symbol bytes.
         * @param skipSymbols Flag indicating symbols should be skipped (this is used for DMR).
         */
        void encode34(const uint8_t* payload, uint8_t* data, bool skipSymbols = false);

        /**
         * @brief Decodes 1/2 rate Trellis.
         * @param[in] data Trellis symbol bytes.
         * @param[out] payload Output bytes.
         * @returns bool True, if Trellis decoded, otherwise false.
         */
        bool decode12(const uint8_t* data, uint8_t* payload);
        /**
         * @brief Encodes 1/2 rate Trellis.
         * @param[in] payload Input bytes.
         * @param[out] data Trellis symbol bytes.
         */
        void encode12(const uint8_t* payload, uint8_t* data);

    private:
        /**
         * @brief Helper to deinterleave the input symbols into dibits.
         * @param[in] data Trellis symbol bytes.
         * @param[out] dibits Dibits.
         * @param skipSymbols Flag indicating symbols should be skipped (this is used for DMR).
         */
        void deinterleave(const uint8_t* in, int8_t* dibits, bool skipSymbols = false) const;
        /**
         * @brief Helper to interleave the input dibits into symbols.
         * @param[in] dibits Dibits.
         * @param[out] data Trellis symbol bytes.
         * @param skipSymbols Flag indicating symbols should be skipped (this is used for DMR).
         */
        void interleave(const int8_t* dibits, uint8_t* out, bool skipSymbols = false) const;
        /**
         * @brief Helper to map dibits to trellis constellation points.
         * @param[in] dibits Dibits.
         * @param[out] points Trellis constellation points.
         */
        void dibitsToPoints(const int8_t* dibits, uint8_t* points) const;
        /**
         * @brief Helper to map trellis constellation points to dibits.
         * @param[in] points Trellis Constellation points.
         * @param[out] dibits Dibits.
         */
        void pointsToDibits(const uint8_t* points, int8_t* dibits) const;
        /**
         * @brief Helper to convert a byte payload into tribits.
         * @param[in] payload Byte payload.
         * @param[out] tribits Tribits.
         */
        void bitsToTribits(const uint8_t* payload, uint8_t* tribits) const;
        /**
         * @brief Helper to convert a byte payload into dibits.
         * @param[in] payload Byte payload.
         * @param[out] dibits Dibits.
         */
        void bitsToDibits(const uint8_t* payload, uint8_t* dibits) const;
        /**
         * @brief Helper to convert tribits into a byte payload.
         * @param[in] tribits Tribits.
         * @param[out] payload Byte payload.
         */
        void tribitsToBits(const uint8_t* tribits, uint8_t* payload) const;
        /**
         * @brief Helper to convert dibits into a byte payload.
         * @param[in] dibits Dibits.
         * @param[out] payload Byte payload.
         */
        void dibitsToBits(const uint8_t* dibits, uint8_t* payload) const;

        /**
         * @brief Helper to fix errors in Trellis coding.
         * @param points Trellis constellation points.
         * @param failPos Failure position.
         * @param payload Byte payload.
         * @returns bool True, if error corrected, otherwise false.
         */
        bool fixCode34(uint8_t* points, uint32_t failPos, uint8_t* payload) const;
        /**
         * @brief Helper to detect errors in Trellis coding.
         * @param points Trellis constellation points.
         * @param tribits Tribits.
         * @returns uint32_t Position.
         */
        uint32_t checkCode34(const uint8_t* points, uint8_t* tribits) const;

        /**
         * @brief Helper to fix errors in Trellis coding.
         * @param points Trelli constellation points.
         * @param failPos Failure position.
         * @param payload Byte payload.
         * @returns bool True, if error corrected, otherwise false.
         */
        bool fixCode12(uint8_t* points, uint32_t failPos, uint8_t* payload) const;
        /**
         * @brief Helper to detect errors in Trellis coding.
         * @param points Trelli constellation points.
         * @param dibits Dibits.
         * @returns uint32_t Position.
         */
        uint32_t checkCode12(const uint8_t* points, uint8_t* dibits) const;
    };
} // namespace edac

#endif // __EDAC__TRELLIS_H__
