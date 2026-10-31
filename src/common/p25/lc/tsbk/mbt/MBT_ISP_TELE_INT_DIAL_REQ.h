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
 * @file MBT_ISP_TELE_INT_DIAL_REQ.h
 * @ingroup p25_ambt
 * @file MBT_ISP_TELE_INT_DIAL_REQ.cpp
 * @ingroup p25_ambt
 */
#if !defined(__P25_LC_TSBK__MBT_ISP_TELE_INT_DIAL_REQ_H__)
#define  __P25_LC_TSBK__MBT_ISP_TELE_INT_DIAL_REQ_H__

#include "common/Defines.h"
#include "common/p25/lc/AMBT.h"

namespace p25
{
    namespace lc
    {
        namespace tsbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Implements TELE INT DIAL REQ - Telephone Interconnect Request,
             *        Explicit Dialing (standard unconfirmed MBT format).
             * @ingroup p25_ambt
             */
            class DVM_COMMON_API MBT_ISP_TELE_INT_DIAL_REQ : public AMBT {
            public:
                /**
                 * @brief Initializes a new instance of the MBT_ISP_TELE_INT_DIAL_REQ class.
                 */
                MBT_ISP_TELE_INT_DIAL_REQ();

                /**
                 * @brief Decode a standard multi-block trunking signalling block.
                 * @param[in] dataHeader P25 PDU data header
                 * @param[in] blocks P25 PDU data blocks
                 * @returns bool True, if MBT decoded, otherwise false.
                 */
                bool decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks) override;
                /**
                 * @brief Encode a standard multi-block trunking signalling block.
                 * @param[out] dataHeader P25 PDU data header
                 * @param[out] pduUserData P25 PDU user data
                 */
                void encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData) override;

                /**
                 * @brief Returns a string that represents the current MBT.
                 * @returns std::string String representation of the MBT.
                 */
                std::string toString(bool isp = false) override;

                /**
                 * @brief Copies the encoded four-bit dial digit values to the supplied buffer.
                 * @param[out] digits Buffer containing dial digit values.
                 * @returns uint8_t Number of dial digit values copied.
                 */
                uint8_t getDialDigits(uint8_t* digits) const;
                /**
                 * @brief Sets encoded four-bit dial digit values.
                 * @param[in] digits Buffer containing dial digit values.
                 * @param count Number of dial digit values.
                 * @returns bool True, if the digit values were set, otherwise false.
                 */
                bool setDialDigits(const uint8_t* digits, uint8_t count);

            public:
                /**
                 * @brief Number of dial digit values.
                 */
                DECLARE_PROTECTED_RO_PROPERTY(uint8_t, digitCount, DigitCount);
                /**
                 * @brief Flag indicating full-duplex service.
                 */
                DECLARE_PROPERTY(bool, duplex, Duplex);
                /**
                 * @brief Flag indicating packet-mode service.
                 */
                DECLARE_PROPERTY(bool, packetMode, PacketMode);

            private:
                static const uint8_t MAX_DIGITS = 34U;
                uint8_t m_digits[MAX_DIGITS];
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__MBT_ISP_TELE_INT_DIAL_REQ_H__
