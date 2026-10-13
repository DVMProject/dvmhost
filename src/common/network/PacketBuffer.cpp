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
#include "network/PacketBuffer.h"
#include "Log.h"
#include "Utils.h"

using namespace network;
using namespace compress;

#include <cassert>
#include <algorithm>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define MAX_FRAGMENT_SIZE 8192 * 1024 // 8MB max

#define MAX_BLOCK_CNT 256U
#define MAX_EX_BLOCK_CNT 65535U

#define MAX_256_COMPRESSED_SIZE (MAX_BLOCK_CNT * FRAG_BLOCK_SIZE)

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the PacketBuffer class. */

PacketBuffer::PacketBuffer(bool compression, const char* name) :
    fragments(),
    m_compression(compression),
    m_name(name)
{
    /* stub */
}

/* Finalizes a instance of the PacketBuffer class. */

PacketBuffer::~PacketBuffer()
{
    clear();
}

/* Decode a network packet fragment. */

bool PacketBuffer::decode(const uint8_t* data, uint8_t** message, uint32_t* outLength)
{
    assert(data != nullptr);
    assert(message != nullptr);

    *message = nullptr;
    if (outLength != nullptr) {
        *outLength = 0;
    }

    uint32_t size = GET_UINT32(data, 0U);
    uint32_t compressedSize = GET_UINT32(data, 4U);
    bool extended = compressedSize > MAX_256_COMPRESSED_SIZE;

    uint16_t curBlock = data[8U];
    uint16_t lastBlock = data[9U];

    if (extended) {
        curBlock = GET_UINT16(data, 8U);
        lastBlock = GET_UINT16(data, 10U);
    }

    uint32_t blockCnt = (uint32_t)lastBlock + 1U;
    uint32_t headerSize = extended ? EXTENDED_FRAG_HDR_SIZE : FRAG_HDR_SIZE;
    uint32_t blockSize = extended ? EXTENDED_FRAG_BLOCK_SIZE : FRAG_BLOCK_SIZE;

    if ((uint32_t)curBlock >= blockCnt || blockCnt > (extended ? MAX_EX_BLOCK_CNT : MAX_BLOCK_CNT)) {
        LogError(LOG_NET, "%s, invalid fragment block %u of %u", m_name, curBlock, blockCnt);
        clear();
        return false;
    }

    // validate fragment size and block count for extended fragments
    if (extended) {
        if (size == 0U || size > MAX_FRAGMENT_SIZE || compressedSize > MAX_FRAGMENT_SIZE) {
            LogError(LOG_NET, "%s, invalid extended fragment size metadata", m_name);
            clear();
            return false;
        }

        uint32_t expectedBlocks = (compressedSize + blockSize - 1U) / blockSize;
        if (expectedBlocks != blockCnt) {
            LogError(LOG_NET, "%s, extended fragment block count mismatch, expected %u, got %u", m_name,
                expectedBlocks, blockCnt);
            clear();
            return false;
        }
    } else if (curBlock != 0U && (size != 0U || compressedSize != 0U)) {
        LogError(LOG_NET, "%s, invalid legacy fragment metadata in block %u", m_name, curBlock);
        clear();
        return false;
    }

    // do not permit fragments from the two wire formats to be mixed in one assembly
    if (!fragments.empty()) {
        auto existing = fragments.begin();
        if (existing->second == nullptr || existing->second->extended != extended || existing->second->blockCount != blockCnt ||
            (extended && (existing->second->size != size || existing->second->compressedSize != compressedSize))) {
            LogError(LOG_NET, "%s, inconsistent packet fragment metadata", m_name);
            clear();
            return false;
        }
    }

    Fragment* frag = new Fragment();
    frag->compressedSize = 0U;
    frag->size = 0U;
    frag->blockSize = 0U;
    frag->blockId = 0U;
    frag->blockCount = 0U;
    frag->extended = extended;
    frag->data = nullptr;

    // if this is the first block store sizes and initialize temp buffer
    if (curBlock == 0U) {
        // make sure we can't exceed max fragment size -- prevent potential DOS attack by sending
        // enormous fragment sizes
        if (size == 0U || compressedSize == 0U || size > MAX_FRAGMENT_SIZE || compressedSize > MAX_FRAGMENT_SIZE ||
            (!extended && compressedSize > MAX_256_COMPRESSED_SIZE)) {
            LogError(LOG_NET, "%s, fragment size exceeds maximum. BUGBUG.", m_name);
            delete frag;
            clear();
            return false;
        }

        uint32_t expectedBlocks = (compressedSize + blockSize - 1U) / blockSize;
        if (expectedBlocks != blockCnt) {
            LogError(LOG_NET, "%s, fragment block count mismatch, expected %u, got %u", m_name,
                expectedBlocks, blockCnt);
            delete frag;
            clear();
            return false;
        }

        // non-zero blocks may arrive before block zero; reject the transfer if
        // accepting block zero would leave more fragments than its metadata
        // declares, rather than retaining an assembly that can never finish
        if (fragments.size() >= expectedBlocks) {
            LogError(LOG_NET, "%s, fragment count exceeds block zero metadata, expected %u, already buffered %u",
                m_name, expectedBlocks, fragments.size());
            delete frag;
            clear();
            return false;
        }

        frag->size = size;
        frag->compressedSize = compressedSize;
    } else if (extended) {
        frag->size = size;
        frag->compressedSize = compressedSize;
    }

    // scope is intentional
    {
        frag->blockId = curBlock;
        frag->blockCount = (uint16_t)blockCnt;
        frag->blockSize = blockSize;
        frag->data = new uint8_t[blockSize + 1U];

        ::memcpy(frag->data, data + headerSize, blockSize);

        if (fragments.find(curBlock) != fragments.end()) {
            LogWarning(LOG_NET, "%s, duplicate fragment block %u ignored", m_name, curBlock);
            delete[] frag->data;
            delete frag;
            return false;
        }

        fragments.insert(curBlock, frag);
    }

    LogInfoEx(LOG_NET, "%s, Inbound Packet Fragment%s, block %u of %u, rxFragments = %u", m_name,
        extended ? " Extended" : "", curBlock, blockCnt - 1U, fragments.size());

    // do we have all the blocks?
    if (fragments.size() == blockCnt) {
        fragments.lock(false);
        auto firstIt = fragments.find(0U);
        if (firstIt == fragments.end() || firstIt->second == nullptr) {
            LogError(LOG_NET, "%s, Packet Fragment, error missing block 0? Packet dropped.", m_name);
            fragments.unlock();
            clear();
            return false;
        }

        Fragment* firstFrag = firstIt->second;

        if (firstFrag->size == 0U) {
            LogError(LOG_NET, "%s, Packet Fragment, error missing size information", m_name);
            fragments.unlock();
            clear();
            return false;
        }

        if (firstFrag->compressedSize == 0U) {
            LogError(LOG_NET, "%s, Packet Fragment, error missing compressed size information", m_name);
            fragments.unlock();
            clear();
            return false;
        }

        uint32_t compressedLen = firstFrag->compressedSize;
        uint32_t len = firstFrag->size;

        if (compressedLen > MAX_FRAGMENT_SIZE || len > MAX_FRAGMENT_SIZE) {
            LogError(LOG_NET, "%s, Packet Fragment, error invalid packet length metadata", m_name);
            fragments.unlock();
            clear();
            return false;
        }

        uint32_t expectedBlocks = (compressedLen + blockSize - 1U) / blockSize;
        if (fragments.size() != expectedBlocks || expectedBlocks != blockCnt) {
            LogError(LOG_NET, "%s, Packet Fragment, inconsistent block count, expected %u, got %u", m_name,
                expectedBlocks, fragments.size());
            fragments.unlock();
            clear();
            return false;
        }

        // reassembly buffer must be compressed length, not uncompressed length
        DECLARE_UINT8_ARRAY(buffer, compressedLen + 1U);
        ::memset(buffer, 0x00U, compressedLen + 1U);

        if (fragments.size() == 1U) {
            uint32_t copyLen = (compressedLen > blockSize) ? blockSize : compressedLen;
            ::memcpy(buffer, firstFrag->data, copyLen);
        } else {
            for (uint32_t i = 0U; i < blockCnt; i++) {
                auto it = fragments.find(i);
                if (it == fragments.end() || it->second == nullptr || it->second->data == nullptr) {
                    LogError(LOG_NET, "%s, Packet Fragment, error missing block %u, packet dropped", m_name, i);
                    fragments.unlock();
                    clear();
                    return false;
                }

                uint32_t offs = i * blockSize;
                if (offs >= compressedLen)
                    break;

                uint32_t copyLen = std::min<uint32_t>(blockSize, compressedLen - offs);

                ::memcpy(buffer + offs, it->second->data, copyLen);
            }
        }

        if (m_compression) {
            uint32_t decompressedLen = 0U;
            UInt8Array decompressed = Compression::decompress(buffer, compressedLen, &decompressedLen);
            if (decompressed == nullptr || decompressedLen == 0U) {
                LogError(LOG_NET, "%s, Packet Fragment, error decompressing packet data", m_name);
                fragments.unlock();
                clear();
                return false;
            }

            *message = new uint8_t[decompressedLen];
            ::memset(*message, 0x00U, decompressedLen);
            ::memcpy(*message, decompressed.get(), decompressedLen);

            // check that we got the appropriate data
            if (decompressedLen == len && message != nullptr) {
                if (outLength != nullptr) {
                    *outLength = len;
                }

                fragments.unlock();
                clear();
                return true;
            }

            delete[] *message;
            *message = nullptr;
            LogError(LOG_NET, "%s, Packet Fragment, decompressed size mismatch, expected %u, got %u", m_name,
                len, decompressedLen);
        }
        else {
            if (compressedLen != len) {
                LogError(LOG_NET, "%s, Packet Fragment, uncompressed size mismatch, expected %u, got %u", m_name,
                    len, compressedLen);
                fragments.unlock();
                clear();
                return false;
            }

            uint8_t* outBuf = new uint8_t[len + 1U];
            ::memset(outBuf, 0x00U, len + 1U);
            ::memcpy(outBuf, buffer, len);

            *message = outBuf;
            if (outLength != nullptr) {
                *outLength = len;
            }

            fragments.unlock();
            clear();
            return true;
        }

        fragments.unlock();
        clear();
    }

    return false;
}

/* Encode a network packet fragment. */

bool PacketBuffer::encode(uint8_t* data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    // erase any buffered fragments
    clear();

    // create temporary buffer
    uint32_t compressedLen = 0U;
    UInt8Array buffer = nullptr;
    if (m_compression) {
        buffer = Compression::compress(data, length, &compressedLen);
    } else {
        buffer = std::make_unique<uint8_t[]>(length + 1U);
        ::memset(buffer.get(), 0x00U, length + 1U);
        ::memcpy(buffer.get(), data, length);
        compressedLen = length;
    }

    // create packet fragments
    if (length > MAX_FRAGMENT_SIZE || compressedLen == 0U || compressedLen > MAX_FRAGMENT_SIZE) {
        LogError(LOG_NET, "%s, Outbound Packet Fragment, packet size exceeds maximum", m_name);
        return false;
    }

    bool extended = compressedLen > MAX_256_COMPRESSED_SIZE;
    uint32_t headerSize = extended ? EXTENDED_FRAG_HDR_SIZE : FRAG_HDR_SIZE;
    uint32_t blockSize = extended ? EXTENDED_FRAG_BLOCK_SIZE : FRAG_BLOCK_SIZE;
    uint32_t maxBlockCnt = extended ? MAX_EX_BLOCK_CNT : MAX_BLOCK_CNT;
    uint32_t blockCnt = (compressedLen + blockSize - 1U) / blockSize;
    if (blockCnt == 0U || blockCnt > maxBlockCnt) {
        LogError(LOG_NET, "%s, Outbound Packet Fragment, too many packet fragments, %u (max %u)", m_name, blockCnt, maxBlockCnt);
        return false;
    }

    uint32_t offs = 0U;
    for (uint32_t i = 0U; i < blockCnt; i++) {
        // build dataset
        uint16_t bufSize = FRAG_SIZE;

        Fragment* frag = new Fragment();
        frag->compressedSize = 0U;
        frag->size = 0U;
        frag->blockSize = 0U;
        frag->blockId = (uint16_t)i;
        frag->blockCount = (uint16_t)blockCnt;
        frag->extended = extended;
        frag->data = nullptr;

        frag->data = new uint8_t[bufSize];
        ::memset(frag->data, 0x00U, bufSize);

        if (i == 0U || extended) {
            SET_UINT32(length, frag->data, 0U);
            frag->size = length;
            SET_UINT32(compressedLen, frag->data, 4U);
            frag->compressedSize = compressedLen;
        }

        if (extended) {
            SET_UINT16((uint16_t)i, frag->data, 8U);
            SET_UINT16((uint16_t)(blockCnt - 1U), frag->data, 10U);
        } else {
            frag->data[8U] = (uint8_t)i;
            frag->data[9U] = (uint8_t)(blockCnt - 1U);
        }

        uint32_t copySize = blockSize;
        if (offs + blockSize > compressedLen)
            copySize = blockSize - ((offs + blockSize) - compressedLen);

        frag->blockSize = copySize;
        ::memcpy(frag->data + headerSize, buffer.get() + offs, copySize);

        offs += blockSize;

        fragments.insert((uint16_t)i, frag);
        LogInfoEx(LOG_NET, "%s, Outbound Packet Fragment%s, block %u of %u, txFragments = %u", m_name,
            extended ? " Extended" : "", i, blockCnt - 1U, fragments.size());
    }

    return true;
}

/* Helper to clear currently buffered fragments. */

void PacketBuffer::clear()
{
    fragments.lock(false);
    for (auto entry : fragments) {
        Fragment* frag = entry.second;
        if (frag != nullptr) {
            if (frag->data != nullptr)
                delete[] frag->data;
            frag->data = nullptr;
            delete frag;
        }
    }
    fragments.unlock();

    fragments.clear();
}
