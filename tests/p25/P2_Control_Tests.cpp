// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */

#include "host/p25/phase2/Control.h"
#include "host/p25/phase2/Slot.h"
#include "host/HostTestHooks.h"
#include "common/BitManipulation.h"
#include "common/edac/RS634717.h"
#include "common/p25/P25Defines.h"
#include "common/p25/lc/mac/MACFactory.h"
#include "host/modem/Modem.h"
#include "host/modem/port/IModemPort.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>
#include <vector>

using namespace p25::phase2;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------
namespace {

/**
 * @brief Creates an inbound MAC frame for testing purposes.
 * @param duid The DUID of the MAC frame.
 * @param opcode The MAC PDU opcode.
 * @param srcId The source ID for the MAC frame.
 * @param dstId The destination ID for the MAC frame.
 * @return An array representing the inbound MAC frame.
 */
std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> makeInboundMAC(
    P25DEF::P2_DUID::E duid, uint8_t opcode, uint32_t srcId = 0x123456U,
    uint32_t dstId = 0x2345U, uint16_t colorCode = 0U)
{
    p25::lc::LC control;
    control.setGroup(true);
    control.setLCO(P25DEF::P2_MAC_MCO::GROUP);
    control.setSrcId(srcId);
    control.setDstId(dstId);
    control.setColorCode(colorCode);
    control.setP2DUID(static_cast<uint8_t>(duid));
    control.setMACPDUOpcode(opcode);

    std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> frame{};
    frame[0U] = modem::TAG_DATA;
    frame[1U] = static_cast<uint8_t>(duid);
    control.encodeVCH_MACPDU_IEMI(frame.data() + 2U, duid == P25DEF::P2_DUID::FACCH_UNSCRAMBLED);
    return frame;
}

TEST_CASE("P25 Phase 2 ignores END_PTT for another call", "[p25][p2][end-ptt]")
{
    p25::phase2::Control control(true, 1U, 20U, 20U, nullptr, nullptr, nullptr, nullptr,
        nullptr, 4096U, false, false);
    p25::phase2::Slot& slot = HostTestHooks::p25P2Slot(control, 0U);

    auto ptt = makeInboundMAC(P25DEF::P2_DUID::FACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::PTT);
    REQUIRE(control.processFrame(0U, ptt.data(), ptt.size()));

    auto wrongColor = makeInboundMAC(P25DEF::P2_DUID::FACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::END_PTT, 0xFFFFFFU, 0x2345U, 1U);
    REQUIRE(control.processFrame(0U, wrongColor.data(), wrongColor.size()));
    REQUIRE(control.processFrame(0U, wrongColor.data(), wrongColor.size()));
    REQUIRE(HostTestHooks::p25P2RFVCHState(slot) == p25::phase2::Slot::VCH_STATE::PTT);

    auto wrongAddress = makeInboundMAC(P25DEF::P2_DUID::FACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::END_PTT, 0xFFFFFFU, 0x3456U);
    REQUIRE(control.processFrame(0U, wrongAddress.data(), wrongAddress.size()));
    REQUIRE(control.processFrame(0U, wrongAddress.data(), wrongAddress.size()));
    REQUIRE(HostTestHooks::p25P2RFVCHState(slot) == p25::phase2::Slot::VCH_STATE::PTT);

    auto matching = makeInboundMAC(P25DEF::P2_DUID::FACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::END_PTT, 0xFFFFFFU);
    REQUIRE(control.processFrame(0U, matching.data(), matching.size()));
    REQUIRE(control.processFrame(0U, matching.data(), matching.size()));
    REQUIRE(HostTestHooks::p25P2RFVCHState(slot) == p25::phase2::Slot::VCH_STATE::HANGTIME);
}

/**
 * @brief Creates a P25 Phase 2 voice burst frame for testing purposes.
 * @param duid The DUID of the voice burst.
 * @param ess The encoded speech segment data.
 * @param essBNo The block number within the ESS.
 * @return An array representing the voice burst frame.
 */
std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> makeVoiceBurst(
    P25DEF::P2_DUID::E duid, const std::array<uint8_t, 44U>& ess, uint32_t essBNo)
{
    std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> frame{};
    frame[0U] = modem::TAG_DATA;
    frame[1U] = static_cast<uint8_t>(duid);
    uint8_t* burst = frame.data() + 2U;

    if (duid == P25DEF::P2_DUID::VTCH_4V) {
        for (uint32_t bit = 0U; bit < 24U; ++bit)
            WRITE_BIT(burst, 148U + bit, READ_BIT(ess.data(), essBNo * 24U + bit));
    } else {
        for (uint32_t bit = 0U; bit < 168U; ++bit)
            WRITE_BIT(burst, 148U + bit, READ_BIT(ess.data(), 96U + bit));
    }

    return frame;
}

/**
 * @brief Decodes the MAC PDU opcode from a queued P25 Phase 2 frame.
 * @param frame The queued P25 Phase 2 frame.
 * @return The decoded MAC PDU opcode.
 */
uint8_t decodeQueuedMACOpcode(const std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES>& frame)
{
    p25::lc::LC decoded;
    const auto duid = static_cast<P25DEF::P2_DUID::E>(frame[1U]);
    const bool sync = duid == P25DEF::P2_DUID::FACCH_UNSCRAMBLED;
    REQUIRE(decoded.decodeVCH_MACPDU_OEMI(frame.data() + 2U, sync));
    return decoded.getMACPDUOpcode();
}

} // namespace

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief A mock implementation of the P25AffiliationLookup for testing grant behavior.
 */
class P2GrantLookup final : public p25::lookups::P25AffiliationLookup {
public:
    /**
     * @brief Initializes a new P2GrantLookup instance.
     */
    P2GrantLookup() : P25AffiliationLookup(nullptr, nullptr, false), grantedDstId(0U),
        touches(0U), releases(0U) { }

    /**
     * @brief Checks if a grant is currently active for the specified destination ID.
     * @param dstId The destination ID to check.
     * @return True if the grant is active, false otherwise.
     */
    bool isGranted(uint32_t dstId) const override { return dstId != 0U && dstId == grantedDstId; }
    /**
     * @brief Marks the grant for the specified destination ID as touched.
     * @param dstId The destination ID whose grant is being touched.
     */
    void touchGrant(uint32_t dstId) override
    {
        if (isGranted(dstId))
            ++touches;
    }
    /**
     * @brief Releases the grant for the specified destination ID.
     * @param dstId The destination ID whose grant is being released.
     * @param releaseAll Indicates whether all grants should be released.
     * @return True if the grant was successfully released, false otherwise.
     */
    bool releaseGrant(uint32_t dstId, bool releaseAll) override
    {
        (void)releaseAll;
        if (!isGranted(dstId))
            return false;
        ++releases;
        grantedDstId = 0U;
        return true;
    }

    uint32_t grantedDstId;
    uint32_t touches;
    uint32_t releases;
};

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief A loopback port implementation for testing P25 Phase 2 modems.
 */
class P2LoopbackPort final : public modem::port::IModemPort {
public:
    /**
     * @brief Opens the loopback port.
     * @return Always returns true.
     */
    bool open() override { return true; }

    /**
     * @brief Reads data from the loopback port.
     * @param buffer The buffer to store the read data.
     * @param length The maximum number of bytes to read.
     * @return Always returns 0, as no data is available.
     */
    int read(uint8_t* buffer, uint32_t length) override
    {
        (void)buffer;
        (void)length;
        return 0;
    }

    /**
     * @brief Writes data to the loopback port.
     * @param buffer The buffer containing the data to write.
     * @param length The number of bytes to write.
     * @return The number of bytes written.
     */
    int write(const uint8_t* buffer, uint32_t length) override
    {
        writes.emplace_back(buffer, buffer + length);
        return static_cast<int>(length);
    }

    /**
     * @brief Closes the loopback port.
     */
    void close() override { }

    std::vector<std::vector<uint8_t>> writes;
};

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief A loopback modem implementation for testing P25 Phase 2 modems.
 */
class P2LoopbackModem final : public modem::Modem {
public:
    /**
     * @brief Constructs a loopback modem with the specified loopback port.
     * @param port The loopback port to use with the modem.
     */
    explicit P2LoopbackModem(P2LoopbackPort* port) :
        modem::Modem(port, false, false, false, false, false, false, 0U, 0U, 0U,
            1024U, 4096U, 1024U, true, true, false, false, false, false)
    {
        setModeParams(false, true, false);
        m_p25P2Capable = true;
        m_p25P2Space1 = 4096U;
        m_p25P2Space2 = 4096U;
    }
};

TEST_CASE("P25 Phase 2 control isolates slot queues", "[p25][p2][control]")
{
    p25::phase2::Control control(true, 1U, 1U, 1U, nullptr, nullptr, nullptr, nullptr, nullptr, 4096U, false, false);
    std::array<uint8_t, P25DEF::P25_P2_FRAME_LENGTH_BYTES> slot0Frame{};
    std::array<uint8_t, P25DEF::P25_P2_FRAME_LENGTH_BYTES> slot1Frame{};
    slot0Frame[0U] = 0x10U;
    slot1Frame[0U] = 0x20U;

    REQUIRE(control.processFrame(0U, slot0Frame.data(), slot0Frame.size()));
    REQUIRE(control.processFrame(1U, slot1Frame.data(), slot1Frame.size()));
    REQUIRE(control.peekFrameLength(0U) == P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES);
    REQUIRE(control.peekFrameLength(1U) == P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES);

    std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> output{};
    REQUIRE(control.getFrame(0U, output.data()) == P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES);
    auto slot0Output = output;
    REQUIRE(slot0Output[0U] == modem::TAG_DATA);
    REQUIRE(slot0Output[1U] == P25DEF::P2_DUID::VTCH_4V);
    REQUIRE(control.getFrame(1U, output.data()) == P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES);
    auto slot1Output = output;
    REQUIRE(slot1Output[0U] == modem::TAG_DATA);
    REQUIRE(slot1Output[1U] == P25DEF::P2_DUID::VTCH_4V);
    REQUIRE(slot0Output.size() == slot1Output.size());
    REQUIRE(control.getFrame(2U, output.data()) == 0U);
}

TEST_CASE("P25 Phase 2 RF processing forwards to network and RF", "[p25][p2][repeater]")
{
    p25::phase2::Control control(true, 1000U, 1000U, 1000U, nullptr, nullptr, nullptr, nullptr, nullptr, 4096U, false, false);

    std::array<uint8_t, P25DEF::P25_P2_FRAME_LENGTH_BYTES> frame{};
    frame[0U] = 0x5AU;
    REQUIRE(control.processFrame(1U, frame.data(), frame.size()));
    REQUIRE(HostTestHooks::p25P2RFState(HostTestHooks::p25P2Slot(control, 1U)) == RS_RF_LATE_ENTRY);
    REQUIRE(control.peekFrameLength(1U) == P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES);
}

TEST_CASE("P25 Phase 2 rejects invalid frame lengths", "[p25][p2][validation]")
{
    p25::phase2::Control control(true, 1000U, 1000U, 1000U, nullptr, nullptr, nullptr, nullptr, nullptr, 4096U, false, false);
    std::array<uint8_t, P25DEF::P25_P2_FRAME_LENGTH_BYTES - 1U> frame{};

    REQUIRE_FALSE(control.processFrame(0U, frame.data(), frame.size()));
    REQUIRE_FALSE(control.processFrame(0U, frame.data(), frame.size()));
    REQUIRE(control.peekFrameLength(0U) == 0U);
    REQUIRE(control.isQueueFull(2U));
}

TEST_CASE("P25 Phase 2 slot returns to listening after RF timeout", "[p25][p2][lifecycle]")
{
    p25::phase2::Control control(true, 1U, 1U, 1U, nullptr, nullptr, nullptr, nullptr, nullptr, 4096U, false, false);
    std::array<uint8_t, P25DEF::P25_P2_FRAME_LENGTH_BYTES> frame{};

    REQUIRE(control.processFrame(0U, frame.data(), frame.size()));
    REQUIRE(HostTestHooks::p25P2RFState(HostTestHooks::p25P2Slot(control, 0U)) == RS_RF_LATE_ENTRY);

    control.clock(1001U);

    REQUIRE(HostTestHooks::p25P2RFState(HostTestHooks::p25P2Slot(control, 0U)) == RS_RF_LISTENING);
    REQUIRE(HostTestHooks::p25P2RFState(HostTestHooks::p25P2Slot(control, 0U)) == RS_RF_LISTENING);
}

TEST_CASE("P25 Phase 2 RF processing populates slot call state", "[p25][p2][rf][metadata]")
{
    p25::phase2::Control control(true, 1U, 1U, 1U, nullptr, nullptr, nullptr, nullptr, nullptr, 4096U, false, false);
    std::array<uint8_t, P25DEF::P25_P2_FRAME_LENGTH_BYTES> frame{};

    REQUIRE(control.processFrame(1U, frame.data(), frame.size()));
    REQUIRE(HostTestHooks::p25P2LastDstId(HostTestHooks::p25P2Slot(control, 1U)) == 0U);
    REQUIRE(HostTestHooks::p25P2LastSrcId(HostTestHooks::p25P2Slot(control, 1U)) == 0U);
    REQUIRE(HostTestHooks::p25P2RFState(HostTestHooks::p25P2Slot(control, 1U)) == RS_RF_LATE_ENTRY);
}

TEST_CASE("P25 Phase 2 tracks burst scrambler offsets", "[p25][p2][scrambler]")
{
    p25::phase2::Control control(true, 1000U, 1000U, 1000U, nullptr, nullptr, nullptr, nullptr, nullptr, 4096U, false, false);
    std::array<uint8_t, P25DEF::P25_P2_FRAME_LENGTH_BYTES> frame{};

    REQUIRE(control.processFrame(0U, frame.data(), frame.size()));
    REQUIRE(HostTestHooks::p25P2RFScrambleOffset(HostTestHooks::p25P2Slot(control, 0U)) == 0U);

    REQUIRE(control.processFrame(0U, frame.data(), frame.size()));
    REQUIRE(HostTestHooks::p25P2RFScrambleOffset(HostTestHooks::p25P2Slot(control, 0U)) == P25DEF::P25_P2_BURST_LENGTH_BITS * 2U);

    REQUIRE(control.processFrame(1U, frame.data(), frame.size()));
    REQUIRE(HostTestHooks::p25P2RFScrambleOffset(HostTestHooks::p25P2Slot(control, 1U)) == P25DEF::P25_P2_BURST_LENGTH_BITS);

    control.reset();
    REQUIRE(HostTestHooks::p25P2Slot(control, 0U).processNetwork(frame.data(), frame.size(), p25::lc::LC(),
        P25DEF::P2_DUID::VTCH_4V, P25DEF::P25_P2_BURST_LENGTH_BITS / 2U, 0U));
    REQUIRE(HostTestHooks::p25P2NetScrambleOffset(HostTestHooks::p25P2Slot(control, 0U)) == P25DEF::P25_P2_BURST_LENGTH_BITS / 2U);
}

TEST_CASE("P25 Phase 2 tagged modem frame decodes FACCH PTT before dispatch", "[p25][p2][vch]")
{
    p25::phase2::Control control(true, 2U, 10U, 2U, nullptr, nullptr, nullptr, nullptr, nullptr, 4096U, false, false);
    p25::lc::LC mac;
    mac.setP2DUID(P25DEF::P2_DUID::FACCH_UNSCRAMBLED);
    mac.setMACPDUOpcode(P25DEF::P2_MAC_HEADER_OPCODE::PTT);
    mac.setSrcId(0x123456U);
    mac.setDstId(0x2345U);

    std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> frame{};
    frame[0U] = modem::TAG_DATA;
    frame[1U] = P25DEF::P2_DUID::FACCH_UNSCRAMBLED;
    mac.encodeVCH_MACPDU_IEMI(frame.data() + 2U, true);

    REQUIRE(control.processFrame(0U, frame.data(), frame.size()));
    const p25::phase2::Slot& slot = HostTestHooks::p25P2Slot(control, 0U);
    REQUIRE(HostTestHooks::p25P2DUID(slot) == P25DEF::P2_DUID::FACCH_UNSCRAMBLED);
    REQUIRE(HostTestHooks::p25P2RFVCHState(slot) == p25::phase2::Slot::VCH_STATE::PTT);
    REQUIRE(HostTestHooks::p25P2RFPTTCount(slot) == 1U);

    std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> output{};
    bool imm = false;
    REQUIRE(control.getFrame(0U, output.data(), &imm) == output.size());
    REQUIRE(imm);
    REQUIRE(output[0U] == modem::TAG_DATA);
    REQUIRE(output[1U] == P25DEF::P2_DUID::FACCH_UNSCRAMBLED);
    p25::lc::LC decoded;
    REQUIRE(decoded.decodeVCH_MACPDU_OEMI(output.data() + 2U, true));
    REQUIRE(decoded.getMACPDUOpcode() == P25DEF::P2_MAC_HEADER_OPCODE::PTT);
}

TEST_CASE("P25 Phase 2 VCH processor rejects typed LCCH bursts", "[p25][p2][lcch]")
{
    p25::phase2::Control control(true, 2U, 10U, 2U, nullptr, nullptr, nullptr, nullptr, nullptr,
        4096U, false, false);
    std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> frame{};
    frame[0U] = modem::TAG_DATA;
    frame[1U] = P25DEF::P2_DUID::LCCH_UNSCRAMBLED;

    REQUIRE_FALSE(control.processFrame(0U, frame.data(), frame.size()));
    REQUIRE(control.peekFrameLength(0U) == 0U);
}

TEST_CASE("P25 Phase 2 lost modem frame terminates the active slot", "[p25][p2][loss]")
{
    p25::phase2::Control control(true, 0U, 10U, 2U, nullptr, nullptr, nullptr, nullptr, nullptr, 4096U, false, false);
    auto ptt = makeInboundMAC(P25DEF::P2_DUID::FACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::PTT);
    REQUIRE(control.processFrame(1U, ptt.data(), ptt.size()));
    std::array<uint8_t, P25DEF::P25_P2_FRAME_LENGTH_BYTES> voice{};
    REQUIRE(control.processFrame(1U, voice.data(), voice.size()));

    uint8_t lost = modem::TAG_LOST;
    REQUIRE_FALSE(control.processFrame(1U, &lost, 1U));
    control.clock(1501U);
    REQUIRE(HostTestHooks::p25P2RFVCHState(HostTestHooks::p25P2Slot(control, 1U)) == p25::phase2::Slot::VCH_STATE::TERMINATING);
}

TEST_CASE("P25 Phase 2 public state management covers late entry collision and reset",
    "[p25][p2][state]")
{
    p25::phase2::Control control(true, 0U, 20U, 20U, nullptr, nullptr, nullptr, nullptr,
        nullptr, 4096U, false, false);
    p25::phase2::Slot& slot = control.slot(0U);

    REQUIRE(control.getRFState(0U) == RS_RF_LISTENING);
    REQUIRE(control.getNetState(0U) == RS_NET_IDLE);
    REQUIRE(control.getRFState(2U) == RS_RF_INVALID);
    REQUIRE_FALSE(control.isBusy());

    std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> voice{};
    voice[0U] = modem::TAG_DATA;
    voice[1U] = P25DEF::P2_DUID::VTCH_4V;
    REQUIRE(control.processFrame(0U, voice.data(), voice.size()));
    REQUIRE(control.getRFState(0U) == RS_RF_LATE_ENTRY);
    REQUIRE(control.isBusy());

    auto active = makeInboundMAC(P25DEF::P2_DUID::SACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::ACTIVE);
    REQUIRE(control.processFrame(0U, active.data(), active.size()));
    REQUIRE(control.getRFState(0U) == RS_RF_AUDIO);
    REQUIRE(HostTestHooks::p25P2RFVCHState(slot) == p25::phase2::Slot::VCH_STATE::ACTIVE);

    control.reset();
    REQUIRE(control.getRFState(0U) == RS_RF_LISTENING);
    REQUIRE(control.getNetState(0U) == RS_NET_IDLE);
    REQUIRE_FALSE(control.isBusy());

    p25::lc::LC call;
    call.setSrcId(0x123456U);
    call.setDstId(0x2345U);
    std::array<uint8_t, P25DEF::P25_P2_FRAME_LENGTH_BYTES> networkVoice{};
    REQUIRE(slot.processNetwork(networkVoice.data(), networkVoice.size(), call,
        P25DEF::P2_DUID::VTCH_4V, 0U, 0U));
    REQUIRE(control.getNetState(0U) == RS_NET_AUDIO);

    auto ptt = makeInboundMAC(P25DEF::P2_DUID::FACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::PTT);
    REQUIRE_FALSE(control.processFrame(0U, ptt.data(), ptt.size()));
    REQUIRE(control.getRFState(0U) == RS_RF_LISTENING);
    REQUIRE(control.getNetState(0U) == RS_NET_AUDIO);

    control.clock(1501U);
    std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> queued{};
    while (control.getFrame(0U, queued.data()) != 0U) { }
    REQUIRE(control.getNetState(0U) == RS_NET_IDLE);
    REQUIRE(control.getRFState(0U) == RS_RF_LISTENING);
    REQUIRE_FALSE(control.isBusy());
}

TEST_CASE("P25 Phase 2 non-authoritative RF state requires a permitted destination",
    "[p25][p2][state][authoritative]")
{
    p25::phase2::Control control(false, 0U, 20U, 20U, nullptr, nullptr, nullptr, nullptr,
        nullptr, 4096U, false, false);
    auto ptt = makeInboundMAC(P25DEF::P2_DUID::FACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::PTT);

    REQUIRE_FALSE(control.processFrame(0U, ptt.data(), ptt.size()));
    REQUIRE(control.getRFState(0U) == RS_RF_LISTENING);
    REQUIRE_FALSE(control.isBusy());

    control.slot(0U).permittedTG(0x2345U);
    REQUIRE(control.processFrame(0U, ptt.data(), ptt.size()));
    REQUIRE(control.getRFState(0U) == RS_RF_AUDIO);
    REQUIRE(control.isBusy());
}

TEST_CASE("P25 Phase 2 rejected RF state is explicitly recoverable",
    "[p25][p2][state][rejection]")
{
    p25::phase2::Control control(true, 0U, 20U, 20U, nullptr, nullptr, nullptr, nullptr,
        nullptr, 4096U, false, false);
    p25::phase2::Slot& slot = control.slot(1U);

    HostTestHooks::p25P2SetRFState(slot, RS_RF_REJECTED);
    REQUIRE(control.getRFState(1U) == RS_RF_REJECTED);
    REQUIRE(control.isBusy());

    control.clearRFReject(1U);
    REQUIRE(control.getRFState(1U) == RS_RF_LISTENING);
    REQUIRE_FALSE(control.isBusy());
}

TEST_CASE("P25 Phase 2 network timeout is clocked once and honors authorization",
    "[p25][p2][state][network][timeout]")
{
    p25::phase2::Control control(false, 0U, 2U, 20U, nullptr, nullptr, nullptr, nullptr,
        nullptr, 4096U, false, false);
    p25::phase2::Slot& slot = control.slot(0U);
    p25::lc::LC call;
    call.setSrcId(0x123456U);
    call.setDstId(0x2345U);
    std::array<uint8_t, P25DEF::P25_P2_FRAME_LENGTH_BYTES> voice{};

    REQUIRE_FALSE(slot.processNetwork(voice.data(), voice.size(), call,
        P25DEF::P2_DUID::VTCH_4V, 0U, 0U));
    REQUIRE(control.getNetState(0U) == RS_NET_IDLE);

    slot.permittedTG(0x2345U);
    REQUIRE(slot.processNetwork(voice.data(), voice.size(), call,
        P25DEF::P2_DUID::VTCH_4V, 0U, 0U));
    REQUIRE(control.getNetState(0U) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::p25P2NetVCHState(slot) == p25::phase2::Slot::VCH_STATE::IDLE);

    control.clock(1001U);
    REQUIRE(control.getNetState(0U) == RS_NET_AUDIO);
    REQUIRE(HostTestHooks::p25P2NetVCHState(slot) != p25::phase2::Slot::VCH_STATE::TERMINATING);
}

TEST_CASE("P25 Phase 2 permits are slot specific", "[p25][p2][permit][authoritative]")
{
    p25::phase2::Control control(false, 0U, 20U, 20U, nullptr, nullptr, nullptr, nullptr,
        nullptr, 4096U, false, false);
    auto ptt = makeInboundMAC(P25DEF::P2_DUID::FACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::PTT);

    control.permittedTG(0x2345U, 0U);
    REQUIRE(control.processFrame(0U, ptt.data(), ptt.size()));
    REQUIRE_FALSE(control.processFrame(1U, ptt.data(), ptt.size()));
    REQUIRE(control.getRFState(0U) == RS_RF_AUDIO);
    REQUIRE(control.getRFState(1U) == RS_RF_LISTENING);
}

TEST_CASE("P25 Phase 2 accepted traffic touches and releases its CC grant",
    "[p25][p2][grant]")
{
    P2GrantLookup grants;
    grants.grantedDstId = 0x2345U;
    p25::phase2::Control control(true, 0U, 20U, 20U, nullptr, nullptr, &grants, nullptr,
        nullptr, 4096U, false, false);
    auto ptt = makeInboundMAC(P25DEF::P2_DUID::FACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::PTT);

    REQUIRE(control.processFrame(0U, ptt.data(), ptt.size()));
    REQUIRE(grants.touches == 1U);

    std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> voice{};
    voice[0U] = modem::TAG_DATA;
    voice[1U] = P25DEF::P2_DUID::VTCH_4V;
    REQUIRE(control.processFrame(0U, voice.data(), voice.size()));
    REQUIRE(grants.touches == 2U);

    control.touchGrantTG(0x2345U, 0U);
    REQUIRE(grants.touches == 3U);
    control.releaseGrantTG(0x2345U, 0U);
    REQUIRE(grants.releases == 1U);
    REQUIRE(grants.grantedDstId == 0U);
}

TEST_CASE("P25 Phase 2 RF call establishment enforces RID and TGID ACLs",
    "[p25][p2][acl]")
{
    ::lookups::RadioIdLookup ridLookup("", 0U, true, false);
    ::lookups::TalkgroupRulesLookup tidLookup("", 0U, true, false);
    ridLookup.addEntry(0x123456U, true, "allowed source");
    tidLookup.addEntry(0x2345U, 0U, true);

    p25::phase2::Control control(true, 0U, 20U, 20U, nullptr, nullptr, nullptr, &ridLookup,
        &tidLookup, 4096U, false, false);
    auto groupPTT = makeInboundMAC(P25DEF::P2_DUID::FACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::PTT);
    REQUIRE(control.processFrame(0U, groupPTT.data(), groupPTT.size()));
    REQUIRE(control.getRFState(0U) == RS_RF_AUDIO);

    control.reset();
    auto deniedSource = makeInboundMAC(P25DEF::P2_DUID::FACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::PTT, 0x654321U, 0x2345U);
    REQUIRE_FALSE(control.processFrame(0U, deniedSource.data(), deniedSource.size()));
    REQUIRE(control.getRFState(0U) == RS_RF_REJECTED);
    control.clearRFReject(0U);

    auto deniedTG = makeInboundMAC(P25DEF::P2_DUID::FACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::PTT, 0x123456U, 0x3456U);
    REQUIRE_FALSE(control.processFrame(0U, deniedTG.data(), deniedTG.size()));
    REQUIRE(control.getRFState(0U) == RS_RF_REJECTED);
    control.clearRFReject(0U);

    p25::lc::LC privateCall;
    privateCall.setGroup(false);
    privateCall.setMACPartition(P25DEF::P2_MAC_MCO_PARTITION::UNIQUE);
    privateCall.setLCO(P25DEF::P2_MAC_MCO::PRIVATE);
    privateCall.setSrcId(0x123456U);
    privateCall.setDstId(0x456789U);
    privateCall.setP2DUID(P25DEF::P2_DUID::SACCH_UNSCRAMBLED);
    privateCall.setMACPDUOpcode(P25DEF::P2_MAC_HEADER_OPCODE::ACTIVE);
    std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> privatePTT{};
    privatePTT[0U] = modem::TAG_DATA;
    privatePTT[1U] = P25DEF::P2_DUID::SACCH_UNSCRAMBLED;
    privateCall.encodeVCH_MACPDU_IEMI(privatePTT.data() + 2U, false);

    std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> lateVoice{};
    lateVoice[0U] = modem::TAG_DATA;
    lateVoice[1U] = P25DEF::P2_DUID::VTCH_4V;
    REQUIRE(control.processFrame(0U, lateVoice.data(), lateVoice.size()));
    REQUIRE_FALSE(control.processFrame(0U, privatePTT.data(), privatePTT.size()));
    REQUIRE(control.getRFState(0U) == RS_RF_REJECTED);
    control.clearRFReject(0U);

    ridLookup.addEntry(0x456789U, true, "allowed target");
    REQUIRE(control.processFrame(0U, lateVoice.data(), lateVoice.size()));
    REQUIRE(control.processFrame(0U, privatePTT.data(), privatePTT.size()));
    REQUIRE(control.getRFState(0U) == RS_RF_AUDIO);
}

TEST_CASE("P25 Phase 2 host modem logical loopback preserves both slots", "[p25][p2][e2e][modem]")
{
    p25::phase2::Control transmitter(true, 1000U, 1000U, 1000U, nullptr, nullptr, nullptr,
        nullptr, nullptr, 4096U, false, false);
    p25::phase2::Control receiver(true, 1000U, 1000U, 1000U, nullptr, nullptr, nullptr,
        nullptr, nullptr, 4096U, false, false);
    auto* port = new P2LoopbackPort();
    P2LoopbackModem airModem(port);

    for (uint32_t slotNo = 0U; slotNo < p25::phase2::Control::SLOT_COUNT; ++slotNo) {
        std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> input{};
        input[0U] = modem::TAG_DATA;
        input[1U] = P25DEF::P2_DUID::VTCH_4V;
        input[2U] = static_cast<uint8_t>(0x20U + slotNo);

        REQUIRE(transmitter.processFrame(slotNo, input.data(), input.size()));

        std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> hostFrame{};
        REQUIRE(transmitter.getFrame(slotNo, hostFrame.data()) == hostFrame.size());
        REQUIRE(hostFrame[0U] == modem::TAG_DATA);
        REQUIRE(hostFrame[1U] == P25DEF::P2_DUID::VTCH_4V);

        const bool written = slotNo == 0U ?
            airModem.writeP25P2Frame1(hostFrame.data(), hostFrame.size()) :
            airModem.writeP25P2Frame2(hostFrame.data(), hostFrame.size());
        REQUIRE(written);
        REQUIRE(port->writes.size() == slotNo + 1U);

        const std::vector<uint8_t>& serialFrame = port->writes.back();
        REQUIRE(serialFrame.size() == P25DEF::P25_P2_FRAME_LENGTH_BYTES + 4U);
        REQUIRE(serialFrame[0U] == modem::DVM_SHORT_FRAME_START);
        REQUIRE(serialFrame[1U] == serialFrame.size());
        REQUIRE(serialFrame[2U] == (slotNo == 0U ? modem::CMD_P25_P2_DATA1 : modem::CMD_P25_P2_DATA2));
        REQUIRE(serialFrame[3U] == P25DEF::P2_DUID::VTCH_4V);
        REQUIRE(std::memcmp(serialFrame.data() + 4U, hostFrame.data() + 2U,
            P25DEF::P25_P2_FRAME_LENGTH_BYTES) == 0);

        std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> received{};
        received[0U] = modem::TAG_DATA;
        std::memcpy(received.data() + 1U, serialFrame.data() + 3U, serialFrame.size() - 3U);
        REQUIRE(receiver.processFrame(slotNo, received.data(), received.size()));

        std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> repeated{};
        REQUIRE(receiver.getFrame(slotNo, repeated.data()) == repeated.size());
        REQUIRE(repeated == hostFrame);
        REQUIRE(receiver.peekFrameLength(1U - slotNo) == 0U);
    }
}

TEST_CASE("P25 Phase 2 deterministic full RF call lifecycle", "[p25][p2][e2e][call]")
{
    p25::phase2::Control control(true, 1U, 20U, 20U, nullptr, nullptr, nullptr, nullptr,
        nullptr, 8192U, false, false);
    p25::phase2::Slot& slot = HostTestHooks::p25P2Slot(control, 0U);
    std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> queued{};
    bool immediate = false;

    auto ptt = makeInboundMAC(P25DEF::P2_DUID::FACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::PTT);
    REQUIRE(control.processFrame(0U, ptt.data(), ptt.size()));
    REQUIRE(HostTestHooks::p25P2RFVCHState(slot) == p25::phase2::Slot::VCH_STATE::PTT);
    REQUIRE(control.getFrame(0U, queued.data(), &immediate) == queued.size());
    REQUIRE(immediate);
    REQUIRE(queued[1U] == P25DEF::P2_DUID::FACCH_UNSCRAMBLED);
    REQUIRE(decodeQueuedMACOpcode(queued) == P25DEF::P2_MAC_HEADER_OPCODE::PTT);

    std::array<uint8_t, 44U> ess{};
    ess[0U] = 0x80U;
    ess[1U] = 0x12U;
    ess[2U] = 0x34U;
    for (uint32_t i = 0U; i < 9U; ++i)
        ess[3U + i] = static_cast<uint8_t>(0xA0U + i);
    edac::RS634717().encode441629(ess.data());

    auto active = makeInboundMAC(P25DEF::P2_DUID::SACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::ACTIVE);
    p25::lc::LC decodedActive;
    REQUIRE(decodedActive.decodeVCH_MACPDU_IEMI(active.data() + 2U, false));
    REQUIRE(decodedActive.getP2DUID() == P25DEF::P2_DUID::SACCH_UNSCRAMBLED);
    REQUIRE(decodedActive.getMACPDUOpcode() == P25DEF::P2_MAC_HEADER_OPCODE::ACTIVE);
    REQUIRE(decodedActive.getLCO() == P25DEF::P2_MAC_MCO::GROUP);
    REQUIRE(p25::lc::mac::MACFactory::createMACPDU(decodedActive) != nullptr);
    for (uint32_t essBNo = 0U; essBNo < 4U; ++essBNo) {
        auto voice4V = makeVoiceBurst(P25DEF::P2_DUID::VTCH_4V, ess, essBNo);
        REQUIRE(control.processFrame(0U, voice4V.data(), voice4V.size()));
        REQUIRE(control.processFrame(0U, active.data(), active.size()));
        REQUIRE(HostTestHooks::p25P2RFVCHState(slot) == p25::phase2::Slot::VCH_STATE::ACTIVE);

        auto voice2V = makeVoiceBurst(P25DEF::P2_DUID::VTCH_2V, ess, essBNo);
        REQUIRE(control.processFrame(0U, voice2V.data(), voice2V.size()));
    }
    REQUIRE(HostTestHooks::p25P2ESSComplete(slot));
    REQUIRE(HostTestHooks::p25P2ESSAlgId(slot) == 0x80U);
    REQUIRE(HostTestHooks::p25P2ESSKeyId(slot) == 0x1234U);
    REQUIRE(HostTestHooks::p25P2RFFrames(slot) == 8U);
    REQUIRE(HostTestHooks::p25P2RFBits(slot) == 1729U);
    REQUIRE(HostTestHooks::p25P2RFErrs(slot) > 0U);
    REQUIRE(HostTestHooks::p25P2RFErrs(slot) <= HostTestHooks::p25P2RFBits(slot));

    auto endPTT = makeInboundMAC(P25DEF::P2_DUID::FACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::END_PTT);
    REQUIRE(control.processFrame(0U, endPTT.data(), endPTT.size()));
    REQUIRE(HostTestHooks::p25P2RFVCHState(slot) == p25::phase2::Slot::VCH_STATE::ACTIVE);
    REQUIRE(control.processFrame(0U, endPTT.data(), endPTT.size()));
    REQUIRE(HostTestHooks::p25P2RFVCHState(slot) == p25::phase2::Slot::VCH_STATE::HANGTIME);

    REQUIRE(control.getFrame(0U, queued.data(), &immediate) == queued.size());
    REQUIRE(immediate);
    REQUIRE(queued[1U] == P25DEF::P2_DUID::SACCH_UNSCRAMBLED);
    REQUIRE(decodeQueuedMACOpcode(queued) == P25DEF::P2_MAC_HEADER_OPCODE::HANGTIME);

    control.clock(360U);
    control.clock(641U);
    REQUIRE(HostTestHooks::p25P2RFVCHState(slot) == p25::phase2::Slot::VCH_STATE::TERMINATING);

    for (uint32_t i = 0U; i < 2U; ++i) {
        REQUIRE(control.getFrame(0U, queued.data(), &immediate) == queued.size());
        REQUIRE(immediate);
        REQUIRE(queued[1U] == P25DEF::P2_DUID::FACCH_UNSCRAMBLED);
        REQUIRE(decodeQueuedMACOpcode(queued) == P25DEF::P2_MAC_HEADER_OPCODE::END_PTT);
    }

    bool saw4V = false;
    bool saw2V = false;
    uint32_t sacchCount = 0U;
    while (control.getFrame(0U, queued.data(), &immediate) != 0U) {
        REQUIRE_FALSE(immediate);
        saw4V |= queued[1U] == P25DEF::P2_DUID::VTCH_4V;
        saw2V |= queued[1U] == P25DEF::P2_DUID::VTCH_2V;
        if (queued[1U] == P25DEF::P2_DUID::SACCH_UNSCRAMBLED)
            ++sacchCount;
    }
    REQUIRE(saw4V);
    REQUIRE(saw2V);
    // Four inbound ACTIVE SACCH bursts are repeated, and the 1,001 ms
    // hangtime interval inserts two additional 360 ms cadence SACCH bursts.
    REQUIRE(sacchCount == 6U);
    REQUIRE(HostTestHooks::p25P2RFVCHState(slot) == p25::phase2::Slot::VCH_STATE::IDLE);
    REQUIRE(HostTestHooks::p25P2RFState(slot) == RS_RF_LISTENING);
    REQUIRE(HostTestHooks::p25P2RFFrames(slot) == 0U);
    REQUIRE(HostTestHooks::p25P2RFBits(slot) == 1U);
    REQUIRE(HostTestHooks::p25P2RFErrs(slot) == 0U);
}

TEST_CASE("P25 Phase 2 slot accounts network frames loss misses and BER",
    "[p25][p2][statistics]")
{
    p25::phase2::Control control(true, 1U, 20U, 20U, nullptr, nullptr, nullptr, nullptr,
        nullptr, 4096U, false, false);
    p25::phase2::Slot& slot = HostTestHooks::p25P2Slot(control, 0U);
    p25::lc::LC call;
    call.setSrcId(0x123456U);
    call.setDstId(0x2345U);

    std::array<uint8_t, P25DEF::P25_P2_FRAME_LENGTH_BYTES> voice4V{};
    voice4V[0U] = 0x20U; // one error in the first outbound AMBE+2 codeword
    REQUIRE(slot.processNetwork(voice4V.data(), voice4V.size(), call,
        P25DEF::P2_DUID::VTCH_4V, 0U, 0U));
    REQUIRE(HostTestHooks::p25P2NetFrames(slot) == 1U);
    REQUIRE(HostTestHooks::p25P2NetBits(slot) == 289U);
    REQUIRE(HostTestHooks::p25P2NetErrs(slot) > 0U);

    std::array<uint8_t, P25DEF::P25_P2_FRAME_LENGTH_BYTES> voice2V{};
    REQUIRE(slot.processNetwork(voice2V.data(), voice2V.size(), call,
        P25DEF::P2_DUID::VTCH_2V, 0U, 0U));
    REQUIRE(HostTestHooks::p25P2NetFrames(slot) == 2U);
    REQUIRE(HostTestHooks::p25P2NetBits(slot) == 433U);

    p25::lc::LC competing(call);
    competing.setDstId(0x3456U);
    REQUIRE_FALSE(slot.processNetwork(voice4V.data(), voice4V.size(), competing,
        P25DEF::P2_DUID::VTCH_4V, 0U, 0U));
    REQUIRE(HostTestHooks::p25P2NetMissed(slot) == 1U);

    control.clock(1501U);
    REQUIRE(HostTestHooks::p25P2NetLost(slot) == 1U);

    slot.reset();
    REQUIRE(HostTestHooks::p25P2NetFrames(slot) == 0U);
    REQUIRE(HostTestHooks::p25P2NetLost(slot) == 0U);
    REQUIRE(HostTestHooks::p25P2NetMissed(slot) == 0U);
    REQUIRE(HostTestHooks::p25P2NetBits(slot) == 1U);
    REQUIRE(HostTestHooks::p25P2NetErrs(slot) == 0U);
}

TEST_CASE("P25 Phase 2 deterministic call recovers from missing termination",
    "[p25][p2][e2e][call][loss]")
{
    p25::phase2::Control control(true, 1U, 20U, 20U, nullptr, nullptr, nullptr, nullptr,
        nullptr, 4096U, false, false);
    p25::phase2::Slot& slot = HostTestHooks::p25P2Slot(control, 1U);
    auto ptt = makeInboundMAC(P25DEF::P2_DUID::FACCH_UNSCRAMBLED,
        P25DEF::P2_MAC_HEADER_OPCODE::PTT);
    REQUIRE(control.processFrame(1U, ptt.data(), ptt.size()));

    std::array<uint8_t, P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES> queued{};
    bool immediate = false;
    REQUIRE(control.getFrame(1U, queued.data(), &immediate) == queued.size());
    REQUIRE(immediate);
    REQUIRE(decodeQueuedMACOpcode(queued) == P25DEF::P2_MAC_HEADER_OPCODE::PTT);

    std::array<uint8_t, 44U> emptyESS{};
    auto voice = makeVoiceBurst(P25DEF::P2_DUID::VTCH_4V, emptyESS, 0U);
    REQUIRE(control.processFrame(1U, voice.data(), voice.size()));

    uint8_t lost = modem::TAG_LOST;
    REQUIRE_FALSE(control.processFrame(1U, &lost, 1U));
    control.clock(1501U);
    REQUIRE(HostTestHooks::p25P2RFVCHState(slot) == p25::phase2::Slot::VCH_STATE::TERMINATING);

    for (uint32_t i = 0U; i < 2U; ++i) {
        REQUIRE(control.getFrame(1U, queued.data(), &immediate) == queued.size());
        REQUIRE(immediate);
        REQUIRE(decodeQueuedMACOpcode(queued) == P25DEF::P2_MAC_HEADER_OPCODE::END_PTT);
    }
    while (control.getFrame(1U, queued.data(), &immediate) != 0U) { }
    REQUIRE(HostTestHooks::p25P2RFVCHState(slot) == p25::phase2::Slot::VCH_STATE::IDLE);
    REQUIRE(HostTestHooks::p25P2RFState(slot) == RS_RF_LISTENING);
}
