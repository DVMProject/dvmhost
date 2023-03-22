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
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
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
#if !defined(__P25_AUDIO_H__)
#define __P25_AUDIO_H__

#include "Defines.h"
#include "edac/AMBEFEC.h"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements P25 audio processing and interleaving.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Audio {
    public:
        /// <summary>Initializes a new instance of the Audio class.</summary>
        Audio();
        /// <summary>Finalizes a instance of the Audio class.</summary>
        ~Audio();

        /// <summary>Process P25 IMBE audio data.</summary>
        uint32_t process(uint8_t* data);

        /// <summary>Decode a P25 IMBE audio frame.</summary>
        void decode(const uint8_t* data, uint8_t* imbe, uint32_t n);
        /// <summary>Encode a P25 IMBE audio frame.</summary>
        void encode(uint8_t* data, const uint8_t* imbe, uint32_t n);

    private:
        edac::AMBEFEC m_fec;
    };
} // namespace p25

#endif // __P25_AUDIO_H__
