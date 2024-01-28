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
*   Copyright (C) 2016,2018 Jonathan Naylor, G4KLX
*
*/
#if !defined(__EDAC__TRELLIS_H__)
#define __EDAC__TRELLIS_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements 1/2 rate and 3/4 rate Trellis for DMR/P25.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Trellis {
    public:
        /// <summary>Initializes a new instance of the Trellis class.</summary>
        Trellis();
        /// <summary>Finalizes a instance of the Trellis class.</summary>
        ~Trellis();

        /// <summary>Decodes 3/4 rate Trellis.</summary>
        bool decode34(const uint8_t* data, uint8_t* payload);
        /// <summary>Encodes 3/4 rate Trellis.</summary>
        void encode34(const uint8_t* payload, uint8_t* data);

        /// <summary>Decodes 1/2 rate Trellis.</summary>
        bool decode12(const uint8_t* data, uint8_t* payload);
        /// <summary>Encodes 1/2 rate Trellis.</summary>
        void encode12(const uint8_t* payload, uint8_t* data);

    private:
        /// <summary>Helper to deinterleave the input symbols into dibits.</summary>
        void deinterleave(const uint8_t* in, int8_t* dibits) const;
        /// <summary>Helper to interleave the input dibits into symbols.</summary>
        void interleave(const int8_t* dibits, uint8_t* out) const;
        /// <summary>Helper to map dibits to C4FM constellation points.</summary>
        void dibitsToPoints(const int8_t* dibits, uint8_t* points) const;
        /// <summary>Helper to map C4FM constellation points to dibits.</summary>
        void pointsToDibits(const uint8_t* points, int8_t* dibits) const;
        /// <summary>Helper to convert a byte payload into tribits.</summary>
        void bitsToTribits(const uint8_t* payload, uint8_t* tribits) const;
        /// <summary>Helper to convert a byte payload into dibits.</summary>
        void bitsToDibits(const uint8_t* payload, uint8_t* dibits) const;
        /// <summary>Helper to convert tribits into a byte payload.</summary>
        void tribitsToBits(const uint8_t* tribits, uint8_t* payload) const;
        /// <summary>Helper to convert dibits into a byte payload.</summary>
        void dibitsToBits(const uint8_t* dibits, uint8_t* payload) const;

        /// <summary>Helper to fix errors in Trellis coding.</summary>
        bool fixCode34(uint8_t* points, uint32_t failPos, uint8_t* payload) const;
        /// <summary>Helper to detect errors in Trellis coding.</summary>
        uint32_t checkCode34(const uint8_t* points, uint8_t* tribits) const;

        /// <summary>Helper to fix errors in Trellis coding.</summary>
        bool fixCode12(uint8_t* points, uint32_t failPos, uint8_t* payload) const;
        /// <summary>Helper to detect errors in Trellis coding.</summary>
        uint32_t checkCode12(const uint8_t* points, uint8_t* dibits) const;
    };
} // namespace edac

#endif // __EDAC__TRELLIS_H__
