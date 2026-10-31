// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file KMMChangeRSI.h
 * @ingroup p25_kmm
 * @file KMMChangeRSI.cpp
 * @ingroup p25_kmm
 */
#if !defined(__P25_KMM__KMM_CHANGE_RSI_H__)
#define __P25_KMM__KMM_CHANGE_RSI_H__

#include "common/Defines.h"
#include "common/p25/kmm/KMMFrame.h"
#include "common/Utils.h"

#include <string>

namespace p25
{
    namespace kmm
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents a KMM Change-RSI command or response.
         * @ingroup p25_kmm
         */
        class DVM_COMMON_API KMMChangeRSI : public KMMFrame {
        public:
            /**
             * @brief Initializes a new instance of the KMMChangeRSI class.
             */
            KMMChangeRSI();
            /**
             * @brief Finalizes a instance of the KMMChangeRSI class.
             */
            ~KMMChangeRSI();

            /**
             * @brief Gets the byte length of this KMM frame.
             * @returns uint32_t Length of this KMM frame.
             */
            uint32_t length() const override;

            /**
             * @brief Decode a KMM Change-RSI command or response.
             * @param data Buffer containing KMM frame data.
             * @returns bool True, if KMM frame decoded; otherwise, false.
             */
            bool decode(const uint8_t* data) override;
            /**
             * @brief Encode a KMM Change-RSI command or response.
             * @param data Buffer to encode KMM frame data to.
             */
            void encode(uint8_t* data) override;

            /**
             * @brief Returns a string that represents the current KMM frame.
             * @returns std::string String representation of the KMM frame.
             */
            std::string toString() override;

        public:
            /**
             * @brief Old RSI. 
             */
            DECLARE_PROPERTY(uint32_t, oldRSI, OldRSI);
            /** 
             * @brief New RSI. 
             */
            DECLARE_PROPERTY(uint32_t, newRSI, NewRSI);
            /** 
             * @brief Initial message number.
             */
            DECLARE_PROPERTY(uint16_t, initialMessageNumber, InitialMessageNumber);
            /** 
             * @brief Command execution status.
             */
            DECLARE_PROPERTY(uint8_t, status, Status);

            DECLARE_COPY(KMMChangeRSI);
        };
    } // namespace kmm
} // namespace p25

#endif // __P25_KMM__KMM_CHANGE_RSI_H__
