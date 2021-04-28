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
*   Copyright (C) 2019 by Jonathan Naylor G4KLX
*   Copyright (C) 2019 by Bryan Biedenkapp N2PLL
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
#if !defined(__REMOTE_CONTROL_H__)
#define __REMOTE_CONTROL_H__

#include "Defines.h"
#include "network/UDPSocket.h"
#include "dmr/Control.h"
#include "p25/Control.h"
#include "host/Host.h"
#include "lookups/RadioIdLookup.h"
#include "lookups/TalkgroupIdLookup.h"

#include <vector>
#include <string>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------
class HOST_SW_API Host;
namespace dmr { class HOST_SW_API Control; }
namespace p25 { class HOST_SW_API Control; }

// ---------------------------------------------------------------------------
//  Class Declaration
//      Implements the remote control networking logic.
// ---------------------------------------------------------------------------

class HOST_SW_API RemoteControl {
public:
    /// <summary>Initializes a new instance of the RemoteControl class.</summary>
    RemoteControl(const std::string& address, uint16_t port, const std::string& password, bool debug);
    /// <summary>Finalizes a instance of the RemoteControl class.</summary>
    ~RemoteControl();

    /// <summary>Sets the instances of the Radio ID and Talkgroup ID lookup tables.</summary>
    void setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupIdLookup* tidLookup);

    /// <summary>Process remote network command data.</summary>
    void process(Host* host, dmr::Control* dmr, p25::Control* p25);

    /// <summary>Opens connection to the network.</summary>
    bool open();

    /// <summary>Closes connection to the network.</summary>
    void close();

private:
    network::UDPSocket m_socket;
    uint8_t m_p25MFId;

    std::string m_password;
    uint8_t* m_passwordHash;
    bool m_debug;

    lookups::RadioIdLookup* m_ridLookup;
    lookups::TalkgroupIdLookup* m_tidLookup;

    /// <summary></summary>
    std::string getArgString(std::vector<std::string> args, uint32_t n) const;

    /// <summary></summary>
    uint64_t getArgUInt64(std::vector<std::string> args, uint32_t n) const;
    /// <summary></summary>
    uint32_t getArgUInt32(std::vector<std::string> args, uint32_t n) const;
    /// <summary></summary>
    int32_t getArgInt32(std::vector<std::string> args, uint32_t n) const;
    /// <summary></summary>
    uint16_t getArgUInt16(std::vector<std::string> args, uint32_t n) const;
    /// <summary></summary>
    int16_t getArgInt16(std::vector<std::string> args, uint32_t n) const;
    /// <summary></summary>
    uint8_t getArgUInt8(std::vector<std::string> args, uint32_t n) const;
    /// <summary></summary>
    int8_t getArgInt8(std::vector<std::string> args, uint32_t n) const;
};

#endif // __REMOTE_CONTROL_H__
