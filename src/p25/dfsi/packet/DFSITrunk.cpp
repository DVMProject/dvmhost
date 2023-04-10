/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
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
#include "p25/P25Defines.h"
#include "p25/dfsi/DFSIDefines.h"
#include "p25/packet/Trunk.h"
#include "p25/dfsi/packet/DFSITrunk.h"
#include "p25/P25Utils.h"
#include "p25/Sync.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::dfsi;
using namespace p25::dfsi::packet;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Process a data frame from the RF interface.
/// </summary>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <param name="preDecodedTSBK">Pre-decoded TSBK.</param>
/// <returns></returns>
bool DFSITrunk::process(uint8_t* data, uint32_t len, std::unique_ptr<lc::TSBK> preDecodedTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    if (!m_p25->m_control)
        return false;

    if (preDecodedTSBK != nullptr) {
        return Trunk::process(data + 2U, len, std::move(preDecodedTSBK));
    }
    else {
        if (m_rfDFSILC.decodeTSBK(data + 2U)) {
            return Trunk::process(tsbk, P25_TSBK_LENGTH_BYTES, std::unique_ptr<lc::TSBK>(m_rfDFSILC.tsbk()));
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the DFSITrunk class.
/// </summary>
/// <param name="p25">Instance of the Control class.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="dumpTSBKData">Flag indicating whether TSBK data is dumped to the log.</param>
/// <param name="debug">Flag indicating whether P25 debug is enabled.</param>
/// <param name="verbose">Flag indicating whether P25 verbose logging is enabled.</param>
DFSITrunk::DFSITrunk(Control* p25, network::BaseNetwork* network, bool dumpTSBKData, bool debug, bool verbose) :
    Trunk(p25, network, dumpTSBKData, debug, verbose)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the DFSITrunk class.
/// </summary>
DFSITrunk::~DFSITrunk()
{
    /* stub */
}

/// <summary>
/// Helper to write a P25 TDU w/ link control packet.
/// </summary>
/// <param name="lc"></param>
/// <param name="noNetwork"></param>
void DFSITrunk::writeRF_TDULC(lc::TDULC* lc, bool noNetwork)
{
    // for now this is ignored...
}

/// <summary>
/// Helper to write a single-block P25 TSDU packet.
/// </summary>
/// <param name="tsbk"></param>
/// <param name="noNetwork"></param>
/// <param name="clearBeforeWrite"></param>
/// <param name="force"></param>
void DFSITrunk::writeRF_TSDU_SBF(lc::TSBK* tsbk, bool noNetwork, bool clearBeforeWrite, bool force)
{
    if (!m_p25->m_control)
        return;

    assert(tsbk != nullptr);

    writeRF_DFSI_Start(P25_DFSI_TYPE_TSBK);

    uint8_t data[P25_TSDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, P25_TSDU_FRAME_LENGTH_BYTES);

    m_rfDFSILC.setFrameType(P25_DFSI_TSBK);
    m_rfDFSILC.setStartStop(P25_DFSI_START_FLAG);
    m_rfDFSILC.setType(P25_DFSI_TYPE_TSBK);
    m_rfDFSILC.tsbk(tsbk);

    // Generate Sync
    Sync::addP25Sync(data + 2U);

    // Generate NID
    m_p25->m_nid.encode(data + 2U, P25_DUID_TSDU);

    // Generate TSBK block
    tsbk->setLastBlock(true); // always set last block -- this a Single Block TSDU
    tsbk->encode(data + 2U);

    if (m_debug) {
        LogDebug(LOG_RF, P25_TSDU_STR " DFSI, lco = $%02X, mfId = $%02X, lastBlock = %u, AIV = %u, EX = %u, srcId = %u, dstId = %u, sysId = $%03X, netId = $%05X",
            tsbk->getLCO(), tsbk->getMFId(), tsbk->getLastBlock(), tsbk->getAIV(), tsbk->getEX(), tsbk->getSrcId(), tsbk->getDstId(),
            tsbk->getSysId(), tsbk->getNetId());

        Utils::dump(1U, "!!! *TSDU (SBF) TSBK Block Data", data + P25_PREAMBLE_LENGTH_BYTES + 2U, P25_TSBK_FEC_LENGTH_BYTES);
    }

    // Add busy bits
    P25Utils::addBusyBits(data + 2U, P25_TSDU_FRAME_LENGTH_BITS, true, false);

    // Set first busy bits to 1,1
    P25Utils::setBusyBits(data + 2U, P25_SS0_START, true, true);

    if (!noNetwork)
        writeNetworkRF(tsbk, data + 2U, true);

    if (!force) {
        if (clearBeforeWrite) {
            m_p25->m_modem->clearP25Data();
            m_p25->m_queue.clear();
        }
    }

    ::memset(data + 2U, 0x00U, P25_TSDU_FRAME_LENGTH_BYTES);

    // Generate DFSI TSBK block
    m_rfDFSILC.encodeTSBK(data + 2U);

    data[0U] = modem::TAG_DATA;
    data[1U] = 0x00U;

    m_p25->addFrame(data, P25_DFSI_TSBK_FRAME_LENGTH_BYTES + 2U);

    writeRF_DSFI_Stop(P25_DFSI_TYPE_TSBK);
}

/// <summary>
/// Helper to write a alternate multi-block trunking PDU packet.
/// </summary>
/// <param name="ambt"></param>
/// <param name="clearBeforeWrite"></param>
void DFSITrunk::writeRF_TSDU_AMBT(lc::AMBT* ambt, bool clearBeforeWrite)
{
    if (!m_p25->m_control)
        return;

    assert(ambt != nullptr);

    // for now this is ignored...
}

/// <summary>
/// Helper to write a network single-block P25 TSDU packet.
/// </summary>
/// <param name="tsbk"></param>
void DFSITrunk::writeNet_TSDU(lc::TSBK* tsbk)
{
    assert(tsbk != nullptr);

    uint8_t buffer[P25_DFSI_TSBK_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_DFSI_TSBK_FRAME_LENGTH_BYTES + 2U);

    buffer[0U] = modem::TAG_DATA;
    buffer[1U] = 0x00U;

    // Regenerate TSDU Data
    m_netDFSILC.tsbk(tsbk);
    m_netDFSILC.encodeTSBK(buffer + 2U);

    m_p25->addFrame(buffer, P25_DFSI_TSBK_FRAME_LENGTH_BYTES + 2U, true);

    if (m_network != nullptr)
        m_network->resetP25();
}

/// <summary>
/// Helper to write start DFSI data.
/// </summary>
/// <param name="type"></param>
void DFSITrunk::writeRF_DFSI_Start(uint8_t type)
{
    uint8_t buffer[P25_DFSI_SS_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_DFSI_SS_FRAME_LENGTH_BYTES + 2U);

    // Generate Start/Stop
    m_rfDFSILC.setFrameType(P25_DFSI_START_STOP);
    m_rfDFSILC.setStartStop(P25_DFSI_START_FLAG);
    m_rfDFSILC.setType(type);

    // Generate Identifier Data
    m_rfDFSILC.encodeNID(buffer + 2U);

    buffer[0U] = modem::TAG_DATA;
    buffer[1U] = 0x00U;

    m_p25->addFrame(buffer, P25_DFSI_SS_FRAME_LENGTH_BYTES + 2U);
}

/// <suimmary>
/// Helper to write stop DFSI data.
/// </summary>
/// <param name="type"></param>
void DFSITrunk::writeRF_DSFI_Stop(uint8_t type)
{
    uint8_t buffer[P25_DFSI_SS_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_DFSI_SS_FRAME_LENGTH_BYTES + 2U);

    // Generate Start/Stop
    m_rfDFSILC.setFrameType(P25_DFSI_START_STOP);
    m_rfDFSILC.setStartStop(P25_DFSI_STOP_FLAG);
    m_rfDFSILC.setType(type);

    // Generate Identifier Data
    m_rfDFSILC.encodeNID(buffer + 2U);

    buffer[0U] = modem::TAG_EOT;
    buffer[1U] = 0x00U;

    // for whatever reason this is almost always sent twice
    for (uint8_t i = 0; i < 2;i ++) {
        m_p25->addFrame(buffer, P25_DFSI_SS_FRAME_LENGTH_BYTES + 2U);
    }
}
