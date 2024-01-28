// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2016 Jonathan Naylor, G4KLX
*
*/
#if !defined(__P25_AUDIO_H__)
#define __P25_AUDIO_H__

#include "common/Defines.h"
#include "common/edac/AMBEFEC.h"

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
