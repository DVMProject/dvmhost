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
 * @ingroup p25
 * @file Sync.cpp
 * @ingroup p25
 */
#if !defined(__P25_SYNC_H__)
#define __P25_SYNC_H__

#include "common/Defines.h"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Helper class for generating P25 sync data.
     * @ingroup p25
     */
    class HOST_SW_API Sync {
    public:
        /**
         * @brief Helper to append P25 sync bytes to the passed buffer.
         * @param data Buffer to append P25 sync bytes to.
         */
        static void addP25Sync(uint8_t* data);
    };
} // namespace p25

#endif // __P25_SYNC_H__
