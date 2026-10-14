// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
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
            /**
             * @brief Implements TELE INT DIAL REQ - Telephone Interconnect Request,
             *        Explicit Dialing (standard unconfirmed MBT format).
             * @ingroup p25_ambt
             */
            class DVM_COMMON_API MBT_ISP_TELE_INT_DIAL_REQ : public AMBT {
            public:
                MBT_ISP_TELE_INT_DIAL_REQ();

                bool decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks) override;
                void encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData) override;

                std::string toString(bool isp = false) override;

                /** @brief Copies the encoded four-bit dial digit values to the supplied buffer. */
                uint8_t getDialDigits(uint8_t* digits) const;
                /** @brief Sets encoded four-bit dial digit values. */
                bool setDialDigits(const uint8_t* digits, uint8_t count);

            public:
                DECLARE_PROTECTED_RO_PROPERTY(uint8_t, digitCount, DigitCount);
                DECLARE_PROPERTY(bool, duplex, Duplex);
                DECLARE_PROPERTY(bool, packetMode, PacketMode);

            private:
                static const uint8_t MAX_DIGITS = 34U;
                uint8_t m_digits[MAX_DIGITS];
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__MBT_ISP_TELE_INT_DIAL_REQ_H__
