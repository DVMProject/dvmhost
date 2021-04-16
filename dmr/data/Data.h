/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2015,2016 by Jonathan Naylor, G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; version 2 of the License.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*/
#if !defined(__DMR_DATA__DATA_H__)
#define __DMR_DATA__DATA_H__

#include "Defines.h"
#include "dmr/DMRDefines.h"

namespace dmr
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents general DMR data.
        // ---------------------------------------------------------------------------

        class HOST_SW_API Data {
        public:
            /// <summary>Initializes a new instance of the Data class.</summary>
            Data(const Data& data);
            /// <summary>Initializes a new instance of the Data class.</summary>
            Data();
            /// <summary>Finalizes a instance of the Data class.</summary>
            ~Data();

            /// <summary>Equals operator.</summary>
            Data& operator=(const Data& data);

            /// <summary>Sets raw data.</summary>
            void setData(const uint8_t* buffer);
            /// <summary>Gets raw data.</summary>
            uint32_t getData(uint8_t* buffer) const;

        public:
            /// <summary>DMR slot number.</summary>
            __PROPERTY(uint32_t, slotNo, SlotNo);

            /// <summary>Source ID.</summary>
            __PROPERTY(uint32_t, srcId, SrcId);
            /// <summary>Destination ID.</summary>
            __PROPERTY(uint32_t, dstId, DstId);

            /// <summary>Sets the full-link control opcode.</summary>
            __PROPERTY(uint8_t, flco, FLCO);

            /// <summary></summary>
            __PROPERTY(uint8_t, n, N);

            /// <summary>Sequence number.</summary>
            __PROPERTY(uint8_t, seqNo, SeqNo);

            /// <summary>Embedded data type.</summary>
            __PROPERTY(uint8_t, dataType, DataType);

            /// <summary>Bit Error Rate.</summary>
            __PROPERTY(uint8_t, ber, BER);

            /// <summary>Received Signal Strength Indicator.</summary>
            __PROPERTY(uint8_t, rssi, RSSI);

        private:
            uint8_t* m_data;
        };
    } // namespace data
} // namespace dmr

#endif // __DMR_DATA__DATA_H__
