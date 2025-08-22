// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup compression Compression Routines
 * @brief Defines and implements common compression routines.
 * @ingroup common
 * 
 * @file Compression.h
 * @ingroup compression
 * @file Compression.cpp
 * @ingroup compression
 */
#if !defined(__COMPRESSION_H__)
#define __COMPRESSION_H__

#include "common/Defines.h"

namespace compress
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief zlib Compression Helper.
     * @ingroup compression
     */
    class HOST_SW_API Compression {
    public:
        /**
         * @brief Compress the given input buffer using zlib compression.
         * @param[in] buffer Buffer containing data to zlib compress.
         * @param[in] len Length of data to compress.
         * @param[out] compressedLen Length of compressed data.
         * @returns UInt8Array Buffer containing compressed data.
         */
        static UInt8Array compress(const uint8_t* buffer, uint32_t len, uint32_t* compressedLen);

        /**
         * @brief Decompress the given input buffer using zlib compression.
         * @param[in] buffer Buffer containing zlib compressed data.
         * @param[in] len Length of compressed data.
         * @param[out] decompressedLen Length of decompressed data.
         * @returns UInt8Array Buffer containing decompressed data.
         */
        static UInt8Array decompress(const uint8_t* buffer, uint32_t len, uint32_t* decompressedLen);
    };
} // namespace compress

#endif // __COMPRESSION_H__
