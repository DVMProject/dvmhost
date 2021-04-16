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
*   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
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
#if !defined(__DMR_DATA__EMBEDDED_DATA_H__)
#define __DMR_DATA__EMBEDDED_DATA_H__

#include "Defines.h"
#include "dmr/DMRDefines.h"
#include "dmr/lc/LC.h"

namespace dmr
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        enum LC_STATE {
            LCS_NONE,
            LCS_FIRST,
            LCS_SECOND,
            LCS_THIRD
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents a DMR embedded data.
        // ---------------------------------------------------------------------------

        class HOST_SW_API EmbeddedData {
        public:
            /// <summary>Initializes a new instance of the EmbeddedData class.</summary>
            EmbeddedData();
            /// <summary>Finalizes a instance of the EmbeddedData class.</summary>
            ~EmbeddedData();

            /// <summary>Add LC data (which may consist of 4 blocks) to the data store.</summary>
            bool addData(const uint8_t* data, uint8_t lcss);
            /// <summary>Get LC data from the data store.</summary>
            uint8_t getData(uint8_t* data, uint8_t n) const;

            /// <summary>Sets link control data.</summary>
            void setLC(const lc::LC& lc);
            /// <summary>Gets link control data.</summary>
            lc::LC* getLC() const;

            /// <summary>Get raw embeded data buffer.</summary>
            bool getRawData(uint8_t* data) const;

            /// <summary>Helper to reset data values to defaults.</summary>
            void reset();

        public:
            /// <summary>Flag indicating whether or not the embedded data is valid.</summary>
            __READONLY_PROPERTY_PLAIN(bool, valid, isValid);
            /// <summary>Full-link control opcode.</summary>
            __READONLY_PROPERTY(uint8_t, FLCO, FLCO);

        private:
            LC_STATE m_state;
            bool* m_data;

            bool* m_raw;

            /// <summary>Unpack and error check an embedded LC.</summary>
            void decodeEmbeddedData();
            /// <summary>Pack and FEC for an embedded LC.</summary>
            void encodeEmbeddedData();
        };
    } // namespace data
} // namespace dmr

#endif // __DMR_DATA__EMBEDDED_DATA_H__
