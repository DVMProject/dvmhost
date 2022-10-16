/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
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
#if !defined(__NXDN_LC__RCCH_FACTORY_H__)
#define  __NXDN_LC__RCCH_FACTORY_H__

#include "Defines.h"

#include "nxdn/lc/RCCH.h"
#include "nxdn/lc/rcch/MESSAGE_TYPE_DCALL_HDR.h"
#include "nxdn/lc/rcch/MESSAGE_TYPE_DST_ID_INFO.h"
#include "nxdn/lc/rcch/MESSAGE_TYPE_GRP_REG.h"
#include "nxdn/lc/rcch/MESSAGE_TYPE_IDLE.h"
#include "nxdn/lc/rcch/MESSAGE_TYPE_REG_C.h"
#include "nxdn/lc/rcch/MESSAGE_TYPE_REG_COMM.h"
#include "nxdn/lc/rcch/MESSAGE_TYPE_REG.h"
#include "nxdn/lc/rcch/MESSAGE_TYPE_SITE_INFO.h"
#include "nxdn/lc/rcch/MESSAGE_TYPE_SRV_INFO.h"
#include "nxdn/lc/rcch/MESSAGE_TYPE_VCALL_ASSGN.h"
#include "nxdn/lc/rcch/MESSAGE_TYPE_VCALL_CONN.h"

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
