// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017,2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file NID.h
 * @ingroup p25
 * @file NID.cpp
 * @ingroup p25
 */
#if !defined(__P25_NID_H__)
#define  __P25_NID_H__

#include "common/Defines.h"
#include "common/p25/P25Defines.h"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents the P25 network identifier.
     * @ingroup p25
     */
    class HOST_SW_API NID {
    public:
        /**
         * @brief Initializes a new instance of the NID class.
         * @param nac Network Access Code.
         */
        NID(uint32_t nac);
        /**
         * @brief Finalizes a instance of the NID class.
         */
        ~NID();

        /**
         * @brief Decodes P25 network identifier data.
         * @param data Buffer containing NID.
         * @returns bool True, if NID decoded, otherwise false.
         */
        bool decode(const uint8_t* data);
        /**
         * @brief Encodes P25 network identifier data.
         * @param data Buffer to encode NID.
         * @param duid Data Unit ID.
         */
        void encode(uint8_t* data, defines::DUID::E duid);

        /**
         * @brief Helper to configure a separate Tx NAC.
         * @param nac Transmit Network Access Code
         */
        void setTxNAC(uint32_t nac);

    public:
        /**
         * @brief Data unit ID.
         */
        __READONLY_PROPERTY(defines::DUID::E, duid, DUID);

    private:
        uint32_t m_nac;

        uint8_t** m_rxTx;
        uint8_t** m_tx;

        bool m_splitNac;

        /**
         * @brief Cleanup NID arrays.
         */
        void cleanupArrays();
        /**
         * @brief Internal helper to create the Rx/Tx NID.
         * @param nac Network Access Code
         */
        void createRxTxNID(uint32_t nac);
        /**
         * @brief Internal helper to create Tx NID.
         * @param nac Network Access Code
         */
        void createTxNID(uint32_t nac);
    };
} // namespace p25

#endif // __P25_NID_H__
