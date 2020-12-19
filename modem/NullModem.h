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
*   Copyright (C) 2011-2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2020 by Bryan Biedenkapp N2PLL
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
#if !defined(__NULL_MODEM_H__)
#define __NULL_MODEM_H__

#include "Defines.h"
#include "modem/Modem.h"

namespace modem
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements the interface to a null modem.
    // ---------------------------------------------------------------------------

    class HOST_SW_API NullModem : public Modem {
    public:
        /// <summary>Initializes a new instance of the NullModem class.</summary>
        NullModem(const std::string& port, bool duplex, bool rxInvert, bool txInvert, bool pttInvert, bool dcBlocker,
            bool cosLockout, uint8_t fdmaPreamble, uint8_t dmrRxDelay, uint8_t packetPlayoutTime, bool disableOFlowReset, bool trace, bool debug);
        /// <summary>Finalizes a instance of the NullModem class.</summary>
        ~NullModem();

        /// <summary>Sets the modem DSP RF DC offset parameters.</summary>
        virtual void setDCOffsetParams(int txDCOffset, int rxDCOffset) { return; }
        /// <summary>Sets the modem DSP enabled modes.</summary>
        virtual void setModeParams(bool dmrEnabled, bool p25Enabled) { return; }
        /// <summary>Sets the modem DSP RF deviation levels.</summary>
        virtual void setLevels(float rxLevel, float cwIdTXLevel, float dmrTXLevel, float p25TXLevel) { return; }
        /// <summary>Sets the modem DSP DMR color code.</summary>
        virtual void setDMRParams(uint32_t colorCode) { return; }
        /// <summary>Sets the modem DSP RF receive deviation levels.</summary>
        virtual void setRXLevel(float rxLevel) { return; }

        /// <summary>Opens connection to the modem DSP.</summary>
        virtual bool open();

        /// <summary>Updates the timer by the passed number of milliseconds.</summary>
        virtual void clock(uint32_t ms) { return; }

        /// <summary>Closes connection to the modem DSP.</summary>
        virtual void close() { return; }

        /// <summary>Flag indicating whether or not the modem DSP is transmitting.</summary>
        virtual bool hasTX() const { return false; }
        /// <summary>Flag indicating whether or not the modem DSP has carrier detect.</summary>
        virtual bool hasCD() const { return false; }

        /// <summary>Flag indicating whether or not the modem DSP is currently locked out.</summary>
        virtual bool hasLockout() const { return false; }
        /// <summary>Flag indicating whether or not the modem DSP is currently in an error condition.</summary>
        virtual bool hasError() const { return false; }

        /// <summary>Writes DMR Slot 1 frame data to the DMR Slot 1 ring buffer.</summary>
        virtual bool writeDMRData1(const uint8_t* data, uint32_t length) { return true; }
        /// <summary>Writes DMR Slot 2 frame data to the DMR Slot 2 ring buffer.</summary>
        virtual bool writeDMRData2(const uint8_t* data, uint32_t length) { return true; }
        /// <summary>Writes P25 frame data to the P25 ring buffer.</summary>
        virtual bool writeP25Data(const uint8_t* data, uint32_t length) { return true; }

        /// <summary>Triggers the start of DMR transmit.</summary>
        virtual bool writeDMRStart(bool tx) { return true; }
        /// <summary>Writes a DMR short LC to the modem DSP.</summary>
        virtual bool writeDMRShortLC(const uint8_t* lc) { return true; }
        /// <summary>Writes a DMR abort message for the given slot to the modem DSP.</summary>
        virtual bool writeDMRAbort(uint32_t slotNo) { return true; }

        /// <summary>Sets the current operating mode for the modem DSP.</summary>
        virtual bool setMode(DVM_STATE state) { return true; }

        /// <summary>Transmits the given string as CW morse.</summary>
        virtual bool sendCWId(const std::string& callsign) { return true; }
    };
} // namespace modem

#endif // __NULL_MODEM_H__
