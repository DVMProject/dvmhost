// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file SlotType.h
 * @ingroup dmr
 * @file SlotType.cpp
 * @ingroup dmr
 */
#if !defined(__DMR_SLOT_TYPE_H__)
#define __DMR_SLOT_TYPE_H__

#include "common/Defines.h"
#include "common/dmr/DMRDefines.h"

namespace dmr
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents DMR slot type.
     * @ingroup dmr
     */
    class HOST_SW_API SlotType {
    public:
        /**
         * @brief Initializes a new instance of the SlotType class.
         */
        SlotType();
        /**
         * @brief Finalizes a instance of the SlotType class.
         */
        ~SlotType();

        /**
         * @brief Decodes DMR slot type.
         * @param[in] data Buffer containing DMR slot type.
         */
        void decode(const uint8_t* data);
        /**
         * @brief Encodes DMR slot type.
         * @param[out] data Buffer to encode DMR slot type.
         */
        void encode(uint8_t* data) const;

    public:
        /**
         * @brief DMR access color code.
         */
        DECLARE_PROPERTY(uint8_t, colorCode, ColorCode);

        /**
         * @brief Slot data type.
         */
        DECLARE_PROPERTY(defines::DataType::E, dataType, DataType);
    };
} // namespace dmr

#endif // __DMR_SLOT_TYPE_H__
