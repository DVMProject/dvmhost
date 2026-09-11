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
#include "common/network/NetRPC.h"
#include "common/network/Network.h"
#include "common/p25/P25Defines.h"
#include "common/p25/data/LowSpeedData.h"
#include "common/p25/lc/LC.h"
#include "host/modem/Modem.h"
#include "modem/port/IModemPort.h"
#include "host/HostTestHooks.h"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstring>
#include <thread>

extern network::NetRPC* g_RPC;
#include "host/p25/Control.h"

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

// LDU1_1K payload bytes from firmware calibration constants (without leading control byte).
static const uint8_t CAL_P25_LDU1_1K[p25::defines::P25_LDU_FRAME_LENGTH_BYTES] = {
    0x55U, 0x75U, 0xF5U, 0xFFU, 0x77U, 0xFFU, 0x29U, 0x35U, 0x54U, 0x7BU, 0xCBU, 0x19U, 0x4DU, 0x0DU, 0xCEU, 0x24U, 0xA1U, 0x24U,
    0x0DU, 0x43U, 0x3CU, 0x0BU, 0xE1U, 0xB9U, 0x18U, 0x44U, 0xFCU, 0xC1U, 0x62U, 0x96U, 0x27U, 0x60U, 0xE4U, 0xE2U, 0x4AU, 0x10U,
    0x90U, 0xD4U, 0x33U, 0xC0U, 0xBEU, 0x1BU, 0x91U, 0x84U, 0x4CU, 0xFCU, 0x16U, 0x29U, 0x62U, 0x76U, 0x0EU, 0xC0U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x03U, 0x89U, 0x28U, 0x49U, 0x0DU, 0x43U, 0x3CU, 0x02U, 0xF8U, 0x6EU, 0x46U, 0x11U, 0x3FU, 0xC1U, 0x62U, 0x94U,
    0x89U, 0xD8U, 0x39U, 0x00U, 0x00U, 0x00U, 0x00U, 0x1CU, 0x38U, 0x24U, 0xA1U, 0x24U, 0x35U, 0x0CU, 0xF0U, 0x2FU, 0x86U, 0xE4U,
    0x18U, 0x44U, 0xFFU, 0x05U, 0x8AU, 0x58U, 0x9DU, 0x83U, 0xB0U, 0x00U, 0x00U, 0x00U, 0x00U, 0x70U, 0xE2U, 0x4AU, 0x12U, 0x40U,
    0xD4U, 0x33U, 0xC0U, 0xBEU, 0x1BU, 0x91U, 0x84U, 0x4FU, 0xF0U, 0x16U, 0x29U, 0x62U, 0x76U, 0x0EU, 0x6DU, 0xE5U, 0xD5U, 0x48U,
    0xADU, 0xE3U, 0x89U, 0x28U, 0x49U, 0x0DU, 0x43U, 0x3CU, 0x08U, 0xF8U, 0x6EU, 0x46U, 0x11U, 0x3FU, 0xC1U, 0x62U, 0x96U, 0x24U,
    0xD8U, 0x3BU, 0xA1U, 0x41U, 0xC2U, 0xD2U, 0xBAU, 0x38U, 0x90U, 0xA1U, 0x24U, 0x35U, 0x0CU, 0xF0U, 0x2FU, 0x86U, 0xE4U, 0x60U,
    0x44U, 0xFFU, 0x05U, 0x8AU, 0x58U, 0x9DU, 0x83U, 0x94U, 0xC8U, 0xFBU, 0x02U, 0x35U, 0xA4U, 0xE2U, 0x4AU, 0x12U, 0x43U, 0x50U,
    0x33U, 0xC0U, 0xBEU, 0x1BU, 0x91U, 0x84U, 0x4FU, 0xF0U, 0x58U, 0x29U, 0x62U, 0x76U, 0x0EU, 0xC0U, 0x00U, 0x00U, 0x00U, 0x0CU,
    0x89U, 0x28U, 0x49U, 0x0DU, 0x43U, 0x3CU, 0x0BU, 0xE1U, 0xB8U, 0x46U, 0x11U, 0x3FU, 0xC1U, 0x62U, 0x96U, 0x27U, 0x60U, 0xE4U
};

// LDU2_1K payload bytes from firmware calibration constants (without leading control byte).
static const uint8_t CAL_P25_LDU2_1K[p25::defines::P25_LDU_FRAME_LENGTH_BYTES] = {
    0x55U, 0x75U, 0xF5U, 0xFFU, 0x77U, 0xFFU, 0x29U, 0x3AU, 0xB8U, 0xA4U, 0xEFU, 0xB0U, 0x9AU, 0x8AU, 0xCEU, 0x24U, 0xA1U, 0x24U,
    0x0DU, 0x43U, 0x3CU, 0x0BU, 0xE1U, 0xB9U, 0x18U, 0x44U, 0xFCU, 0xC1U, 0x62U, 0x96U, 0x27U, 0x60U, 0xECU, 0xE2U, 0x4AU, 0x10U,
    0x90U, 0xD4U, 0x33U, 0xC0U, 0xBEU, 0x1BU, 0x91U, 0x84U, 0x4CU, 0xFCU, 0x16U, 0x29U, 0x62U, 0x76U, 0x0EU, 0x40U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x03U, 0x89U, 0x28U, 0x49U, 0x0DU, 0x43U, 0x3CU, 0x02U, 0xF8U, 0x6EU, 0x46U, 0x11U, 0x3FU, 0xC1U, 0x62U, 0x94U,
    0x89U, 0xD8U, 0x3BU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x38U, 0x24U, 0xA1U, 0x24U, 0x35U, 0x0CU, 0xF0U, 0x2FU, 0x86U, 0xE4U,
    0x18U, 0x44U, 0xFFU, 0x05U, 0x8AU, 0x58U, 0x9DU, 0x83U, 0x90U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xE2U, 0x4AU, 0x12U, 0x40U,
    0xD4U, 0x33U, 0xC0U, 0xBEU, 0x1BU, 0x91U, 0x84U, 0x4FU, 0xF0U, 0x16U, 0x29U, 0x62U, 0x76U, 0x0EU, 0xE0U, 0xE0U, 0x00U, 0x00U,
    0x00U, 0x03U, 0x89U, 0x28U, 0x49U, 0x0DU, 0x43U, 0x3CU, 0x08U, 0xF8U, 0x6EU, 0x46U, 0x11U, 0x3FU, 0xC1U, 0x62U, 0x96U, 0x24U,
    0xD8U, 0x39U, 0xAEU, 0x8BU, 0x48U, 0xB6U, 0x49U, 0x38U, 0x90U, 0xA1U, 0x24U, 0x35U, 0x0CU, 0xF0U, 0x2FU, 0x86U, 0xE4U, 0x60U,
    0x44U, 0xFFU, 0x05U, 0x8AU, 0x58U, 0x9DU, 0x83U, 0xB9U, 0xA8U, 0xF4U, 0xF1U, 0xFDU, 0x60U, 0xE2U, 0x4AU, 0x12U, 0x43U, 0x50U,
    0x33U, 0xC0U, 0xBEU, 0x1BU, 0x91U, 0x84U, 0x4FU, 0xF0U, 0x58U, 0x29U, 0x62U, 0x76U, 0x0EU, 0x40U, 0x00U, 0x00U, 0x00U, 0x0CU,
    0x89U, 0x28U, 0x49U, 0x0DU, 0x43U, 0x3CU, 0x0BU, 0xE1U, 0xB8U, 0x46U, 0x11U, 0x3FU, 0xC1U, 0x62U, 0x96U, 0x27U, 0x60U, 0xECU
};

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
 * @brief Builds a P25 RF frame.
 * @param payload The payload data to include in the frame.
 * @param frame The buffer to store the constructed frame.
 */
void buildP25RFFrame(const uint8_t* payload, uint8_t* frame)
{
    frame[0U] = modem::TAG_DATA;
    frame[1U] = 0x01U;
    ::memcpy(frame + 2U, payload, p25::defines::P25_LDU_FRAME_LENGTH_BYTES);
}

} // namespace

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Dummy implementation of a modem port for testing purposes.
 */
class P25TestModemPort final : public modem::port::IModemPort {
public:
    /**
     * @brief Finalizes the instance of the P25TestModemPort class.
     */
    ~P25TestModemPort() override = default;

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
 * @brief Lightweight network test double that records P25 reset calls.
 */
class P25TestNetwork final : public network::Network {
public:
    /**
     * @brief Initializes a new instance of the P25TestNetwork class.
     * @param localPort The local port number.
     * @param peerId The peer ID.
     */
    P25TestNetwork(uint16_t localPort = 0U, uint32_t peerId = 1U) :
        network::Network("127.0.0.1", 1U, localPort, peerId, "test", true, true, false, true, false, false, true, true, false, false, false, false),
        m_resetP25Count(0U)
    {
        // keep protocol gates deterministic for this P25-focused harness
        m_dmrEnabled = false;
        m_p25Enabled = true;
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
     * @brief Sends a P25 LDU1 frame.
     * @param targetPeerId The target peer ID.
     * @param streamId The stream ID.
     * @param seq The sequence number.
     * @param srcId The source ID.
     * @param dstId The destination ID.
     * @returns bool True if the frame was successfully sent, false otherwise.
     */
    bool sendP25LDU1Frame(uint32_t targetPeerId, uint32_t streamId, uint16_t seq, uint32_t srcId, uint32_t dstId)
    {
        p25::lc::LC control;
        control.setLCO(p25::defines::LCO::GROUP);
        control.setMFId(p25::defines::MFG_STANDARD);
        control.setSrcId(srcId);
        control.setDstId(dstId);

        p25::data::LowSpeedData lsd;

        uint32_t messageLength = 0U;
        UInt8Array message = createP25_LDU1Message(messageLength, control, lsd, CAL_P25_LDU1_1K, p25::defines::FrameType::DATA_UNIT, 0x00U);
        if (message == nullptr || messageLength == 0U) {
            return false;
        }

        return m_frameQueue->write(message.get(), messageLength, streamId, targetPeerId, m_peerId,
            { network::NET_FUNC::PROTOCOL, network::NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, seq, m_addr, m_addrLen);
    }

    /**
     * @brief Sends a P25 LDU2 frame.
     * @param targetPeerId The target peer ID.
     * @param streamId The stream ID.
     * @param seq The sequence number.
     * @param srcId The source ID.
     * @param dstId The destination ID.
     * @returns bool True if the frame was successfully sent, false otherwise.
     */
    bool sendP25LDU2Frame(uint32_t targetPeerId, uint32_t streamId, uint16_t seq, uint32_t srcId, uint32_t dstId)
    {
        p25::lc::LC control;
        control.setLCO(p25::defines::LCO::GROUP);
        control.setMFId(p25::defines::MFG_STANDARD);
        control.setSrcId(srcId);
        control.setDstId(dstId);

        p25::data::LowSpeedData lsd;

        uint32_t messageLength = 0U;
        UInt8Array message = createP25_LDU2Message(messageLength, control, lsd, CAL_P25_LDU2_1K, 0x00U);
        if (message == nullptr || messageLength == 0U) {
            return false;
        }

        return m_frameQueue->write(message.get(), messageLength, streamId, targetPeerId, m_peerId,
            { network::NET_FUNC::PROTOCOL, network::NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, seq, m_addr, m_addrLen);
    }

    /**
     * @brief Sends a P25 TDU frame.
     * @param targetPeerId The target peer ID.
     * @param streamId The stream ID.
     * @param seq The sequence number.
     * @param srcId The source ID.
     * @param dstId The destination ID.
     * @returns bool True if the frame was successfully sent, false otherwise.
     */
    bool sendP25TDUFrame(uint32_t targetPeerId, uint32_t streamId, uint16_t seq, uint32_t srcId, uint32_t dstId)
    {
        p25::lc::LC control;
        control.setLCO(p25::defines::LCO::GROUP);
        control.setMFId(p25::defines::MFG_STANDARD);
        control.setSrcId(srcId);
        control.setDstId(dstId);

        p25::data::LowSpeedData lsd;

        uint32_t messageLength = 0U;
        UInt8Array message = createP25_TDUMessage(messageLength, control, lsd, 0x00U);
        if (message == nullptr || messageLength == 0U) {
            return false;
        }

        return m_frameQueue->write(message.get(), messageLength, streamId, targetPeerId, m_peerId,
            { network::NET_FUNC::PROTOCOL, network::NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, seq, m_addr, m_addrLen);
    }

    /**
     * @brief 
     */
    void resetP25() override
    {
        ++m_resetP25Count;
        network::Network::resetP25();
    }

    /**
     * @brief Returns the number of times the P25 subsystem has been reset.
     * @returns uint32_t The number of times the P25 subsystem has been reset.
     */
    uint32_t resetP25Count() const
    {
        return m_resetP25Count;
    }

    /**
     * @brief Returns the currently locked incoming P25 stream ID.
     * @returns uint32_t Active incoming stream lock ID, or 0 when unlocked.
     */
    uint32_t rxP25StreamId() const
    {
        return m_rxP25StreamId;
    }

private:
    uint32_t m_resetP25Count;
};

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Harness class for testing P25 host control functionality.
 */
class P25HostHarness {
public:
    /**
     * @brief Initializes a new instance of the P25HostHarness class.
     * @param authoritative Indicates whether the host is authoritative.
     */
    explicit P25HostHarness(bool authoritative = true, bool withNetwork = false, uint16_t networkLocalPort = 0U, uint32_t networkPeerId = 1U) :
        m_rpc("127.0.0.1", 1U, 0U, "test", false),
        m_modem(new P25TestModemPort(), false, false, false, false, false, false,
            0U, 0U, 0U, 1024U, 4096U, 1024U, true, true, false, false, false, false),
        m_chLookup(),
        m_ridLookup("", 0U, false, false),
        m_tidLookup("", 0U, false, false),
        m_idenLookup("", 0U),
        m_rssiMapper(),
        m_network(withNetwork ? new P25TestNetwork(networkLocalPort, networkPeerId) : nullptr),
        m_control(nullptr)
    {
        g_RPC = &m_rpc;
        m_modem.setModeParams(false, true, false);

        m_control = new p25::Control(authoritative, 0x293U, 1U, 4096U, &m_modem, m_network,
            5U, 2U, false, &m_chLookup, &m_ridLookup, &m_tidLookup, &m_idenLookup,
            &m_rssiMapper, false, false, false, true, true);
    }
    /**
     * @brief Finalizes an instance of the P25HostHarness class.
     */
    ~P25HostHarness()
    {
        delete m_control;
        delete m_network;
        g_RPC = nullptr;
    }

    P25TestNetwork* network() const
    {
        return m_network;
    }

    /**
     * @brief Starts a network voice call with the specified source and destination IDs.
     * @param srcId The source ID for the network voice call.
     * @param dstId The destination ID for the network voice call.
     */
    void startNetworkVoiceCall(uint32_t srcId = 1001U, uint32_t dstId = 2001U)
    {
        p25::lc::LC control;
        control.setLCO(p25::defines::LCO::GROUP);
        control.setMFId(p25::defines::MFG_STANDARD);
        control.setSrcId(srcId);
        control.setDstId(dstId);

        p25::data::LowSpeedData lsd;

        HostTestHooks::p25StartNetCall(*m_control, control, lsd);
    }

public:
    network::NetRPC m_rpc;
    modem::Modem m_modem;
    lookups::ChannelLookup m_chLookup;
    lookups::RadioIdLookup m_ridLookup;
    lookups::TalkgroupRulesLookup m_tidLookup;
    lookups::IdenTableLookup m_idenLookup;
    lookups::RSSIInterpolator m_rssiMapper;
    P25TestNetwork* m_network;
    p25::Control* m_control;
};

TEST_CASE("P25 active talkgroup updates preserve the working TGID", "[p25][active_tg]")
{
    P25HostHarness harness;
    const uint32_t workingDstId = 2001U;
    uint8_t index = 0U;
    uint32_t dstIdB = 0U;
    bool hasDstIdB = false;

    const std::vector<uint32_t> activeTG = { workingDstId, 2002U, 2003U, 2004U, 2005U };
    REQUIRE(HostTestHooks::p25NextActiveTalkgroups(*harness.m_control, activeTG, index,
        workingDstId, dstIdB, hasDstIdB));
    REQUIRE(hasDstIdB);
    REQUIRE(dstIdB == 2002U);
    REQUIRE(dstIdB != workingDstId);

    REQUIRE(HostTestHooks::p25NextActiveTalkgroups(*harness.m_control, activeTG, index,
        workingDstId, dstIdB, hasDstIdB));
    REQUIRE(hasDstIdB);
    REQUIRE(dstIdB == 2003U);
    REQUIRE(dstIdB != workingDstId);

    REQUIRE(HostTestHooks::p25NextActiveTalkgroups(*harness.m_control, activeTG, index,
        workingDstId, dstIdB, hasDstIdB));
    REQUIRE(hasDstIdB);
    REQUIRE(dstIdB == 2004U);
    REQUIRE(dstIdB != workingDstId);

    REQUIRE(HostTestHooks::p25NextActiveTalkgroups(*harness.m_control, activeTG, index,
        workingDstId, dstIdB, hasDstIdB));
    REQUIRE(hasDstIdB);
    REQUIRE(dstIdB == 2005U);
    REQUIRE(dstIdB != workingDstId);

    const std::vector<uint32_t> onlyWorkingTG = { workingDstId, workingDstId };
    REQUIRE(HostTestHooks::p25NextActiveTalkgroups(*harness.m_control, onlyWorkingTG, index,
        workingDstId, dstIdB, hasDstIdB));
    REQUIRE_FALSE(hasDstIdB);
    REQUIRE(dstIdB == 0U);
}

TEST_CASE("P25 host arms the network watchdog when network voice starts", "[p25][host][control]")
{
    P25HostHarness harness;

    harness.startNetworkVoiceCall();

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::p25NetworkWatchdog(*harness.m_control).isRunning());
    REQUIRE_FALSE(HostTestHooks::p25NetworkWatchdog(*harness.m_control).hasExpired());
}

TEST_CASE("P25 host watchdog expiry returns network voice to idle", "[p25][host][control]")
{
    P25HostHarness harness;
    harness.startNetworkVoiceCall();

    HostTestHooks::p25NetworkWatchdog(*harness.m_control).clock(expireTimerTicks(HostTestHooks::p25NetworkWatchdog(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(HostTestHooks::p25TailOnIdle(*harness.m_control));
    REQUIRE_FALSE(HostTestHooks::p25NetworkWatchdog(*harness.m_control).isRunning());
}

TEST_CASE("P25 watchdog expiry resets network stream state", "[p25][host][control][net][stream]")
{
    P25HostHarness harness(true, true);
    harness.startNetworkVoiceCall();

    REQUIRE(harness.network() != nullptr);
    REQUIRE(harness.network()->resetP25Count() == 0U);

    HostTestHooks::p25NetworkWatchdog(*harness.m_control).clock(expireTimerTicks(HostTestHooks::p25NetworkWatchdog(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(harness.network()->resetP25Count() == 1U);
}

TEST_CASE("P25 host net hang expiry clears active network voice state", "[p25][host][control]")
{
    P25HostHarness harness;
    harness.startNetworkVoiceCall();

    HostTestHooks::p25NetTGHang(*harness.m_control).clock(expireTimerTicks(HostTestHooks::p25NetTGHang(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(HostTestHooks::p25TailOnIdle(*harness.m_control));
    REQUIRE(HostTestHooks::p25NetLastDstId(*harness.m_control) == 0U);
    REQUIRE(HostTestHooks::p25NetLastSrcId(*harness.m_control) == 0U);
    REQUIRE_FALSE(HostTestHooks::p25NetworkWatchdog(*harness.m_control).isRunning());
}

TEST_CASE("P25 net hang expiry resets network stream state", "[p25][host][control][net][stream]")
{
    P25HostHarness harness(true, true);
    harness.startNetworkVoiceCall();

    REQUIRE(harness.network() != nullptr);
    REQUIRE(harness.network()->resetP25Count() == 0U);

    HostTestHooks::p25NetTGHang(*harness.m_control).clock(expireTimerTicks(HostTestHooks::p25NetTGHang(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(harness.network()->resetP25Count() == 1U);
}

TEST_CASE("P25 recovers inconsistent net state via network reset", "[p25][host][control][net][stream]")
{
    P25HostHarness harness(true, true);

    REQUIRE(harness.network() != nullptr);
    REQUIRE(harness.network()->resetP25Count() == 0U);

    // Simulate a stream lockup/inconsistency: non-idle net state with no last dst and stopped watchdog.
    HostTestHooks::p25SetNetState(*harness.m_control, RS_NET_AUDIO, 0U, 0U);
    HostTestHooks::p25NetworkWatchdog(*harness.m_control).stop();

    p25::lc::LC control;
    control.setLCO(p25::defines::LCO::GROUP);
    control.setSrcId(1001U);
    control.setDstId(2001U);

    (void)HostTestHooks::p25TerminateNetCall(*harness.m_control, control, p25::defines::DUID::TDU);

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(harness.network()->resetP25Count() == 1U);
}

TEST_CASE("P25 host e2e loopback handles missed frames without dropping active call", "[p25][host][control][net][e2e]")
{
    P25HostHarness harness;
    harness.startNetworkVoiceCall(1001U, 2001U);

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::p25NetLastSrcId(*harness.m_control) == 1001U);
    REQUIRE(HostTestHooks::p25NetLastDstId(*harness.m_control) == 2001U);

    // Follow-on network traffic on the same call should keep the call active.
    harness.startNetworkVoiceCall(1001U, 2001U);

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::p25NetworkWatchdog(*harness.m_control).isRunning());
    REQUIRE(HostTestHooks::p25NetLastSrcId(*harness.m_control) == 1001U);
    REQUIRE(HostTestHooks::p25NetLastDstId(*harness.m_control) == 2001U);
}

TEST_CASE("P25 host e2e loopback handles out-of-order frames without dropping active call", "[p25][host][control][net][e2e]")
{
    P25HostHarness harness;
    harness.startNetworkVoiceCall(1501U, 2501U);

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::p25NetLastSrcId(*harness.m_control) == 1501U);
    REQUIRE(HostTestHooks::p25NetLastDstId(*harness.m_control) == 2501U);

    // Re-entrant same-destination network audio remains active.
    harness.startNetworkVoiceCall(1501U, 2501U);

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::p25NetworkWatchdog(*harness.m_control).isRunning());
    REQUIRE(HostTestHooks::p25NetLastSrcId(*harness.m_control) == 1501U);
    REQUIRE(HostTestHooks::p25NetLastDstId(*harness.m_control) == 2501U);
}

TEST_CASE("P25 host e2e loopback handles dropped call terminator and returns idle", "[p25][host][control][net][e2e]")
{
    P25HostHarness harness;
    harness.startNetworkVoiceCall(1101U, 2101U);

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_AUDIO);

    p25::lc::LC control;
    control.setLCO(p25::defines::LCO::GROUP);
    control.setSrcId(1101U);
    control.setDstId(2101U);
    REQUIRE(HostTestHooks::p25TerminateNetCall(*harness.m_control, control, p25::defines::DUID::TDU));

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(HostTestHooks::p25TailOnIdle(*harness.m_control));
    REQUIRE_FALSE(HostTestHooks::p25NetworkWatchdog(*harness.m_control).isRunning());
}

TEST_CASE("P25 host e2e loopback times out stale call and resets stream state", "[p25][host][control][net][e2e]")
{
    P25HostHarness harness(true, true);
    harness.startNetworkVoiceCall(1201U, 2201U);

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(harness.network()->resetP25Count() == 0U);

    HostTestHooks::p25NetworkWatchdog(*harness.m_control).clock(expireTimerTicks(HostTestHooks::p25NetworkWatchdog(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(harness.network()->resetP25Count() == 1U);
}

TEST_CASE("P25 host e2e loopback times out a stream before call state starts", "[p25][host][control][net][e2e]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 6007U;
    const uint32_t staleStream = 0x500101U;
    const uint32_t nextStream = 0x500102U;

    P25HostHarness harness(true, true, hostPort, hostPeerId);
    P25TestNetwork sender(senderPort, 6008U);

    REQUIRE(harness.network() != nullptr);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    REQUIRE(sender.sendP25LDU1Frame(hostPeerId, staleStream, 400U, 1301U, 2301U));

    for (uint32_t i = 0U; i < 40U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (harness.network()->rxP25StreamId() == staleStream) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(harness.network()->rxP25StreamId() == staleStream);
    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(HostTestHooks::p25NetworkWatchdog(*harness.m_control).isRunning());

    HostTestHooks::p25NetworkWatchdog(*harness.m_control).clock(expireTimerTicks(HostTestHooks::p25NetworkWatchdog(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(harness.network()->rxP25StreamId() == 0U);

    REQUIRE(sender.sendP25LDU1Frame(hostPeerId, nextStream, 500U, 1301U, 2301U));
    REQUIRE(sender.sendP25LDU2Frame(hostPeerId, nextStream, 501U, 1301U, 2301U));

    for (uint32_t i = 0U; i < 40U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::p25NetState(*harness.m_control) == RS_NET_AUDIO) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(harness.network()->rxP25StreamId() == nextStream);
}

TEST_CASE("P25 host e2e loopback preserves a continuing network stream across an RF collision", "[p25][host][control][net][e2e]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 6009U;
    const uint32_t streamId = 0x500201U;

    P25HostHarness harness(true, true, hostPort, hostPeerId);
    P25TestNetwork sender(senderPort, 6010U);

    REQUIRE(harness.network() != nullptr);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    HostTestHooks::p25SetRFCall(*harness.m_control, 1401U, 2401U);
    REQUIRE(sender.sendP25LDU1Frame(hostPeerId, streamId, 600U, 1501U, 2501U));

    for (uint32_t i = 0U; i < 40U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (harness.network()->rxP25StreamId() == streamId &&
            HostTestHooks::p25NetworkWatchdog(*harness.m_control).isRunning()) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(harness.network()->rxP25StreamId() == streamId);
    REQUIRE(harness.network()->resetP25Count() == 0U);

    HostTestHooks::p25NetworkWatchdog(*harness.m_control).clock(expireTimerTicks(HostTestHooks::p25NetworkWatchdog(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::p25NetworkWatchdog(*harness.m_control).hasExpired());
    REQUIRE(harness.network()->rxP25StreamId() == streamId);
    REQUIRE(harness.network()->resetP25Count() == 0U);

    HostTestHooks::p25ClearRFCall(*harness.m_control);
    REQUIRE(sender.sendP25LDU1Frame(hostPeerId, streamId, 601U, 1501U, 2501U));
    REQUIRE(sender.sendP25LDU2Frame(hostPeerId, streamId, 602U, 1501U, 2501U));

    for (uint32_t i = 0U; i < 40U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::p25NetState(*harness.m_control) == RS_NET_AUDIO) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(harness.network()->rxP25StreamId() == streamId);
    REQUIRE(harness.network()->resetP25Count() == 0U);
}

TEST_CASE("P25 host e2e loopback clears a stopped network stream after an RF collision", "[p25][host][control][net][e2e]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 6011U;
    const uint32_t streamId = 0x500202U;

    P25HostHarness harness(true, true, hostPort, hostPeerId);
    P25TestNetwork sender(senderPort, 6012U);

    REQUIRE(harness.network() != nullptr);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    HostTestHooks::p25SetRFCall(*harness.m_control, 1401U, 2401U);
    REQUIRE(sender.sendP25LDU1Frame(hostPeerId, streamId, 700U, 1501U, 2501U));

    for (uint32_t i = 0U; i < 40U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (harness.network()->rxP25StreamId() == streamId &&
            HostTestHooks::p25NetworkWatchdog(*harness.m_control).isRunning()) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(harness.network()->rxP25StreamId() == streamId);
    REQUIRE(harness.network()->resetP25Count() == 0U);

    HostTestHooks::p25NetworkWatchdog(*harness.m_control).clock(expireTimerTicks(HostTestHooks::p25NetworkWatchdog(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::p25NetworkWatchdog(*harness.m_control).hasExpired());
    REQUIRE(harness.network()->rxP25StreamId() == streamId);
    REQUIRE(harness.network()->resetP25Count() == 0U);

    HostTestHooks::p25ClearRFCall(*harness.m_control);
    harness.m_control->clock();

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE_FALSE(HostTestHooks::p25NetworkWatchdog(*harness.m_control).isRunning());
    REQUIRE(harness.network()->rxP25StreamId() == 0U);
    REQUIRE(harness.network()->resetP25Count() == 1U);
}

TEST_CASE("P25 host e2e loopback enforces stream lock until active stream terminates", "[p25][host][control][net][e2e]")
{
    const uint16_t hostPort = reserveLoopbackPort();
    const uint16_t senderPort = reserveLoopbackPort();
    REQUIRE(hostPort != 0U);
    REQUIRE(senderPort != 0U);
    REQUIRE(hostPort != senderPort);

    const uint32_t hostPeerId = 6007U;
    const uint32_t streamA = 0x500101U;
    const uint32_t streamB = 0x500102U;

    P25HostHarness harness(true, true, hostPort, hostPeerId);
    P25TestNetwork sender(senderPort, 6008U);

    REQUIRE(harness.network() != nullptr);
    REQUIRE(harness.network()->activateLoopback("127.0.0.1", senderPort));
    REQUIRE(sender.activateLoopback("127.0.0.1", hostPort));

    // Start call on stream A.
    REQUIRE(sender.sendP25LDU1Frame(hostPeerId, streamA, 400U, 1301U, 2301U));
    REQUIRE(sender.sendP25LDU2Frame(hostPeerId, streamA, 401U, 1301U, 2301U));

    for (uint32_t i = 0U; i < 40U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::p25NetState(*harness.m_control) == RS_NET_AUDIO) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::p25NetLastSrcId(*harness.m_control) == 1301U);
    REQUIRE(HostTestHooks::p25NetLastDstId(*harness.m_control) == 2301U);
    REQUIRE(harness.network()->rxP25StreamId() == streamA);

    // Competing stream B should be ignored while stream A is active and locked.
    REQUIRE(sender.sendP25LDU1Frame(hostPeerId, streamB, 500U, 1301U, 2301U));
    REQUIRE(sender.sendP25LDU2Frame(hostPeerId, streamB, 501U, 1301U, 2301U));

    for (uint32_t i = 0U; i < 30U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::p25NetLastSrcId(*harness.m_control) == 1301U);
    REQUIRE(HostTestHooks::p25NetLastDstId(*harness.m_control) == 2301U);
    REQUIRE(harness.network()->rxP25StreamId() == streamA);

    // End stream A; stream lock should be released.
    REQUIRE(sender.sendP25TDUFrame(hostPeerId, streamA, RTP_END_OF_CALL_SEQ, 1301U, 2301U));

    for (uint32_t i = 0U; i < 50U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(harness.network()->rxP25StreamId() == 0U);

    // Now stream B should be admitted after stream A terminates.
    REQUIRE(sender.sendP25LDU1Frame(hostPeerId, streamB, 502U, 1301U, 2301U));
    REQUIRE(sender.sendP25LDU2Frame(hostPeerId, streamB, 503U, 1301U, 2301U));

    for (uint32_t i = 0U; i < 40U; i++) {
        sender.clock(1U);
        harness.network()->clock(1U);
        harness.m_control->clock();
        if (HostTestHooks::p25NetState(*harness.m_control) == RS_NET_AUDIO) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    REQUIRE(harness.network()->rxP25StreamId() == streamB);
}

TEST_CASE("P25 host network terminator clears active network voice state", "[p25][host][control]")
{
    P25HostHarness harness;
    harness.startNetworkVoiceCall();

    p25::lc::LC control;
    control.setLCO(p25::defines::LCO::GROUP);
    control.setSrcId(1001U);
    control.setDstId(2001U);

    REQUIRE(HostTestHooks::p25TerminateNetCall(*harness.m_control, control));
    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE(HostTestHooks::p25TailOnIdle(*harness.m_control));
    REQUIRE_FALSE(HostTestHooks::p25NetworkWatchdog(*harness.m_control).isRunning());
}

TEST_CASE("P25 non-authoritative unpermit clears active network state", "[p25][host][control]")
{
    P25HostHarness harness(false);
    harness.m_control->permittedTG(2001U);
    harness.startNetworkVoiceCall();

    harness.m_control->permittedTG(0U);

    REQUIRE(HostTestHooks::p25PermittedDstId(*harness.m_control) == 0U);
    REQUIRE(HostTestHooks::p25NetState(*harness.m_control) == RS_NET_IDLE);
    REQUIRE_FALSE(HostTestHooks::p25NetworkWatchdog(*harness.m_control).isRunning());
}

TEST_CASE("P25 clearRFReject returns rejected RF state to listening", "[p25][host][control][rf]")
{
    P25HostHarness harness;
    HostTestHooks::p25SetRFRejected(*harness.m_control);

    harness.m_control->clearRFReject();

    REQUIRE(HostTestHooks::p25RFState(*harness.m_control) == RS_RF_LISTENING);
}

TEST_CASE("P25 processFrame accepts calibration-based RF LDU1 frame", "[p25][host][rf]")
{
    P25HostHarness harness;
    harness.m_control->reset();

    uint8_t frame[p25::defines::P25_LDU_FRAME_LENGTH_BYTES + 2U];
    buildP25RFFrame(CAL_P25_LDU1_1K, frame);
    HostTestHooks::p25StampRFFrameNID(*harness.m_control, frame, p25::defines::DUID::LDU1);

    REQUIRE(harness.m_control->processFrame(frame, sizeof(frame)));
    REQUIRE(HostTestHooks::p25RFState(*harness.m_control) == RS_RF_AUDIO);
    REQUIRE(harness.m_control->getLastSrcId() == 1U);
    REQUIRE(harness.m_control->getLastDstId() == 1U);
}

TEST_CASE("P25 processFrame accepts calibration-based RF LDU2 frame after LDU1", "[p25][host][rf]")
{
    P25HostHarness harness;
    harness.m_control->reset();

    uint8_t ldu1[p25::defines::P25_LDU_FRAME_LENGTH_BYTES + 2U];
    uint8_t ldu2[p25::defines::P25_LDU_FRAME_LENGTH_BYTES + 2U];
    buildP25RFFrame(CAL_P25_LDU1_1K, ldu1);
    buildP25RFFrame(CAL_P25_LDU2_1K, ldu2);
    HostTestHooks::p25StampRFFrameNID(*harness.m_control, ldu1, p25::defines::DUID::LDU1);
    HostTestHooks::p25StampRFFrameNID(*harness.m_control, ldu2, p25::defines::DUID::LDU2);

    REQUIRE(harness.m_control->processFrame(ldu1, sizeof(ldu1)));
    REQUIRE(harness.m_control->processFrame(ldu2, sizeof(ldu2)));
    REQUIRE(HostTestHooks::p25RFState(*harness.m_control) == RS_RF_AUDIO);
    REQUIRE(harness.m_control->getLastSrcId() == 1U);
    REQUIRE(harness.m_control->getLastDstId() == 1U);
}

TEST_CASE("P25 rfTGHang expiry ends active RF call and returns to listening", "[p25][host][control][rf]")
{
    P25HostHarness harness;
    harness.m_control->reset();

    uint8_t ldu1[p25::defines::P25_LDU_FRAME_LENGTH_BYTES + 2U];
    buildP25RFFrame(CAL_P25_LDU1_1K, ldu1);
    HostTestHooks::p25StampRFFrameNID(*harness.m_control, ldu1, p25::defines::DUID::LDU1);

    REQUIRE(harness.m_control->processFrame(ldu1, sizeof(ldu1)));
    REQUIRE(HostTestHooks::p25RFState(*harness.m_control) == RS_RF_AUDIO);

    HostTestHooks::p25RFTGHang(*harness.m_control).start();
    HostTestHooks::p25RFTGHang(*harness.m_control).clock(expireTimerTicks(HostTestHooks::p25RFTGHang(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::p25RFState(*harness.m_control) == RS_RF_LISTENING);
    REQUIRE_FALSE(HostTestHooks::p25RFTGHang(*harness.m_control).isRunning());
    REQUIRE_FALSE(HostTestHooks::p25RFLossWatchdog(*harness.m_control).isRunning());
}

TEST_CASE("P25 rfTGHang zero fallback arms rfLossWatchdog and recovers RF state", "[p25][host][control][rf]")
{
    P25HostHarness harness;
    harness.m_control->reset();

    uint8_t ldu1[p25::defines::P25_LDU_FRAME_LENGTH_BYTES + 2U];
    uint8_t ldu2[p25::defines::P25_LDU_FRAME_LENGTH_BYTES + 2U];
    buildP25RFFrame(CAL_P25_LDU1_1K, ldu1);
    buildP25RFFrame(CAL_P25_LDU2_1K, ldu2);
    HostTestHooks::p25StampRFFrameNID(*harness.m_control, ldu1, p25::defines::DUID::LDU1);
    HostTestHooks::p25StampRFFrameNID(*harness.m_control, ldu2, p25::defines::DUID::LDU2);

    REQUIRE(harness.m_control->processFrame(ldu1, sizeof(ldu1)));
    REQUIRE(HostTestHooks::p25RFState(*harness.m_control) == RS_RF_AUDIO);

    HostTestHooks::p25RFTGHang(*harness.m_control).setTimeout(0U);
    REQUIRE_FALSE(HostTestHooks::p25RFLossWatchdog(*harness.m_control).isRunning());

    REQUIRE(harness.m_control->processFrame(ldu2, sizeof(ldu2)));
    REQUIRE(HostTestHooks::p25RFLossWatchdog(*harness.m_control).isRunning());

    HostTestHooks::p25RFLossWatchdog(*harness.m_control).clock(expireTimerTicks(HostTestHooks::p25RFLossWatchdog(*harness.m_control)));
    harness.m_control->clock();

    REQUIRE(HostTestHooks::p25RFState(*harness.m_control) == RS_RF_LISTENING);
    REQUIRE_FALSE(HostTestHooks::p25RFLossWatchdog(*harness.m_control).isRunning());
}
