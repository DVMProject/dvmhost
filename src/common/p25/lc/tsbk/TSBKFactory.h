// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup p25_tsbk Trunk Signalling Block
 * @brief Implementation for the data handling of the TIA-102.AABC Project 25 standard (TSDU/TSBK)
 * @ingroup p25_lc
 *
 * @defgroup p25_ambt Alternate Multiblock Trunking
 * @brief Implementation for the data handling of the TIA-102.AABC Project 25 standard (PDU)
 * @ingroup p25_tsbk
 * 
 * @file TSBKFactory.h
 * @ingroup p25_tsbk
 * @file TSBKFactory.cpp
 * @ingroup p25_tsbk
 */
#if !defined(__P25_LC__TSBK_FACTORY_H__)
#define  __P25_LC__TSBK_FACTORY_H__

#include "common/Defines.h"

#include "common/edac/Trellis.h"

// TSBK
#include "common/p25/lc/TSBK.h"
#include "common/p25/lc/tsbk/IOSP_ACK_RSP.h"
#include "common/p25/lc/tsbk/IOSP_CALL_ALRT.h"
#include "common/p25/lc/tsbk/IOSP_EXT_FNCT.h"
#include "common/p25/lc/tsbk/IOSP_GRP_AFF.h"
#include "common/p25/lc/tsbk/IOSP_GRP_VCH.h"
#include "common/p25/lc/tsbk/IOSP_MSG_UPDT.h"
#include "common/p25/lc/tsbk/IOSP_RAD_MON.h"
#include "common/p25/lc/tsbk/IOSP_STS_UPDT.h"
#include "common/p25/lc/tsbk/IOSP_U_REG.h"
#include "common/p25/lc/tsbk/IOSP_UU_ANS.h"
#include "common/p25/lc/tsbk/IOSP_UU_VCH.h"
#include "common/p25/lc/tsbk/ISP_AUTH_FNE_RST.h"
#include "common/p25/lc/tsbk/ISP_AUTH_RESP.h"
#include "common/p25/lc/tsbk/ISP_AUTH_SU_DMD.h"
#include "common/p25/lc/tsbk/ISP_CAN_SRV_REQ.h"
#include "common/p25/lc/tsbk/ISP_EMERG_ALRM_REQ.h"
#include "common/p25/lc/tsbk/ISP_GRP_AFF_Q_RSP.h"
#include "common/p25/lc/tsbk/ISP_LOC_REG_REQ.h"
#include "common/p25/lc/tsbk/ISP_SNDCP_CH_REQ.h"
#include "common/p25/lc/tsbk/ISP_SNDCP_REC_REQ.h"
#include "common/p25/lc/tsbk/ISP_U_DEREG_REQ.h"
#include "common/p25/lc/tsbk/OSP_ADJ_STS_BCAST.h"
#include "common/p25/lc/tsbk/OSP_AUTH_FNE_RESP.h"
#include "common/p25/lc/tsbk/OSP_DENY_RSP.h"
#include "common/p25/lc/tsbk/OSP_DVM_LC_CALL_TERM.h"
#include "common/p25/lc/tsbk/OSP_GRP_AFF_Q.h"
#include "common/p25/lc/tsbk/OSP_GRP_VCH_GRANT_UPD.h"
#include "common/p25/lc/tsbk/OSP_IDEN_UP_VU.h"
#include "common/p25/lc/tsbk/OSP_IDEN_UP.h"
#include "common/p25/lc/tsbk/OSP_LOC_REG_RSP.h"
#include "common/p25/lc/tsbk/OSP_MOT_CC_BSI.h"
#include "common/p25/lc/tsbk/OSP_MOT_GRG_ADD.h"
#include "common/p25/lc/tsbk/OSP_MOT_GRG_DEL.h"
#include "common/p25/lc/tsbk/OSP_MOT_GRG_VCH_GRANT.h"
#include "common/p25/lc/tsbk/OSP_MOT_GRG_VCH_UPD.h"
#include "common/p25/lc/tsbk/OSP_MOT_PSH_CCH.h"
#include "common/p25/lc/tsbk/OSP_NET_STS_BCAST.h"
#include "common/p25/lc/tsbk/OSP_QUE_RSP.h"
#include "common/p25/lc/tsbk/OSP_RFSS_STS_BCAST.h"
#include "common/p25/lc/tsbk/OSP_SCCB_EXP.h"
#include "common/p25/lc/tsbk/OSP_SCCB.h"
#include "common/p25/lc/tsbk/OSP_SNDCP_CH_ANN.h"
#include "common/p25/lc/tsbk/OSP_SNDCP_CH_GNT.h"
#include "common/p25/lc/tsbk/OSP_SYNC_BCAST.h"
#include "common/p25/lc/tsbk/OSP_SYS_SRV_BCAST.h"
#include "common/p25/lc/tsbk/OSP_TIME_DATE_ANN.h"
#include "common/p25/lc/tsbk/OSP_TSBK_RAW.h"
#include "common/p25/lc/tsbk/OSP_U_DEREG_ACK.h"
#include "common/p25/lc/tsbk/OSP_U_REG_CMD.h"
#include "common/p25/lc/tsbk/OSP_UU_VCH_GRANT_UPD.h"

// AMBT
#include "common/p25/lc/AMBT.h"
#include "common/p25/lc/tsbk/mbt/MBT_IOSP_ACK_RSP.h"
#include "common/p25/lc/tsbk/mbt/MBT_IOSP_CALL_ALRT.h"
#include "common/p25/lc/tsbk/mbt/MBT_IOSP_EXT_FNCT.h"
#include "common/p25/lc/tsbk/mbt/MBT_IOSP_GRP_AFF.h"
#include "common/p25/lc/tsbk/mbt/MBT_IOSP_MSG_UPDT.h"
#include "common/p25/lc/tsbk/mbt/MBT_IOSP_STS_UPDT.h"
#include "common/p25/lc/tsbk/mbt/MBT_ISP_AUTH_RESP_M.h"
#include "common/p25/lc/tsbk/mbt/MBT_ISP_AUTH_SU_DMD.h"
#include "common/p25/lc/tsbk/mbt/MBT_ISP_CAN_SRV_REQ.h"
#include "common/p25/lc/tsbk/mbt/MBT_ISP_GRP_AFF_Q_RSP.h"
#include "common/p25/lc/tsbk/mbt/MBT_OSP_ADJ_STS_BCAST.h"
#include "common/p25/lc/tsbk/mbt/MBT_OSP_AUTH_DMD.h"
#include "common/p25/lc/tsbk/mbt/MBT_OSP_GRP_VCH_GRANT.h"
#include "common/p25/lc/tsbk/mbt/MBT_OSP_NET_STS_BCAST.h"
#include "common/p25/lc/tsbk/mbt/MBT_OSP_RFSS_STS_BCAST.h"
#include "common/p25/lc/tsbk/mbt/MBT_OSP_UU_VCH_GRANT.h"

namespace p25
{
    namespace lc
    {
        namespace tsbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Helper class to instantiate an instance of a TSBK.
             * @ingroup p25_tsbk
             */
            class HOST_SW_API TSBKFactory {
            public:
                /**
                 * @brief Initializes a new instance of the TSBKFactory class.
                 */
                TSBKFactory();
                /**
                 * @brief Finalizes a instance of the TSBKFactory class.
                 */
                ~TSBKFactory();

                /**
                 * @brief Create an instance of a TSBK.
                 * @param[in] data Buffer containing TSBK packet data to decode.
                 * @param rawTSBK Flag indicating whether or not the passed buffer is raw.
                 * @returns TSBK* Instance of a TSBK representing the decoded data.
                 */
                static std::unique_ptr<TSBK> createTSBK(const uint8_t* data, bool rawTSBK = false);
                /**
                 * @brief Create an instance of a AMBT.
                 * @param[in] dataHeader P25 PDU data header
                 * @param[in] blocks P25 PDU data blocks
                 * @returns AMBT* Instance of a AMBT representing the decoded data.
                 */
                static std::unique_ptr<AMBT> createAMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks);

                /**
                 * @brief Sets the flag indicating CRC-errors should be warnings and not errors.
                 * @param warnCRC Flag indicating CRC-errors should be treated as warnings.
                 */
                static void setWarnCRC(bool warnCRC) { m_warnCRC = warnCRC; }

            private:
                static bool m_warnCRC;

                /**
                 * @brief Decode a TSBK.
                 * @param tsbk Instance of a TSBK.
                 * @param[in] data Buffer containing TSBK packet data to decode.
                 * @param rawTSBK Flag indicating whether or not the passed buffer is raw.
                 * @returns TSBK* Instance of a TSBK representing the decoded data.
                 */
                static std::unique_ptr<TSBK> decode(TSBK* tsbk, const uint8_t* data, bool rawTSBK = false);
                /**
                 * @brief Decode an AMBT.
                 * @param tsbk Instance of a TSBK.
                 * @param[in] dataHeader P25 PDU data header
                 * @param[in] blocks P25 PDU data blocks
                 * @returns AMBT* Instance of a AMBT representing the decoded data.
                 */
                static std::unique_ptr<AMBT> decode(AMBT* ambt, const data::DataHeader& dataHeader, const data::DataBlock* blocks);
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC__TSBK_FACTORY_H__
