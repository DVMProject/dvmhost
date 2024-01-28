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
*   Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DMR_PACKET_DATA_H__)
#define __DMR_PACKET_DATA_H__

#include "Defines.h"
#include "common/dmr/data/Data.h"
#include "common/dmr/data/DataHeader.h"
#include "common/dmr/data/EmbeddedData.h"
#include "common/dmr/lc/LC.h"
#include "common/edac/AMBEFEC.h"
#include "common/network/BaseNetwork.h"
#include "common/RingBuffer.h"
#include "common/StopWatch.h"
#include "common/Timer.h"
#include "modem/Modem.h"

#include <vector>

namespace dmr
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    namespace packet { class HOST_SW_API Voice; }
    namespace packet { class HOST_SW_API ControlSignaling; }
    class HOST_SW_API Slot;

    namespace packet
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      This class implements core logic for handling DMR data packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API Data {
        public:
            /// <summary>Process a data frame from the RF interface.</summary>
            bool process(uint8_t* data, uint32_t len);
            /// <summary>Process a data frame from the network.</summary>
            void processNetwork(const data::Data& dmrData);

        private:
            friend class packet::Voice;
            friend class packet::ControlSignaling;
            friend class dmr::Slot;
            Slot* m_slot;

            uint8_t* m_pduUserData;
            uint32_t m_pduDataOffset;
            uint32_t m_lastRejectId;

            bool m_dumpDataPacket;
            bool m_repeatDataPacket;

            bool m_verbose;
            bool m_debug;

            /// <summary>Initializes a new instance of the Data class.</summary>
            Data(Slot* slot, network::BaseNetwork* network, bool dumpDataPacket, bool repeatDataPacket, bool debug, bool verbose);
            /// <summary>Finalizes a instance of the Data class.</summary>
            ~Data();
        };
    } // namespace packet
} // namespace dmr

#endif // __DMR_PACKET_DATA_H__
