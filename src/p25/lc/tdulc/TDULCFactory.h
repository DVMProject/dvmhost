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
#if !defined(__P25_LC__TDULC_FACTORY_H__)
#define  __P25_LC__TDULC_FACTORY_H__

#include "Defines.h"
#include "edac/RS634717.h"

#include "p25/lc/TDULC.h"
#include "p25/lc/tdulc/LC_ADJ_STS_BCAST.h"
#include "p25/lc/tdulc/LC_CALL_TERM.h"
#include "p25/lc/tdulc/LC_CONV_FALLBACK.h"
#include "p25/lc/tdulc/LC_GROUP_UPDT.h"
#include "p25/lc/tdulc/LC_GROUP.h"
#include "p25/lc/tdulc/LC_IDEN_UP.h"
#include "p25/lc/tdulc/LC_NET_STS_BCAST.h"
#include "p25/lc/tdulc/LC_PRIVATE.h"
#include "p25/lc/tdulc/LC_RFSS_STS_BCAST.h"
#include "p25/lc/tdulc/LC_SYS_SRV_BCAST.h"
#include "p25/lc/tdulc/LC_TEL_INT_VCH_USER.h"

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
