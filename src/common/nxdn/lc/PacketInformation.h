// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__NXDN_LC__PACKET_INFORMATION_H__)
#define  __NXDN_LC__PACKET_INFORMATION_H__

#include "common/Defines.h"

namespace nxdn
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents the packet information data for link control data.
        // ---------------------------------------------------------------------------

        class HOST_SW_API PacketInformation {
        public:
            /// <summary>Initializes a new instance of the PacketInformation class.</summary>
            PacketInformation();
            /// <summary>Finalizes a instance of the PacketInformation class.</summary>
            ~PacketInformation();

            /// <summary>Decodes packet information.</summary>
            bool decode(const uint8_t messageType, const uint8_t* data);
            /// <summary>Encodes packet information.</summary>
            void encode(const uint8_t messageType, uint8_t* data);

            /// <summary>Helper to reset data values to defaults.</summary>
            void reset();

        public:
            /** Common Data **/
            /// <summary>Flag indicating if confirmed delivery is needed.</summary>
            __PROPERTY(bool, delivery, Delivery);
            /// <summary>Flag indicating if the packet is a selective retry packet.</summary>
            __PROPERTY(bool, selectiveRetry, SelectiveRetry);
            /// <summary>Count of data blocks in t he transmission packet.</summary>
            __PROPERTY(uint8_t, blockCount, BlockCount);
            /// <summary>Number of padding octets of the last block.</summary>
            __PROPERTY(uint8_t, padCount, PadCount);
            /// <summary>Flag indicating the first fragment.</summary>
            __PROPERTY(bool, start, Start);
            /// <summary>Flag indicating if the Tx fragment count circulates.</summary>
            __PROPERTY(bool, circular, Circular);
            /// <summary>The number and sequence of fragments.</summary>
            __PROPERTY(uint16_t, fragmentCount, FragmentCount);

            /** Response Data */
            /// <summary>Response class.</summary>
            __PROPERTY(uint8_t, rspClass, ResponseClass);
            /// <summary>Response type.</summary>
            __PROPERTY(uint8_t, rspType, ResponseType);
            /// <summary>Error Block Flag.</summary>
            __PROPERTY(uint16_t, rspErrorBlock, ResponseErrorBlock);
        };
    } // namespace lc
} // namespace nxdn

#endif // __NXDN_LC__PACKET_INFORMATION_H__
