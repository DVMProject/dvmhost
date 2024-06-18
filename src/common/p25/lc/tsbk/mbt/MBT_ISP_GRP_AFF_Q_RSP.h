// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__P25_LC_TSBK__MBT_ISP_GRP_AFF_Q_RSP_H__)
#define  __P25_LC_TSBK__MBT_ISP_GRP_AFF_Q_RSP_H__

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
            //      Implements GRP AFF Q RSP - Group Affiliation Query Response
            // ---------------------------------------------------------------------------

            class HOST_SW_API MBT_ISP_GRP_AFF_Q_RSP : public AMBT {
            public:
                /// <summary>Initializes a new instance of the MBT_ISP_GRP_AFF_Q_RSP class.</summary>
                MBT_ISP_GRP_AFF_Q_RSP();

                /// <summary>Decode a alternate trunking signalling block.</summary>
                bool decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks) override;
                /// <summary>Encode a alternate trunking signalling block.</summary>
                void encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData) override;

                /// <summary>Returns a string that represents the current TSBK.</summary>
                std::string toString(bool isp = true) override;
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__MBT_ISP_GRP_AFF_Q_RSP_H__
