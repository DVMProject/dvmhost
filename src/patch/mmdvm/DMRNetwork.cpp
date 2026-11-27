// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - TG Patch
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017,2018,2020,2026 by Jonathan Naylor G4KLX
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "patch/Defines.h"
#include "common/dmr/DMRDefines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "mmdvm/DMRNetwork.h"

using namespace dmr::defines;
using namespace network::udp;
using namespace mmdvm;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t HOMEBREW_DMRD_LENGTH = 55U;
const uint32_t HOMEBREW_BUFFER_LENGTH = 100U;
const uint32_t HOMEBREW_TX_BUFFER_LENGTH = 11000U;
const uint32_t DMR_BURST_INTERVAL_MS = 60U;
const uint32_t DMR_PING_INTERVAL_MS = 10000U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the DMRNetwork class. */

DMRNetwork::DMRNetwork(const std::string& gatewayAddress, uint16_t gatewayPort, uint16_t localPort, uint32_t repeaterId, bool debug) :
    m_socket(localPort),
    m_addr(),
    m_addrLen(0U),
    m_repeaterId(repeaterId),
    m_debug(debug),
    m_rxBuffer(6000U, "MMDVM DMR Rx Network"),
    m_txBuffer(HOMEBREW_TX_BUFFER_LENGTH, "MMDVM DMR Tx Network"),
    m_txElapsed(DMR_BURST_INTERVAL_MS),
    m_pingElapsed(0U),
    m_streamId{ 0U, 0U },
    m_random(std::random_device{}())
{
    if (Socket::lookup(gatewayAddress, gatewayPort, m_addr, m_addrLen) != 0)
        m_addrLen = 0U;
}

/* Finalizes an instance of the DMRNetwork class. */

DMRNetwork::~DMRNetwork() = default;

/* Reads one received Homebrew DMRD record. */

bool DMRNetwork::read(dmr::data::NetData& data, uint32_t& streamId)
{
    if (m_rxBuffer.isEmpty())
        return false;

    uint8_t length = 0U;
    m_rxBuffer.get(&length, 1U);
    if (length != HOMEBREW_DMRD_LENGTH)
        return false;

    uint8_t buffer[HOMEBREW_DMRD_LENGTH];
    m_rxBuffer.get(buffer, length);

    data.setSeqNo(buffer[4U]);
    uint32_t srcId = GET_UINT24(buffer, 5U);
    uint32_t dstId = GET_UINT24(buffer, 8U);
    data.setSrcId(srcId);
    data.setDstId(dstId);
    data.setSlotNo((buffer[15U] & 0x80U) == 0x80U ? 2U : 1U);
    data.setFLCO((buffer[15U] & 0x40U) == 0x40U ? FLCO::PRIVATE : FLCO::GROUP);
    data.setBER(buffer[53U]);
    data.setRSSI(buffer[54U]);
    data.setData(buffer + 20U);

    if ((buffer[15U] & 0x20U) == 0x20U) {
        data.setDataType((DataType::E)(buffer[15U] & 0x0FU));
        data.setN(0U);
    }
    else if ((buffer[15U] & 0x10U) == 0x10U) {
        data.setDataType(DataType::VOICE_SYNC);
        data.setN(0U);
    }
    else {
        data.setDataType(DataType::VOICE);
        data.setN(buffer[15U] & 0x0FU);
    }

    ::memcpy(&streamId, buffer + 16U, sizeof(uint32_t));
    return true;
}

/* Queues one DMR burst for paced Homebrew transmission. */

bool DMRNetwork::write(const dmr::data::NetData& data)
{
    uint32_t slotNo = data.getSlotNo();
    if (slotNo < 1U || slotNo > 2U)
        return false;

    uint32_t slotIndex = slotNo - 1U;
    DataType::E dataType = data.getDataType();
    if (dataType == DataType::VOICE_LC_HEADER || m_streamId[slotIndex] == 0U)
        m_streamId[slotIndex] = createStreamId();

    uint8_t buffer[HOMEBREW_DMRD_LENGTH];
    ::memset(buffer, 0x00U, HOMEBREW_DMRD_LENGTH);
    ::memcpy(buffer, "DMRD", 4U);

    buffer[4U] = data.getSeqNo();
    SET_UINT24(data.getSrcId(), buffer, 5U);
    SET_UINT24(data.getDstId(), buffer, 8U);
    SET_UINT32(m_repeaterId, buffer, 11U);

    buffer[15U] = slotNo == 2U ? 0x80U : 0x00U;
    if (data.getFLCO() != FLCO::GROUP)
        buffer[15U] |= 0x40U;

    if (dataType == DataType::VOICE_SYNC)
        buffer[15U] |= 0x10U;
    else if (dataType == DataType::VOICE)
        buffer[15U] |= data.getN() & 0x0FU;
    else
        buffer[15U] |= 0x20U | dataType;

    ::memcpy(buffer + 16U, &m_streamId[slotIndex], sizeof(uint32_t));
    data.getData(buffer + 20U);
    buffer[53U] = data.getBER();
    buffer[54U] = data.getRSSI();

    if (!m_txBuffer.hasSpace(HOMEBREW_DMRD_LENGTH)) {
        LogError(LOG_NET, "MMDVM DMR, transmit queue overflow");
        return false;
    }

    if (m_txBuffer.isEmpty())
        m_txElapsed = DMR_BURST_INTERVAL_MS;

    bool ret = m_txBuffer.addData(buffer, HOMEBREW_DMRD_LENGTH);
    if (dataType == DataType::TERMINATOR_WITH_LC)
        m_streamId[slotIndex] = 0U;

    return ret;
}

/* Clocks receive, ping, and paced transmit processing. */

void DMRNetwork::clock(uint32_t ms)
{
    if (m_txBuffer.isEmpty())
        m_txElapsed = DMR_BURST_INTERVAL_MS;
    else
        m_txElapsed += ms;

    if (m_txElapsed >= DMR_BURST_INTERVAL_MS && !m_txBuffer.isEmpty()) {
        uint8_t buffer[HOMEBREW_DMRD_LENGTH];
        m_txBuffer.get(buffer, HOMEBREW_DMRD_LENGTH);

        if (m_debug)
            Utils::dump(1U, "MMDVM DMR Network Data Sent", buffer, HOMEBREW_DMRD_LENGTH);

        if (!m_socket.write(buffer, HOMEBREW_DMRD_LENGTH, m_addr, m_addrLen))
            LogError(LOG_NET, "MMDVM DMR, failed to write paced DMRD record");

        m_txElapsed = 0U;
    }

    m_pingElapsed += ms;
    if (m_pingElapsed >= DMR_PING_INTERVAL_MS) {
        const uint8_t ping[] = { 'D', 'M', 'R', 'P' };
        m_socket.write(ping, sizeof(ping), m_addr, m_addrLen);
        m_pingElapsed = 0U;
    }

    uint8_t buffer[HOMEBREW_BUFFER_LENGTH];
    sockaddr_storage address;
    uint32_t addrLen = 0U;
    int length = m_socket.read(buffer, HOMEBREW_BUFFER_LENGTH, address, addrLen);
    if (length <= 0)
        return;

    if (!Socket::match(m_addr, address)) {
        LogWarning(LOG_NET, "MMDVM DMR, packet received from an invalid source");
        return;
    }

    if (length == 4 && ::memcmp(buffer, "DMRP", 4U) == 0)
        return;

    if (length != (int)HOMEBREW_DMRD_LENGTH || ::memcmp(buffer, "DMRD", 4U) != 0) {
        LogWarning(LOG_NET, "MMDVM DMR, invalid packet, length = %d", length);
        return;
    }

    if (m_debug)
        Utils::dump(1U, "MMDVM DMR Network Data Received", buffer, length);

    uint8_t recordLength = (uint8_t)length;
    if (!m_rxBuffer.hasSpace(recordLength + 1U)) {
        LogError(LOG_NET, "MMDVM DMR, receive queue overflow");
        return;
    }

    m_rxBuffer.addData(&recordLength, 1U);
    m_rxBuffer.addData(buffer, recordLength);
}

/* Opens the UDP transport. */

bool DMRNetwork::open()
{
    if (m_addrLen == 0U) {
        LogError(LOG_NET, "MMDVM DMR, unable to resolve DMRGateway address");
        return false;
    }

    LogInfoEx(LOG_NET, "MMDVM DMR, opening DMRGateway connection");
    return m_socket.open(m_addr);
}

/* Closes the UDP transport. */

void DMRNetwork::close()
{
    m_socket.close();
    LogInfoEx(LOG_NET, "MMDVM DMR, closing DMRGateway connection");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Creates a non-zero Homebrew stream identifier. */

uint32_t DMRNetwork::createStreamId()
{
    std::uniform_int_distribution<uint32_t> distribution(1U, UINT32_MAX);
    return distribution(m_random);
}
