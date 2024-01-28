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
#if !defined(__NXDN_LC__RCCH_FACTORY_H__)
#define  __NXDN_LC__RCCH_FACTORY_H__

#include "common/Defines.h"

#include "common/nxdn/lc/RCCH.h"
#include "common/nxdn/lc/rcch/MESSAGE_TYPE_DCALL_HDR.h"
#include "common/nxdn/lc/rcch/MESSAGE_TYPE_DST_ID_INFO.h"
#include "common/nxdn/lc/rcch/MESSAGE_TYPE_GRP_REG.h"
#include "common/nxdn/lc/rcch/MESSAGE_TYPE_IDLE.h"
#include "common/nxdn/lc/rcch/MESSAGE_TYPE_REG_C.h"
#include "common/nxdn/lc/rcch/MESSAGE_TYPE_REG_COMM.h"
#include "common/nxdn/lc/rcch/MESSAGE_TYPE_REG.h"
#include "common/nxdn/lc/rcch/MESSAGE_TYPE_SITE_INFO.h"
#include "common/nxdn/lc/rcch/MESSAGE_TYPE_SRV_INFO.h"
#include "common/nxdn/lc/rcch/MESSAGE_TYPE_VCALL_ASSGN.h"
#include "common/nxdn/lc/rcch/MESSAGE_TYPE_VCALL_CONN.h"

namespace nxdn
{
    namespace lc
    {
        namespace rcch
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Helper class to instantiate an instance of a RCCH.
            // ---------------------------------------------------------------------------

            class HOST_SW_API RCCHFactory {
            public:
                /// <summary>Initializes a new instance of the RCCHFactory class.</summary>
                RCCHFactory();
                /// <summary>Finalizes a instance of the RCCHFactory class.</summary>
                ~RCCHFactory();

                /// <summary>Create an instance of a RCCH.</summary>
                static std::unique_ptr<RCCH> createRCCH(const uint8_t* data, uint32_t length, uint32_t offset = 0U);

            private:
                /// <summary></summary>
                static std::unique_ptr<RCCH> decode(RCCH* rcch, const uint8_t* data, uint32_t length, uint32_t offset = 0U);
            };
        } // namespace rcch
    } // namespace lc
} // namespace nxdn

#endif // __NXDN_LC__RCCH_FACTORY_H__
