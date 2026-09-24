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
#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/Sync.h"
#include "common/nxdn/channel/FACCH1.h"
#include "common/nxdn/channel/LICH.h"
#include "common/nxdn/channel/SACCH.h"
#include "common/nxdn/NXDNUtils.h"
#include "common/nxdn/lc/RTCH.h"
#include "host/modem/Modem.h"
#include "modem/port/IModemPort.h"
#include "host/HostTestHooks.h"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstring>
#include <thread>

extern network::NetRPC* g_RPC;
#include "host/nxdn/Control.h"

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

} // namespace

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Dummy implementation of a modem port for testing purposes.
 */
class NXDNTestModemPort final : public modem::port::IModemPort {
public:
    /**
     * @brief Finalizes the instance of the NXDNTestModemPort class.
     */
    ~NXDNTestModemPort() override = default;

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

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Lightweight network test double for NXDN ingress stream-lock tests.
 */
class NXDNTestNetwork final : public network::Network {
public:
    /**
     * @brief Initializes a new instance of the NXDNTestNetwork class.
     * @param localPort The local port number to bind the network socket to.
     * @param peerId The peer ID for the network connection.
     */
    NXDNTestNetwork(uint16_t localPort = 0U, uint32_t peerId = 1U) :
        network::Network("127.0.0.1", 1U, localPort, peerId, "test", true, true, false, false, true, false, true, true, false, false, false, false),
        m_resetNXDNCount(0U)
    {
        // keep protocol gates deterministic for this NXDN-focused harness
        m_dmrEnabled = false;
        m_p25Enabled = false;
        m_nxdnEnabled = true;
        m_analogEnabled = false;
    }

    /**
     * @brief Activates loopback mode for the network connection.
     * @param remoteAddress The remote address to connect to.
     * @param remotePort The remote port to connect to.
     * @returns bool True if loopback mode was successfully activated, false otherwise.
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
     * @brief Sends an NXDN control frame over the network.
     * @param targetPeerId The target peer ID for the network connection.
     * @param streamId The stream ID for the network connection.
     * @param seq The sequence number for the network frame.
     * @param messageType The message type of the NXDN control frame.
     * @param srcId The source ID for the NXDN control frame.
     * @param dstId The destination ID for the NXDN control frame.
     * @returns bool True if the control frame was successfully sent, false otherwise.
     */
    bool sendNXDNControlFrame(uint32_t targetPeerId, uint32_t streamId, uint16_t seq,
        uint8_t messageType, uint16_t srcId, uint16_t dstId)
    {
        uint8_t frame[nxdn::defines::NXDN_FRAME_LENGTH_BYTES + 2U];
        ::memset(frame, 0x00U, sizeof(frame));

        frame[0U] = messageType == nxdn::defines::MessageType::RTCH_TX_REL ? modem::TAG_EOT : modem::TAG_DATA;
        frame[1U] = 0x01U;

        nxdn::Sync::addNXDNSync(frame + 2U);

        nxdn::channel::LICH lich;
        lich.setRFCT(nxdn::defines::RFChannelType::RTCH);
        lich.setFCT(nxdn::defines::FuncChannelType::USC_SACCH_NS);
        lich.setOption(nxdn::defines::ChOption::STEAL_FACCH);
        lich.setOutbound(true);
        lich.encode(frame + 2U);

        nxdn::channel::SACCH sacch;
        sacch.setData(nxdn::defines::SACCH_IDLE);
        sacch.setRAN(1U);
        sacch.setStructure(nxdn::defines::ChStructure::SR_SINGLE);
        sacch.encode(frame + 2U);

        uint8_t lcBuffer[nxdn::defines::NXDN_RTCH_LC_LENGTH_BYTES];
        ::memset(lcBuffer, 0x00U, sizeof(lcBuffer));
        nxdn::lc::RTCH lc;
        lc.setMessageType(messageType);
        lc.setCallType(nxdn::defines::CallType::UNSPECIFIED);
        lc.setSrcId(srcId);
        lc.setDstId(dstId);
        lc.setGroup(true);
        lc.setTransmissionMode(nxdn::defines::TransmissionMode::MODE_4800);
        lc.encode(lcBuffer, nxdn::defines::NXDN_RTCH_LC_LENGTH_BITS);

        nxdn::channel::FACCH1 facch;
        facch.setData(lcBuffer);
        facch.encode(frame + 2U, nxdn::defines::NXDN_FSW_LENGTH_BITS + nxdn::defines::NXDN_LICH_LENGTH_BITS + nxdn::defines::NXDN_SACCH_FEC_LENGTH_BITS);
        facch.encode(frame + 2U, nxdn::defines::NXDN_FSW_LENGTH_BITS + nxdn::defines::NXDN_LICH_LENGTH_BITS + nxdn::defines::NXDN_SACCH_FEC_LENGTH_BITS + nxdn::defines::NXDN_FACCH1_FEC_LENGTH_BITS);

        nxdn::NXDNUtils::scrambler(frame + 2U);

        uint32_t messageLength = 0U;
        UInt8Array message = createNXDN_Message(messageLength, lc, frame + 2U,
            nxdn::defines::NXDN_FRAME_LENGTH_BYTES);
        if (message == nullptr || messageLength == 0U) {
            return false;
        }

        return m_frameQueue->write(message.get(), messageLength, streamId, targetPeerId, m_peerId,
            { network::NET_FUNC::PROTOCOL, network::NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN }, seq, m_addr, m_addrLen);
    }

    /**
     * @brief Sends an NXDN call header over the network.
     * @param targetPeerId The target peer ID for the network connection.
     * @param streamId The stream ID for the network connection.
     * @param seq The sequence number for the network frame.
     * @param srcId The source ID for the NXDN call header.
     * @param dstId The destination ID for the NXDN call header.
     * @returns bool True if the call header was successfully sent, false otherwise.
     */
    bool sendNXDNCallHeader(uint32_t targetPeerId, uint32_t streamId, uint16_t seq,
        uint16_t srcId, uint16_t dstId)
    {
        return sendNXDNControlFrame(targetPeerId, streamId, seq,
            nxdn::defines::MessageType::RTCH_VCALL, srcId, dstId);
    }

    /**
     * @brief Sends an NXDN call terminator over the network.
     * @param targetPeerId The target peer ID for the network connection.
     * @param streamId The stream ID for the network connection.
     * @param srcId The source ID for the NXDN call terminator.
     * @param dstId The destination ID for the NXDN call terminator.
     * @returns bool True if the call terminator was successfully sent, false otherwise.
     */
    bool sendNXDNCallTerminator(uint32_t targetPeerId, uint32_t streamId,
        uint16_t srcId, uint16_t dstId)
    {
        return sendNXDNControlFrame(targetPeerId, streamId, RTP_END_OF_CALL_SEQ,
            nxdn::defines::MessageType::RTCH_TX_REL, srcId, dstId);
    }

    /**
     * @brief Sends an NXDN voice frame over the network.
     * @param targetPeerId The target peer ID for the network connection.
     * @param streamId The stream ID for the network connection.
     * @param seq The sequence number for the network frame.
     * @param fragment The fragment number of the voice frame.
     * @param srcId The source ID for the NXDN voice frame.
     * @param dstId The destination ID for the NXDN voice frame.
     * @returns bool True if the voice frame was successfully sent, false otherwise.
     */
    bool sendNXDNVoiceFrame(uint32_t targetPeerId, uint32_t streamId, uint16_t seq,
        uint8_t fragment, uint16_t srcId, uint16_t dstId)
    {
        using namespace nxdn;
        using namespace nxdn::defines;

        if (fragment >= 4U)
            return false;

        uint8_t frame[NXDN_FRAME_LENGTH_BYTES];
        ::memset(frame, 0x00U, sizeof(frame));
        Sync::addNXDNSync(frame);

        channel::LICH lich;
        lich.setRFCT(RFChannelType::RTCH);
        lich.setFCT(FuncChannelType::USC_SACCH_SS);
        lich.setOption(ChOption::STEAL_NONE);
        lich.setOutbound(true);
        lich.encode(frame);

        lc::RTCH lc;
        lc.setMessageType(MessageType::RTCH_VCALL);
        lc.setSrcId(srcId);
        lc.setDstId(dstId);
        lc.setGroup(true);
        lc.setTransmissionMode(TransmissionMode::MODE_4800);

        uint8_t lcBuffer[NXDN_RTCH_LC_LENGTH_BYTES];
        ::memset(lcBuffer, 0x00U, sizeof(lcBuffer));
        lc.encode(lcBuffer, NXDN_RTCH_LC_LENGTH_BITS);

        uint8_t sacchData[3U];
        ::memset(sacchData, 0x00U, sizeof(sacchData));
        for (uint32_t bit = 0U; bit < 18U; bit++)
            WRITE_BIT(sacchData, bit, READ_BIT(lcBuffer, fragment * 18U + bit));

        const ChStructure::E structures[] = {
            ChStructure::SR_1_4, ChStructure::SR_2_4,
            ChStructure::SR_3_4, ChStructure::SR_4_4
        };
        channel::SACCH sacch;
        sacch.setData(sacchData);
        sacch.setRAN(1U);
        sacch.setStructure(structures[fragment]);
        sacch.encode(frame);

        // Four null VCH payloads form a complete half-rate voice frame.
        ::memcpy(frame + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 0U, NULL_AMBE, 9U);
        ::memcpy(frame + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 9U, NULL_AMBE, 9U);
        ::memcpy(frame + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 18U, NULL_AMBE, 9U);
        ::memcpy(frame + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + 27U, NULL_AMBE, 9U);

        NXDNUtils::scrambler(frame);

        uint32_t messageLength = 0U;
        UInt8Array message = createNXDN_Message(messageLength, lc, frame, NXDN_FRAME_LENGTH_BYTES);
        if (message == nullptr || messageLength == 0U)
            return false;

        return m_frameQueue->write(message.get(), messageLength, streamId, targetPeerId, m_peerId,
            { network::NET_FUNC::PROTOCOL, network::NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN }, seq, m_addr, m_addrLen);
    }

    /**
     * @brief Resets the NXDN network connection.
     */
    void resetNXDN() override
    {
        ++m_resetNXDNCount;
        network::Network::resetNXDN();
    }

    /**
     * @brief Retrieves the number of times the NXDN network connection has been reset.
     * @returns uint32_t The reset count for the NXDN network connection.
     */
    uint32_t resetNXDNCount() const
    {
        return m_resetNXDNCount;
    }

    /**
     * @brief Retrieves the stream ID of the last received NXDN frame.
     * @returns uint32_t The stream ID of the last received NXDN frame.
     */
    uint32_t rxNXDNStreamId() const
    {
        return m_rxNXDNStreamId;
    }

private:
    uint32_t m_resetNXDNCount;
};

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Harness class for testing NXDN host control functionality.
 */
class NXDNHostHarness {
public:
    /**
     * @brief Initializes a new instance of the NXDNHostHarness class.
     * @param authoritative Indicates whether the host is authoritative.
     */
    explicit NXDNHostHarness(bool authoritative = true, bool withNetwork = false, uint16_t networkLocalPort = 0U, uint32_t networkPeerId = 1U) :
        m_rpc("127.0.0.1", 1U, 0U, "test", false),
        m_modem(new NXDNTestModemPort(), false, false, false, false, false, false,
            0U, 0U, 0U, 1024U, 4096U, 1024U, true, true, false, false, false, false),
        m_chLookup(),
        m_ridLookup("", 0U, false, false),
        m_tidLookup("", 0U, false, false),
        m_idenLookup("", 0U),
        m_rssiMapper(),
        m_network(withNetwork ? new NXDNTestNetwork(networkLocalPort, networkPeerId) : nullptr),
        m_control(nullptr)
    {
        g_RPC = &m_rpc;
        m_modem.setModeParams(false, false, true);

        m_control = new nxdn::Control(authoritative, 1U, 1U, 4096U, 5U, 2U, &m_modem, m_network,
            false, &m_chLookup, &m_ridLookup, &m_tidLookup, &m_idenLookup, &m_rssiMapper, false, true, true);
    }
    /**
     * @brief Finalizes an instance of the NXDNHostHarness class.
     */
    ~NXDNHostHarness()
    {
        delete m_control;
        delete m_network;
        g_RPC = nullptr;
    }

    /**
     * @brief Retrieves the NXDN test network instance associated with the harness.
     * @returns NXDNTestNetwork* The NXDN test network instance.
     */
    NXDNTestNetwork* network() const
    {
        return m_network;
    }

    /**
     * @brief Starts a network voice call with the specified source and destination IDs.
     * @param srcId The source ID for the network voice call.
     * @param dstId The destination ID for the network voice call.
     */
    void startNetworkVoiceCall(uint16_t srcId = 1001U, uint16_t dstId = 2001U)
    {
        nxdn::lc::RTCH control;
        control.setMessageType(nxdn::defines::MessageType::RTCH_VCALL);
        control.setSrcId(srcId);
        control.setDstId(dstId);
        control.setGroup(true);
        control.setTransmissionMode(nxdn::defines::TransmissionMode::MODE_4800);

        REQUIRE(HostTestHooks::nxdnStartNetCall(*m_control, control));
    }

public:
    network::NetRPC m_rpc;
    modem::Modem m_modem;
    lookups::ChannelLookup m_chLookup;
    lookups::RadioIdLookup m_ridLookup;
    lookups::TalkgroupRulesLookup m_tidLookup;
    lookups::IdenTableLookup m_idenLookup;
    lookups::RSSIInterpolator m_rssiMapper;
    NXDNTestNetwork* m_network;
    nxdn::Control* m_control;
};

TEST_CASE("NXDN host e2e loopback handles missed frames without dropping active call", "[nxdn][host][control][net][e2e]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 8001U;
    const uint32_t streamId = 0x620001U;
    NXDNHostHarness harness(true, true, hostPort, hostPeerId);
    NXDNTestNetwork sender(senderPort, 8002U);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    REQUIRE(sender.sendNXDNCallHeader(hostPeerId, streamId, 100U, 1001U, 2001U));
    for (uint32_t i = 0U; i < 40U && HostTestHooks::nxdnNetState(*harness.m_control) != RS_NET_AUDIO; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO);

    // Skip RTP sequence 101 while continuing with the first two SACCH fragments.
    REQUIRE(sender.sendNXDNVoiceFrame(hostPeerId, streamId, 102U, 0U, 1001U, 2001U));
    REQUIRE(sender.sendNXDNVoiceFrame(hostPeerId, streamId, 103U, 1U, 1001U, 2001U));
    for (uint32_t i = 0U; i < 20U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::nxdnNetworkWatchdog(*harness.m_control).isRunning());
    REQUIRE(HostTestHooks::nxdnNetLastSrcId(*harness.m_control) == 1001U);
    REQUIRE(HostTestHooks::nxdnNetLastDstId(*harness.m_control) == 2001U);
}

TEST_CASE("NXDN host e2e loopback handles dropped call terminator and returns idle", "[nxdn][host][control][net][e2e]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 8003U;
    const uint32_t streamId = 0x620002U;
    NXDNHostHarness harness(true, true, hostPort, hostPeerId);
    NXDNTestNetwork sender(senderPort, 8004U);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    REQUIRE(sender.sendNXDNCallHeader(hostPeerId, streamId, 200U, 1101U, 2101U));
    for (uint32_t i = 0U; i < 40U && HostTestHooks::nxdnNetState(*harness.m_control) != RS_NET_AUDIO; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO);

    // Deliberately omit TX_REL and verify watchdog recovery.
    HostTestHooks::nxdnNetworkWatchdog(*harness.m_control).clock(expireTimerTicks(HostTestHooks::nxdnNetworkWatchdog(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE_FALSE(HostTestHooks::nxdnNetworkWatchdog(*harness.m_control).isRunning());
    REQUIRE(harness.network()->rxNXDNStreamId() == 0U);
    REQUIRE(harness.network()->resetNXDNCount() == 1U);
}

TEST_CASE("NXDN host e2e loopback times out stale call and resets stream state", "[nxdn][host][control][net][e2e]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 8005U;
    const uint32_t streamId = 0x620003U;
    NXDNHostHarness harness(true, true, hostPort, hostPeerId);
    NXDNTestNetwork sender(senderPort, 8006U);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    REQUIRE(sender.sendNXDNCallHeader(hostPeerId, streamId, 300U, 1201U, 2201U));
    for (uint32_t i = 0U; i < 40U && HostTestHooks::nxdnNetState(*harness.m_control) != RS_NET_AUDIO; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(harness.network()->rxNXDNStreamId() == streamId);

    HostTestHooks::nxdnNetworkWatchdog(*harness.m_control).clock(expireTimerTicks(HostTestHooks::nxdnNetworkWatchdog(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(harness.network()->rxNXDNStreamId() == 0U);
    REQUIRE(harness.network()->resetNXDNCount() == 1U);
}

TEST_CASE("NXDN host e2e loopback completes the conventional voice call flow", "[nxdn][host][control][net][e2e][call-flow]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 8013U;
    const uint32_t streamId = 0x620301U;
    NXDNHostHarness harness(true, true, hostPort, hostPeerId);
    NXDNTestNetwork sender(senderPort, 8014U);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    // TS 1-B conventional flow: FACCH VCALL, one four-frame SACCH
    // voice superframe, then a FACCH TX_REL.
    REQUIRE(sender.sendNXDNCallHeader(hostPeerId, streamId, 700U, 1601U, 2601U));
    REQUIRE(sender.sendNXDNVoiceFrame(hostPeerId, streamId, 701U, 0U, 1601U, 2601U));
    REQUIRE(sender.sendNXDNVoiceFrame(hostPeerId, streamId, 702U, 1U, 1601U, 2601U));
    REQUIRE(sender.sendNXDNVoiceFrame(hostPeerId, streamId, 703U, 2U, 1601U, 2601U));
    REQUIRE(sender.sendNXDNVoiceFrame(hostPeerId, streamId, 704U, 3U, 1601U, 2601U));

    for (uint32_t i = 0U; i < 60U && HostTestHooks::nxdnNetState(*harness.m_control) != RS_NET_AUDIO; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::nxdnNetLastSrcId(*harness.m_control) == 1601U);
    REQUIRE(HostTestHooks::nxdnNetLastDstId(*harness.m_control) == 2601U);
    REQUIRE(harness.network()->rxNXDNStreamId() == streamId);

    REQUIRE(sender.sendNXDNCallTerminator(hostPeerId, streamId, 1601U, 2601U));
    for (uint32_t i = 0U; i < 50U && HostTestHooks::nxdnNetState(*harness.m_control) != RS_NET_IDLE; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE_FALSE(HostTestHooks::nxdnNetworkWatchdog(*harness.m_control).isRunning());
    REQUIRE(harness.network()->rxNXDNStreamId() == 0U);
}

TEST_CASE("NXDN host e2e loopback tolerates out-of-order voice frames", "[nxdn][host][control][net][e2e][call-flow]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 8015U;
    const uint32_t streamId = 0x620302U;
    NXDNHostHarness harness(true, true, hostPort, hostPeerId);
    NXDNTestNetwork sender(senderPort, 8016U);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    REQUIRE(sender.sendNXDNCallHeader(hostPeerId, streamId, 800U, 1701U, 2701U));
    for (uint32_t i = 0U; i < 40U && HostTestHooks::nxdnNetState(*harness.m_control) != RS_NET_AUDIO; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO);

    REQUIRE(sender.sendNXDNVoiceFrame(hostPeerId, streamId, 802U, 1U, 1701U, 2701U));
    REQUIRE(sender.sendNXDNVoiceFrame(hostPeerId, streamId, 801U, 0U, 1701U, 2701U));
    for (uint32_t i = 0U; i < 25U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::nxdnNetworkWatchdog(*harness.m_control).isRunning());
    REQUIRE(harness.network()->rxNXDNStreamId() == streamId);
}

TEST_CASE("NXDN host e2e loopback preserves a continuing network stream across an RF collision", "[nxdn][host][control][net][e2e]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 8011U;
    const uint32_t streamId = 0x620201U;

    NXDNHostHarness harness(true, true, hostPort, hostPeerId);
    NXDNTestNetwork sender(senderPort, 8012U);

    REQUIRE(harness.network() != nullptr);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    HostTestHooks::nxdnSetRFCall(*harness.m_control, 1401U, 2401U);
    REQUIRE(sender.sendNXDNControlFrame(hostPeerId, streamId, 600U, nxdn::defines::MessageType::RTCH_VCALL, 1501U, 2501U));

    for (uint32_t i = 0U; i < 40U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (harness.network()->rxNXDNStreamId() == streamId) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(harness.network()->rxNXDNStreamId() == streamId);
    REQUIRE(harness.network()->resetNXDNCount() == 0U);

    HostTestHooks::nxdnClearRFCall(*harness.m_control);
    REQUIRE(sender.sendNXDNControlFrame(hostPeerId, streamId, 601U, nxdn::defines::MessageType::RTCH_VCALL, 1501U, 2501U));

    for (uint32_t i = 0U; i < 40U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(harness.network()->rxNXDNStreamId() == streamId);
    REQUIRE(harness.network()->resetNXDNCount() == 0U);
}

TEST_CASE("NXDN host e2e loopback enforces stream lock until active stream terminates", "[nxdn][host][control][net][e2e]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 8007U;
    const uint32_t streamA = 0x620101U;
    const uint32_t streamB = 0x620102U;

    NXDNHostHarness harness(true, true, hostPort, hostPeerId);
    NXDNTestNetwork sender(senderPort, 8008U);

    REQUIRE(harness.network() != nullptr);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    REQUIRE(sender.sendNXDNControlFrame(hostPeerId, streamA, 400U, nxdn::defines::MessageType::RTCH_VCALL, 1301U, 2301U));

    for (uint32_t i = 0U; i < 40U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::nxdnNetLastSrcId(*harness.m_control) == 1301U);
    REQUIRE(HostTestHooks::nxdnNetLastDstId(*harness.m_control) == 2301U);
    REQUIRE(harness.network()->rxNXDNStreamId() == streamA);

    REQUIRE(sender.sendNXDNControlFrame(hostPeerId, streamB, 500U, nxdn::defines::MessageType::RTCH_VCALL, 1301U, 2301U));

    for (uint32_t i = 0U; i < 30U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::nxdnNetLastSrcId(*harness.m_control) == 1301U);
    REQUIRE(HostTestHooks::nxdnNetLastDstId(*harness.m_control) == 2301U);
    REQUIRE(harness.network()->rxNXDNStreamId() == streamA);

    REQUIRE(sender.sendNXDNControlFrame(hostPeerId, streamA, RTP_END_OF_CALL_SEQ, nxdn::defines::MessageType::RTCH_TX_REL, 1301U, 2301U));

    for (uint32_t i = 0U; i < 50U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_IDLE) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(harness.network()->rxNXDNStreamId() == 0U);

    REQUIRE(sender.sendNXDNControlFrame(hostPeerId, streamB, 502U, nxdn::defines::MessageType::RTCH_VCALL, 1301U, 2301U));

    for (uint32_t i = 0U; i < 40U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::nxdnNetLastSrcId(*harness.m_control) == 1301U);
    REQUIRE(HostTestHooks::nxdnNetLastDstId(*harness.m_control) == 2301U);
    REQUIRE(harness.network()->rxNXDNStreamId() == streamB);
}

TEST_CASE("NXDN host arms the network watchdog when network voice starts", "[nxdn][host][control]")
{
    NXDNHostHarness harness;
    harness.startNetworkVoiceCall();

    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::nxdnNetworkWatchdog(*harness.m_control).isRunning());
    REQUIRE_FALSE(HostTestHooks::nxdnNetworkWatchdog(*harness.m_control).hasExpired());
}

TEST_CASE("NXDN host watchdog expiry returns network voice to idle", "[nxdn][host][control]")
{
    NXDNHostHarness harness;
    harness.startNetworkVoiceCall();

    HostTestHooks::nxdnNetworkWatchdog(*harness.m_control).clock(expireTimerTicks(HostTestHooks::nxdnNetworkWatchdog(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE_FALSE(HostTestHooks::nxdnNetworkWatchdog(*harness.m_control).isRunning());
}

TEST_CASE("NXDN host net hang expiry clears active network voice state", "[nxdn][host][control]")
{
    NXDNHostHarness harness;
    harness.startNetworkVoiceCall();

    HostTestHooks::nxdnNetTGHang(*harness.m_control).clock(expireTimerTicks(HostTestHooks::nxdnNetTGHang(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::nxdnNetLastDstId(*harness.m_control) == 0U);
    REQUIRE(HostTestHooks::nxdnNetLastSrcId(*harness.m_control) == 0U);
}

TEST_CASE("NXDN clearRFReject returns rejected RF state to listening", "[nxdn][host][control][rf]")
{
    NXDNHostHarness harness;
    HostTestHooks::nxdnSetRFRejected(*harness.m_control);

    harness.m_control->clearRFReject();

    REQUIRE(HostTestHooks::nxdnRFState(*harness.m_control) == RS_RF_LISTENING);
}

TEST_CASE("NXDN rejects mismatched network traffic while RF hang is active", "[nxdn][host][control][rf]")
{
    NXDNHostHarness harness;
    HostTestHooks::nxdnSetRFCall(*harness.m_control, 3001U, 4001U);

    nxdn::lc::RTCH control;
    control.setMessageType(nxdn::defines::MessageType::RTCH_VCALL);
    control.setSrcId(1001U);
    control.setDstId(2001U);
    control.setGroup(true);
    control.setTransmissionMode(nxdn::defines::TransmissionMode::MODE_4800);

    REQUIRE_FALSE(HostTestHooks::nxdnStartNetCall(*harness.m_control, control));
    REQUIRE(HostTestHooks::nxdnNetState(*harness.m_control) == RS_NET_IDLE);
}

TEST_CASE("NXDN processFrame accepts a synthetic RF voice call", "[nxdn][host][control][rf]")
{
    NXDNHostHarness harness;

    REQUIRE(HostTestHooks::nxdnStartRFCall(*harness.m_control, 1001U, 2001U));
    REQUIRE(HostTestHooks::nxdnRFState(*harness.m_control) == RS_RF_AUDIO);
    REQUIRE(harness.m_control->getLastDstId() == 2001U);
}

TEST_CASE("NXDN rfTGHang expiry ends active RF call and returns to listening", "[nxdn][host][control][rf]")
{
    NXDNHostHarness harness;

    REQUIRE(HostTestHooks::nxdnStartRFCall(*harness.m_control, 1001U, 2001U));
    REQUIRE(HostTestHooks::nxdnRFState(*harness.m_control) == RS_RF_AUDIO);

    HostTestHooks::nxdnRFTGHang(*harness.m_control).start();
    HostTestHooks::nxdnRFTGHang(*harness.m_control).clock(expireTimerTicks(HostTestHooks::nxdnRFTGHang(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::nxdnRFState(*harness.m_control) == RS_RF_LISTENING);
    REQUIRE_FALSE(HostTestHooks::nxdnRFTGHang(*harness.m_control).isRunning());
    REQUIRE_FALSE(HostTestHooks::nxdnRFLossWatchdog(*harness.m_control).isRunning());
}

TEST_CASE("NXDN rfTGHang zero fallback arms rfLossWatchdog and recovers RF state", "[nxdn][host][control][rf]")
{
    NXDNHostHarness harness;

    REQUIRE(HostTestHooks::nxdnStartRFCall(*harness.m_control, 1101U, 2101U));
    REQUIRE(HostTestHooks::nxdnRFState(*harness.m_control) == RS_RF_AUDIO);

    HostTestHooks::nxdnRFTGHang(*harness.m_control).setTimeout(0U);
    REQUIRE_FALSE(HostTestHooks::nxdnRFLossWatchdog(*harness.m_control).isRunning());

    REQUIRE(HostTestHooks::nxdnStartRFCall(*harness.m_control, 1101U, 2101U));
    REQUIRE(HostTestHooks::nxdnRFLossWatchdog(*harness.m_control).isRunning());

    HostTestHooks::nxdnRFLossWatchdog(*harness.m_control).clock(expireTimerTicks(HostTestHooks::nxdnRFLossWatchdog(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::nxdnRFState(*harness.m_control) == RS_RF_LISTENING);
    REQUIRE_FALSE(HostTestHooks::nxdnRFLossWatchdog(*harness.m_control).isRunning());
}
