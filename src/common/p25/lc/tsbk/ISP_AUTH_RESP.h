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
#if !defined(__P25_LC_TSBK__ISP_AUTH_RESP_H__)
#define  __P25_LC_TSBK__ISP_AUTH_RESP_H__

#include "common/Defines.h"
#include "common/p25/lc/TSBK.h"

namespace p25
{
    namespace lc
    {
        namespace tsbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Implements AUTH RESP - Authentication Response
            // ---------------------------------------------------------------------------

            class HOST_SW_API ISP_AUTH_RESP : public TSBK {
            public:
                /// <summary>Initializes a new instance of the ISP_AUTH_RESP class.</summary>
                ISP_AUTH_RESP();
                /// <summary>Finalizes a instance of the ISP_AUTH_RESP class.</summary>
                ~ISP_AUTH_RESP();

                /// <summary>Decode a trunking signalling block.</summary>
                bool decode(const uint8_t* data, bool rawTSBK = false) override;
                /// <summary>Encode a trunking signalling block.</summary>
                void encode(uint8_t* data, bool rawTSBK = false, bool noTrellis = false) override;

                /// <summary>Returns a string that represents the current TSBK.</summary>
                std::string toString(bool isp = true) override;

                /** Authentication data */
                /// <summary>Gets the authentication result.</summary>
                void getAuthRes(uint8_t* res) const;

            public:
                /// <summary>Flag indicating authentication is standalone.</summary>
                __PROPERTY(bool, authStandalone, AuthStandalone);

            private:
                /** Authentication data */
                uint8_t* m_authRes;

                __COPY(ISP_AUTH_RESP);
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__ISP_AUTH_RESP_H__
