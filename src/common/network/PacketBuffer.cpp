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

    *message = nullptr;
    if (outLength != nullptr) {
        *outLength = 0;
    }

    uint8_t curBlock = data[8U];
    uint8_t blockCnt = data[9U];

    Fragment* frag = new Fragment();

    // if this is the first block store sizes and initialize temp buffer
    if (curBlock == 0U) {
        uint32_t size = __GET_UINT32(data, 0U);
        uint32_t compressedSize = __GET_UINT32(data, 4U);

        frag->size = size;
        frag->compressedSize = compressedSize;
    }

    // scope is intentional
    {
        frag->blockId = curBlock;
        if (frag->size < FRAG_BLOCK_SIZE)
            frag->data = new uint8_t[FRAG_BLOCK_SIZE + 1U];
        else 
            frag->data = new uint8_t[frag->size + 1U];

        ::memcpy(frag->data, data + FRAG_HDR_SIZE, FRAG_BLOCK_SIZE);
        // Utils::dump(1U, "Block Payload", frag->data, FRAG_BLOCK_SIZE);
        fragments.insert(curBlock, frag);
    }

    LogInfoEx(LOG_NET, "%s, Inbound Packet Fragment, block %u of %u, rxFragments = %u", m_name, curBlock, blockCnt, fragments.size());

    // do we have all the blocks?
    if (fragments.size() == blockCnt + 1U) {
        uint8_t* buffer = nullptr;
        fragments.lock(false);
        if (fragments[0] == nullptr) {
            LogError(LOG_NET, "%s, Packet Fragment, error missing block 0? Packet dropped.", m_name);
            fragments.unlock();
            clear();
            return false;
        }

        if (fragments[0]->size == 0U) {
            LogError(LOG_NET, "%s, Packet Fragment, error missing size information", m_name);
            fragments.unlock();
            clear();
            return false;
        }

        if (fragments[0]->compressedSize == 0U) {
            LogError(LOG_NET, "%s, Packet Fragment, error missing compressed size information", m_name);
            fragments.unlock();
            clear();
            return false;
        }

        uint32_t compressedLen = fragments[0]->compressedSize;
        uint32_t len = fragments[0]->size;

        buffer = new uint8_t[len +1U];
        ::memset(buffer, 0x00U, len + 1U);
        if (fragments.size() == 1U) {
            ::memcpy(buffer, fragments[0U]->data, len);
        } else {
            for (uint8_t i = 0U; i < fragments.size(); i++) {
                uint32_t offs = i * FRAG_BLOCK_SIZE;
                ::memcpy(buffer + offs, fragments[i]->data, FRAG_BLOCK_SIZE);
            }
        }

        if (m_compression) {
            uint32_t decompressedLen = 0U;
            *message = Compression::decompress(buffer, compressedLen, &decompressedLen);

            // check that we got the appropriate data
            if (decompressedLen == len && message != nullptr) {
                if (outLength != nullptr) {
                    *outLength = len;
                }

                fragments.unlock();
                fragments.clear();
                return true;
            }
        }
        else {
            *message = buffer;
            if (outLength != nullptr) {
                *outLength = len;
            }

            fragments.unlock();
            fragments.clear();
            return true;
        }

        fragments.unlock();
    }

    return false;
}

/* Encode a network packet fragment. */

void PacketBuffer::encode(uint8_t* data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    // erase any buffered fragments
    clear();

    // create temporary buffer
    uint32_t compressedLen = 0U;
    uint8_t* buffer = nullptr;
    if (m_compression) {
        buffer = Compression::compress(data, length, &compressedLen);
    } else {
        buffer = new uint8_t[length];
        ::memset(buffer, 0x00U, length);
        ::memcpy(buffer, data, length);
        compressedLen = length;
    }

    // create packet fragments
    uint8_t blockCnt = (compressedLen / FRAG_BLOCK_SIZE) + (compressedLen % FRAG_BLOCK_SIZE ? 1U : 0U);
    uint32_t offs = 0U;
    for (uint8_t i = 0U; i < blockCnt; i++) {
        // build dataset
        uint16_t bufSize = FRAG_SIZE;

        Fragment* frag = new Fragment();
        frag->blockId = i;

        frag->data = new uint8_t[bufSize];
        ::memset(frag->data, 0x00U, bufSize);

        if (i == 0U) {
            __SET_UINT32(length, frag->data, 0U);
            frag->size = length;
            __SET_UINT32(compressedLen, frag->data, 4U);
            frag->compressedSize = compressedLen;
        }

        frag->data[8U] = i;
        frag->data[9U] = blockCnt - 1U;

        uint32_t blockSize = FRAG_BLOCK_SIZE;
        if (offs + FRAG_BLOCK_SIZE > compressedLen)
            blockSize = FRAG_BLOCK_SIZE - ((offs + FRAG_BLOCK_SIZE) - compressedLen);

        ::memcpy(frag->data + FRAG_HDR_SIZE, buffer + offs, blockSize);

        offs += FRAG_BLOCK_SIZE;

        fragments.insert(i, frag);
        LogInfoEx(LOG_NET, "%s, Outbound Packet Fragment, block %u of %u, txFragments = %u", m_name, i, blockCnt - 1U, fragments.size());
    }
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
