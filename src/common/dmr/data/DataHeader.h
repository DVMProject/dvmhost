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
*   Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
*   Copyright (C) 2021 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DMR_DATA__DATA_HEADER_H__)
#define __DMR_DATA__DATA_HEADER_H__

#include "common/Defines.h"

namespace dmr
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents a DMR data header.
        // ---------------------------------------------------------------------------

        class HOST_SW_API DataHeader {
        public:
            /// <summary>Initializes a new instance of the DataHeader class.</summary>
            DataHeader();
            /// <summary>Finalizes a instance of the DataHeader class.</summary>
            ~DataHeader();

            /// <summary>Equals operator.</summary>
            DataHeader& operator=(const DataHeader& header);

            /// <summary>Decodes a DMR data header.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encodes a DMR data header.</summary>
            void encode(uint8_t* data) const;

        public:
            /// <summary>Flag indicating whether the CSBK is group or individual.</summary>
            __PROPERTY(bool, GI, GI);

            /// <summary></summary>
            __PROPERTY(uint8_t, DPF, DPF);

            /// <summary>Service access point.</summary>
            __PROPERTY(uint8_t, sap, SAP);
            /// <summary>Fragment Sequence Number.</summary>
            __PROPERTY(uint8_t, fsn, FSN);
            /// <summary>Send Sequence Number.</summary>
            __PROPERTY(uint8_t, Ns, Ns);

            /// <summary>Count of block padding.</summary>
            __PROPERTY(uint8_t, padCount, PadCount);

            /// <summary>Full Message Flag.</summary>
            __PROPERTY(bool, F, FullMesage);
            /// <summary>Synchronize Flag.</summary>
            __PROPERTY(bool, S, Synchronize);

            /// <summary>Unified Data or Defined Data Format.</summary>
            __PROPERTY(uint8_t, dataFormat, DataFormat);

            /// <summary>Source ID.</summary>
            __PROPERTY(uint32_t, srcId, SrcId);
            /// <summary>Destination ID.</summary>
            __PROPERTY(uint32_t, dstId, DstId);

            /// <summary>Gets the number of data blocks following the header.</summary>
            __PROPERTY(uint32_t, blocks, Blocks);

            /// <summary>Response class.</summary>
            __PROPERTY(uint8_t, rspClass, Class);
            /// <summary>Response type.</summary>
            __PROPERTY(uint8_t, rspType, Type);
            /// <summary>Response status.</summary>
            __PROPERTY(uint8_t, rspStatus, Status);

            /// <summary>Source Port.</summary>
            __PROPERTY(uint8_t, srcPort, SrcPort);
            /// <summary>Destination Port.</summary>
            __PROPERTY(uint8_t, dstPort, DstPort);

        private:
            uint8_t* m_data;
            bool m_A;
            bool m_SF;
            bool m_PF;
            uint8_t m_UDTO;
        };
    } // namespace data
} // namespace dmr

#endif // __DMR_DATA__DATA_HEADER_H__
