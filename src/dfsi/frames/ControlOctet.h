// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - DFSI Peer Application
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI Peer Application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__CONTROL_OCTET_H__)
#define __CONTROL_OCTET_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "frames/FrameDefines.h"

namespace p25
{
    namespace dfsi
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements a DFSI control octet packet.
        // 
        // Byte 0
        // Bit  0 1 2 3 4 5 6 7
        //     +-+-+-+-+-+-+-+-+
        //     |S|C|   BHC     |
        //     +-+-+-+-+-+-+-+-+
        // ---------------------------------------------------------------------------

        class HOST_SW_API ControlOctet {
        public:
            static const uint8_t LENGTH = 1;

            /// <summary>Initializes a copy instance of the ControlOctet class.</summary>
            ControlOctet();
            /// <summary>Initializes a copy instance of the ControlOctet class.</summary>
            ControlOctet(uint8_t* data);

            /// <summary>Decode a control octet frame.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encode a control octet frame.</summary>
            void encode(uint8_t* data);
        
        public:
            /// <summary></summary>
            __PROPERTY(bool, signal, Signal);
            /// <summary>Indicates a compact (1) or verbose (0) block header.</summary>
            __PROPERTY(bool, compact, Compact);
            /// <summary>Number of block headers following this control octet.</summary>
            __PROPERTY(uint8_t, blockHeaderCnt, BlockHeaderCnt);
        };
    } // namespace dfsi
} // namespace p25

#endif // __CONTROL_OCTET_H__