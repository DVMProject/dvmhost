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
*   Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DMR_SLOT_TYPE_H__)
#define __DMR_SLOT_TYPE_H__

#include "common/Defines.h"
#include "common/dmr/DMRDefines.h"

namespace dmr
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Represents DMR slot type.
    // ---------------------------------------------------------------------------

    class HOST_SW_API SlotType {
    public:
        /// <summary>Initializes a new instance of the SlotType class.</summary>
        SlotType();
        /// <summary>Finalizes a instance of the SlotType class.</summary>
        ~SlotType();

        /// <summary>Decodes DMR slot type.</summary>
        void decode(const uint8_t* data);
        /// <summary>Encodes DMR slot type.</summary>
        void encode(uint8_t* data) const;

    public:
        /// <summary>DMR access color code.</summary>
        __PROPERTY(uint8_t, colorCode, ColorCode);

        /// <summary>Slot data type.</summary>
        __PROPERTY(defines::DataType::E, dataType, DataType);
    };
} // namespace dmr

#endif // __DMR_SLOT_TYPE_H__
