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
#if !defined(__P25_LC_TSBK__MBT_ISP_AUTH_RESP_M_H__)
#define  __P25_LC_TSBK__MBT_ISP_AUTH_RESP_M_H__

#include "common/Defines.h"
#include "common/p25/lc/AMBT.h"

namespace p25
{
    namespace lc
    {
        namespace tsbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Implements AUTH RESP M - Authentication Response Mutual
            // ---------------------------------------------------------------------------

            class HOST_SW_API MBT_ISP_AUTH_RESP_M : public AMBT {
            public:
                /// <summary>Initializes a new instance of the MBT_ISP_AUTH_RESP_M class.</summary>
                MBT_ISP_AUTH_RESP_M();
                /// <summary>Finalizes a instance of the MBT_ISP_AUTH_RESP_M class.</summary>
                ~MBT_ISP_AUTH_RESP_M();

                /// <summary>Decode a alternate trunking signalling block.</summary>
                bool decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks);
                /// <summary>Encode a alternate trunking signalling block.</summary>
                void encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData);

                /// <summary>Returns a string that represents the current TSBK.</summary>
                virtual std::string toString(bool isp = false) override;

                /** Authentication data */
                /// <summary>Gets the authentication result.</summary>
                void getAuthRes(uint8_t* res) const;

                /// <summary>Sets the authentication random challenge.</summary>
                void setAuthRC(const uint8_t* rc);
                /// <summary>Gets the authentication random challenge.</summary>
                void getAuthRC(uint8_t* rc) const;

            public:
                /// <summary>Flag indicating authentication is standalone.</summary>
                __PROPERTY(bool, authStandalone, AuthStandalone);

            private:
                /** Authentication data */
                uint8_t* m_authRes;
                uint8_t* m_authRC;

                __COPY(MBT_ISP_AUTH_RESP_M);
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__MBT_ISP_AUTH_RESP_M_H__
