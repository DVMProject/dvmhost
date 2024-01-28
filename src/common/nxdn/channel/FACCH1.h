// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2018 Jonathan Naylor, G4KLX
*
*/
#if !defined(__NXDN_CHANNEL__FACCH1_H__)
#define  __NXDN_CHANNEL__FACCH1_H__

#include "common/Defines.h"

namespace nxdn
{
    namespace channel
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements NXDN Fast Associated Control Channel 1.
        // ---------------------------------------------------------------------------

        class HOST_SW_API FACCH1 {
        public:
            /// <summary>Initializes a new instance of the FACCH1 class.</summary>
            FACCH1();
            /// <summary>Initializes a copy instance of the FACCH1 class.</summary>
            FACCH1(const FACCH1& data);
            /// <summary>Finalizes a instance of the FACCH1 class.</summary>
            ~FACCH1();

            /// <summary>Equals operator.</summary>
            FACCH1& operator=(const FACCH1& data);

            /// <summary>Decode a fast associated control channel 1.</summary>
            bool decode(const uint8_t* data, uint32_t offset);
            /// <summary>Encode a fast associated control channel 1.</summary>
            void encode(uint8_t* data, uint32_t offset) const;

            /// <summary>Gets the raw FACCH1 data.</summary>
            void getData(uint8_t* data) const;
            /// <summary>Sets the raw FACCH1 data.</summary>
            void setData(const uint8_t* data);

        private:
            uint8_t* m_data;

            /// <summary>Internal helper to copy the class.</summary>
            void copy(const FACCH1& data);
        };
    } // namespace channel
} // namespace nxdn

#endif // __NXDN_CHANNEL__FACCH1_H__
