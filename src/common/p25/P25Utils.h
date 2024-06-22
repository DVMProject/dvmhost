// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2016 Jonathan Naylor, G4KLX
*   Copyright (C) 2021 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__P25_UTILS_H__)
#define __P25_UTILS_H__

#include "common/Defines.h"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements various helper functions for validating and
    //      for interleaving P25 data.
    // ---------------------------------------------------------------------------

    class HOST_SW_API P25Utils {
    public:
        /// <summary>Helper to test and clamp a P25 NAC.</summary>
        /// <param name="nac">Network Access Code</param>
        /// <returns>Clamped network access code.</returns>
        static uint32_t nac(uint32_t nac)
        {
            if (nac < 0U) { // clamp to $000
                nac = 0U;
            }
            if (nac > 0xF7FU) { // clamp to $F7F
                nac = 0xF7FU;
            }

            return nac;
        }

        /// <summary>Helper to test and clamp a P25 site ID.</summary>
        /// <param name="id">Site ID</param>
        /// <returns>Clamped site ID.</returns>
        static uint8_t siteId(uint8_t id)
        {
            if (id == 0U) { // clamp to 1
                id = 1U;
            }
            if (id > 0xFEU) { // clamp to $FE
                id = 0xFEU;
            }

            return id;
        }

        /// <summary>Helper to test and clamp a P25 network ID.</summary>
        /// <param name="id">Network ID</param>
        /// <returns>Clamped network ID.</returns>
        static uint32_t netId(uint32_t id)
        {
            if (id == 0U) { // clamp to 1
                id = 1U;
            }
            if (id > 0xFFFFEU) { // clamp to $FFFFE
                id = 0xFFFFEU;
            }

            return id;
        }

        /// <summary>Helper to test and clamp a P25 system ID.</summary>
        /// <param name="id">System ID</param>
        /// <returns>Clamped system ID.</returns>
        static uint32_t sysId(uint32_t id)
        {
            if (id == 0U) { // clamp to 1
                id = 1U;
            }
            if (id > 0xFFEU) { // clamp to $FFE
                id = 0xFFEU;
            }

            return id;
        }

        /// <summary>Helper to test and clamp a P25 RFSS ID.</summary>
        /// <param name="id">RFSS ID</param>
        /// <returns>Clamped RFSS ID.</returns>
        static uint8_t rfssId(uint8_t id)
        {
            if (id == 0U) { // clamp to 1
                id = 1U;
            }
            if (id > 0xFEU) { // clamp to $FE
                id = 0xFEU;
            }

            return id;
        }

        /// <summary>Helper to set the busy status bits on P25 frame data.</summary>
        static void setBusyBits(uint8_t* data, uint32_t ssOffset, bool b1, bool b2);
        /// <summary>Helper to add the busy status bits on P25 frame data.</summary>
        static void addBusyBits(uint8_t* data, uint32_t length, bool b1, bool b2);
        /// <summary>Helper to add the idle status bits on P25 frame data.</summary>
        static void addIdleBits(uint8_t* data, uint32_t length, bool b1, bool b2);

        /// <summary>Decode bit interleaving.</summary>
        static uint32_t decode(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop);
        /// <summary>Encode bit interleaving.</summary>
        static uint32_t encode(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop);
        /// <summary>Encode bit interleaving for a given length.</summary>
        static uint32_t encode(const uint8_t* in, uint8_t* out, uint32_t length);

        /// <summary>Compare two datasets for the given length.</summary>
        static uint32_t compare(const uint8_t* data1, const uint8_t* data2, uint32_t length);
    };
} // namespace p25

#endif // __P25_UTILS_H__
