// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI peer application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__RTP_DEFINES_H__)
#define  __RTP_DEFINES_H__

#include "common/Defines.h"

namespace p25
{
    namespace dfsi
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        /// <summary>
        /// 
        /// </summary>
        enum RTFlag {
            ENABLED = 0x02U,
            DISABLED = 0x04U
        };

        /// <summary>
        /// 
        /// </summary>
        enum StartStopFlag {
            START = 0x0CU,
            STOP = 0x25U
        };

        /// <summary>
        /// 
        /// </summary>
        enum StreamTypeFlag {
            VOICE = 0x0BU
        };

        /// <summary>
        /// 
        /// </summary>
        enum RssiValidityFlag {
            INVALID = 0x00U,
            VALID = 0x1A
        };

        /// <summary>
        /// 
        /// </summary>
        enum SourceFlag {
            SOURCE_DIU = 0x00U,
            SOURCE_QUANTAR = 0x02U
        };

        /// <summary>
        /// 
        /// </summary>
        enum ICWFlag {
            ICW_DIU = 0x00U,
            ICW_QUANTAR = 0x1B
        };
    } // namespace dfsi
} // namespace p25

#endif // __RTP_DEFINES_H__
