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
#include "Defines.h"
#include "modem/NullModem.h"
#include "Log.h"

using namespace modem;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the NullModem class.
/// </summary>
/// <param name="port">Serial port the modem DSP is connected to.</param>
/// <param name="duplex">Flag indicating the modem is operating in duplex mode.</param>
/// <param name="rxInvert">Flag indicating the Rx polarity should be inverted.</param>
/// <param name="txInvert">Flag indicating the Tx polarity should be inverted.</param>
/// <param name="pttInvert">Flag indicating the PTT polarity should be inverted.</param>
/// <param name="dcBlocker">Flag indicating whether the DSP DC-level blocking should be enabled.</param>
/// <param name="cosLockout">Flag indicating whether the COS signal should be used to lockout the modem.</param>
/// <param name="txDelay">Compensation for transmitter to settle in ms.</param>
/// <param name="dmrDelay">Compensate for delay in transmitter audio chain in ms. Usually DSP based.</param>
/// <param name="disableOFlowReset">Flag indicating whether the ADC/DAC overflow reset logic is disabled.</param>
/// <param name="trace">Flag indicating whether modem DSP trace is enabled.</param>
/// <param name="debug">Flag indicating whether modem DSP debug is enabled.</param>
NullModem::NullModem(const std::string& port, bool duplex, bool rxInvert, bool txInvert, bool pttInvert, bool dcBlocker,
    bool cosLockout, uint32_t txDelay, uint32_t dmrDelay, bool disableOFlowReset, bool trace, bool debug) :
    Modem(port, duplex, rxInvert, txInvert, pttInvert, dcBlocker, cosLockout, txDelay, dmrDelay, disableOFlowReset, trace, debug)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the NullModem class.
/// </summary>
NullModem::~NullModem()
{
    /* stub */
}

/// <summary>
/// Opens connection to the modem DSP.
/// </summary>
/// <returns>True, if connection to modem is established, otherwise false.</returns>
bool NullModem::open()
{
    LogMessage(LOG_MODEM, "Initializing NULL modem");
    return true;
}
