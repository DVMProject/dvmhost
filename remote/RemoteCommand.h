/**
* Digital Voice Modem - Remote Command Client
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Remote Command Client
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2019 by Jonathan Naylor G4KLX
*   Copyright (C) 2019 by Bryan Biedenkapp <gatekeep@gmail.com>
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
#if !defined(__REMOTE_COMMAND_H__)
#define __REMOTE_COMMAND_H__

#include "Defines.h"

#include <string>

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the core remote command logic.
// ---------------------------------------------------------------------------

class HOST_SW_API CRemoteCommand
{
public:
    /// <summary>Initializes a new instance of the CRemoteCommand class.</summary>
    CRemoteCommand(const std::string& address, uint32_t port, const std::string& password);
    /// <summary>Finalizes a instance of the CRemoteCommand class.</summary>
    ~CRemoteCommand();

    /// <summary>Sends remote control command to the specified modem.</summary>
    int send(const std::string& command);

private:
    std::string m_address;
    uint32_t m_port;
    std::string m_password;
};

#endif // __REMOTE_COMMAND_H__
