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
 * @file ControlSignaling.h
 * @ingroup host_dmr
 * @file ControlSignaling.cpp
 * @ingroup host_dmr
 */
#if !defined(__DMR_PACKET_CONTROL_SIGNALING_H__)
#define __DMR_PACKET_CONTROL_SIGNALING_H__

#include "Defines.h"
#include "common/dmr/data/Data.h"
#include "common/dmr/data/EmbeddedData.h"
#include "common/dmr/lc/LC.h"
#include "common/dmr/lc/CSBK.h"
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

    class HOST_SW_API Slot;

    namespace packet
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements core logic for handling DMR control signaling (CSBK)
         *  packets.
         * @ingroup host_dmr
         */
        class HOST_SW_API ControlSignaling
        {
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
             * @param[in] data Instance of data::Data DMR data container class.
             * @returns bool True, if data frame is processed, otherwise false.
             */
            void processNetwork(const data::Data& dmrData);
            /** @} */

            /**
             * @brief Helper to write P25 adjacent site information to the network.
             */
            void writeAdjSSNetwork();

            /** @name Externally Callable CSBK Commands */
            /**
             * @brief Helper to write a extended function packet on the RF interface.
             */
            void writeRF_Ext_Func(uint32_t func, uint32_t arg, uint32_t dstId);
            /**
             * @brief Helper to write a call alert packet on the RF interface.
             */
            void writeRF_Call_Alrt(uint32_t srcId, uint32_t dstId);
            /** @} */

        private:
            friend class dmr::Slot;
            Slot* m_slot;

            bool m_dumpCSBKData;
            bool m_verbose;
            bool m_debug;

            /**
             * @brief Initializes a new instance of the ControlSignaling class.
             * @param slot DMR slot.
             * @param network Instance of the BaseNetwork class.
             * @param dumpCSBKData 
             * @param debug Flag indicating whether DMR debug is enabled.
             * @param verbose Flag indicating whether DMR verbose logging is enabled.
             */
            ControlSignaling(Slot* slot, network::BaseNetwork* network, bool dumpCSBKData, bool debug, bool verbose);
            /**
             * @brief Finalizes a instance of the ControlSignaling class.
             */
            ~ControlSignaling();

            /*
            ** Modem Frame Queuing
            */

            /**
             * @brief Helper to write a immediate CSBK packet.
             * @param csbk CSBK to write to the modem.
             */
            void writeRF_CSBK_Imm(lc::CSBK *csbk) { writeRF_CSBK(csbk, true); }
            /**
             * @brief Helper to write a CSBK packet.
             * @param csbk CSBK to write to the modem.
             * @param imm Flag indicating the TSBK should be written to the immediate queue.
             */
            void writeRF_CSBK(lc::CSBK* csbk, bool imm = false);
            /**
             * @brief Helper to write a network CSBK packet.
             * @param csbk CSBK to write to the network.
             */
            void writeNet_CSBK(lc::CSBK* csbk);

            /*
            ** Control Signalling Logic
            */

            /**
             * @brief Helper to write a ACK RSP packet.
             * @param dstId Destination ID.
             * @param reason Denial Reason.
             * @param responseInfo Response code.
             */
            void writeRF_CSBK_ACK_RSP(uint32_t dstId, uint8_t reason, uint8_t responseInfo);
            /**
             * @brief Helper to write a NACK RSP packet.
             * @param dstId Destination ID.
             * @param reason Denial Reason.
             * @param service Service being denied.
             */
            void writeRF_CSBK_NACK_RSP(uint32_t dstId, uint8_t reason, uint8_t service);
            /**
             * @brief Helper to write a grant packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param serviceOptions Service Options.
             * @param grp Flag indicating the destination ID is a talkgroup.
             * @param net Flag indicating this grant is coming from network traffic.
             * @param skip Flag indicating normal grant checking is skipped.
             * @param chNo Channel Number.
             */
            bool writeRF_CSBK_Grant(uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp, bool net = false, bool skip = false, uint32_t chNo = 0U);
            /**
             * @brief Helper to write a data grant packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param serviceOptions Service Options.
             * @param grp Flag indicating the destination ID is a talkgroup.
             * @param net Flag indicating this grant is coming from network traffic.
             * @param skip Flag indicating normal grant checking is skipped.
             * @param chNo Channel Number.
             */
            bool writeRF_CSBK_Data_Grant(uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp, bool net = false, bool skip = false, uint32_t chNo = 0U);
            /**
             * @brief Helper to write a unit registration response packet.
             * @param srcId Source Radio ID.
             * @param serviceOptions Service Options.
             */
            void writeRF_CSBK_U_Reg_Rsp(uint32_t srcId, uint8_t serviceOptions);

            /**
             * @brief Helper to write a TSCC late entry channel grant packet on the RF interface.
             * @param dstId Destination ID.
             * @param srcId Source Radio ID.
             * @param grp Flag indicating the destination ID is a talkgroup.
             */
            void writeRF_CSBK_Grant_LateEntry(uint32_t dstId, uint32_t srcId, bool grp);
            /**
             * @brief Helper to write a payload activation to a TSCC payload channel on the RF interface.
             * @param dstId Destination ID.
             * @param srcId Source Radio ID.
             * @param grp Flag indicating group grant.
             * @param voice Flag indicating if the payload traffic is voice.
             * @param imm Flag indicating the TSBK should be written to the immediate queue.
             */
            void writeRF_CSBK_Payload_Activate(uint32_t dstId, uint32_t srcId, bool grp, bool voice, bool imm = false);
            /**
             * @brief Helper to write a payload clear to a TSCC payload channel on the RF interface.
             * @param dstId Destination ID.
             * @param srcId Source Radio ID.
             * @param grp Flag indicating group grant.
             * @param imm Flag indicating the TSBK should be written to the immediate queue.
             */
            void writeRF_CSBK_Payload_Clear(uint32_t dstId, uint32_t srcId, bool grp, bool imm = false);

            /**
             * @brief Helper to write a TSCC Aloha broadcast packet on the RF interface.
             */
            void writeRF_TSCC_Aloha();
            /**
             * @brief Helper to write a TSCC Ann-Wd broadcast packet on the RF interface.
             * @param channelNo Channel number.
             * @param annWd Flag indicating whether the channel is being announced or withdrawn.
             * @param uint32_t System Identity.
             * @param requireReq Flag indicating this site requires registration.
             */
            void writeRF_TSCC_Bcast_Ann_Wd(uint32_t channelNo, bool annWd, uint32_t systemIdentity, bool requireReg);
            /**
             * @brief Helper to write a TSCC Sys_Parm broadcast packet on the RF interface.
             */
            void writeRF_TSCC_Bcast_Sys_Parm();
            /**
             * @brief Helper to write a TSCC Git Hash broadcast packet on the RF interface.
             */
            void writeRF_TSCC_Git_Hash();
        };
    } // namespace packet
} // namespace dmr

#endif // __DMR_PACKET_CONTROL_SIGNALING_H__
