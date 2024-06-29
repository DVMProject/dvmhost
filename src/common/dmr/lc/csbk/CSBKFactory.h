// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
*
*/
/**
 * @defgroup dmr_csbk Control Signalling Block
 * @brief Implementation for the data handling of the ETSI TS-102 control signalling block (CSBK).
 * @ingroup dmr_lc
 * 
 * @file CSBKFactory.h
 * @ingroup dmr_csbk
 * @file CSBKFactory.cpp
 * @ingroup dmr_csbk
 */
#if !defined(__DMR_LC__CSBK_FACTORY_H__)
#define  __DMR_LC__CSBK_FACTORY_H__

#include "common/Defines.h"

#include "common/dmr/DMRDefines.h"
#include "common/dmr/lc/CSBK.h"
#include "common/dmr/lc/csbk/CSBK_ACK_RSP.h"
#include "common/dmr/lc/csbk/CSBK_ALOHA.h"
#include "common/dmr/lc/csbk/CSBK_BROADCAST.h"
#include "common/dmr/lc/csbk/CSBK_BSDWNACT.h"
#include "common/dmr/lc/csbk/CSBK_CALL_ALRT.h"
#include "common/dmr/lc/csbk/CSBK_EXT_FNCT.h"
#include "common/dmr/lc/csbk/CSBK_MAINT.h"
#include "common/dmr/lc/csbk/CSBK_NACK_RSP.h"
#include "common/dmr/lc/csbk/CSBK_P_CLEAR.h"
#include "common/dmr/lc/csbk/CSBK_P_GRANT.h"
#include "common/dmr/lc/csbk/CSBK_PD_GRANT.h"
#include "common/dmr/lc/csbk/CSBK_PRECCSBK.h"
#include "common/dmr/lc/csbk/CSBK_PV_GRANT.h"
#include "common/dmr/lc/csbk/CSBK_RAND.h"
#include "common/dmr/lc/csbk/CSBK_RAW.h"
#include "common/dmr/lc/csbk/CSBK_TD_GRANT.h"
#include "common/dmr/lc/csbk/CSBK_TV_GRANT.h"
#include "common/dmr/lc/csbk/CSBK_UU_ANS_RSP.h"
#include "common/dmr/lc/csbk/CSBK_UU_V_REQ.h"

namespace dmr
{
    namespace lc
    {
        namespace csbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Helper class to instantiate an instance of a CSBK.
             * @ingroup dmr_csbk
             */
            class HOST_SW_API CSBKFactory {
            public:
                /**
                 * @brief Initializes a new instance of the CSBKFactory class.
                 */
                CSBKFactory();
                /**
                 * @brief Finalizes a instance of the CSBKFactory class.
                 */
                ~CSBKFactory();

                /**
                 * @brief Create an instance of a CSBK.
                 * @param[in] data Buffer containing CSBK packet data to decode.
                 * @param dataType Data Type.
                 * @returns CSBK* Instance of a CSBK representing the decoded data.
                 */
                static std::unique_ptr<CSBK> createCSBK(const uint8_t* data, defines::DataType::E dataType);

            private:
                /**
                 * @brief Decode a CSBK.
                 * @param csbk Instance of a CSBK.
                 * @param[in] data Buffer containing CSBK packet data to decode.
                 * @returns CSBK* Instance of a CSBK representing the decoded data.
                 */
                static std::unique_ptr<CSBK> decode(CSBK* csbk, const uint8_t* data);
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC__CSBK_FACTORY_H__
