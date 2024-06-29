// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file LC.h
 * @ingroup p25_dfsi
 * @file LC.cpp
 * @ingroup p25_dfsi
 */
#if !defined(__P25_DFSI__LC_H__)
#define  __P25_DFSI__LC_H__

#include "common/Defines.h"
#include "common/p25/data/LowSpeedData.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/p25/lc/LC.h"
#include "common/edac/RS634717.h"

#include <string>

namespace p25
{
    namespace dfsi
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents link control data for DFSI VHDR, LDU1 and 2 packets.
         * @ingroup p25_dfsi
         */
        class HOST_SW_API LC {
        public:
            /**
             * @brief Initializes a new instance of the LC class.
             */
            LC();
            /**
             * @brief Initializes a copy instance of the LC class.
             * @param data Instance of LC class to copy from.
             */
            LC(const LC& data);
            /**
             * @brief Initializes a new instance of the LC class from OTA link control.
             * @param control 
             * @param lsd Instance of p25::data::LowSpeedData.
             */
            LC(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd);
            /**
             * @brief Finalizes a instance of the LC class.
             */
            ~LC();

            /**
             * @brief Equals operator.
             * @param data Instance of LC class to copy from.
             */
            LC& operator=(const LC& data);

            /**
             * @brief Helper to set the LC data.
             * @param data Instance of p25::lc::LC.
             */
            void setControl(const lc::LC& data);

            /**
             * @brief Decode a logical link data unit 1.
             * @param[in] data Buffer containing the LDU1 to decode.
             * @param imbe Raw IMBE from LDU1 frame.
             * @returns True, if LDU1 decoded, otherwise false.
             */
            bool decodeLDU1(const uint8_t* data, uint8_t* imbe);
            /**
             * @brief Encode a logical link data unit 1.
             * @param[out] data Buffer to encode an LDU1.
             * @param imbe Raw IMBE from LDU1 frame.
             */
            void encodeLDU1(uint8_t* data, const uint8_t* imbe);

            /**
             * @brief Decode a logical link data unit 2.
             * @param[in] data Buffer containing the LDU2 to decode.
             * @param imbe Raw IMBE from LDU2 frame.
             * @returns True, if LDU2 decoded, otherwise false.
             */
            bool decodeLDU2(const uint8_t* data, uint8_t* imbe);
            /**
             * @brief Encode a logical link data unit 2.
             * @param[out] data Buffer to encode an LDU2.
             * @param imbe Raw IMBE from LDU2 frame.
             */
            void encodeLDU2(uint8_t* data, const uint8_t* imbe);

        public:
            // Common Data
            /**
             * @brief Frame Type.
             */
            __PROPERTY(defines::DFSIFrameType::E, frameType, FrameType);

            /**
             * @brief RSSI.
             */
            __PROPERTY(uint8_t, rssi, RSSI);

            /**
             * @brief Link control data.
             */
            __READONLY_PROPERTY_PLAIN(p25::lc::LC*, control);
            /**
             * @brief Low speed data.
             */
            __READONLY_PROPERTY_PLAIN(p25::data::LowSpeedData*, lsd);

        private:
            edac::RS634717 m_rs;

            uint8_t* m_rsBuffer;

            // Encryption data
            uint8_t* m_mi;

            /**
             * @brief Internal helper to copy the class.
             * @param data Instance of LC class to copy from.
             */
            void copy(const LC& data);
        };
    } // namespace dfsi
} // namespace p25

#endif // __P25_DFSI__LC_H__
