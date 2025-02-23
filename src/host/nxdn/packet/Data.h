// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015-2020 Jonathan Naylor, G4KLX
 *  Copyright (C) 2022-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file Data.h
 * @ingroup host_nxdn
 * @file Data.cpp
 * @ingroup host_nxdn
 */
#if !defined(__NXDN_PACKET_DATA_H__)
#define __NXDN_PACKET_DATA_H__

#include "Defines.h"
#include "nxdn/Control.h"

#include <cstdio>
#include <string>

namespace nxdn
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    class HOST_SW_API Control;

    namespace packet
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements handling logic for NXDN data packets.
         * @ingroup host_nxdn
         */
        class HOST_SW_API Data {
        public:
            /**
             * @brief Resets the data states for the RF interface.
             */
            void resetRF();
            /**
             * @brief Resets the data states for the network.
             */
            void resetNet();

            /** @name Frame Processing */
            /**
             * @brief Process a data frame from the RF interface.
             * @param option Channel options.
             * @param data Buffer containing data frame.
             * @param len Length of data frame.
             * @returns bool True, if data frame is processed, otherwise false.
             */
            bool process(defines::ChOption::E option, uint8_t* data, uint32_t len);
            /**
             * @brief Process a data frame from the network.
             * @param option Channel options.
             * @param netLC RTCH from the network.
             * @param data Buffer containing data frame.
             * @param len Length of data frame.
             * @returns bool True, if data frame is processed, otherwise false.
             */
            bool processNetwork(defines::ChOption::E option, lc::RTCH& netLC, uint8_t* data, uint32_t len);
            /** @} */

        protected:
            friend class nxdn::Control;
            Control* m_nxdn;

            uint16_t m_lastRejectId;

            bool m_verbose;
            bool m_debug;

            /**
             * @brief Initializes a new instance of the Data class.
             * @param nxdn Instance of the Control class.
             * @param debug Flag indicating whether NXDN debug is enabled.
             * @param verbose Flag indicating whether NXDN verbose logging is enabled.
             */
            Data(Control* nxdn, bool debug, bool verbose);
            /**
             * @brief Finalizes a instance of the Data class.
             */
            ~Data();

            /**
             * @brief Write data processed from RF to the network.
             * @param[in] data Buffer to write to the network.
             * @param len Length of data frame.
             */
            void writeNetwork(const uint8_t* data, uint32_t len);
        };
    } // namespace packet
} // namespace nxdn

#endif // __NXDN_PACKET_DATA_H__
