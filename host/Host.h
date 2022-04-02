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
*   Copyright (C) 2017-2021 by Bryan Biedenkapp N2PLL
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
#if !defined(__HOST_H__)
#define __HOST_H__

#include "Defines.h"
#include "network/Network.h"
#include "network/RemoteControl.h"
#include "modem/Modem.h"
#include "Timer.h"
#include "lookups/IdenTableLookup.h"
#include "lookups/RadioIdLookup.h"
#include "lookups/TalkgroupIdLookup.h"
#include "yaml/Yaml.h"

#include <string>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------
class HOST_SW_API RemoteControl;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the core host service logic.
// ---------------------------------------------------------------------------

class HOST_SW_API Host {
public:
    /// <summary>Initializes a new instance of the Host class.</summary>
    Host(const std::string& confFile);
    /// <summary>Finalizes a instance of the Host class.</summary>
    ~Host();

    /// <summary>Executes the main modem host processing loop.</summary>
    int run();

private:
    const std::string& m_confFile;
    yaml::Node m_conf;

    modem::Modem* m_modem;
    bool m_modemRemote;
    network::Network* m_network;

    modem::port::IModemPort* m_modemRemotePort;

    uint8_t m_state;
    
    Timer m_modeTimer;
    Timer m_dmrTXTimer;
    Timer m_cwIdTimer;

    bool m_dmrEnabled;
    bool m_p25Enabled;

    bool m_p25CtrlChannel;
    bool m_p25CtrlBroadcast;
    bool m_dmrCtrlChannel;

    bool m_duplex;
    bool m_fixedMode;
    bool m_useDFSI;

    uint32_t m_timeout;
    uint32_t m_rfModeHang;
    uint32_t m_rfTalkgroupHang;
    uint32_t m_netModeHang;

    std::string m_identity;
    std::string m_cwCallsign;
    uint32_t m_cwIdTime;

    float m_latitude;
    float m_longitude;
    int m_height;
    uint32_t m_power;
    std::string m_location;

    uint32_t m_rxFrequency;
    uint32_t m_txFrequency;
    uint8_t m_channelId;
    uint32_t m_channelNo;
    std::vector<uint32_t> m_voiceChNo;

    lookups::IdenTableLookup* m_idenTable;
    lookups::RadioIdLookup* m_ridLookup;
    lookups::TalkgroupIdLookup* m_tidLookup;

    bool m_dmrBeacons;
    bool m_dmrTSCCData;
    bool m_controlData;

    uint8_t m_siteId;
    uint32_t m_dmrNetId;
    uint32_t m_dmrColorCode;
    uint32_t m_p25NAC;
    uint32_t m_p25PatchSuperGroup;
    uint32_t m_p25NetId;
    uint32_t m_p25SysId;
    uint8_t m_p25RfssId;

    friend class RemoteControl;
    RemoteControl* m_remoteControl;

    /// <summary>Reads basic configuration parameters from the INI.</summary>
    bool readParams();
    /// <summary>Initializes the modem DSP.</summary>
    bool createModem();
    /// <summary>Initializes network connectivity.</summary>
    bool createNetwork();
    
    /// <summary>Modem port open callback.</summary>
    bool rmtPortModemOpen(modem::Modem* modem);
    /// <summary>Modem port close callback.</summary>
    bool rmtPortModemClose(modem::Modem* modem);
    /// <summary>Modem clock callback.</summary>
    bool rmtPortModemHandler(modem::Modem* modem, uint32_t ms, modem::RESP_TYPE_DVM rspType, bool rspDblLen, const uint8_t* buffer, uint16_t len);

    /// <summary>Helper to set the host/modem running state.</summary>
    void setState(uint8_t mode);

    /// <summary>Helper to create the state lock file.</summary>
    void createLockFile(const char* state) const;
    /// <summary>Helper to remove the state lock file.</summary>
    void removeLockFile() const;
};

#endif // __HOST_H__
