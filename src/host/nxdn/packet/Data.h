// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2015-2020 Jonathan Naylor, G4KLX
*   Copyright (C) 2022-2024 Bryan Biedenkapp, N2PLL
*
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
        //      This class implements handling logic for NXDN data packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API Data {
        public:
            /// <summary>Resets the data states for the RF interface.</summary>
            void resetRF();
            /// <summary>Resets the data states for the network.</summary>
            void resetNet();

            /// <summary>Process a data frame from the RF interface.</summary>
            bool process(defines::ChOption::E option, uint8_t* data, uint32_t len);
            /// <summary>Process a data frame from the network.</summary>
            bool processNetwork(defines::ChOption::E option, lc::RTCH& netLC, uint8_t* data, uint32_t len);

        protected:
            friend class nxdn::Control;
            Control* m_nxdn;

            uint16_t m_lastRejectId;

            bool m_verbose;
            bool m_debug;

            /// <summary>Initializes a new instance of the Data class.</summary>
            Data(Control* nxdn, bool debug, bool verbose);
            /// <summary>Finalizes a instance of the Data class.</summary>
            ~Data();

            /// <summary>Write data processed from RF to the network.</summary>
            void writeNetwork(const uint8_t* data, uint32_t len);
        };
    } // namespace packet
} // namespace nxdn

#endif // __NXDN_PACKET_DATA_H__
