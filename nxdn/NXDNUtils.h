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
*   Copyright (C) 2021 by Bryan Biedenkapp N2PLL
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
#if !defined(__NXDN_UTILS_H__)
#define __NXDN_UTILS_H__

#include "Defines.h"
#include "nxdn/NXDNDefines.h"

#include <cstring>

namespace nxdn
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements various helper functions for NXDN.
    // ---------------------------------------------------------------------------

    class HOST_SW_API NXDNUtils {
    public:
        /// <summary>Helper to convert a NXDN message type to a textual string.</summary>
        static const char* messageTypeToString(uint8_t messageType, bool rcch = false, bool outbound = false)
        {
            std::string ret = std::string();
            switch (messageType)
            {
                case RTCH_MESSAGE_TYPE_VCALL:
                    if (rcch) {
                        if (outbound) {
                            ret = "VCALL_RESP (Voice Call Response)";
                        }
                        else {
                            ret = "VCALL_REQ (Voice Call Request)";
                        }
                    }
                    else {
                        ret = "VCALL (Voice Call)";
                    }
                    break;
                case RTCH_MESSAGE_TYPE_VCALL_IV:
                    if (rcch) {
                        if (outbound) {
                            ret = "VCALL_CONN_RESP (Voice Call Connection Response)";
                        } 
                        else {
                            ret = "VCALL_CONN_REQ (Voice Call Connection Request)";
                        }
                    }
                    else {
                        ret = "VCALL_IV (Voice Call Init Vector)";
                    }
                    break;
                case RCCH_MESSAGE_TYPE_VCALL_ASSGN:
                    if (rcch) {
                        if (outbound) {
                            ret = "VCALL_ASSGN (Voice Call Assignment)";
                        }
                    }
                    break;
                case RTCH_MESSAGE_TYPE_TX_REL_EX:
                    ret = "TX_REL_EX (Transmission Release Extension)";
                    break;
                case RTCH_MESSAGE_TYPE_TX_REL:
                    ret = "TX_REL (Transmission Release)";
                    break;
                case RTCH_MESSAGE_TYPE_DCALL_HDR:
                    if (rcch) {
                        if (outbound) {
                            ret = "DCALL_RESP (Data Call Response)";
                        }
                        else {
                            ret = "DCALL_REQ (Data Call Request)";
                        }
                    }
                    else {
                        ret = "DCALL (Data Call Header)";
                    }
                    break;
                case RTCH_MESSAGE_TYPE_DCALL_DATA:
                    ret = "DCALL (Data Call User Data)";
                    break;
                case RTCH_MESSAGE_TYPE_DCALL_ACK:
                    ret = "DCALL_ACL (Data Call Acknowledge)";
                    break;
                case RTCH_MESSAGE_TYPE_HEAD_DLY:
                    ret = "HEAD_DLY (Header Delay)";
                    break;
                case MESSAGE_TYPE_IDLE:
                    ret = "IDLE (Idle)";
                    break;
                case MESSAGE_TYPE_DISC:
                    ret = "DISC (Disconnect)";
                    break;
                case RCCH_MESSAGE_TYPE_DCALL_ASSGN:
                    if (outbound) {
                        ret = "DCALL_ASSGN (Data Call Assignment)";
                    }
                    break;
                case MESSAGE_TYPE_DST_ID_INFO:
                    ret = "DST_ID_INFO (Digital Station ID)";
                    break;
                case RCCH_MESSAGE_TYPE_SITE_INFO:
                    ret = "SITE_INFO (Site Information)";
                    break;
                case MESSAGE_TYPE_SRV_INFO:
                    ret = "SRV_INFO (Service Information)";
                    break;
                case MESSAGE_TYPE_CCH_INFO:
                    ret = "CCH_INFO (Control Channel Information)";
                    break;
                case MESSAGE_TYPE_ADJ_SITE_INFO:
                    ret = "ADJ_SITE_INFO (Adjacent Site Information)";
                    break;
                case RCCH_MESSAGE_TYPE_REG:
                    if (outbound) {
                        ret = "REG_RESP (Registration Response)";
                    }
                    else {
                        ret = "REG_REQ (Registration Request)";
                    }
                    break;
                case RCCH_MESSAGE_TYPE_REG_C:
                    if (outbound) {
                        ret = "REG_C_RESP (Registration Clear Response)";
                    }
                    else {
                        ret = "REG_C_REQ (Registration Clear Request)";
                    }
                    break;
                case RCCH_MESSAGE_TYPE_REG_COMM:
                    ret = "REG_COMM (Registration Command)";
                    break;
                case RCCH_MESSAGE_TYPE_GRP_REG:
                    if (outbound) {
                        ret = "GRP_REG_RESP (Group Registration Response)";
                    }
                    else {
                        ret = "GRP_REG_REQ (Group Registration Request)";
                    }
                    break;
                case RCCH_MESSAGE_TYPE_PROP_FORM:
                    ret = "PROP_FORM (Proprietary Form)";
                    break;
                default:
                    ret = "UNKNOWN - Unknown";
                    break;
            }

            return ret.c_str();
        }
    };
} // namespace nxdn

#endif // __NXDN_UTILS_H__
