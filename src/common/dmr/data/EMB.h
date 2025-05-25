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
 * @file EMB.h
 * @ingroup dmr
 * @file EMB.cpp
 * @ingroup dmr
 */
#if !defined(__DMR_DATA__EMB_H__)
#define __DMR_DATA__EMB_H__

#include "common/Defines.h"

namespace dmr
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents a DMR embedded signalling data.
         * @ingroup dmr
         */
        class HOST_SW_API EMB {
        public:
            /**
             * @brief Initializes a new instance of the EMB class.
             */
            EMB();
            /**
             * @brief Finalizes a instance of the EMB class.
             */
            ~EMB();

            /**
             * @brief Decodes DMR embedded signalling data.
             * @param[in] data Buffer containing embedded signalling data.
             */
            void decode(const uint8_t* data);
            /**
             * @brief Encodes DMR embedded signalling data.
             * @param[out] data Buffer to encode embedded signalling data.
             */
            void encode(uint8_t* data) const;

        public:
            /**
             * @brief DMR access color code.
             */
            DECLARE_PROPERTY(uint8_t, colorCode, ColorCode);

            /**
             * @brief Flag indicating whether the privacy indicator is set or not.
             */
            DECLARE_PROPERTY(bool, PI, PI);

            /**
             * @brief Link control start/stop.
             */
            DECLARE_PROPERTY(uint8_t, LCSS, LCSS);
        };
    } // namespace data
} // namespace dmr

#endif // __DMR_DATA__EMB_H__
