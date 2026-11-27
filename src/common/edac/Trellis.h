// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016,2018 Jonathan Naylor, G4KLX
 *  Copyright (C) 2023-2026 Bryan Biedenkapp, N2PLL
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
    //  Constants
    // ---------------------------------------------------------------------------

    /**
     * @addtogroup edac
     * @{
     */

    const uint32_t TRELLIS_34_MAX_LIST_PATHS = 8U;          // Maximum number of candidate paths returned by the 3/4 rate Trellis list decoder.
    const uint32_t TRELLIS_34_PAYLOAD_LENGTH_BYTES = 18U;   // Length in bytes of one decoded 3/4 rate Trellis payload.

    /** @} */

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements 1/2 rate and 3/4 rate Trellis for DMR/P25.
     * @ingroup edac
     */
    class DVM_COMMON_API Trellis {
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
         *
         * This performs a maximum-likelihood decode and always produces the closest retained
         * codeword. The caller must use a CRC or another integrity check when one is available.
         * @param[in] data Trellis symbol bytes.
         * @param[out] payload Output buffer of TRELLIS_34_PAYLOAD_LENGTH_BYTES bytes.
         * @param skipSymbols Flag indicating symbols should be skipped (this is used for DMR).
         * @returns bool True if a closest codeword was produced.
         */
        bool decode34(const uint8_t* data, uint8_t* payload, bool skipSymbols = false);
        /**
         * @brief Decodes 3/4 rate Trellis and returns up to maxPaths candidates tied for the best metric.
         * @param[in] data Trellis symbol bytes.
         * @param[out] payloads Output buffer of maxPaths * TRELLIS_34_PAYLOAD_LENGTH_BYTES bytes.
         * @param maxPaths Maximum number of candidates to return, from 1 through TRELLIS_34_MAX_LIST_PATHS.
         * @param[out] truncated True if additional equally likely candidates were omitted; otherwise false.
         * @param skipSymbols Flag indicating symbols should be skipped (this is used for DMR).
         * @returns uint32_t Number of candidates written, or zero when maxPaths is invalid.
         */
        uint32_t decode34List(const uint8_t* data, uint8_t* payloads, uint32_t maxPaths, bool& truncated, bool skipSymbols = false);
        /**
         * @brief Encodes 3/4 rate Trellis.
         * @param[in] payload Input bytes.
         * @param[out] data Trellis symbol bytes.
         * @param skipSymbols Flag indicating symbols should be skipped (this is used for DMR).
         */
        void encode34(const uint8_t* payload, uint8_t* data, bool skipSymbols = false);

        /**
         * @brief Decodes 1/2 rate Trellis.
         *
         * This performs a maximum-likelihood decode and always produces the closest codeword.
         * @param[in] data Trellis symbol bytes.
         * @param[out] payload Output bytes.
         * @returns bool True if a closest codeword was produced.
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
         * @brief Runs a bounded list Viterbi decode of the 3/4 rate Trellis.
         * @param[in] dibits Deinterleaved received dibits.
         * @param[out] payloads Output buffer for maxPaths decoded payloads.
         * @param[out] metrics Path metric for each returned candidate.
         * @param maxPaths Number of survivors retained per state.
         * @param[out] truncated True if an equal-metric survivor was discarded.
         * @returns uint32_t Number of candidates written.
         */
        uint32_t viterbi34(const int8_t* dibits, uint8_t* payloads, uint32_t* metrics, uint32_t maxPaths,
            bool& truncated) const;
        /**
         * @brief Runs a Viterbi decode of the 1/2 rate Trellis.
         * @param[in] dibits Deinterleaved received dibits.
         * @param[out] payload Decoded 12-byte payload.
         * @param[out] metric Best path metric.
         * @returns bool True if a path terminating in the flushing state was decoded.
         */
        bool viterbi12(const int8_t* dibits, uint8_t* payload, uint32_t& metric) const;
    };
} // namespace edac

#endif // __EDAC__TRELLIS_H__
