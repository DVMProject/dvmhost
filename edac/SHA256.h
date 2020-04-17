/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2005, 2006, 2008, 2009 Free Software Foundation, Inc.
*   Copyright (C) 2011,2015,2016 by Jonathan Naylor G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__SHA256_H__)
#define __SHA256_H__

#include "Defines.h"

#include <cstdint>

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    enum {
        SHA256_DIGEST_SIZE = 256 / 8
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements SHA-256 hashing.
    // ---------------------------------------------------------------------------

    class HOST_SW_API SHA256 {
    public:
        /// <summary>Initializes a new instance of the SHA256 class.</summary>
        SHA256();
        /// <summary>Finalizes a instance of the SHA256 class.</summary>
        ~SHA256();

        /// <summary>Starting with the result of former calls of this function (or the initialization
        /// function update the context for the next LEN bytes starting at BUFFER. It is necessary
        /// that LEN is a multiple of 64!!!</summary>
        void processBlock(const uint8_t* buffer, uint32_t len);

        /// <summary>Starting with the result of former calls of this function (or the initialization
        /// function update the context for the next LEN bytes starting at BUFFER. It is NOT required
        /// that LEN is a multiple of 64.</summary>
        void processBytes(const uint8_t* buffer, uint32_t len);

        /// <summary>Process the remaining bytes in the bufferand put result from context in first 32
        /// bytes following buffer. The result is always in little endian byte order, so that a byte-wise
        /// output yields to the wanted ASCII representation of the message digest.</summary>
        uint8_t* finish(uint8_t* buffer);

        /// <summary>Put result from context in first 32 bytes following buffer. The result is always
        /// in little endian byte order, so that a byte - wise output yields to the wanted ASCII
        /// representation of the message digest.</summary>
        uint8_t* read(uint8_t* buffer);

        /// <summary>Compute SHA256 message digest for the length bytes beginning at buffer. The result
        /// is always in little endian byte order, so that a byte-wise output yields to the wanted ASCII
        /// representation of the message digest.</summary>
        uint8_t* buffer(const uint8_t* buffer, uint32_t len, uint8_t* resblock);

    private:
        uint32_t* m_state;
        uint32_t* m_total;
        uint32_t            m_buflen;
        uint32_t* m_buffer;

        /// <summary></summary>
        void init();
        /// <summary>Process the remaining bytes in the internal buffer and the usual prolog according to 
        /// the standard and write the result to the buffer.</summary>
        void conclude();
    };
} // namespace edac

#endif // __SHA256_H__
