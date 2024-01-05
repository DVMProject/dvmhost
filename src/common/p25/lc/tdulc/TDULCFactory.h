/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
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
#if !defined(__P25_LC__TDULC_FACTORY_H__)
#define  __P25_LC__TDULC_FACTORY_H__

#include "common/Defines.h"
#include "common/edac/RS634717.h"

#include "common/p25/lc/TDULC.h"
#include "common/p25/lc/tdulc/LC_ADJ_STS_BCAST.h"
#include "common/p25/lc/tdulc/LC_CALL_TERM.h"
#include "common/p25/lc/tdulc/LC_CONV_FALLBACK.h"
#include "common/p25/lc/tdulc/LC_GROUP_UPDT.h"
#include "common/p25/lc/tdulc/LC_GROUP.h"
#include "common/p25/lc/tdulc/LC_IDEN_UP.h"
#include "common/p25/lc/tdulc/LC_NET_STS_BCAST.h"
#include "common/p25/lc/tdulc/LC_PRIVATE.h"
#include "common/p25/lc/tdulc/LC_RFSS_STS_BCAST.h"
#include "common/p25/lc/tdulc/LC_SYS_SRV_BCAST.h"
#include "common/p25/lc/tdulc/LC_TEL_INT_VCH_USER.h"

#include "common/p25/lc/tdulc/LC_FAILSOFT.h"

namespace p25
{
    namespace lc
    {
        namespace tdulc
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Helper class to instantiate an instance of a TDULC.
            // ---------------------------------------------------------------------------

            class HOST_SW_API TDULCFactory {
            public:
                /// <summary>Initializes a new instance of the TDULCFactory class.</summary>
                TDULCFactory();
                /// <summary>Finalizes a instance of the TDULCFactory class.</summary>
                ~TDULCFactory();

                /// <summary>Create an instance of a TDULC.</summary>
                static std::unique_ptr<TDULC> createTDULC(const uint8_t* data);

            private:
                static edac::RS634717 m_rs;

                /// <summary></summary>
                static std::unique_ptr<TDULC> decode(TDULC* tdulc, const uint8_t* data);
            };
        } // namespace tdulc
    } // namespace lc
} // namespace p25

#endif // __P25_LC__TDULC_FACTORY_H__
