/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2016,2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2022 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__P25_DATA_PACKET_H__)
#define __P25_DATA_PACKET_H__

#include "Defines.h"
#include "p25/data/DataBlock.h"
#include "p25/data/DataHeader.h"
#include "p25/data/LowSpeedData.h"
#include "p25/lc/LC.h"
#include "p25/Control.h"
#include "network/BaseNetwork.h"

#include <cstdio>
#include <string>
#include <unordered_map>

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------
    class HOST_SW_API Control;

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements handling logic for P25 data packets.
    // ---------------------------------------------------------------------------

    class HOST_SW_API DataPacket {
    public:
        /// <summary>Resets the data states for the RF interface.</summary>
        void resetRF();

        /// <summary>Process a data frame from the RF interface.</summary>
        bool process(uint8_t* data, uint32_t len);
        /// <summary>Process a data frame from the network.</summary>
        bool processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, uint8_t& duid);

        /// <summary>Helper to check if a logical link ID has registered with data services.</summary>
        bool hasLLIdFNEReg(uint32_t llId) const;

    private:
        friend class Control;
        Control* m_p25;

        network::BaseNetwork* m_network;

        RPT_RF_STATE m_prevRfState;

        data::DataBlock* m_rfData;
        data::DataHeader m_rfDataHeader;
        data::DataHeader m_rfSecondHeader;
        bool m_rfUseSecondHeader;
        uint8_t m_rfDataBlockCnt;
        uint8_t* m_rfPDU;
        uint32_t m_rfPDUCount;
        uint32_t m_rfPDUBits;

        data::DataBlock* m_netData;
        data::DataHeader m_netDataHeader;
        data::DataHeader m_netSecondHeader;
        bool m_netUseSecondHeader;
        uint32_t m_netDataOffset;
        uint8_t m_netDataBlockCnt;
        uint8_t* m_netPDU;
        uint32_t m_netPDUCount;

        uint8_t* m_pduUserData;
        uint32_t m_pduUserDataLength;

        std::unordered_map<uint32_t, ulong64_t> m_fneRegTable;

        bool m_dumpPDUData;
        bool m_repeatPDU;

        bool m_verbose;
        bool m_debug;

        /// <summary>Initializes a new instance of the DataPacket class.</summary>
        DataPacket(Control* p25, network::BaseNetwork* network, bool dumpPDUData, bool repeatPDU, bool debug, bool verbose);
        /// <summary>Finalizes a instance of the DataPacket class.</summary>
        ~DataPacket();

        /// <summary>Write data processed from RF to the network.</summary>
        void writeNetworkRF(const uint8_t currentBlock, const uint8_t* data, uint32_t len);

        /// <summary>Helper to write a P25 PDU packet.</summary>
        void writeRF_PDU(const uint8_t* pdu, uint32_t bitLength, bool noNulls = false);
        /// <summary>Helper to write a network P25 PDU packet.</summary>
        void writeNet_PDU_Buffered();
        /// <summary>Helper to re-write a received P25 PDU packet.</summary>
        void writeRF_PDU_Buffered();
        /// <summary>Helper to write a PDU registration response.</summary>
        void writeRF_PDU_Reg_Response(uint8_t regType, uint32_t llId, ulong64_t ipAddr);
        /// <summary>Helper to write a PDU acknowledge response.</summary>
        void writeRF_PDU_Ack_Response(uint8_t ackClass, uint8_t ackType, uint32_t llId);
    };
} // namespace p25

#endif // __P25_DATA_PACKET_H__
