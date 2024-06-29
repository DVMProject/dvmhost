// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *
 */
/**
 * @file Sync.h
 * @ingroup dmr
 * @file Sync.cpp
 * @ingroup dmr
 */
#if !defined(__DMR_SYNC_H__)
#define __DMR_SYNC_H__

#include "common/Defines.h"

namespace dmr
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Helper class for generating DMR sync data.
     * @ingroup dmr
     */
    class HOST_SW_API Sync {
    public:
        /**
         * @brief Helper to append DMR data sync bytes to the passed buffer.
         * @param data Buffer to append DMR data sync bytes to.
         * @param duplex Flag indicating whether or not duplex sync is required.
         */
        static void addDMRDataSync(uint8_t* data, bool duplex);
        /**
         * @brief Helper to append DMR voice sync bytes to the passed buffer.
         * @param data Buffer to append DMR voice sync bytes to.
         * @param duplex Flag indicating whether or not duplex sync is required.
         */
        static void addDMRAudioSync(uint8_t* data, bool duplex);
    };
} // namespace dmr

#endif // __DMR_SYNC_H__
