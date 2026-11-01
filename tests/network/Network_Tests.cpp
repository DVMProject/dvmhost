// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES FROM THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/network/FrameQueue.h"
#include "common/network/Network.h"
#include "common/network/RTPFNEHeader.h"
#include "common/network/RTPHeader.h"
#include "common/network/udp/Socket.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace network;
using namespace network::frame;
using namespace network::udp;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief TestableNetwork is a subclass of Network that exposes internal state for testing purposes. It allows
 * direct manipulation of the network status, activity transfer flags, and diagnostic transfer flags. This class
 */
class TestableNetwork : public Network {
public:
    using Network::Network;

    /**
     * @brief Sets the network connection status for testing purposes.
     * @param status The NET_CONN_STATUS value to set.
     */
    void setStatusForTest(NET_CONN_STATUS status) { m_status = status; }
    /**
     * @brief Gets the current network connection status for testing purposes.
     * @returns NET_CONN_STATUS The current network connection status.
     */
    NET_CONN_STATUS getStatusForTest() const { return m_status; }

    /**
     * @brief Sets the allow activity transfer flag for testing purposes.
     * @param enabled True to enable activity transfer, false to disable.
     */
    void setAllowActivityTransferForTest(bool enabled) { m_allowActivityTransfer = enabled; }
    /**
     * @brief Sets the allow diagnostic transfer flag for testing purposes.
     * @param enabled True to enable diagnostic transfer, false to disable.
     */
    void setAllowDiagnosticTransferForTest(bool enabled) { m_allowDiagnosticTransfer = enabled; }
    /**
     * @brief Gets the allow activity transfer flag for testing purposes.
     * @returns bool True if activity transfer is allowed, false otherwise.
     */
    bool getAllowActivityTransferForTest() const { return m_allowActivityTransfer; }
    /**
     * @brief Gets the allow diagnostic transfer flag for testing purposes.
     * @returns bool True if diagnostic transfer is allowed, false otherwise.
     */
    bool getAllowDiagnosticTransferForTest() const { return m_allowDiagnosticTransfer; }

    /**
     * @brief Gets the duplicate connection flag for testing purposes.
     * @returns bool True if a duplicate connection has been flagged, false otherwise.
     */
    bool hasDuplicateConnFlagForTest() const { return m_flaggedDuplicateConn; }
    /**
     * @brief Gets the maximum retry count for testing purposes.
     * @returns uint8_t The maximum retry count.
     */
    uint8_t getMaxRetryCountForTest() const { return m_maxRetryCount; }
    /**
     * @brief Gets the current retry count for testing purposes.
     * @returns uint8_t The current retry count.
     */
    uint32_t getRemotePeerIdForTest() const { return m_remotePeerId; }
};

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------
namespace {

/**
 * @brief Reserve a UDP port on the loopback interface for testing.
 * @returns uint16_t Reserved UDP port number, or 0 if reservation failed.
 */
static uint16_t reserveLoopbackPort()
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
 * @brief Wait for a frame message to be received on the specified FrameQueue.
 * @param queue Reference to the FrameQueue to read from.
 * @param[out] out String to store the received message.
 * @param[out] rtpHeader RTPHeader to store the received RTP header.
 * @param[out] fneHeader RTPFNEHeader to store the received FNE header.
 * @returns bool True if a message was received, otherwise false.
 */
static bool waitForFrameMessage(FrameQueue& queue, std::string& out, RTPHeader& rtpHeader, RTPFNEHeader& fneHeader)
{
    for (uint32_t attempt = 0U; attempt < 100U; attempt++) {
        int messageLength = -1;
        sockaddr_storage source = {};
        uint32_t sourceLen = 0U;
        UInt8Array message = queue.read(messageLength, source, sourceLen, &rtpHeader, &fneHeader);
        if (message != nullptr && messageLength > 0) {
            out.assign(reinterpret_cast<char*>(message.get()), (size_t)messageLength);
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return false;
}

/**
 * @brief Send a frame message to the specified destination using the FrameQueue.
 * @param senderQueue Reference to the FrameQueue to send from.
 * @param destination sockaddr_storage containing the destination address.
 * @param destinationLen Length of the destination address.
 * @param payload Vector containing the message payload to send.
 * @param opcode OpcodePair containing the function and sub-function.
 * @param streamId Stream ID for the message.
 * @param peerId Peer ID for the message.
 * @param ssrc SSRC for the RTP header.
 * @param sequence Sequence number for the RTP header.
 * @returns bool True if the message was sent successfully, otherwise false.
 */
static bool sendLoopbackFrame(FrameQueue& senderQueue, sockaddr_storage& destination, uint32_t destinationLen,
    const std::vector<uint8_t>& payload, FrameQueue::OpcodePair opcode, uint32_t streamId, uint32_t peerId,
    uint32_t ssrc, uint16_t sequence)
{
    return senderQueue.write(payload.data(), (uint32_t)payload.size(), streamId, peerId, ssrc, opcode, sequence,
        destination, destinationLen);
}

/**
 * @brief Complete the handshake process for the TestableNetwork instance.
 * @param network Reference to the TestableNetwork instance.
 * @param masterReceiverQueue Reference to the FrameQueue for receiving messages from the master.
 * @param masterSenderQueue Reference to the FrameQueue for sending messages to the master.
 * @param networkDestination sockaddr_storage containing the destination address for the network.
 * @param networkDestinationLen Length of the destination address.
 * @param peerId Peer ID for the network.
 */
static void completeHandshake(TestableNetwork& network, FrameQueue& masterReceiverQueue, FrameQueue& masterSenderQueue,
    sockaddr_storage& networkDestination, uint32_t networkDestinationLen, uint32_t peerId)
{
    network.clock(11000U);

    std::string loginPayload;
    RTPHeader loginRtp;
    RTPFNEHeader loginFne;
    REQUIRE(waitForFrameMessage(masterReceiverQueue, loginPayload, loginRtp, loginFne));
    REQUIRE(loginFne.getFunction() == NET_FUNC::RPTL);

    std::array<uint8_t, 10U> saltPayload = {};
    ::memcpy(saltPayload.data() + 6U, "ABCD", 4U);
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, std::vector<uint8_t>(saltPayload.begin(), saltPayload.end()),
        { NET_FUNC::ACK, NET_SUBFUNC::NOP }, loginFne.getStreamId(), peerId, peerId, 0U));

    network.clock(1000U);

    std::string authPayload;
    RTPHeader authRtp;
    RTPFNEHeader authFne;
    REQUIRE(waitForFrameMessage(masterReceiverQueue, authPayload, authRtp, authFne));
    REQUIRE(authFne.getFunction() == NET_FUNC::RPTK);

    std::array<uint8_t, 8U> ackPayload = {};
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, std::vector<uint8_t>(ackPayload.begin(), ackPayload.end()),
        { NET_FUNC::ACK, NET_SUBFUNC::NOP }, authFne.getStreamId(), peerId, peerId, 0U));

    network.clock(1000U);

    std::string configPayload;
    RTPHeader configRtp;
    RTPFNEHeader configFne;
    REQUIRE(waitForFrameMessage(masterReceiverQueue, configPayload, configRtp, configFne));
    REQUIRE(configFne.getFunction() == NET_FUNC::RPTC);

    std::array<uint8_t, 8U> configAckPayload = { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x00U };
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, std::vector<uint8_t>(configAckPayload.begin(), configAckPayload.end()),
        { NET_FUNC::ACK, NET_SUBFUNC::NOP }, configFne.getStreamId(), peerId, peerId, 0U));

    network.clock(1000U);
}

} // namespace

TEST_CASE("Network performs a full loopback login handshake", "[network][loopback]")
{
    const uint16_t masterPort = reserveLoopbackPort();
    REQUIRE(masterPort != 0U);

    const uint16_t localPort = reserveLoopbackPort();
    REQUIRE(localPort != 0U);

    Socket masterReceiver("127.0.0.1", masterPort);
    REQUIRE(masterReceiver.open(AF_INET));

    FrameQueue masterReceiverQueue(&masterReceiver, 0x1234U, false);
    FrameQueue masterSenderQueue(&masterReceiver, 0x5678U, false);

    sockaddr_storage networkDestination = {};
    uint32_t networkDestinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", localPort, networkDestination, networkDestinationLen) == 0);

    Network network("127.0.0.1", masterPort, localPort, 4242U, "loopback-password", true, false, true, true, true, true, true, true, true, true, false, false);
    network.enable(true);
    REQUIRE(network.open());

    bool connected = false;
    network.setPeerConnectedCallback([&connected]() {
        connected = true;
    });

    network.clock(11000U);

    std::string loginPayload;
    RTPHeader loginRtp;
    RTPFNEHeader loginFne;
    REQUIRE(waitForFrameMessage(masterReceiverQueue, loginPayload, loginRtp, loginFne));
    REQUIRE(loginFne.getFunction() == NET_FUNC::RPTL);
    REQUIRE(loginPayload.size() == 8U);
    REQUIRE(std::string(loginPayload.begin(), loginPayload.begin() + 4U) == TAG_REPEATER_LOGIN);

    std::array<uint8_t, 10U> saltPayload = {};
    ::memcpy(saltPayload.data() + 6U, "ABCD", 4U);

    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, std::vector<uint8_t>(saltPayload.begin(), saltPayload.end()),
        { NET_FUNC::ACK, NET_SUBFUNC::NOP }, loginFne.getStreamId(), 4242U, 4242U, 0U));

    network.clock(1000U);

    std::string authPayload;
    RTPHeader authRtp;
    RTPFNEHeader authFne;
    REQUIRE(waitForFrameMessage(masterReceiverQueue, authPayload, authRtp, authFne));
    REQUIRE(authFne.getFunction() == NET_FUNC::RPTK);
    REQUIRE(authPayload.size() == 40U);
    REQUIRE(std::string(authPayload.begin(), authPayload.begin() + 4U) == TAG_REPEATER_AUTH);

    std::array<uint8_t, 8U> ackPayload = {};
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, std::vector<uint8_t>(ackPayload.begin(), ackPayload.end()),
        { NET_FUNC::ACK, NET_SUBFUNC::NOP }, authFne.getStreamId(), 4242U, 4242U, 0U));

    network.clock(1000U);

    std::string configPayload;
    RTPHeader configRtp;
    RTPFNEHeader configFne;
    REQUIRE(waitForFrameMessage(masterReceiverQueue, configPayload, configRtp, configFne));
    REQUIRE(configFne.getFunction() == NET_FUNC::RPTC);
    REQUIRE(configPayload.size() >= 8U);
    REQUIRE(std::string(configPayload.begin(), configPayload.begin() + 4U) == TAG_REPEATER_CONFIG);

    std::array<uint8_t, 8U> configAckPayload = { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x00U };
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, std::vector<uint8_t>(configAckPayload.begin(), configAckPayload.end()),
        { NET_FUNC::ACK, NET_SUBFUNC::NOP }, configFne.getStreamId(), 4242U, 4242U, 0U));

    network.clock(1000U);
    REQUIRE(connected);

    masterReceiver.close();
}

TEST_CASE("Network stores loopback DMR protocol frames into the receive ring buffer", "[network][loopback]")
{
    const uint16_t masterPort = reserveLoopbackPort();
    REQUIRE(masterPort != 0U);

    const uint16_t localPort = reserveLoopbackPort();
    REQUIRE(localPort != 0U);

    Socket masterReceiver("127.0.0.1", masterPort);
    REQUIRE(masterReceiver.open(AF_INET));

    FrameQueue masterReceiverQueue(&masterReceiver, 0x9ABCU, false);
    FrameQueue masterSenderQueue(&masterReceiver, 0xDEF0U, false);

    sockaddr_storage networkDestination = {};
    uint32_t networkDestinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", localPort, networkDestination, networkDestinationLen) == 0);

    Network network("127.0.0.1", masterPort, localPort, 7777U, "loopback-password", true, false, true, false, false, false, true, true, true, true, false, false);
    network.enable(true);
    REQUIRE(network.open());

    network.resetDMR(1U);

    network.clock(11000U);

    std::string loginPayload;
    RTPHeader loginRtp;
    RTPFNEHeader loginFne;
    REQUIRE(waitForFrameMessage(masterReceiverQueue, loginPayload, loginRtp, loginFne));

    std::array<uint8_t, 10U> saltPayload = {};
    ::memcpy(saltPayload.data() + 6U, "ABCD", 4U);
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, std::vector<uint8_t>(saltPayload.begin(), saltPayload.end()),
        { NET_FUNC::ACK, NET_SUBFUNC::NOP }, loginFne.getStreamId(), 7777U, 7777U, 0U));

    network.clock(1000U);

    std::string authPayload;
    RTPHeader authRtp;
    RTPFNEHeader authFne;
    REQUIRE(waitForFrameMessage(masterReceiverQueue, authPayload, authRtp, authFne));

    std::array<uint8_t, 8U> ackPayload = {};
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, std::vector<uint8_t>(ackPayload.begin(), ackPayload.end()),
        { NET_FUNC::ACK, NET_SUBFUNC::NOP }, authFne.getStreamId(), 7777U, 7777U, 0U));

    network.clock(1000U);

    std::string configPayload;
    RTPHeader configRtp;
    RTPFNEHeader configFne;
    REQUIRE(waitForFrameMessage(masterReceiverQueue, configPayload, configRtp, configFne));

    std::array<uint8_t, 8U> configAckPayload = { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x00U };
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, std::vector<uint8_t>(configAckPayload.begin(), configAckPayload.end()),
        { NET_FUNC::ACK, NET_SUBFUNC::NOP }, configFne.getStreamId(), 7777U, 7777U, 0U));

    network.clock(1000U);

    std::vector<uint8_t> payload(DMR_PACKET_LENGTH + PACKET_PAD, 0x00U);
    ::memcpy(payload.data(), TAG_DMR_DATA, 4U);
    payload[4U] = 0x10U;       // DMR sequence number
    payload[15U] = 0x21U;      // Slot 1, data sync, Voice LC Header
    payload[20U] = 0x11U;      // Distinct CAI bytes for the round-trip check
    payload[21U] = 0x12U;
    payload[52U] = 0x13U;
    payload[53U] = 0x14U;      // BER
    payload[54U] = 0x15U;      // RSSI
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, payload,
        { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, 0x12345678U, 7777U, 7777U, 0x1234U));

    network.clock(0U);

    bool ret = false;
    uint32_t frameLength = 0U;
    UInt8Array decoded = network.readDMR(ret, frameLength);
    REQUIRE(ret);
    REQUIRE(frameLength == payload.size());
    REQUIRE(decoded != nullptr);
    REQUIRE(std::memcmp(decoded.get(), payload.data(), payload.size()) == 0);

    masterReceiver.close();
}

TEST_CASE("Network stores loopback NXDN protocol frames into the receive ring buffer", "[network][loopback][nxdn]")
{
    const uint16_t masterPort = reserveLoopbackPort();
    REQUIRE(masterPort != 0U);

    const uint16_t localPort = reserveLoopbackPort();
    REQUIRE(localPort != 0U);

    Socket masterReceiver("127.0.0.1", masterPort);
    REQUIRE(masterReceiver.open(AF_INET));

    FrameQueue masterReceiverQueue(&masterReceiver, 0x9ABDU, false);
    FrameQueue masterSenderQueue(&masterReceiver, 0xDEF1U, false);

    sockaddr_storage networkDestination = {};
    uint32_t networkDestinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", localPort, networkDestination, networkDestinationLen) == 0);

    TestableNetwork network("127.0.0.1", masterPort, localPort, 7778U, "loopback-password", true, false, false, false, true, false, true, true, true, true, false, false);
    network.enable(true);
    REQUIRE(network.open());

    completeHandshake(network, masterReceiverQueue, masterSenderQueue, networkDestination, networkDestinationLen, 7778U);

    std::vector<uint8_t> payload(NXDN_PACKET_LENGTH + PACKET_PAD, 0x00U);
    ::memcpy(payload.data(), TAG_NXDN_DATA, 4U);
    payload[4U] = 0x20U;       // NXDN sequence number
    payload[24U] = 0x21U;      // Distinct CAI bytes for the round-trip check
    payload[25U] = 0x22U;
    payload[71U] = 0x23U;
    payload[72U] = 0x24U;      // BER
    payload[73U] = 0x25U;      // RSSI
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, payload,
        { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN }, 0x23456789U, 7778U, 7778U, 0x2345U));

    network.clock(0U);

    bool ret = false;
    uint32_t frameLength = 0U;
    UInt8Array decoded = network.readNXDN(ret, frameLength);
    REQUIRE(ret);
    REQUIRE(frameLength == payload.size());
    REQUIRE(decoded != nullptr);
    REQUIRE(std::memcmp(decoded.get(), payload.data(), payload.size()) == 0);

    masterReceiver.close();
}

TEST_CASE("Network stores loopback P25 Phase 2 protocol frames into the receive ring buffer", "[network][loopback][p25][p2]")
{
    const uint16_t masterPort = reserveLoopbackPort();
    REQUIRE(masterPort != 0U);

    const uint16_t localPort = reserveLoopbackPort();
    REQUIRE(localPort != 0U);

    Socket masterReceiver("127.0.0.1", masterPort);
    REQUIRE(masterReceiver.open(AF_INET));

    FrameQueue masterReceiverQueue(&masterReceiver, 0x9ABEU, false);
    FrameQueue masterSenderQueue(&masterReceiver, 0xDEF2U, false);

    sockaddr_storage networkDestination = {};
    uint32_t networkDestinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", localPort, networkDestination, networkDestinationLen) == 0);

    TestableNetwork network("127.0.0.1", masterPort, localPort, 7779U, "loopback-password", true, false, false, true, false, false, true, true, true, true, false, false);
    network.enable(true);
    REQUIRE(network.open());

    completeHandshake(network, masterReceiverQueue, masterSenderQueue, networkDestination, networkDestinationLen, 7779U);
    network.resetP25P2(2U);

    std::vector<uint8_t> payload(P25_P2_PACKET_LENGTH + PACKET_PAD, 0x00U);
    ::memcpy(payload.data(), TAG_P25_DATA, 4U);
    payload[4U] = 0x30U;       // MAC PDU opcode
    payload[19U] = 0x80U;      // Slot 2
    payload[20U] = 0x31U;      // Scrambler offset
    payload[21U] = 0x32U;
    payload[23U] = 0x40U;      // Header plus 40-byte Phase 2 frame
    payload[24U] = 0x33U;      // Distinct CAI bytes for the round-trip check
    payload[57U] = 0x34U;
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, payload,
        { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2 }, 0x3456789AU, 7779U, 7779U, 0x3456U));

    network.clock(0U);

    bool ret = false;
    uint32_t frameLength = 0U;
    UInt8Array decoded = network.readP25P2(ret, frameLength);
    REQUIRE(ret);
    REQUIRE(frameLength == payload.size());
    REQUIRE(decoded != nullptr);
    REQUIRE(std::memcmp(decoded.get(), payload.data(), payload.size()) == 0);

    masterReceiver.close();
}

TEST_CASE("Network stores loopback analog protocol frames into the receive ring buffer", "[analog][network][loopback]")
{
    const uint16_t masterPort = reserveLoopbackPort();
    REQUIRE(masterPort != 0U);

    const uint16_t localPort = reserveLoopbackPort();
    REQUIRE(localPort != 0U);

    Socket masterReceiver("127.0.0.1", masterPort);
    REQUIRE(masterReceiver.open(AF_INET));

    FrameQueue masterReceiverQueue(&masterReceiver, 0x9ABFU, false);
    FrameQueue masterSenderQueue(&masterReceiver, 0xDEF3U, false);

    sockaddr_storage networkDestination = {};
    uint32_t networkDestinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", localPort, networkDestination, networkDestinationLen) == 0);

    TestableNetwork network("127.0.0.1", masterPort, localPort, 7780U, "loopback-password", true, false, false, false, false, true, true, true, true, true, false, false);
    network.enable(true);
    REQUIRE(network.open());

    completeHandshake(network, masterReceiverQueue, masterSenderQueue, networkDestination, networkDestinationLen, 7780U);
    network.resetAnalog();

    std::vector<uint8_t> payload(ANALOG_PACKET_LENGTH + PACKET_PAD, 0x00U);
    ::memcpy(payload.data(), TAG_ANALOG_DATA, 4U);
    payload[4U] = 0x40U;       // Analog sequence number
    payload[15U] = 0x01U;      // Audio frame type
    payload[20U] = 0x41U;      // Distinct PCM bytes for the round-trip check
    payload[21U] = 0x42U;
    payload[339U] = 0x43U;
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, payload,
        { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG }, 0x456789ABU, 7780U, 7780U, 0x4567U));

    network.clock(0U);

    bool ret = false;
    uint32_t frameLength = 0U;
    UInt8Array decoded = network.readAnalog(ret, frameLength);
    REQUIRE(ret);
    REQUIRE(frameLength == payload.size());
    REQUIRE(decoded != nullptr);
    REQUIRE(std::memcmp(decoded.get(), payload.data(), payload.size()) == 0);

    masterReceiver.close();
}

TEST_CASE("Network invokes loopback in-call callbacks for incoming control frames", "[network][loopback]")
{
    const uint16_t masterPort = reserveLoopbackPort();
    REQUIRE(masterPort != 0U);

    const uint16_t localPort = reserveLoopbackPort();
    REQUIRE(localPort != 0U);

    Socket masterReceiver("127.0.0.1", masterPort);
    REQUIRE(masterReceiver.open(AF_INET));

    FrameQueue masterReceiverQueue(&masterReceiver, 0xA1B2U, false);
    FrameQueue masterSenderQueue(&masterReceiver, 0xC3D4U, false);

    sockaddr_storage networkDestination = {};
    uint32_t networkDestinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", localPort, networkDestination, networkDestinationLen) == 0);

    TestableNetwork network("127.0.0.1", masterPort, localPort, 6000U, "loopback-password", true, false, true, false, false, false, true, true, true, true, false, false);
    network.enable(true);
    REQUIRE(network.open());

    completeHandshake(network, masterReceiverQueue, masterSenderQueue, networkDestination, networkDestinationLen, 6000U);

    bool callbackFired = false;
    NET_ICC::ENUM lastCommand = NET_ICC::NOP;
    uint32_t lastDstId = 0U;
    uint8_t lastSlot = 0U;
    uint32_t lastPeerId = 0U;
    uint32_t lastSsrc = 0U;
    uint32_t lastStreamId = 0U;

    network.setDMRICCCallback([&](NET_ICC::ENUM command, uint32_t dstId, uint8_t slotNo, uint32_t peerId, uint32_t ssrc, uint32_t streamId) {
        callbackFired = true;
        lastCommand = command;
        lastDstId = dstId;
        lastSlot = slotNo;
        lastPeerId = peerId;
        lastSsrc = ssrc;
        lastStreamId = streamId;
    });

    std::vector<uint8_t> payload(15U, 0x00U);
    payload[10U] = static_cast<uint8_t>(NET_ICC::REJECT_TRAFFIC);
    payload[11U] = 0x0AU;
    payload[12U] = 0x0BU;
    payload[13U] = 0x0CU;
    payload[14U] = 0x02U;
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, payload,
        { NET_FUNC::INCALL_CTRL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, 0x55AA55AAU, 6000U, 6000U, 0x0102U));

    network.clock(1000U);

    REQUIRE(callbackFired);
    REQUIRE(lastCommand == NET_ICC::REJECT_TRAFFIC);
    REQUIRE(lastDstId == 0x0A0B0CU);
    REQUIRE(lastSlot == 0x02U);
    REQUIRE(lastPeerId == 6000U);
    REQUIRE(lastSsrc == 6000U);
    REQUIRE(lastStreamId == 0x55AA55AAU);

    masterReceiver.close();
}

TEST_CASE("Network handles duplicate-connection NAKs by returning to waiting-connect", "[network][loopback]")
{
    const uint16_t masterPort = reserveLoopbackPort();
    REQUIRE(masterPort != 0U);

    const uint16_t localPort = reserveLoopbackPort();
    REQUIRE(localPort != 0U);

    Socket masterReceiver("127.0.0.1", masterPort);
    REQUIRE(masterReceiver.open(AF_INET));

    FrameQueue masterReceiverQueue(&masterReceiver, 0xB1B2U, false);
    FrameQueue masterSenderQueue(&masterReceiver, 0xD1D2U, false);

    sockaddr_storage networkDestination = {};
    uint32_t networkDestinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", localPort, networkDestination, networkDestinationLen) == 0);

    TestableNetwork network("127.0.0.1", masterPort, localPort, 9001U, "loopback-password", true, false, true, false, false, false, true, true, true, true, false, false);
    network.enable(true);
    REQUIRE(network.open());

    completeHandshake(network, masterReceiverQueue, masterSenderQueue, networkDestination, networkDestinationLen, 9001U);

    std::vector<uint8_t> nakPayload(12U, 0x00U);
    nakPayload[10U] = static_cast<uint8_t>((NET_CONN_NAK_FNE_DUPLICATE_CONN >> 8U) & 0xFFU);
    nakPayload[11U] = static_cast<uint8_t>(NET_CONN_NAK_FNE_DUPLICATE_CONN & 0xFFU);
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, nakPayload,
        { NET_FUNC::NAK, NET_SUBFUNC::NOP }, 0x12345678U, 9001U, 9001U, 0x0102U));

    network.clock(1000U);

    REQUIRE(network.getStatusForTest() == NET_STAT_WAITING_CONNECT);
    REQUIRE(network.hasDuplicateConnFlagForTest());
    REQUIRE(network.getMaxRetryCountForTest() == 2U);
    REQUIRE(network.getRemotePeerIdForTest() == 0U);

    masterReceiver.close();
}

TEST_CASE("Network enables or disables metadata-transfer flags from the config ack", "[network][loopback]")
{
    const uint16_t masterPort = reserveLoopbackPort();
    REQUIRE(masterPort != 0U);

    const uint16_t localPort = reserveLoopbackPort();
    REQUIRE(localPort != 0U);

    Socket masterReceiver("127.0.0.1", masterPort);
    REQUIRE(masterReceiver.open(AF_INET));

    FrameQueue masterReceiverQueue(&masterReceiver, 0xE1E2U, false);
    FrameQueue masterSenderQueue(&masterReceiver, 0xF1F2U, false);

    sockaddr_storage networkDestination = {};
    uint32_t networkDestinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", localPort, networkDestination, networkDestinationLen) == 0);

    TestableNetwork network("127.0.0.1", masterPort, localPort, 9002U, "loopback-password", true, false, true, false, false, false, true, true, true, true, false, false);
    network.enable(true);
    REQUIRE(network.open());

    network.setAllowActivityTransferForTest(false);
    network.setAllowDiagnosticTransferForTest(false);
    network.setStatusForTest(NET_STAT_WAITING_CONFIG);

    std::array<uint8_t, 8U> configAckPayload = { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U };
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, std::vector<uint8_t>(configAckPayload.begin(), configAckPayload.end()),
        { NET_FUNC::ACK, NET_SUBFUNC::NOP }, 0x12345678U, 9002U, 9002U, 0U));

    network.clock(1000U);

    REQUIRE(!network.getAllowActivityTransferForTest());
    REQUIRE(!network.getAllowDiagnosticTransferForTest());

    network.setAllowActivityTransferForTest(true);
    network.setAllowDiagnosticTransferForTest(true);

    network.setStatusForTest(NET_STAT_WAITING_CONFIG);
    configAckPayload[6U] = 0x80U;
    REQUIRE(sendLoopbackFrame(masterSenderQueue, networkDestination, networkDestinationLen, std::vector<uint8_t>(configAckPayload.begin(), configAckPayload.end()),
        { NET_FUNC::ACK, NET_SUBFUNC::NOP }, 0x12345678U, 9002U, 9002U, 0U));

    network.clock(1000U);

    REQUIRE(network.getAllowActivityTransferForTest());
    REQUIRE(network.getAllowDiagnosticTransferForTest());

    masterReceiver.close();
}

TEST_CASE("BaseNetwork emits announcement frames with the expected payloads while running", "[network][loopback]")
{
    const uint16_t masterPort = reserveLoopbackPort();
    REQUIRE(masterPort != 0U);

    const uint16_t localPort = reserveLoopbackPort();
    REQUIRE(localPort != 0U);

    Socket masterReceiver("127.0.0.1", masterPort);
    REQUIRE(masterReceiver.open(AF_INET));

    Socket metadataReceiver("127.0.0.1", masterPort + 1U);
    REQUIRE(metadataReceiver.open(AF_INET));

    FrameQueue masterReceiverQueue(&masterReceiver, 0x1111U, false);
    FrameQueue masterSenderQueue(&masterReceiver, 0x2222U, false);
    FrameQueue metadataReceiverQueue(&metadataReceiver, 0x3333U, false);

    sockaddr_storage networkDestination = {};
    uint32_t networkDestinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", localPort, networkDestination, networkDestinationLen) == 0);

    TestableNetwork network("127.0.0.1", masterPort, localPort, 9003U, "loopback-password", true, false, true, false, false, false, true, true, true, true, false, false);
    network.enable(true);
    REQUIRE(network.open());

    completeHandshake(network, masterReceiverQueue, masterSenderQueue, networkDestination, networkDestinationLen, 9003U);

    REQUIRE(network.announceGroupAffiliation(0x123456U, 0x654321U));
    std::string payload;
    RTPHeader rtpHeader;
    RTPFNEHeader fneHeader;
    REQUIRE(waitForFrameMessage(metadataReceiverQueue, payload, rtpHeader, fneHeader));
    REQUIRE(fneHeader.getFunction() == NET_FUNC::ANNOUNCE);
    REQUIRE(fneHeader.getSubFunction() == NET_SUBFUNC::ANNC_SUBFUNC_GRP_AFFIL);
    REQUIRE(payload.size() == MSG_ANNC_GRP_AFFIL);
    const uint32_t grpSrc = GET_UINT24(reinterpret_cast<const uint8_t*>(payload.data()), 0U);
    const uint32_t grpDst = GET_UINT24(reinterpret_cast<const uint8_t*>(payload.data()), 3U);
    REQUIRE(grpSrc == 0x123456U);
    REQUIRE(grpDst == 0x654321U);

    REQUIRE(network.announceUnitRegistration(0x112233U));
    REQUIRE(waitForFrameMessage(metadataReceiverQueue, payload, rtpHeader, fneHeader));
    REQUIRE(fneHeader.getFunction() == NET_FUNC::ANNOUNCE);
    REQUIRE(fneHeader.getSubFunction() == NET_SUBFUNC::ANNC_SUBFUNC_UNIT_REG);
    REQUIRE(payload.size() == MSG_ANNC_UNIT_REG);
    const uint32_t unitRegId = GET_UINT24(reinterpret_cast<const uint8_t*>(payload.data()), 0U);
    REQUIRE(unitRegId == 0x112233U);

    std::unordered_map<uint32_t, uint32_t> affs = { {0x111111U, 0x222222U}, {0x333333U, 0x444444U} };
    REQUIRE(network.announceAffiliationUpdate(affs));
    REQUIRE(waitForFrameMessage(metadataReceiverQueue, payload, rtpHeader, fneHeader));
    REQUIRE(fneHeader.getFunction() == NET_FUNC::ANNOUNCE);
    REQUIRE(fneHeader.getSubFunction() == NET_SUBFUNC::ANNC_SUBFUNC_AFFILS);
    REQUIRE(payload.size() == 4U + (affs.size() * 8U));

    std::vector<uint32_t> regs = { 0xAAAAAAU, 0xBBBBBBU };
    REQUIRE(network.announceUnitRegUpdate(regs));
    REQUIRE(waitForFrameMessage(metadataReceiverQueue, payload, rtpHeader, fneHeader));
    REQUIRE(fneHeader.getFunction() == NET_FUNC::ANNOUNCE);
    REQUIRE(fneHeader.getSubFunction() == NET_SUBFUNC::ANNC_SUBFUNC_UNIT_REGS);
    REQUIRE(payload.size() == 4U + (regs.size() * 3U));

    std::vector<uint32_t> peers = { 0x010203U, 0x040506U };
    REQUIRE(network.announceSiteVCs(peers));
    REQUIRE(waitForFrameMessage(metadataReceiverQueue, payload, rtpHeader, fneHeader));
    REQUIRE(fneHeader.getFunction() == NET_FUNC::ANNOUNCE);
    REQUIRE(fneHeader.getSubFunction() == NET_SUBFUNC::ANNC_SUBFUNC_SITE_VC);
    REQUIRE(payload.size() == 4U + (peers.size() * 4U));

    masterReceiver.close();
}

TEST_CASE("BaseNetwork emits transfer and status frames over the metadata path when enabled", "[network][loopback]")
{
    const uint16_t masterPort = reserveLoopbackPort();
    REQUIRE(masterPort != 0U);

    const uint16_t localPort = reserveLoopbackPort();
    REQUIRE(localPort != 0U);

    Socket masterReceiver("127.0.0.1", masterPort);
    REQUIRE(masterReceiver.open(AF_INET));

    Socket metadataReceiver("127.0.0.1", masterPort + 1U);
    REQUIRE(metadataReceiver.open(AF_INET));

    FrameQueue masterReceiverQueue(&masterReceiver, 0x3333U, false);
    FrameQueue masterSenderQueue(&masterReceiver, 0x4444U, false);
    FrameQueue metadataReceiverQueue(&metadataReceiver, 0x5555U, false);

    sockaddr_storage networkDestination = {};
    uint32_t networkDestinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", localPort, networkDestination, networkDestinationLen) == 0);

    TestableNetwork network("127.0.0.1", masterPort, localPort, 9004U, "loopback-password", true, false, true, false, false, false, true, true, true, true, false, false);
    network.enable(true);
    REQUIRE(network.open());

    completeHandshake(network, masterReceiverQueue, masterSenderQueue, networkDestination, networkDestinationLen, 9004U);

    REQUIRE(network.writeActLog("activity-test"));
    std::string payload;
    RTPHeader rtpHeader;
    RTPFNEHeader fneHeader;
    REQUIRE(waitForFrameMessage(metadataReceiverQueue, payload, rtpHeader, fneHeader));
    REQUIRE(fneHeader.getFunction() == NET_FUNC::TRANSFER);
    REQUIRE(fneHeader.getSubFunction() == NET_SUBFUNC::TRANSFER_SUBFUNC_ACTIVITY);
    REQUIRE(payload.find("activity-test") != std::string::npos);

    REQUIRE(network.writeDiagLog("diagnostic-test"));
    REQUIRE(waitForFrameMessage(metadataReceiverQueue, payload, rtpHeader, fneHeader));
    REQUIRE(fneHeader.getFunction() == NET_FUNC::TRANSFER);
    REQUIRE(fneHeader.getSubFunction() == NET_SUBFUNC::TRANSFER_SUBFUNC_DIAG);
    REQUIRE(payload.find("diagnostic-test") != std::string::npos);

    json::object statusObj;
    REQUIRE(network.writePeerStatus(statusObj));
    REQUIRE(waitForFrameMessage(metadataReceiverQueue, payload, rtpHeader, fneHeader));
    REQUIRE(fneHeader.getFunction() == NET_FUNC::TRANSFER);
    REQUIRE(fneHeader.getSubFunction() == NET_SUBFUNC::TRANSFER_SUBFUNC_STATUS);
    REQUIRE(payload.size() > 0U);

    masterReceiver.close();
}

TEST_CASE("RTPStreamMultiplex detects lost and out-of-order sequences and resets on end-of-call", "[network][loopback]")
{
    RTPStreamMultiplex mux;

    mux.setPktSeq(0x1234U, 1U);

    uint16_t lastSeq = 0U;
    REQUIRE(mux.verifyStream(0x1234U, 1U, NET_FUNC::PROTOCOL, &lastSeq) == MUX_VALID_SUCCESS);
    REQUIRE(lastSeq == 1U);

    REQUIRE(mux.verifyStream(0x1234U, 3U, NET_FUNC::PROTOCOL, &lastSeq) == MUX_LOST_FRAMES);
    REQUIRE(lastSeq == 1U);

    REQUIRE(mux.verifyStream(0x1234U, 2U, NET_FUNC::PROTOCOL, &lastSeq) == MUX_OUT_OF_ORDER);
    REQUIRE(lastSeq == 3U);

    REQUIRE(mux.verifyStream(0x1234U, RTP_END_OF_CALL_SEQ, NET_FUNC::PROTOCOL, &lastSeq) == MUX_VALID_SUCCESS);
    REQUIRE_FALSE(mux.hasPktSeq(0x1234U));
}
