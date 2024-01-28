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
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__NXDN_PACKET_VOICE_H__)
#define __NXDN_PACKET_VOICE_H__

#include "Defines.h"
#include "nxdn/Control.h"

#include <cstdio>
#include <string>

namespace nxdn
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    namespace packet { class HOST_SW_API Data; }
    class HOST_SW_API Control;

    namespace packet
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      This class implements handling logic for NXDN voice packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API Voice {
        public:
            /// <summary>Resets the data states for the RF interface.</summary>
            void resetRF();
            /// <summary>Resets the data states for the network.</summary>
            void resetNet();

            /// <summary>Process a data frame from the RF interface.</summary>
            bool process(uint8_t fct, uint8_t option, uint8_t* data, uint32_t len);
            /// <summary>Process a data frame from the network.</summary>
            bool processNetwork(uint8_t fct, uint8_t option, lc::RTCH& netLC, uint8_t* data, uint32_t len);

        protected:
            friend class packet::Data;
            friend class nxdn::Control;
            Control* m_nxdn;

            uint32_t m_rfFrames;
            uint32_t m_rfBits;
            uint32_t m_rfErrs;
            uint32_t m_rfUndecodableLC;
            uint32_t m_netFrames;
            uint32_t m_netLost;

            uint16_t m_lastRejectId;

            uint32_t m_silenceThreshold;

            bool m_verbose;
            bool m_debug;

            /// <summary>Initializes a new instance of the Voice class.</summary>
            Voice(Control* nxdn, bool debug, bool verbose);
            /// <summary>Finalizes a instance of the Voice class.</summary>
            ~Voice();

            /// <summary>Write data processed from RF to the network.</summary>
            void writeNetwork(const uint8_t* data, uint32_t len);
        };
    } // namespace packet
} // namespace nxdn

#endif // __NXDN_PACKET_VOICE_H__
