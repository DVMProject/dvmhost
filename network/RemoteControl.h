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
*   Copyright (C) 2019-2023 by Bryan Biedenkapp N2PLL
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
#include "lookups/RadioIdLookup.h"
#include "lookups/TalkgroupIdLookup.h"

#include <vector>
#include <string>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define RCD_GET_VERSION                 "version"
#define RCD_GET_HELP                    "help"
#define RCD_GET_STATUS                  "status"
#define RCD_GET_VOICE_CH                "voice-ch"

#define RCD_MODE                        "mdm-mode"
#define RCD_MODE_OPT_IDLE               "idle"
#define RCD_MODE_OPT_LCKOUT             "lockout"
#define RCD_MODE_OPT_FDMR               "dmr"
#define RCD_MODE_OPT_FP25               "p25"
#define RCD_MODE_OPT_FNXDN              "nxdn"

#define RCD_KILL                        "mdm-kill"
#define RCD_FORCE_KILL                  "mdm-force-kill"

#define RCD_PERMIT_TG                   "permit-tg"
#define RCD_GRANT_TG                    "grant-tg"

#define RCD_RID_WLIST                   "rid-whitelist"
#define RCD_RID_BLIST                   "rid-blacklist"

#define RCD_DMR_BEACON                  "dmr-beacon"
#define RCD_P25_CC                      "p25-cc"
#define RCD_P25_CC_FALLBACK             "p25-cc-fallback"

#define RCD_DMR_RID_PAGE                "dmr-rid-page"
#define RCD_DMR_RID_CHECK               "dmr-rid-check"
#define RCD_DMR_RID_INHIBIT             "dmr-rid-inhibit"
#define RCD_DMR_RID_UNINHIBIT           "dmr-rid-uninhibit"

#define RCD_P25_SET_MFID                "p25-set-mfid"
#define RCD_P25_RID_PAGE                "p25-rid-page"
#define RCD_P25_RID_CHECK               "p25-rid-check"
#define RCD_P25_RID_INHIBIT             "p25-rid-inhibit"
#define RCD_P25_RID_UNINHIBIT           "p25-rid-uninhibit"
#define RCD_P25_RID_GAQ                 "p25-rid-gaq"
#define RCD_P25_RID_UREG                "p25-rid-ureg"

#define RCD_P25_RELEASE_GRANTS          "p25-rel-grnts"
#define RCD_P25_RELEASE_AFFS            "p25-rel-affs"

#define RCD_DMR_CC_DEDICATED            "dmr-cc-dedicated"
#define RCD_DMR_CC_BCAST                "dmr-cc-bcast"

#define RCD_P25_CC_DEDICATED            "p25-cc-dedicated"
#define RCD_P25_CC_BCAST                "p25-cc-bcast"

#define RCD_DMR_DEBUG                   "dmr-debug"
#define RCD_DMR_DUMP_CSBK               "dmr-dump-csbk"
#define RCD_P25_DEBUG                   "p25-debug"
#define RCD_P25_DUMP_TSBK               "p25-dump-tsbk"
#define RCD_NXDN_DEBUG                  "nxdn-debug"
#define RCD_NXDN_DUMP_RCCH              "nxdn-dump-rcch"

#define RCD_DMRD_MDM_INJ                "debug-dmrd-mdm-inj"
#define RCD_P25D_MDM_INJ                "debug-p25d-mdm-inj"
#define RCD_NXDD_MDM_INJ                "debug-nxdd-mdm-inj"

#define RCD_PING                        "ping"

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

class HOST_SW_API Host;
namespace dmr { class HOST_SW_API Control; }
namespace p25 { class HOST_SW_API Control; }
namespace nxdn { class HOST_SW_API Control; }

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
    void setLookups(::lookups::RadioIdLookup* ridLookup, ::lookups::TalkgroupIdLookup* tidLookup);

    /// <summary>Process remote network command data.</summary>
    void process(Host* host, dmr::Control* dmr, p25::Control* p25, nxdn::Control* nxdn);

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

    ::lookups::RadioIdLookup* m_ridLookup;
    ::lookups::TalkgroupIdLookup* m_tidLookup;

    /// <summary>Helper to send response to client.</summary>
    void writeResponse(std::string reply, sockaddr_storage address, uint32_t addrLen);

    /// <summary>Helper to print the remote control help.</summary>
    std::string displayHelp();

    /// <summary></summary>
    std::string rcdGetStatus(Host* host, dmr::Control* dmr, p25::Control* p25, nxdn::Control* nxdn);
    /// <summary></summary>
    std::string rcdGetVoiceCh(Host* host, dmr::Control* dmr, p25::Control* p25, nxdn::Control* nxdn);
    /// <summary></summary>
    std::string rcdMode(std::vector<std::string> args, Host* host, dmr::Control* dmr, p25::Control* p25, nxdn::Control* nxdn);

    /// <summary></summary>
    std::string rcdPermitTG(std::vector<std::string> args, Host* host, dmr::Control* dmr, p25::Control* p25, nxdn::Control* nxdn);
    /// <summary></summary>
    std::string rcdGrantTG(std::vector<std::string> args, Host* host, dmr::Control* dmr, p25::Control* p25, nxdn::Control* nxdn);

    /// <summary></summary>
    std::string rcdDMRModemInj(std::vector<std::string> args, Host* host, dmr::Control* dmr);
    /// <summary></summary>
    std::string rcdP25ModemInj(std::vector<std::string> args, Host* host, p25::Control* p25);
    /// <summary></summary>
    std::string rcdNXDNModemInj(std::vector<std::string> args, Host* host, nxdn::Control* nxdn);

    /// <summary></summary>
    std::string getArgString(std::vector<std::string> args, uint32_t n) const;

    /// <summary></summary>
    __forceinline uint64_t getArgUInt64(std::vector<std::string> args, uint32_t n) const { return (uint64_t)::atol(getArgString(args, n).c_str()); }
    /// <summary></summary>
    __forceinline uint32_t getArgUInt32(std::vector<std::string> args, uint32_t n) const { return (uint32_t)::atoi(getArgString(args, n).c_str()); }
    /// <summary></summary>
    __forceinline int32_t getArgInt32(std::vector<std::string> args, uint32_t n) const { return ::atoi(getArgString(args, n).c_str()); }
    /// <summary></summary>
    __forceinline uint16_t getArgUInt16(std::vector<std::string> args, uint32_t n) const { return (uint16_t)::atoi(getArgString(args, n).c_str()); }
    /// <summary></summary>
    __forceinline int16_t getArgInt16(std::vector<std::string> args, uint32_t n) const { return (int16_t)::atoi(getArgString(args, n).c_str()); }
    /// <summary></summary>
    __forceinline uint8_t getArgUInt8(std::vector<std::string> args, uint32_t n) const { return (uint8_t)::atoi(getArgString(args, n).c_str()); }
    /// <summary></summary>
    __forceinline int8_t getArgInt8(std::vector<std::string> args, uint32_t n) const { return (int8_t)::atoi(getArgString(args, n).c_str()); }
};

#endif // __REMOTE_CONTROL_H__
