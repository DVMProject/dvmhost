// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/lc/MACPDU.h"
#include "common/p25/P25Defines.h"

using namespace p25::lc;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MACPDU class. */

MACPDU::MACPDU(uint8_t opcode) :
    m_opcode(opcode),
    m_srcId(0U),
    m_dstId(0U),
    m_emergency(false),
    m_encrypted(false),
    m_priority(0U),
    m_group(false)
{
    /* stub */
}

/* Decodes a MAC PDU from the specified LC control data. */

bool MACPDU::decode(const LC& control)
{
    if (control.getLCO() != m_opcode)
        return false;

    m_srcId = control.getSrcId();
    m_dstId = control.getDstId();
    m_group = control.getGroup();
    m_emergency = control.getEmergency();
    m_encrypted = control.getEncrypted();
    m_priority = control.getPriority();
    return true;
}

/* Encodes the MAC PDU into the specified LC control data. */

void MACPDU::encode(LC& control) const
{
    control.setLCO(m_opcode);
    control.setMACPartition(p25::defines::P2_MAC_MCO_PARTITION::UNIQUE);
    control.setSrcId(m_srcId);
    control.setDstId(m_dstId);
    control.setGroup(m_group);
    control.setEmergency(m_emergency);
    control.setEncrypted(m_encrypted);
    control.setPriority(m_priority);
}

/* Returns a string that represents the current MAC PDU. */

std::string MACPDU::toString(bool isp)
{
    return std::string("MAC PDU, UNKNOWN (Unknown MAC PDU)");
}
