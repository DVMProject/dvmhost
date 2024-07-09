// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file Data.h
 * @ingroup host_dmr
 * @file Data.cpp
 * @ingroup host_dmr
 */
#if !defined(__DMR_PACKET_DATA_H__)
#define __DMR_PACKET_DATA_H__

#include "Defines.h"
#include "common/dmr/data/NetData.h"
#include "common/dmr/data/DataHeader.h"
#include "common/dmr/data/DataBlock.h"
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
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements core logic for handling DMR data packets.
         * @ingroup host_dmr
         */
        class HOST_SW_API Data {
        public:
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
             * @param[in] data Instance of data::NetData DMR data container class.
             * @returns bool True, if data frame is processed, otherwise false.
             */
            void processNetwork(const data::NetData& dmrData);
            /** @} */

        private:
            friend class packet::Voice;
            friend class packet::ControlSignaling;
            friend class dmr::Slot;
            Slot* m_slot;

            data::DataHeader m_rfDataHeader;
            uint8_t m_rfDataBlockCnt;

            data::DataHeader m_netDataHeader;
            uint8_t m_netDataBlockCnt;

            uint8_t* m_pduUserData;
            uint32_t m_pduDataOffset;
            uint32_t m_lastRejectId;

            bool m_dumpDataPacket;
            bool m_repeatDataPacket;

            bool m_verbose;
            bool m_debug;

            /**
             * @brief Initializes a new instance of the Data class.
             * @param slot DMR slot.
             * @param network Instance of the BaseNetwork class.
             * @param dumpDataPacket Flag indicating whether data packets are dumped to the log.
             * @param repeatDataPacket Flag indicating whether incoming data packets will be repeated automatically.
             * @param debug Flag indicating whether DMR debug is enabled.
             * @param verbose Flag indicating whether DMR verbose logging is enabled.
             */
            Data(Slot* slot, network::BaseNetwork* network, bool dumpDataPacket, bool repeatDataPacket, bool debug, bool verbose);
            /**
             * @brief Finalizes a instance of the Data class.
             */
            ~Data();

            /**
             * @brief Helper to write a DMR PDU packet.
             * @param dataType DMR Data Type for this frame.
             * @param[in] pdu Buffer containing encoded PDU to transmit.
             */
            void writeRF_PDU(defines::DataType::E dataType, const uint8_t* pdu);

            /**
             * @brief Helper to write a PDU acknowledge response.
             * @param rspClass Response Class.
             * @param rspType Response Type.
             * @param rspStatus Response Status.
             * @param sap Service access point.
             * @param gi Flag indicating group or individual.
             * @param srcId Source ID.
             * @param dstId Destination ID.
             */
            void writeRF_PDU_Ack_Response(uint8_t rspClass, uint8_t rspType, uint8_t sap, uint8_t rspStatus, bool gi, uint32_t srcId, uint32_t dstId);
        };
    } // namespace packet
} // namespace dmr

#endif // __DMR_PACKET_DATA_H__
