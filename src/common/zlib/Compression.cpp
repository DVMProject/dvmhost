// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "zlib/Compression.h"
#include "zlib/zlib.h"
#include "Log.h"
#include "Utils.h"

#include <cassert>
#include <vector>

using namespace compress;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Compress the given input buffer. */

UInt8Array Compression::compress(const uint8_t* buffer, uint32_t len, uint32_t* compressedLen)
{
    assert(buffer != nullptr);
    assert(len > 0U);

    if (compressedLen != nullptr) {
        *compressedLen = 0U;
    }

    uint8_t* data = new uint8_t[len];
    ::memset(data, 0x00U, len);
    ::memcpy(data, buffer, len);

    // compression structures
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    // initialize compression
    if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK) {
        LogError(LOG_HOST, "Error initializing ZLIB");
        delete[] data;
        return nullptr;
    }

    // set input data
    strm.avail_in = len;
    strm.next_in = data;

    // compress data
    std::vector<uint8_t> compressedData;
    int ret;
    do {
        // resize the output buffer as needed
        compressedData.resize(compressedData.size() + 16384);
        strm.avail_out = 16384;
        strm.next_out = compressedData.data() + compressedData.size() - 16384;

        ret = deflate(&strm, Z_FINISH);
        if (ret == Z_STREAM_ERROR) {
            LogError(LOG_HOST, "ZLIB error compressing data; stream error");
            deflateEnd(&strm);
            delete[] data;
            return nullptr;
        }
    } while (ret != Z_STREAM_END);

    // resize the output buffer to the actual compressed data size
    compressedData.resize(strm.total_out);

    // cleanup
    deflateEnd(&strm);

    if (compressedLen != nullptr) {
        *compressedLen = strm.total_out;
    }

    uint8_t* compressed = compressedData.data();
#if DEBUG_COMPRESS
    Utils::dump(2U, "Compression::compress(), Compressed Data", compressed, strm.total_out);
#endif

    delete[] data;

    // return compressed data
    UInt8Array out = std::make_unique<uint8_t[]>(strm.total_out);
    ::memset(out.get(), 0x00U, strm.total_out);
    ::memcpy(out.get(), compressed, strm.total_out);

    compressedData.clear(); // clear the vector to release memory
    return out;
}

/* Decompress the given input buffer. */

UInt8Array Compression::decompress(const uint8_t* buffer, uint32_t len, uint32_t* decompressedLen)
{
    assert(buffer != nullptr);
    assert(len > 0U);

    if (decompressedLen != nullptr) {
        *decompressedLen = 0U;
    }

    uint8_t* data = new uint8_t[len];
    ::memset(data, 0x00U, len);
    ::memcpy(data, buffer, len);

    // compression structures
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    // set input data
    strm.avail_in = len;
    strm.next_in = data;

    // initialize decompression
    int ret = inflateInit(&strm);
    if (ret != Z_OK) {
        LogError(LOG_HOST, "Error initializing ZLIB");
        delete[] data;
        return nullptr;
    }

    // decompress data
    std::vector<uint8_t> decompressedData;
    uint8_t outbuffer[1024];
    do {
        strm.avail_out = sizeof(outbuffer);
        strm.next_out = outbuffer;

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR) {
            LogError(LOG_HOST, "ZLIB error decompressing compressed data; stream error");
            inflateEnd(&strm);
            delete[] data;
            return nullptr;
        }

        decompressedData.insert(decompressedData.end(), outbuffer, outbuffer + sizeof(outbuffer) - strm.avail_out);
    } while (ret != Z_STREAM_END);

    // cleanup
    inflateEnd(&strm);

    if (decompressedLen != nullptr) {
        *decompressedLen = strm.total_out;
    }

    uint8_t* decompressed = decompressedData.data();
#if DEBUG_COMPRESS
    Utils::dump(2U, "Compression::decompress(), Decompressed Data", decompressed, strm.total_out);
#endif

    delete[] data;

    // return decompressed data
    UInt8Array out = std::make_unique<uint8_t[]>(strm.total_out);
    ::memset(out.get(), 0x00U, strm.total_out);
    ::memcpy(out.get(), decompressed, strm.total_out);

    decompressedData.clear(); // clear the vector to release memory
    return out;
}
