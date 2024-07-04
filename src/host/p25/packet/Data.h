// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file Data.h
 * @ingroup host_p25
 * @file Data.cpp
 * @ingroup host_p25
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
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements handling logic for P25 data packets.
         * @ingroup host_p25
         */
        class HOST_SW_API Data {
        public:
            /**
             * @brief Resets the data states for the RF interface.
             */
            void resetRF();

            /** @name Frame Processing */
            /**
             * @brief Process a data frame from the RF interface.
             * @param data Buffer containing data frame.
             * @param len Length of data frame.
             * @returns bool True, if data frame is processed, otherwise false.
             */
            bool process(uint8_t* data, uint32_t len);
            /**
             * @brief Process a data frame from the network.
             * @param data Buffer containing data frame.
             * @param len Length of data frame.
             * @param blockLength 
             * @returns bool True, if data frame is processed, otherwise false.
             */
            bool processNetwork(uint8_t* data, uint32_t len, uint32_t blockLength);
            /** @} */

            /**
             * @brief Helper to check if a logical link ID has registered with data services.
             * @param llId Logical Link ID.
             * @returns bool True, if ID has registered, otherwise false.
             */
            bool hasLLIdFNEReg(uint32_t llId) const;

            /**
             * @brief Helper to write user data as a P25 PDU packet.
             * @param dataHeader Instance of a PDU data header.
             * @param secondHeader Instance of a PDU data header.
             * @param useSecondHeader Flag indicating whether or not to use a second data header.
             * @param pduUserData Buffer containing user data to transmit.
             * @param imm Flag indicating the PDU should be written to the immediate queue.
             */
            void writeRF_PDU_User(data::DataHeader& dataHeader, data::DataHeader& secondHeader, bool useSecondHeader, uint8_t* pduUserData, bool imm = false);

            /**
             * @brief Updates the processor by the passed number of milliseconds.
             * @param ms Number of milliseconds.
             */
            void clock(uint32_t ms);

            /** @name SNDCP Helper Routines */
            /**
             * @brief Helper to initialize the SNDCP state for a logical link ID.
             * @param llId Logical Link ID.
             */
            void sndcpInitialize(uint32_t llId);
            /**
             * @brief Helper to determine if the logical link ID has been SNDCP initialized.
             * @param llId Logical Link ID.
             */
            bool isSNDCPInitialized(uint32_t llId) const;
            /**
             * @brief Helper to reset the SNDCP state for a logical link ID.
             * @param llId Logical Link ID.
             * @param callTerm Flag indicating call termination should be transmitted.
             */
            void sndcpReset(uint32_t llId, bool callTerm = false);
            /** @} */

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

            /**
             * @brief Initializes a new instance of the Data class.
             * @param p25 Instance of the Control class.
             * @param dumpPDUData Flag indicating whether PDU data is dumped to the log.
             * @param repeatPDU Flag indicating whether incoming PDUs will be repeated automatically.
             * @param debug Flag indicating whether P25 debug is enabled.
             * @param verbose Flag indicating whether P25 verbose logging is enabled.
             */
            Data(Control* p25, bool dumpPDUData, bool repeatPDU, bool debug, bool verbose);
            /**
             * @brief Finalizes a instance of the Data class.
             */
            ~Data();

            /**
             * @brief Helper used to process SNDCP control data from PDU data.
             * @returns bool True, if SNDCP control data was processed, otherwise false.
             */
            bool processSNDCPControl();

            /**
             * @brief Write data processed from RF to the network.
             * @param currentBlock Current Block ID.
             * @param data Buffer containing block data.
             * @param len Length of buffer.
             * @param lastBlock Flag indicating whether or not this is the last block.
             */
            void writeNetwork(const uint8_t currentBlock, const uint8_t* data, uint32_t len, bool lastBlock);

            /**
             * @brief Helper to write a P25 PDU packet.
             * @param[in] pdu Constructed PDU to transmit.
             * @param bitlength Length of PDU in bits.
             * @param noNulls Flag indicating no trailing nulls should be transmitted.
             * @param imm Flag indicating the PDU should be written to the immediate queue.
             */
            void writeRF_PDU(const uint8_t* pdu, uint32_t bitLength, bool noNulls = false, bool imm = false);
            /**
             * @brief Helper to write a network P25 PDU packet.
             * This will take buffered network PDU data and repeat it over the air.
             */
            void writeNet_PDU_Buffered();
            /**
             * @brief Helper to re-write a received P25 PDU packet.
             * This will take buffered received PDU data and repeat it over the air.
             */
            void writeRF_PDU_Buffered();
            /**
             * @brief Helper to write a PDU registration response.
             * @param regType Registration Response.
             * @param mfId Manufacturer ID.
             * @param llId Logical Link ID.
             * @param ipAddr 
             */
            void writeRF_PDU_Reg_Response(uint8_t regType, uint8_t mfId, uint32_t llId, uint32_t ipAddr);
            /**
             * @brief Helper to write a PDU acknowledge response.
             * @param ackClass Acknowledgement Class.
             * @param ackType Acknowledgement Type.
             * @param ackStatus 
             * @param llId Logical Link ID.
             * @param srcLlId Source Logical Link ID.
             * @param noNulls Flag indicating no trailing nulls should be transmitted.
             */
            void writeRF_PDU_Ack_Response(uint8_t ackClass, uint8_t ackType, uint8_t ackStatus, uint32_t llId, uint32_t srcLlId = 0U, bool noNulls = false);
        };
    } // namespace packet
} // namespace p25

#endif // __P25_PACKET_DATA_H__
