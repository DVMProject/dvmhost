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
 * @file CAC.h
 * @ingroup nxdn_ch
 * @file CAC.cpp
 * @ingroup nxdn_ch
 */
#if !defined(__NXDN_CHANNEL__CAC_H__)
#define  __NXDN_CHANNEL__CAC_H__

#include "common/Defines.h"
#include "common/nxdn/NXDNDefines.h"

namespace nxdn
{
    namespace channel
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements NXDN Common Access Channel.
         * @ingroup nxdn_ch
         */
        class HOST_SW_API CAC {
        public:
            /**
             * @brief Initializes a new instance of the CAC class.
             */
            CAC();
            /**
             * @brief Initializes a copy instance of the CAC class.
             * @param data Instance of CAC to copy.
             */
            CAC(const CAC& data);
            /**
             * @brief Finalizes a instance of the CAC class.
             */
            ~CAC();

            /**
             * @brief Equals operator.
             * @param data Instance of CAC to copy.
             */
            CAC& operator=(const CAC& data);

            /**
             * @brief Decode a common access channel.
             * @param[in] data Buffer containing CAC to decode.
             * @param longInbound Flag indicating whether the CAC is short (false) or long (true).
             * @returns bool True, if CAC decoded, otherwise false.
             */
            bool decode(const uint8_t* data, bool longInbound = false);
            /**
             * @brief Encode a common access channel.
             * @param[out] data Buffer to encode CAC.
             */
            void encode(uint8_t* data) const;

            /**
             * @brief Gets the raw CAC data.
             * @param[out] data Buffer to copy raw CAC data to.
             */
            void getData(uint8_t* data) const;
            /**
             * @brief Sets the raw CAC data.
             * @param[in] data Buffer to copy raw CAC data from.
             */
            void setData(const uint8_t* data);

        public:
            // Common Data
            /**
             * @brief Radio Access Number
             */
            DECLARE_PROPERTY(uint8_t, ran, RAN);
            /**
             * @brief Channel Structure
             */
            DECLARE_PROPERTY(defines::ChStructure::E, structure, Structure);
            /**
             * @brief Flag indicating whether the inbound CAC is long or short.
             */
            DECLARE_PROPERTY(bool, longInbound, LongInbound);

            // Collision Control Field
            /**
             * @brief Idle/Busy.
             */
            DECLARE_PROPERTY(bool, idleBusy, IdleBusy);
            /**
             * @brief Tx Continuously.
             */
            DECLARE_PROPERTY(bool, txContinuous, TxContinuous);
            /**
             * @brief Receive/No Receive.
             */
            DECLARE_PROPERTY(bool, receive, Receive);

        private:
            uint8_t* m_data;
            uint16_t m_rxCRC;

            /**
             * @brief Internal helper to copy the class.
             * @param data Instance of CAC to copy.
             */
            void copy(const CAC& data);
        };
    } // namespace channel
} // namespace nxdn

#endif // __NXDN_CHANNEL__CAC_H__
