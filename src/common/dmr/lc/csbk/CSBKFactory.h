// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DMR_LC__CSBK_FACTORY_H__)
#define  __DMR_LC__CSBK_FACTORY_H__

#include "common/Defines.h"

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
            //      Helper class to instantiate an instance of a CSBK.
            // ---------------------------------------------------------------------------

            class HOST_SW_API CSBKFactory {
            public:
                /// <summary>Initializes a new instance of the CSBKFactory class.</summary>
                CSBKFactory();
                /// <summary>Finalizes a instance of the CSBKFactory class.</summary>
                ~CSBKFactory();

                /// <summary>Create an instance of a CSBK.</summary>
                static std::unique_ptr<CSBK> createCSBK(const uint8_t* data, uint8_t dataType);

            private:
                /// <summary></summary>
                static std::unique_ptr<CSBK> decode(CSBK* csbk, const uint8_t* data);
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC__CSBK_FACTORY_H__
