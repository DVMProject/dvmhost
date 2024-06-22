// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
*
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
        //      Implements NXDN Common Access Channel.
        // ---------------------------------------------------------------------------

        class HOST_SW_API CAC {
        public:
            /// <summary>Initializes a new instance of the CAC class.</summary>
            CAC();
            /// <summary>Initializes a copy instance of the CAC class.</summary>
            CAC(const CAC& data);
            /// <summary>Finalizes a instance of the CAC class.</summary>
            ~CAC();

            /// <summary>Equals operator.</summary>
            CAC& operator=(const CAC& data);

            /// <summary>Decode a common access channel.</summary>
            bool decode(const uint8_t* data, bool longInbound = false);
            /// <summary>Encode a common access channel.</summary>
            void encode(uint8_t* data) const;

            /// <summary>Gets the raw CAC data.</summary>
            void getData(uint8_t* data) const;
            /// <summary>Sets the raw CAC data.</summary>
            void setData(const uint8_t* data);

        public:
            /** Common Data */
            /// <summary>Radio Access Number</summary>
            __PROPERTY(uint8_t, ran, RAN);
            /// <summary></summary>
            __PROPERTY(defines::ChStructure::E, structure, Structure);
            /// <summary></summary>
            __PROPERTY(bool, longInbound, LongInbound);

            /** Collision Control Field */
            /// <summary>Idle/Busy.</summary>
            __PROPERTY(bool, idleBusy, IdleBusy);
            /// <summary>Tx Continuously.</summary>
            __PROPERTY(bool, txContinuous, TxContinuous);
            /// <summary>Receive/No Receive.</summary>
            __PROPERTY(bool, receive, Receive);

        private:
            uint8_t* m_data;
            uint16_t m_rxCRC;

            /// <summary>Internal helper to copy the class.</summary>
            void copy(const CAC& data);
        };
    } // namespace channel
} // namespace nxdn

#endif // __NXDN_CHANNEL__CAC_H__
