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
*   Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
*   Copyright (C) 2021,2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DMR_LC__FULL_LC_H__)
#define __DMR_LC__FULL_LC_H__

#include "common/Defines.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/lc/LC.h"
#include "common/dmr/lc/PrivacyLC.h"
#include "common/edac/BPTC19696.h"

namespace dmr
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents full DMR link control.
        // ---------------------------------------------------------------------------

        class HOST_SW_API FullLC {
        public:
            /// <summary>Initializes a new instance of the FullLC class.</summary>
            FullLC();
            /// <summary>Finalizes a instance of the FullLC class.</summary>
            ~FullLC();

            /// <summary>Decode DMR full-link control data.</summary>
            std::unique_ptr<LC> decode(const uint8_t* data, defines::DataType::E type);
            /// <summary>Encode DMR full-link control data.</summary>
            void encode(const LC& lc, uint8_t* data, defines::DataType::E type);

            /// <summary>Decode DMR privacy control data.</summary>
            std::unique_ptr<PrivacyLC> decodePI(const uint8_t* data);
            /// <summary>Encode DMR privacy control data.</summary>
            void encodePI(const PrivacyLC& lc, uint8_t* data);

        private:
            edac::BPTC19696 m_bptc;
        };
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC__FULL_LC_H__
