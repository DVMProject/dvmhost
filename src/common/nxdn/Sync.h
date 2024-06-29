// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2020 Jonathan Naylor, G4KLX
 *
 */
/**
 * @file Sync.h
 * @ingroup nxdn
 * @file Sync.cpp
 * @ingroup nxdn
 */
#if !defined(__NXDN_SYNC_H__)
#define __NXDN_SYNC_H__

#include "common/Defines.h"

namespace nxdn
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Helper class for generating NXDN sync data.
     * @ingroup nxdn
     */
    class HOST_SW_API Sync {
    public:
        /**
         * @brief Helper to append NXDN sync bytes to the passed buffer.
         * @param data Buffer to append P25 sync bytes to.
         */
        static void addNXDNSync(uint8_t* data);
    };
} // namespace nxdn

#endif // __NXDN_SYNC_H__
