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
*   Copyright (C) 2016,2017 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__P25_PACKET_DATA_H__)
#define __P25_PACKET_DATA_H__

#include "Defines.h"
#include "common/p25/data/DataBlock.h"
#include "common/p25/data/DataHeader.h"
#include "common/p25/data/LowSpeedData.h"
#include "common/p25/lc/LC.h"
#include "common/Timer.h"
#include "p25/Control.h"

#include <cstdio>
#include <string>
#include <unordered_map>

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    class HOST_SW_API Control;
    namespace packet { class HOST_SW_API Trunk; }

    namespace packet
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      This class implements handling logic for P25 data packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API Data {
        public:
            /// <summary>Resets the data states for the RF interface.</summary>
            void resetRF();

            /// <summary>Process a data frame from the RF interface.</summary>
            bool process(uint8_t* data, uint32_t len);
            /// <summary>Process a data frame from the network.</summary>
            bool processNetwork(uint8_t* data, uint32_t len, uint32_t blockLength);

            /// <summary>Helper to check if a logical link ID has registered with data services.</summary>
            bool hasLLIdFNEReg(uint32_t llId) const;

            /// <summary>Helper to write user data as a P25 PDU packet.</summary>
            void writeRF_PDU_User(data::DataHeader& dataHeader, data::DataHeader& secondHeader, bool useSecondHeader, uint8_t* pduUserData);

            /// <summary>Updates the processor by the passed number of milliseconds.</summary>
            void clock(uint32_t ms);

            /** SNDCP */
            /// <summary>Helper to initialize the SNDCP state for a logical link ID.</summary>
            void sndcpInitialize(uint32_t llId);
            /// <summary>Helper to determine if the logical link ID has been SNDCP initialized.</summary>
            bool isSNDCPInitialized(uint32_t llId) const;
            /// <summary>Helper to reset the SNDCP state for a logical link ID.</summary>
            void sndcpReset(uint32_t llId, bool callTerm = false);

        private:
            friend class p25::Control;
            Control* m_p25;

            RPT_RF_STATE m_prevRfState;

            data::DataBlock* m_rfData;
            data::DataHeader m_rfDataHeader;
            data::DataHeader m_rfSecondHeader;
            bool m_rfUseSecondHeader;
            bool m_rfExtendedAddress;
            uint8_t m_rfDataBlockCnt;
            uint8_t* m_rfPDU;
            uint32_t m_rfPDUCount;
            uint32_t m_rfPDUBits;

            data::DataBlock* m_netData;
            data::DataHeader m_netDataHeader;
            data::DataHeader m_netSecondHeader;
            bool m_netUseSecondHeader;
            bool m_netExtendedAddress;
            uint32_t m_netDataOffset;
            uint8_t m_netDataBlockCnt;
            uint8_t* m_netPDU;
            uint32_t m_netPDUCount;

            uint8_t* m_pduUserData;
            uint32_t m_pduUserDataLength;

            std::unordered_map<uint32_t, ulong64_t> m_fneRegTable;

            std::unordered_map<uint32_t, std::tuple<uint8_t, ulong64_t>> m_connQueueTable;
            std::unordered_map<uint32_t, Timer> m_connTimerTable;

            std::unordered_map<uint32_t, defines::SNDCPState::E> m_sndcpStateTable;
            std::unordered_map<uint32_t, Timer> m_sndcpReadyTimers;
            std::unordered_map<uint32_t, Timer> m_sndcpStandbyTimers;

            bool m_dumpPDUData;
            bool m_repeatPDU;

            bool m_verbose;
            bool m_debug;

            /// <summary>Initializes a new instance of the Data class.</summary>
            Data(Control* p25, bool dumpPDUData, bool repeatPDU, bool debug, bool verbose);
            /// <summary>Finalizes a instance of the Data class.</summary>
            ~Data();

            /// <summary>Helper used to process SNDCP control data from PDU data.</summary>
            bool processSNDCPControl();

            /// <summary>Write data processed from RF to the network.</summary>
            void writeNetwork(const uint8_t currentBlock, const uint8_t* data, uint32_t len, bool lastBlock);

            /// <summary>Helper to write a P25 PDU packet.</summary>
            void writeRF_PDU(const uint8_t* pdu, uint32_t bitLength, bool noNulls = false);
            /// <summary>Helper to write a network P25 PDU packet.</summary>
            void writeNet_PDU_Buffered();
            /// <summary>Helper to re-write a received P25 PDU packet.</summary>
            void writeRF_PDU_Buffered();
            /// <summary>Helper to write a PDU registration response.</summary>
            void writeRF_PDU_Reg_Response(uint8_t regType, uint8_t mfId, uint32_t llId, ulong64_t ipAddr);
            /// <summary>Helper to write a PDU acknowledge response.</summary>
            void writeRF_PDU_Ack_Response(uint8_t ackClass, uint8_t ackType, uint8_t ackStatus, uint32_t llId, uint32_t srcLlId = 0U, bool noNulls = false);
        };
    } // namespace packet
} // namespace p25

#endif // __P25_PACKET_DATA_H__
