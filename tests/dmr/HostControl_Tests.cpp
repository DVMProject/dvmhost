// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "host/Defines.h"
#include "common/lookups/ChannelLookup.h"
#include "common/lookups/IdenTableLookup.h"
#include "common/lookups/RSSIInterpolator.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/network/Network.h"
#include "common/network/NetRPC.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/Sync.h"
#include "common/dmr/SlotType.h"
#include "common/dmr/lc/LC.h"
#include "common/dmr/lc/FullLC.h"
#include "common/dmr/data/NetData.h"
#include "host/modem/Modem.h"
#include "modem/port/IModemPort.h"
#include "host/HostTestHooks.h"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstring>
#include <thread>

extern network::NetRPC* g_RPC;
#include "host/dmr/Control.h"

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------
namespace {

/**
 * @brief Helper function to calculate the number of timer ticks needed to expire a given timer.
 * @param timer The timer for which to calculate the expiration ticks.
 * @returns uint32_t The number of timer ticks needed to expire the timer.
 */
uint32_t expireTimerTicks(const Timer& timer)
{
    return (timer.getTimeout() + 1U) * 1000U;
}

/**
 * @brief Finds an available loopback UDP port.
 * @returns uint16_t A free UDP port, or 0 on failure.
 */
uint16_t reserveLoopbackPort()
{
#if defined(_WIN32)
    SOCKET fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == INVALID_SOCKET)
        return 0U;
#else
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return 0U;
#endif // defined(_WIN32)

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(0U);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
#if defined(_WIN32)
        ::closesocket(fd);
#else
        ::close(fd);
#endif // defined(_WIN32)
        return 0U;
    }

    socklen_t addrLen = sizeof(address);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&address), &addrLen) < 0) {
#if defined(_WIN32)
        ::closesocket(fd);
#else
        ::close(fd);
#endif // defined(_WIN32)
        return 0U;
    }

#if defined(_WIN32)
    ::closesocket(fd);
#else
    ::close(fd);
#endif // defined(_WIN32)

    return ntohs(address.sin_port);
}

/**
 * @brief Builds a DMR voice header payload.
 * @param payload The buffer to store the DMR voice header payload.
 * @param srcId The source ID for the DMR header.
 * @param dstId The destination ID for the DMR header.
 * @param group True if the header is for a group call, false for a private call.
 */
void buildDMRVoiceHeaderPayload(uint8_t* payload, uint32_t srcId, uint32_t dstId, bool group)
{
    using namespace dmr;
    using namespace dmr::defines;

    ::memset(payload, 0x00U, DMR_FRAME_LENGTH_BYTES);
    Sync::addDMRDataSync(payload, false);

    lc::LC lc(group ? FLCO::GROUP : FLCO::PRIVATE, srcId, dstId);
    lc::FullLC fullLC;
    fullLC.encode(lc, payload, DataType::VOICE_LC_HEADER);

    SlotType slotType;
    slotType.setColorCode(1U);
    slotType.setDataType(DataType::VOICE_LC_HEADER);
    slotType.encode(payload);
}

/**
 * @brief Builds a DMR terminator payload.
 * @param payload The buffer to store the DMR terminator payload.
 * @param srcId The source ID for the DMR header.
 * @param dstId The destination ID for the DMR header.
 * @param group True if the header is for a group call, false for a private call.
 */
void buildDMRTerminatorPayload(uint8_t* payload, uint32_t srcId, uint32_t dstId, bool group)
{
    using namespace dmr;
    using namespace dmr::defines;

    ::memset(payload, 0x00U, DMR_FRAME_LENGTH_BYTES);
    Sync::addDMRDataSync(payload, false);

    lc::LC lc(group ? FLCO::GROUP : FLCO::PRIVATE, srcId, dstId);
    lc::FullLC fullLC;
    fullLC.encode(lc, payload, DataType::TERMINATOR_WITH_LC);

    SlotType slotType;
    slotType.setColorCode(1U);
    slotType.setDataType(DataType::TERMINATOR_WITH_LC);
    slotType.encode(payload);
}

/**
 * @brief Builds a DMR voice sync payload.
 * @param payload The buffer to store the DMR voice sync payload.
 */
void buildDMRVoiceSyncPayload(uint8_t* payload)
{
    ::memset(payload, 0x00U, dmr::defines::DMR_FRAME_LENGTH_BYTES);
    dmr::Sync::addDMRAudioSync(payload, false);
}

} // namespace

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Dummy implementation of a modem port for testing purposes.
 */
class DMRTestModemPort final : public modem::port::IModemPort {
public:
    /**
     * @brief Finalizes the instance of the DMRTestModemPort class.
     */
    ~DMRTestModemPort() override = default;

    /**
     * @brief Opens the modem port.
     * @returns bool True if the modem port was successfully opened, false otherwise.
     */
    bool open() override { return true; }

    /**
     * @brief Reads data from the modem port.
     * @param buffer The buffer to store the read data.
     * @param length The number of bytes to read.
     * @returns int The number of bytes actually read.
     */
    int read(uint8_t* buffer, uint32_t length) override 
    { 
        (void)buffer; 
        (void)length; 
        return 0; 
    }

    /**
     * @brief Writes data to the modem port.
     * @param buffer The buffer containing the data to write.
     * @param length The number of bytes to write.
     * @returns int The number of bytes actually written.
     */
    int write(const uint8_t* buffer, uint32_t length) override 
    { 
        (void)buffer; 
        return static_cast<int>(length); 
    }
    
    /**
     * @brief Closes the modem port.
     */
    void close() override {}
};

/**
 * @brief Lightweight network test double that records DMR reset calls.
 */
class DMRTestNetwork final : public network::Network {
public:
    /**
     * @brief Initializes a new instance of the DMRTestNetwork class.
     * @param localPort The local port number.
     * @param peerId The peer ID.
     */
    DMRTestNetwork(uint16_t localPort = 0U, uint32_t peerId = 1U) :
        network::Network("127.0.0.1", 1U, localPort, peerId, "test", true, true, true, false, false, false, true, true, false, false, false, false),
        m_resetDMRCount(0U)
    {
        // keep protocol gates deterministic for this P25-focused harness
        m_dmrEnabled = true;
        m_p25Enabled = false;
        m_nxdnEnabled = false;
        m_analogEnabled = false;
    }

    /**
     * @brief Activates the loopback network connection.
     * @param remoteAddress The remote address to connect to.
     * @param remotePort The remote port to connect to.
     * @returns bool True if the loopback network connection was successfully activated, false otherwise.
     */
    bool activateLoopback(const std::string& remoteAddress, uint16_t remotePort)
    {
        if (network::udp::Socket::lookup(remoteAddress, remotePort, m_addr, m_addrLen) != 0) {
            return false;
        }

        if (!m_socket->open(m_addr.ss_family)) {
            return false;
        }

        m_enabled = true;
        m_status = network::NET_STAT_RUNNING;
        return true;
    }

    /**
     * @brief Sends a DMR network voice LC header frame.
     */
    bool sendDMRVoiceHeader(uint32_t targetPeerId, uint32_t streamId, uint16_t seq, uint32_t slotNo, uint32_t srcId, uint32_t dstId, bool group = true)
    {
        using namespace dmr::defines;

        dmr::data::NetData data;
        data.setSlotNo(slotNo);
        data.setSrcId(srcId);
        data.setDstId(dstId);
        data.setFLCO(group ? FLCO::GROUP : FLCO::PRIVATE);
        data.setControl(0x00U);
        data.setN(0U);
        data.setSeqNo((uint8_t)(seq & 0xFFU));
        data.setDataType(DataType::VOICE_LC_HEADER);

        uint8_t payload[DMR_FRAME_LENGTH_BYTES];
        buildDMRVoiceHeaderPayload(payload, srcId, dstId, group);
        data.setData(payload);

        uint32_t messageLength = 0U;
        UInt8Array message = createDMR_Message(messageLength, streamId, data);
        if (message == nullptr || messageLength == 0U) {
            return false;
        }

        return m_frameQueue->write(message.get(), messageLength, streamId, targetPeerId, m_peerId,
            { network::NET_FUNC::PROTOCOL, network::NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, seq, m_addr, m_addrLen);
    }

    /**
     * @brief Sends a DMR network voice sync frame.
     */
    bool sendDMRVoiceSync(uint32_t targetPeerId, uint32_t streamId, uint16_t seq, uint32_t slotNo, uint32_t srcId, uint32_t dstId, bool group = true)
    {
        using namespace dmr::defines;

        dmr::data::NetData data;
        data.setSlotNo(slotNo);
        data.setSrcId(srcId);
        data.setDstId(dstId);
        data.setFLCO(group ? FLCO::GROUP : FLCO::PRIVATE);
        data.setControl(0x00U);
        data.setN(0U);
        data.setSeqNo((uint8_t)(seq & 0xFFU));
        data.setDataType(DataType::VOICE_SYNC);

        uint8_t payload[DMR_FRAME_LENGTH_BYTES];
        buildDMRVoiceSyncPayload(payload);
        data.setData(payload);

        uint32_t messageLength = 0U;
        UInt8Array message = createDMR_Message(messageLength, streamId, data);
        if (message == nullptr || messageLength == 0U) {
            return false;
        }

        return m_frameQueue->write(message.get(), messageLength, streamId, targetPeerId, m_peerId,
            { network::NET_FUNC::PROTOCOL, network::NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, seq, m_addr, m_addrLen);
    }

    /**
     * @brief Sends a DMR network terminator frame.
     */
    bool sendDMRTerminator(uint32_t targetPeerId, uint32_t streamId, uint16_t seq, uint32_t slotNo, uint32_t srcId, uint32_t dstId, bool group = true)
    {
        using namespace dmr::defines;

        dmr::data::NetData data;
        data.setSlotNo(slotNo);
        data.setSrcId(srcId);
        data.setDstId(dstId);
        data.setFLCO(group ? FLCO::GROUP : FLCO::PRIVATE);
        data.setControl(0x00U);
        data.setN(0U);
        data.setSeqNo((uint8_t)(seq & 0xFFU));
        data.setDataType(DataType::TERMINATOR_WITH_LC);

        uint8_t payload[DMR_FRAME_LENGTH_BYTES];
        buildDMRTerminatorPayload(payload, srcId, dstId, group);
        data.setData(payload);

        uint32_t messageLength = 0U;
        UInt8Array message = createDMR_Message(messageLength, streamId, data);
        if (message == nullptr || messageLength == 0U) {
            return false;
        }

        return m_frameQueue->write(message.get(), messageLength, streamId, targetPeerId, m_peerId,
            { network::NET_FUNC::PROTOCOL, network::NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, seq, m_addr, m_addrLen);
    }

    /**
     * @brief
     */
    void resetDMR(uint32_t slotNo) override
    {
        ++m_resetDMRCount;
        network::Network::resetDMR(slotNo);
    }

    /**
     * @brief Returns the number of times the DMR subsystem has been reset.
     * @returns uint32_t The number of times the DMR subsystem has been reset.
     */
    uint32_t resetDMRCount() const
    {
        return m_resetDMRCount;
    }

    /**
     * @brief Returns the currently locked incoming DMR stream ID for a slot.
     * @param slotNo Logical DMR slot number (1 or 2).
     * @returns uint32_t Active incoming stream lock ID, or 0 when unlocked.
     */
    uint32_t rxDMRStreamId(uint32_t slotNo) const
    {
        return m_rxDMRStreamId[slotNo - 1U];
    }

private:
    uint32_t m_resetDMRCount;
};

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Harness class for testing DMR host control functionality.
 */
class DMRHostHarness {
public:
    /**
     * @brief Initializes a new instance of the DMRHostHarness class.
     * @param authoritative Indicates whether the host is authoritative.
     */
    explicit DMRHostHarness(bool authoritative = true, bool withNetwork = false, uint16_t networkLocalPort = 0U, uint32_t networkPeerId = 1U) :
        m_rpc("127.0.0.1", 1U, 0U, "test", false),
        m_modem(new DMRTestModemPort(), false, false, false, false, false, false,
            0U, 0U, 0U, 4096U, 4096U, 1024U, true, true, false, false, false, false),
        m_chLookup(),
        m_ridLookup("", 0U, false, false),
        m_tidLookup("", 0U, false, false),
        m_idenLookup("", 0U),
        m_rssiMapper(),
        m_network(withNetwork ? new DMRTestNetwork(networkLocalPort, networkPeerId) : nullptr),
        m_control(nullptr)
    {
        g_RPC = &m_rpc;
        m_modem.setModeParams(true, false, false);

        m_control = new dmr::Control(authoritative, 1U, 1U, 4096U, false, false, 5U, 2U,
            &m_modem, m_network, false, &m_chLookup, &m_ridLookup, &m_tidLookup, &m_idenLookup,
            &m_rssiMapper, 60U, false, false, false, true, true);
    }
    /**
     * @brief Finalizes an instance of the DMRHostHarness class.
     */
    ~DMRHostHarness()
    {
        delete m_control;
        delete m_network;
        g_RPC = nullptr;
    }

    DMRTestNetwork* network() const
    {
        return m_network;
    }

public:
    network::NetRPC m_rpc;
    modem::Modem m_modem;
    lookups::ChannelLookup m_chLookup;
    lookups::RadioIdLookup m_ridLookup;
    lookups::TalkgroupRulesLookup m_tidLookup;
    lookups::IdenTableLookup m_idenLookup;
    lookups::RSSIInterpolator m_rssiMapper;
    DMRTestNetwork* m_network;
    dmr::Control* m_control;
};

TEST_CASE("DMR host e2e loopback handles missed frames without dropping active call", "[dmr][host][control][net][e2e]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 7001U;
    const uint32_t streamId = 0x610001U;

    DMRHostHarness harness(true, true, hostPort, hostPeerId);
    DMRTestNetwork sender(senderPort, 7002U);

    REQUIRE(harness.network() != nullptr);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    REQUIRE(sender.sendDMRVoiceHeader(hostPeerId, streamId, 100U, 1U, 1001U, 2001U));

    for (uint32_t i = 0U; i < 40U; i++) {
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_AUDIO) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_AUDIO);
    REQUIRE(harness.m_control->getLastSrcId(1U) == 1001U);
    REQUIRE(harness.m_control->getLastDstId(1U) == 2001U);

    // Skip one RTP sequence (101) to emulate a missing network frame.
    REQUIRE(sender.sendDMRVoiceSync(hostPeerId, streamId, 102U, 1U, 1001U, 2001U));

    for (uint32_t i = 0U; i < 20U; i++) {
        harness.network()->clock(1U);
        harness.m_control->clock();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::dmrNetworkWatchdog(*HostTestHooks::dmrSlot1(*harness.m_control)).isRunning());
    REQUIRE(harness.m_control->getLastSrcId(1U) == 1001U);
    REQUIRE(harness.m_control->getLastDstId(1U) == 2001U);
}

TEST_CASE("DMR host e2e loopback handles dropped call terminator and returns idle", "[dmr][host][control][net][e2e]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 7003U;
    const uint32_t streamId = 0x610002U;

    DMRHostHarness harness(true, true, hostPort, hostPeerId);
    DMRTestNetwork sender(senderPort, 7004U);

    REQUIRE(harness.network() != nullptr);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    REQUIRE(sender.sendDMRVoiceHeader(hostPeerId, streamId, 200U, 1U, 1101U, 2101U));

    for (uint32_t i = 0U; i < 40U; i++) {
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_AUDIO) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_AUDIO);
    REQUIRE(sender.sendDMRTerminator(hostPeerId, streamId, RTP_END_OF_CALL_SEQ, 1U, 1101U, 2101U));

    for (uint32_t i = 0U; i < 50U; i++) {
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_IDLE) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_IDLE);
    REQUIRE_FALSE(HostTestHooks::dmrNetworkWatchdog(*HostTestHooks::dmrSlot1(*harness.m_control)).isRunning());
}

TEST_CASE("DMR host e2e loopback times out stale call and resets stream state", "[dmr][host][control][net][e2e]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 7005U;
    const uint32_t streamId = 0x610003U;

    DMRHostHarness harness(true, true, hostPort, hostPeerId);
    DMRTestNetwork sender(senderPort, 7006U);

    REQUIRE(harness.network() != nullptr);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    REQUIRE(sender.sendDMRVoiceHeader(hostPeerId, streamId, 300U, 1U, 1201U, 2201U));

    for (uint32_t i = 0U; i < 40U; i++) {
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_AUDIO) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_AUDIO);
    REQUIRE(harness.network()->resetDMRCount() == 0U);

    HostTestHooks::dmrNetworkWatchdog(*HostTestHooks::dmrSlot1(*harness.m_control)).clock(
        expireTimerTicks(HostTestHooks::dmrNetworkWatchdog(*HostTestHooks::dmrSlot1(*harness.m_control))));
    HostTestHooks::dmrSlot1(*harness.m_control)->clock();

    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_IDLE);
    REQUIRE(harness.network()->resetDMRCount() == 1U);
}

TEST_CASE("DMR host e2e loopback preserves a continuing network stream across an RF collision", "[dmr][host][control][net][e2e]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 7011U;
    const uint32_t streamId = 0x610201U;

    DMRHostHarness harness(true, true, hostPort, hostPeerId);
    DMRTestNetwork sender(senderPort, 7012U);

    REQUIRE(harness.network() != nullptr);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    HostTestHooks::dmrSetRFCall(*HostTestHooks::dmrSlot1(*harness.m_control), 1401U, 2401U);
    REQUIRE(sender.sendDMRVoiceHeader(hostPeerId, streamId, 600U, 1U, 1501U, 2501U));

    for (uint32_t i = 0U; i < 40U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (harness.network()->rxDMRStreamId(1U) == streamId) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_IDLE);
    REQUIRE(harness.network()->rxDMRStreamId(1U) == streamId);
    REQUIRE(harness.network()->resetDMRCount() == 0U);

    HostTestHooks::dmrClearRFCall(*HostTestHooks::dmrSlot1(*harness.m_control));
    REQUIRE(sender.sendDMRVoiceHeader(hostPeerId, streamId, 601U, 1U, 1501U, 2501U));

    for (uint32_t i = 0U; i < 40U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_AUDIO) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_AUDIO);
    REQUIRE(harness.network()->rxDMRStreamId(1U) == streamId);
    REQUIRE(harness.network()->resetDMRCount() == 0U);
}

TEST_CASE("DMR host e2e loopback enforces stream lock until active stream terminates", "[dmr][host][control][net][e2e]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 7007U;
    const uint32_t streamA = 0x610101U;
    const uint32_t streamB = 0x610102U;

    DMRHostHarness harness(true, true, hostPort, hostPeerId);
    DMRTestNetwork sender(senderPort, 7008U);

    REQUIRE(harness.network() != nullptr);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    // Start call on stream A.
    REQUIRE(sender.sendDMRVoiceHeader(hostPeerId, streamA, 400U, 1U, 1301U, 2301U));

    for (uint32_t i = 0U; i < 40U; i++) {
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_AUDIO) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_AUDIO);
    REQUIRE(harness.m_control->getLastSrcId(1U) == 1301U);
    REQUIRE(harness.m_control->getLastDstId(1U) == 2301U);
    REQUIRE(harness.network()->rxDMRStreamId(1U) == streamA);

    // Competing stream B should be ignored while stream A is active and locked.
    REQUIRE(sender.sendDMRVoiceHeader(hostPeerId, streamB, 500U, 1U, 1301U, 2301U));

    for (uint32_t i = 0U; i < 30U; i++) {
        harness.network()->clock(1U);
        harness.m_control->clock();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_AUDIO);
    REQUIRE(harness.m_control->getLastSrcId(1U) == 1301U);
    REQUIRE(harness.m_control->getLastDstId(1U) == 2301U);
    REQUIRE(harness.network()->rxDMRStreamId(1U) == streamA);

    // End stream A; stream lock should be released.
    REQUIRE(sender.sendDMRTerminator(hostPeerId, streamA, RTP_END_OF_CALL_SEQ, 1U, 1301U, 2301U));

    for (uint32_t i = 0U; i < 50U; i++) {
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_IDLE) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_IDLE);
    REQUIRE(harness.network()->rxDMRStreamId(1U) == 0U);

    // Now stream B should be admitted after stream A terminates.
    REQUIRE(sender.sendDMRVoiceHeader(hostPeerId, streamB, 502U, 1U, 1301U, 2301U));

    for (uint32_t i = 0U; i < 40U; i++) {
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_AUDIO) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_AUDIO);
    REQUIRE(harness.network()->rxDMRStreamId(1U) == streamB);
}

TEST_CASE("DMR slot network voice start arms watchdog on the targeted slot", "[dmr][host][control]")
{
    DMRHostHarness harness;
    HostTestHooks::dmrStartNetVoiceCall(*HostTestHooks::dmrSlot1(*harness.m_control), 1001U, 2001U);

    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::dmrNetworkWatchdog(*HostTestHooks::dmrSlot1(*harness.m_control)).isRunning());
    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot2(*harness.m_control)) == RS_NET_IDLE);
}

TEST_CASE("DMR slot watchdog expiry returns that slot to idle", "[dmr][host][control]")
{
    DMRHostHarness harness;
    HostTestHooks::dmrStartNetVoiceCall(*HostTestHooks::dmrSlot1(*harness.m_control), 1001U, 2001U);

    HostTestHooks::dmrNetworkWatchdog(*HostTestHooks::dmrSlot1(*harness.m_control)).clock(
        expireTimerTicks(HostTestHooks::dmrNetworkWatchdog(*HostTestHooks::dmrSlot1(*harness.m_control))));
    HostTestHooks::dmrSlot1(*harness.m_control)->clock();

    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_IDLE);
}

TEST_CASE("DMR permittedTG only affects the targeted slot", "[dmr][host][control]")
{
    DMRHostHarness harness(false);

    harness.m_control->permittedTG(2001U, 1U);

    REQUIRE(HostTestHooks::dmrPermittedDstId(*HostTestHooks::dmrSlot1(*harness.m_control)) == 2001U);
    REQUIRE(HostTestHooks::dmrPermittedDstId(*HostTestHooks::dmrSlot2(*harness.m_control)) == 0U);
}

TEST_CASE("DMR clearRFReject only clears the targeted slot", "[dmr][host][control][rf]")
{
    DMRHostHarness harness;
    HostTestHooks::dmrSetRFRejected(*HostTestHooks::dmrSlot1(*harness.m_control));
    HostTestHooks::dmrSetRFRejected(*HostTestHooks::dmrSlot2(*harness.m_control));

    harness.m_control->clearRFReject(1U);

    REQUIRE(HostTestHooks::dmrRFState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_RF_LISTENING);
    REQUIRE(HostTestHooks::dmrRFState(*HostTestHooks::dmrSlot2(*harness.m_control)) == RS_RF_REJECTED);
}

TEST_CASE("DMR slot rejects mismatched network traffic while RF hang is active", "[dmr][host][control][rf]")
{
    DMRHostHarness harness;
    HostTestHooks::dmrSetRFCall(*HostTestHooks::dmrSlot1(*harness.m_control), 3001U, 4001U);

    HostTestHooks::dmrStartNetVoiceCall(*HostTestHooks::dmrSlot1(*harness.m_control), 1001U, 2001U);

    REQUIRE(HostTestHooks::dmrNetState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_NET_IDLE);
}

TEST_CASE("DMR processFrame accepts a synthetic RF voice call on the targeted slot", "[dmr][host][control][rf]")
{
    DMRHostHarness harness;

    REQUIRE(HostTestHooks::dmrStartRFVoiceCall(*HostTestHooks::dmrSlot1(*harness.m_control), 1001U, 2001U));
    REQUIRE(HostTestHooks::dmrRFState(*HostTestHooks::dmrSlot1(*harness.m_control)) == RS_RF_AUDIO);
    REQUIRE(harness.m_control->getLastDstId(1U) == 2001U);
}

TEST_CASE("DMR Tier III re-key on a retained payload channel recreates its grant", "[dmr][tier3][host][control][rf]")
{
    DMRHostHarness harness;
    REQUIRE(harness.m_chLookup.initializeRFCh(101U));
    HostTestHooks::dmrEnableTier3(*harness.m_control, 101U, 1U);

    dmr::Slot* payload = HostTestHooks::dmrSlot2(*harness.m_control);
    REQUIRE(HostTestHooks::dmrStartRFVoiceCall(*payload, 1001U, 2001U));
    CHECK(HostTestHooks::dmrIsGranted(*payload, 2001U));
    CHECK(HostTestHooks::dmrPayloadDstId(*payload) == 2001U);

    uint8_t terminator[dmr::defines::DMR_FRAME_LENGTH_BYTES + 2U] = { 0U };
    terminator[0U] = modem::TAG_DATA;
    terminator[1U] = dmr::defines::SYNC_DATA | dmr::defines::DataType::TERMINATOR_WITH_LC;
    buildDMRTerminatorPayload(terminator + 2U, 1001U, 2001U, true);
    REQUIRE(payload->processFrame(terminator, sizeof(terminator)));
    CHECK_FALSE(HostTestHooks::dmrIsGranted(*payload, 2001U));
    CHECK(HostTestHooks::dmrPayloadDstId(*payload) == 0U);

    // Some subscribers remain on the payload channel during hangtime and
    // begin transmitting without another RAND request on the TSCC.
    REQUIRE(HostTestHooks::dmrStartRFVoiceCall(*payload, 1002U, 2001U));
    CHECK(HostTestHooks::dmrIsGranted(*payload, 2001U));
    CHECK(HostTestHooks::dmrPayloadDstId(*payload) == 2001U);

    terminator[0U] = modem::TAG_DATA;
    terminator[1U] = dmr::defines::SYNC_DATA | dmr::defines::DataType::TERMINATOR_WITH_LC;
    buildDMRTerminatorPayload(terminator + 2U, 1002U, 2001U, true);
    REQUIRE(payload->processFrame(terminator, sizeof(terminator)));
    CHECK_FALSE(HostTestHooks::dmrIsGranted(*payload, 2001U));
    CHECK(HostTestHooks::dmrPayloadDstId(*payload) == 0U);
}

TEST_CASE("DMR rfTGHang expiry ends active RF call and returns slot to listening", "[dmr][host][control][rf]")
{
    DMRHostHarness harness;
    dmr::Slot* slot = HostTestHooks::dmrSlot1(*harness.m_control);

    REQUIRE(HostTestHooks::dmrStartRFVoiceCall(*slot, 1001U, 2001U));
    REQUIRE(HostTestHooks::dmrRFState(*slot) == RS_RF_AUDIO);

    HostTestHooks::dmrRFTGHang(*slot).start();
    HostTestHooks::dmrRFTGHang(*slot).clock(expireTimerTicks(HostTestHooks::dmrRFTGHang(*slot)));
    slot->clock();

    REQUIRE(HostTestHooks::dmrRFState(*slot) == RS_RF_LISTENING);
    REQUIRE_FALSE(HostTestHooks::dmrRFTGHang(*slot).isRunning());
    REQUIRE_FALSE(HostTestHooks::dmrRFLossWatchdog(*slot).isRunning());
}

TEST_CASE("DMR rfTGHang zero fallback arms rfLossWatchdog and recovers RF state", "[dmr][host][control][rf]")
{
    DMRHostHarness harness;
    dmr::Slot* slot = HostTestHooks::dmrSlot1(*harness.m_control);

    REQUIRE(HostTestHooks::dmrStartRFVoiceCall(*slot, 1101U, 2101U));
    REQUIRE(HostTestHooks::dmrRFState(*slot) == RS_RF_AUDIO);

    HostTestHooks::dmrRFTGHang(*slot).setTimeout(0U);
    REQUIRE_FALSE(HostTestHooks::dmrRFLossWatchdog(*slot).isRunning());

    REQUIRE(HostTestHooks::dmrStartRFVoiceCall(*slot, 1101U, 2101U));
    REQUIRE(HostTestHooks::dmrRFLossWatchdog(*slot).isRunning());

    HostTestHooks::dmrRFLossWatchdog(*slot).clock(expireTimerTicks(HostTestHooks::dmrRFLossWatchdog(*slot)));
    slot->clock();

    REQUIRE(HostTestHooks::dmrRFState(*slot) == RS_RF_LISTENING);
    REQUIRE_FALSE(HostTestHooks::dmrRFLossWatchdog(*slot).isRunning());
}
