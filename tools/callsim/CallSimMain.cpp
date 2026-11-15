// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - FNE Call Simulator
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup callsim FNE Call Simulator
 * @brief Command-line generation of deterministic DMR, P25, and NXDN calls.
 * 
 * @file CallSim.cpp
 * @brief Implements a command-line FNE call simulator for DMR, P25 Phase 1, and NXDN.
 * @ingroup callsim
 *
 * The simulator authenticates as a normal FNE peer and emits group calls at
 * each protocol's native network-frame cadence. Calls are sequential by
 * default; simultaneous mode multiplexes one independent RTP stream per
 * configured talkgroup over the same authenticated connection.
 */
#include "Defines.h"
#include "CallSimNetwork.h"
#include "common/Log.h"
#include "common/Thread.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/data/NetData.h"
#include "common/network/Network.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/NXDNUtils.h"
#include "common/nxdn/Sync.h"
#include "common/nxdn/channel/LICH.h"
#include "common/nxdn/channel/SACCH.h"
#include "common/nxdn/lc/RTCH.h"
#include "common/p25/Audio.h"
#include "common/p25/P25Defines.h"
#include "common/p25/data/LowSpeedData.h"
#include "common/p25/lc/LC.h"
#include "modem/Modem.h"

#undef __PROG_NAME__
#define __PROG_NAME__ "Digital Voice Modem (DVM) FNE Call Simulator"
#undef __EXE_NAME__
#define __EXE_NAME__ "callsim"

using namespace network;

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

static constexpr uint32_t DEFAULT_CALL_COUNT = 1U;             //!< Default number of calls to emit.
static constexpr uint32_t DEFAULT_CALL_DURATION_SECONDS = 5U;  //!< Default call duration in seconds.
static constexpr uint32_t DEFAULT_CALL_GAP_MS = 500U;          //!< Default inter-call delay in milliseconds.
static constexpr uint32_t DEFAULT_CONNECT_TIMEOUT_MS = 10000U; //!< FNE login timeout in milliseconds.
static constexpr uint32_t MILLISECONDS_PER_SECOND = 1000U;     //!< Milliseconds in one second.
static constexpr uint32_t P25_LDU_PERIOD_MS = 180U;            //!< P25 Phase 1 LDU interval in milliseconds.
static constexpr uint32_t DMR_BURST_PERIOD_MS = 60U;           //!< DMR voice burst interval in milliseconds.
static constexpr uint32_t NXDN_FRAME_PERIOD_MS = 80U;          //!< NXDN 4800 voice-frame interval in milliseconds.

/** @brief Alternating raw IMBE codewords decoded from the P25 1011 Hz calibration vector. */
static const uint8_t P25_TONE_IMBE[2U][P25DEF::RAW_IMBE_LENGTH_BYTES] = {
    { 0x09U, 0xB0U, 0x88U, 0x0CU, 0xC6U, 0x21U, 0xF6U, 0x80U, 0xA8U, 0x26U, 0x00U },
    { 0x09U, 0xB0U, 0x88U, 0x0CU, 0xC6U, 0x21U, 0xF6U, 0x80U, 0xA8U, 0x26U, 0x01U }
};

/** @brief Fixed AMBE payload taken from the DMR 1031 Hz calibration vector. */
static const uint8_t DMR_TONE_BURST[DMRDEF::DMR_FRAME_LENGTH_BYTES] = {
    0xCEU, 0xA8U, 0xFEU, 0x83U, 0xACU, 0xC4U, 0x58U, 0x20U, 0x0AU, 0xCEU, 0xA8U,
    0xFEU, 0x83U, 0xA0U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x0CU, 0xC4U, 0x58U,
    0x20U, 0x0AU, 0xCEU, 0xA8U, 0xFEU, 0x83U, 0xACU, 0xC4U, 0x58U, 0x20U, 0x0AU
};

/** @brief Four encoded AMBE codewords from the NXDN 1031 Hz calibration vector. */
static const uint8_t NXDN_TONE_AUDIO[36U] = {
    0x4CU, 0xAAU, 0xDEU, 0x8BU, 0x26U, 0xE4U, 0xF2U, 0x82U, 0x88U,
    0xC6U, 0x8AU, 0x74U, 0x29U, 0xA4U, 0xECU, 0xD0U, 0x08U, 0x22U,
    0xCEU, 0xA2U, 0xFCU, 0x01U, 0x8CU, 0xECU, 0xDAU, 0x0AU, 0xA0U,
    0xEEU, 0x8AU, 0x7EU, 0x2BU, 0x26U, 0xCCU, 0xF8U, 0x8AU, 0x08U
};

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

std::string g_progExe = std::string(__EXE_NAME__);
static std::atomic<bool> g_stop(false); //!< Set by the signal handler to request an orderly shutdown.

// ---------------------------------------------------------------------------
//  Enumeration Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Identifies the air-interface protocol simulated for each call.
 * @ingroup callsim
 */
enum class Mode {
    DMR, //!< Digital Mobile Radio Tier II/III voice.
    P25, //!< APCO Project 25 Phase 1 FDMA voice.
    NXDN //!< NXDN 4800 baud FDMA voice.
};

// ---------------------------------------------------------------------------
//  Structure Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Holds validated command-line configuration for one simulator run.
 * @ingroup callsim
 */
struct Options {
    std::string address = "127.0.0.1";                        //!< FNE hostname or IP address.
    uint16_t port = TRAFFIC_DEFAULT_PORT;                     //!< FNE UDP traffic port.
    uint32_t peerId = 9000998U;                               //!< Peer ID presented during FNE authentication.
    std::string password = "passw0rd";                        //!< Password used for FNE authentication.
    std::string identity = "CallSim";                         //!< Descriptive identity included in peer metadata.

    Mode mode = Mode::P25;                                    //!< Protocol used for generated calls.

    uint32_t callCount = DEFAULT_CALL_COUNT;                  //!< Calls, or simultaneous TG-batch repetitions, to emit.
    uint32_t durationSeconds = DEFAULT_CALL_DURATION_SECONDS; //!< Duration of each call in seconds.
    uint32_t gapMs = DEFAULT_CALL_GAP_MS;                     //!< Delay between completed calls in milliseconds.
    uint32_t srcId = 100001U;                                 //!< Source ID used by the first call; incremented per call.
    uint32_t p25WACN = P25DEF::WACN_STD_DEFAULT;               //!< P25 WACN encoded in generated calls.
    uint32_t p25SysId = P25DEF::SID_STD_DEFAULT;               //!< P25 system ID encoded in generated calls.

    std::vector<uint32_t> talkgroups;                         //!< Destination IDs selected round-robin by call index.
    std::vector<uint8_t> slots;                               //!< DMR slots selected round-robin by call index.

    bool grantDemand = false;                                 //!< Sends an FNE grant request before each call when true.

    bool simultaneous = false;                                //!< Emits all configured talkgroups concurrently when true.

    bool debug = false;                                       //!< Enables verbose network logging when true.
};

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/**
 * @brief Signal handler for orderly simulator shutdown.
 * @param signal The signal number received (ignored).
 */
static void signalHandler(int)
{
    g_stop = true;
}

/**
 * @brief Prints version information for the program.
 */
static void version()
{
    ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
    ::fprintf(stdout, "Copyright (c) 2017-2026 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\n");
    ::fprintf(stdout, "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\n");
    ::fprintf(stdout, HIGHLY_UNNECESSARY_DISCLAIMER_FOR_THE_MENTAL "\n\n");
}

/**
 * @brief Prints command-line usage, optionally preceded by an argument error.
 * @param message An optional error message to display before the usage.
 * @param arg An optional argument to format into the error message.
 */
static void usage(const char* message, const char* arg)
{
    version();
    if (message != nullptr) {
        ::fprintf(stderr, "%s: ", g_progExe.c_str());
        ::fprintf(stderr, message, arg);
        ::fprintf(stderr, "\n\n");
    }

    ::fprintf(stdout,
        "usage: %s --mode <p25|dmr|nxdn> --talkgroups <tg[,tg...]> [options]\n"
        "\n"
        "Connection:\n"
        "  --address <host>       FNE address (default 127.0.0.1)\n"
        "  --port <port>          FNE traffic port (default %u)\n"
        "  --peer-id <id>         Simulated peer ID (default 9000998)\n"
        "  --password <password>  FNE peer password\n"
        "  --identity <name>      Peer identity sent to the FNE\n"
        "\n"
        "Calls:\n"
        "  --mode <mode>          p25, dmr, or nxdn\n"
        "  --calls <count>        Calls, or simultaneous batch repetitions (default 1)\n"
        "  --duration <seconds>   Call duration in seconds (default 5)\n"
        "  --gap <ms>             Delay between calls (default 500)\n"
        "  --source <rid>         First source radio ID (default 100001)\n"
        "  --talkgroups <list>    Comma-separated destination IDs\n"
        "  --slots <list>         DMR slots, one per TG or cycled (default 1)\n"
        "  --wacn <hex>           P25 WACN (default BB800)\n"
        "  --sys-id <hex>         P25 system ID (default 001)\n"
        "  --grant-demand         Send an FNE grant request before each call\n"
        "  --simultaneous         Emit all configured TGs concurrently per repetition\n"
        "  --debug                Enable network debug logging\n"
        "  --help                 Show this help\n",
        g_progExe.c_str(), TRAFFIC_DEFAULT_PORT);
}

/**
 * @brief Parses and range-checks an unsigned decimal command-line value.
 * @param text The string containing the value to parse.
 * @param minimum The minimum allowed value.
 * @param maximum The maximum allowed value.
 * @param value The variable to store the parsed value.
 * @return true if the value was successfully parsed and within range, false otherwise.
 */
static bool parseUnsigned(const char* text, uint32_t minimum, uint32_t maximum, uint32_t& value)
{
    if (text == nullptr || text[0U] == '\0')
        return false;

    errno = 0;
    char* end = nullptr;
    unsigned long parsed = ::strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < minimum || parsed > maximum)
        return false;

    value = static_cast<uint32_t>(parsed);
    return true;
}

/**
 * @brief Parses and range-checks an unsigned hexadecimal command-line value.
 * @param text The string containing the hexadecimal value to parse.
 * @param minimum The minimum allowed value.
 * @param maximum The maximum allowed value.
 * @param value The variable to store the parsed value.
 * @return true if the value was successfully parsed and within range, false otherwise.
 */
static bool parseHexUnsigned(const char* text, uint32_t minimum, uint32_t maximum, uint32_t& value)
{
    if (text == nullptr || text[0U] == '\0')
        return false;

    errno = 0;
    char* end = nullptr;
    unsigned long parsed = ::strtoul(text, &end, 16);
    if (errno != 0 || end == text || *end != '\0' || parsed < minimum || parsed > maximum)
        return false;

    value = static_cast<uint32_t>(parsed);
    return true;
}

/**
 * @brief Parses a comma-separated list of range-checked unsigned decimal values.
 * @param text The comma-separated list of values as a string.
 * @param minimum The minimum allowed value for each item.
 * @param maximum The maximum allowed value for each item.
 * @param values The vector to populate with parsed values.
 * @return true if all values were successfully parsed and within range, false otherwise.
 */
static bool parseList(const std::string& text, uint32_t minimum, uint32_t maximum, std::vector<uint32_t>& values)
{
    // parse a comma-separated list of unsigned decimal values within the specified range
    size_t start = 0U;
    while (start <= text.length()) {
        size_t end = text.find(',', start);
        std::string item = text.substr(start, end == std::string::npos ? std::string::npos : end - start);

        uint32_t value = 0U;
        if (!parseUnsigned(item.c_str(), minimum, maximum, value))
            return false;

        values.push_back(value);
        if (end == std::string::npos)
            break;

        start = end + 1U;
    }
    return !values.empty();
}

/**
 * @brief Parses and validates all command-line options.
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line argument strings.
 * @param options The structure to populate with parsed options.
 * @return true if all options were successfully parsed and validated, false otherwise.
 */
static bool parseOptions(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; i++) {
        const std::string arg(argv[i]);
        auto requireValue = [&](const char* name) -> const char* {
            if (++i >= argc) {
                std::string message = std::string(name) + " requires a value";
                usage("error: %s", message.c_str());
                return nullptr;
            }
            return argv[i];
        };

        if (arg == "--help" || arg == "-h") {
            usage(nullptr, nullptr);
            return false;
        } else if (arg == "--debug") {
            options.debug = true;
        } else if (arg == "--grant-demand") {
            options.grantDemand = true;
        } else if (arg == "--simultaneous") {
            options.simultaneous = true;
        } else if (arg == "--address") {
            const char* value = requireValue("--address");
            if (value == nullptr)
                return false;
            options.address = value;
        } else if (arg == "--password") {
            const char* value = requireValue("--password");
            if (value == nullptr)
                return false;
            options.password = value;
        } else if (arg == "--identity") {
            const char* value = requireValue("--identity");
            if (value == nullptr)
                return false;
            options.identity = value;
        } else if (arg == "--mode") {
            const char* value = requireValue("--mode");
            if (value == nullptr)
                return false;
            if (::strcmp(value, "p25") == 0)
                options.mode = Mode::P25;
            else if (::strcmp(value, "dmr") == 0)
                options.mode = Mode::DMR;
            else if (::strcmp(value, "nxdn") == 0)
                options.mode = Mode::NXDN;
            else {
                usage("error: %s", "--mode must be p25, dmr, or nxdn");
                return false;
            }
        } else if (arg == "--talkgroups") {
            const char* value = requireValue("--talkgroups");
            if (value == nullptr || !parseList(value, 1U, 0xFFFFFFU, options.talkgroups)) {
                usage("error: %s", "invalid --talkgroups list");
                return false;
            }
        } else if (arg == "--slots") {
            const char* value = requireValue("--slots");
            std::vector<uint32_t> slots;
            if (value == nullptr || !parseList(value, 1U, 2U, slots)) {
                usage("error: %s", "invalid --slots list");
                return false;
            }
            for (uint32_t slot : slots)
                options.slots.push_back(static_cast<uint8_t>(slot));
        } else {
            const char* value = nullptr;
            uint32_t parsed = 0U;
            if (arg == "--port") {
                value = requireValue("--port");
                if (value == nullptr || !parseUnsigned(value, 1U, 65535U, parsed)) {
                    usage("error: %s", "invalid --port");
                    return false;
                }
                options.port = static_cast<uint16_t>(parsed);
            } else if (arg == "--peer-id") {
                value = requireValue("--peer-id");
                if (value == nullptr || !parseUnsigned(value, 1U, 999999999U, options.peerId)) {
                    usage("error: %s", "invalid --peer-id");
                    return false;
                }
            } else if (arg == "--calls") {
                value = requireValue("--calls");
                if (value == nullptr || !parseUnsigned(value, 1U, UINT32_MAX, options.callCount)) {
                    usage("error: %s", "invalid --calls");
                    return false;
                }
            } else if (arg == "--duration") {
                value = requireValue("--duration");
                if (value == nullptr || !parseUnsigned(value, 1U, UINT32_MAX / MILLISECONDS_PER_SECOND, options.durationSeconds)) {
                    usage("error: %s", "invalid --duration");
                    return false;
                }
            } else if (arg == "--gap") {
                value = requireValue("--gap");
                if (value == nullptr || !parseUnsigned(value, 0U, UINT32_MAX, options.gapMs)) {
                    usage("error: %s", "invalid --gap");
                    return false;
                }
            } else if (arg == "--source") {
                value = requireValue("--source");
                if (value == nullptr || !parseUnsigned(value, 1U, 0xFFFFFFU, options.srcId)) {
                    usage("error: %s", "invalid --source");
                    return false;
                }
            } else if (arg == "--wacn") {
                value = requireValue("--wacn");
                if (value == nullptr || !parseHexUnsigned(value, 1U, 0xFFFFEU, options.p25WACN)) {
                    usage("error: %s", "invalid --wacn");
                    return false;
                }
                if (options.p25WACN >= 0xBEE00U && options.p25WACN <= 0xBEEFFU) {
                    usage("error: %s", "--wacn must not be in the reserved BEE00-BEEFF range");
                    return false;
                }
            } else if (arg == "--sys-id") {
                value = requireValue("--sys-id");
                if (value == nullptr || !parseHexUnsigned(value, 1U, 0xFFEU, options.p25SysId)) {
                    usage("error: %s", "invalid --sys-id");
                    return false;
                }
            } else {
                usage("error: %s", (std::string("unknown option: ") + arg).c_str());
                return false;
            }
        }
    }

    if (options.talkgroups.empty()) {
        usage("error: %s", "--talkgroups is required");
        return false;
    }

    if (options.mode == Mode::NXDN && (*std::max_element(options.talkgroups.begin(), options.talkgroups.end()) > 65535U || options.srcId > 65535U)) {
        usage("error: %s", "NXDN source and destination IDs must not exceed 65535");
        return false;
    }

    if (options.slots.empty())
        options.slots.push_back(1U);

    return true;
}

/**
 * @brief Clocks FNE timers and transport processing while maintaining a protocol delay.
 * @param network The network interface to use for clocking.
 * @param durationMs The duration to wait and clock in milliseconds.
 */
static void waitAndClock(CallSimNetwork& network, uint32_t durationMs)
{
    auto start = std::chrono::steady_clock::now();
    auto last = start;

    while (!g_stop) {
        auto now = std::chrono::steady_clock::now();
        uint32_t elapsed = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
        uint32_t delta = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count());

        if (elapsed >= durationMs)
            break;
        if (delta > 0U) {
            network.clockConnection(delta);
            last = now;
        }

        Thread::sleep(1U);
    }
}

/**
 * @brief Waits between protocol frames while the owning scheduler clocks the connection.
 * @param durationMs The duration to wait in milliseconds.
 */
static void waitForFrame(uint32_t durationMs)
{
    const auto start = std::chrono::steady_clock::now();
    while (!g_stop) {
        const auto now = std::chrono::steady_clock::now();
        const uint32_t elapsed = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
        if (elapsed >= durationMs)
            break;

        Thread::sleep(1U);
    }
}

/**
 * @brief Sends an optional group-call grant request using the selected protocol and slot.
 * @param network The network interface to use for sending the request.
 * @param mode The protocol mode (DMR, P25, NXDN).
 * @param srcId The source ID of the call.
 * @param dstId The destination ID of the call.
 * @param slot The slot number for the call (relevant for DMR mode).
 * @return true if the grant request was successfully sent, false otherwise.
 */
static bool writeGrantDemand(CallSimNetwork& network, Mode mode, uint32_t srcId, uint32_t dstId, uint8_t slot)
{
    uint8_t dvmState = modem::DVM_STATE::STATE_IDLE;
    switch (mode) {
    case Mode::DMR:
        dvmState = modem::DVM_STATE::STATE_DMR;
        break;
    case Mode::P25:
        dvmState = modem::DVM_STATE::STATE_P25;
        slot = 0U;
        break;
    case Mode::NXDN:
        dvmState = modem::DVM_STATE::STATE_NXDN;
        slot = 0U;
        break;
    }

    // calls emitted by this simulator are group calls with default service
    // options; only DMR carries a meaningful TDMA slot in this implementation
    return network.writeGroupGrantRequest(dvmState, srcId, dstId, slot);
}

/**
 * @brief Encodes the alternating P25 tone codewords into one interleaved LDU.
 * @param data Pointer to the raw P25 LDU data buffer to be filled.
 * @param phase Index of the first tone codeword in the alternating sequence.
 */
static void fillP25Tone(uint8_t* data, uint32_t phase)
{
    ::memset(data, 0x00U, P25DEF::P25_LDU_FRAME_LENGTH_BYTES);

    p25::Audio audio;
    for (uint32_t n = 0U; n < 9U; n++)
        audio.encode(data, P25_TONE_IMBE[(phase + n) % 2U], n);
}

/**
 * @brief Emits alternating P25 LDU1/LDU2 frames followed by a TDU terminator.
 * @param network The network interface to use for emitting the frames.
 * @param srcId The source ID of the call.
 * @param dstId The destination ID of the call.
 * @param durationMs The duration of the call in milliseconds.
 * @param wacn The P25 wide area communications network ID.
 * @param sysId The P25 system ID.
 * @param stream The call stream state.
 * @return true if the frames were successfully emitted, false otherwise.
 */
static bool emitP25(CallSimNetwork& network, uint32_t srcId, uint32_t dstId, uint32_t durationMs,
    uint32_t wacn, uint32_t sysId, CallStreamState& stream)
{
    p25::lc::LC control;
    control.setLCO(P25DEF::LCO::GROUP);
    control.setGroup(true);
    control.setSrcId(srcId);
    control.setDstId(dstId);
    control.setNetId(wacn);
    control.setSysId(sysId);
    control.setPriority(4U);
    p25::data::LowSpeedData lsd;

    uint8_t ldu1[P25DEF::P25_LDU_FRAME_LENGTH_BYTES];
    uint8_t ldu2[P25DEF::P25_LDU_FRAME_LENGTH_BYTES];
    fillP25Tone(ldu1, 0U);
    fillP25Tone(ldu2, 1U);

    uint32_t elapsed = 0U;
    bool useLDU1 = true;
    while (!g_stop && elapsed < durationMs) {
        bool written = useLDU1 ? network.writeP25LDU1Call(control, lsd, ldu1, stream) : network.writeP25LDU2Call(control, lsd, ldu2, stream);
        if (!written)
            return false;

        useLDU1 = !useLDU1;

        uint32_t delay = std::min(P25_LDU_PERIOD_MS, durationMs - elapsed);
        waitForFrame(delay);

        elapsed += delay;
    }

    return network.writeP25TDUCall(control, lsd, stream);
}

/**
 * @brief Emits a six-burst DMR voice sequence followed by a terminator-with-LC marker.
 * @param network The network interface to use for emitting the call.
 * @param srcId The source ID of the call.
 * @param dstId The destination ID of the call.
 * @param slot The slot number for the call (relevant for DMR mode).
 * @param durationMs The duration of the call in milliseconds.
 * @param stream The call stream state.
 * @return true if the call was successfully emitted, false otherwise.
 */
static bool emitDMR(CallSimNetwork& network, uint32_t srcId, uint32_t dstId, uint8_t slot, uint32_t durationMs, CallStreamState& stream)
{
    uint32_t elapsed = 0U;
    uint8_t n = 0U;
    uint8_t seqNo = 0U;

    while (!g_stop && elapsed < durationMs) {
        dmr::data::NetData frame;
        frame.setSlotNo(slot);
        frame.setDataType(n == 0U ? DMRDEF::DataType::VOICE_SYNC : DMRDEF::DataType::VOICE);
        frame.setSrcId(srcId);
        frame.setDstId(dstId);
        frame.setFLCO(DMRDEF::FLCO::GROUP);
        frame.setN(n);
        frame.setSeqNo(seqNo++);
        frame.setBER(0U);
        frame.setRSSI(0U);
        frame.setData(DMR_TONE_BURST);

        if (!network.writeDMRCall(frame, stream))
            return false;

        n = (n + 1U) % 6U;
        uint32_t delay = std::min(DMR_BURST_PERIOD_MS, durationMs - elapsed);
        waitForFrame(delay);

        elapsed += delay;
    }

    dmr::data::NetData terminator;
    uint8_t data[DMRDEF::DMR_FRAME_LENGTH_BYTES] = { 0U };
    terminator.setSlotNo(slot);
    terminator.setDataType(DMRDEF::DataType::TERMINATOR_WITH_LC);
    terminator.setSrcId(srcId);
    terminator.setDstId(dstId);
    terminator.setFLCO(DMRDEF::FLCO::GROUP);
    terminator.setSeqNo(seqNo);
    terminator.setData(data);

    return network.writeDMRCall(terminator, stream, true);
}

/**
 * @brief Emits one NXDN 4800 voice frame with segmented SACCH link control.
 * @param network The network interface to use for emitting the frame.
 * @param control The RTCH link control structure.
 * @param sequence The sequence number of the frame within the superframe.
 * @param stream The call stream state.
 * @param end Indicates if this is the last frame of the call.
 * @return true if the frame was successfully emitted, false otherwise.
 */
static bool emitNXDNFrame(CallSimNetwork& network, nxdn::lc::RTCH& control, uint8_t sequence, CallStreamState& stream, bool end = false)
{
    using namespace nxdn;
    using namespace nxdn::defines;

    uint8_t frame[NXDN_FRAME_LENGTH_BYTES];
    ::memset(frame, 0x00U, sizeof(frame));
    Sync::addNXDNSync(frame);

    channel::LICH lich;
    lich.setRFCT(RFChannelType::RDCH);
    lich.setFCT(FuncChannelType::USC_SACCH_SS);
    lich.setOption(ChOption::STEAL_NONE);
    lich.setOutbound(true);
    lich.encode(frame);

    uint8_t lcData[NXDN_RTCH_LC_LENGTH_BYTES] = { 0U };
    control.encode(lcData, NXDN_RTCH_LC_LENGTH_BITS);
    static const ChStructure::E structures[] = {
        ChStructure::SR_1_4, ChStructure::SR_2_4, ChStructure::SR_3_4, ChStructure::SR_4_4
    };
    uint8_t sacchData[3U] = { 0U };
    uint8_t superframeIndex = sequence % 4U;
    for (uint32_t bit = 0U; bit < 18U; bit++)
        WRITE_BIT(sacchData, bit, READ_BIT(lcData, superframeIndex * 18U + bit));

    channel::SACCH sacch;
    sacch.setData(sacchData);
    sacch.setRAN(0U);
    sacch.setStructure(structures[superframeIndex]);
    sacch.encode(frame);

    NXDNUtils::scrambler(frame);
    ::memcpy(frame + NXDN_FSW_LICH_SACCH_LENGTH_BYTES, NXDN_TONE_AUDIO, sizeof(NXDN_TONE_AUDIO));

    return network.writeNXDNCall(control, frame, NXDN_FRAME_LENGTH_BYTES, stream, end);
}

/**
 * @brief Emits NXDN voice frames followed by an RTCH transmission-release frame.
 * @param network The network interface to use for emitting the frames.
 * @param srcId The source ID of the call.
 * @param dstId The destination ID of the call.
 * @param durationMs The duration of the call in milliseconds.
 * @param stream The call stream state.
 * @return true if the frames were successfully emitted, false otherwise.
 */
static bool emitNXDN(CallSimNetwork& network, uint32_t srcId, uint32_t dstId, uint32_t durationMs, CallStreamState& stream)
{
    using namespace nxdn::defines;
    nxdn::lc::RTCH control;
    control.setMessageType(MessageType::RTCH_VCALL);
    control.setCallType(CallType::UNSPECIFIED);
    control.setGroup(true);
    control.setSrcId(static_cast<uint16_t>(srcId));
    control.setDstId(static_cast<uint16_t>(dstId));
    control.setTransmissionMode(TransmissionMode::MODE_4800);

    uint32_t elapsed = 0U;
    uint8_t sequence = 0U;
    while (!g_stop && elapsed < durationMs) {
        if (!emitNXDNFrame(network, control, sequence++, stream))
            return false;

        uint32_t delay = std::min(NXDN_FRAME_PERIOD_MS, durationMs - elapsed);
        waitForFrame(delay);

        elapsed += delay;
    }

    control.setMessageType(MessageType::RTCH_TX_REL);

    return emitNXDNFrame(network, control, sequence, stream, true);
}

/**
 * @brief Emits one complete call using the specified options and call parameters.
 * @param network The network interface to use for emitting the call.
 * @param options The call simulation options.
 * @param callNumber The current call number.
 * @param totalCalls The total number of calls to simulate.
 * @param srcId The source ID of the call.
 * @param dstId The destination ID of the call.
 * @param slot The slot number for the call (relevant for DMR mode).
 * @param durationMs The duration of the call in milliseconds.
 * @return true if the call was successfully emitted, false otherwise.
 */
static bool emitCall(CallSimNetwork& network, const Options& options, uint32_t callNumber, uint32_t totalCalls,
    uint32_t srcId, uint32_t dstId, uint8_t slot, uint32_t durationMs)
{
    LogInfo("Call %u/%u start: srcId = %u, dstId = %u, slot = %u, duration = %u seconds",
        callNumber, totalCalls, srcId, dstId, slot, options.durationSeconds);

    if (options.grantDemand) {
        LogInfo("Call %u/%u grant demand: srcId = %u, dstId = %u, slot = %u",
            callNumber, totalCalls, srcId, dstId, options.mode == Mode::DMR ? slot : 0U);
        if (!writeGrantDemand(network, options.mode, srcId, dstId, slot)) {
            LogError(LOG_NET, "call %u grant demand failed while writing to the FNE", callNumber);
            return false;
        }
    }

    CallStreamState stream;
    bool success = false;
    switch (options.mode) {
    case Mode::P25:
        success = emitP25(network, srcId, dstId, durationMs, options.p25WACN, options.p25SysId, stream);
        break;
    case Mode::DMR:
        success = emitDMR(network, srcId, dstId, slot, durationMs, stream);
        break;
    case Mode::NXDN:
        success = emitNXDN(network, srcId, dstId, durationMs, stream);
        break;
    }

    if (!success) {
        LogError(LOG_NET, "call %u failed while writing to the FNE", callNumber);
        return false;
    }

    LogInfo("Call %u/%u end: srcId = %u, dstId = %u", callNumber, totalCalls, srcId, dstId);
    return true;
}

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Runs one simulated call through the project's thread abstraction.
 * @ingroup callsim
 */
class CallWorker : public Thread {
public:
    /**
     * @brief Initializes a worker for one simulated call.
     * @param[in] network Network connection shared by the simulated calls.
     * @param[in] options Simulator options shared by the workers.
     * @param callNumber One-based call number.
     * @param totalCalls Total calls scheduled for this simulator run.
     * @param srcId Source subscriber ID.
     * @param dstId Destination talkgroup ID.
     * @param slot DMR slot, or ignored for FDMA protocols.
     * @param durationMs Call duration in milliseconds.
     * @param[out] result Receives one when the call succeeds, or zero on failure.
     * @param[in,out] remaining Number of running workers.
     */
    CallWorker(CallSimNetwork& network, const Options& options, uint32_t callNumber, uint32_t totalCalls,
        uint32_t srcId, uint32_t dstId, uint8_t slot, uint32_t durationMs, uint8_t& result,
        std::atomic<uint32_t>& remaining);

    /**
     * @brief Emits the configured call and records its result.
     */
    void entry() override;

private:
    CallSimNetwork& m_network;                 //!< Network connection shared by the simulated calls.
    const Options& m_options;                  //!< Simulator options shared by the workers.
    uint32_t m_callNumber;                     //!< One-based call number.
    uint32_t m_totalCalls;                     //!< Total calls scheduled for this simulator run.
    uint32_t m_srcId;                          //!< Source subscriber ID.
    uint32_t m_dstId;                          //!< Destination talkgroup ID.
    uint8_t m_slot;                            //!< DMR slot, or ignored for FDMA protocols.
    uint32_t m_durationMs;                     //!< Call duration in milliseconds.
    uint8_t& m_result;                         //!< Result storage owned by the scheduler.
    std::atomic<uint32_t>& m_remaining;        //!< Number of running workers.
};

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a worker for one simulated call. */

CallWorker::CallWorker(CallSimNetwork& network, const Options& options, uint32_t callNumber, uint32_t totalCalls,
    uint32_t srcId, uint32_t dstId, uint8_t slot, uint32_t durationMs, uint8_t& result,
    std::atomic<uint32_t>& remaining) :
    m_network(network),
    m_options(options),
    m_callNumber(callNumber),
    m_totalCalls(totalCalls),
    m_srcId(srcId),
    m_dstId(dstId),
    m_slot(slot),
    m_durationMs(durationMs),
    m_result(result),
    m_remaining(remaining)
{
    /* stub */
}

/* Emits the configured call and records its result. */

void CallWorker::entry()
{
    m_result = emitCall(m_network, m_options, m_callNumber, m_totalCalls, m_srcId, m_dstId, m_slot,
        m_durationMs) ? 1U : 0U;
    m_remaining--;
}

// ---------------------------------------------------------------------------
//  Program Entry Point
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    if (argv[0] != nullptr && *argv[0] != 0)
        g_progExe = std::string(argv[0]);

    Options options;
    if (!parseOptions(argc, argv, options))
        return argc > 1 && (std::string(argv[1U]) == "--help" || std::string(argv[1U]) == "-h") ? EXIT_SUCCESS : EXIT_FAILURE;

    version();

    ::signal(SIGINT, signalHandler);
    ::signal(SIGTERM, signalHandler);
    if (!LogInitialise("", "callsim", 0U, options.debug ? 4U : 2U, false, false)) {
        ::fprintf(stderr, "callsim: failed to initialize logging\n");
        return EXIT_FAILURE;
    }

    CallSimNetwork network(options.address, options.port, 0U, options.peerId, options.password,
        true, options.debug, true, true, true, false, true, true, false, false, false, false);
    p25::lc::LC::setSiteData(p25::SiteData(options.p25WACN, options.p25SysId, 1U, 1U, 0U, 1U, 1U,
        P25DEF::ServiceClass::VOICE | P25DEF::ServiceClass::DATA, 0));
    network.setMetadata(options.identity, 0U, 0U, 0.0F, 0.0F, 0U, 0U, 0U, 0.0F, 0.0F, 0, "");
    network.enable(true);
    if (!network.open()) {
        LogError(LOG_NET, "failed to open FNE connection to %s:%u", options.address.c_str(), options.port);
        LogFinalise();
        return EXIT_FAILURE;
    }

    // Network::open() starts the login exchange; continue clocking until the
    // FNE accepts the peer or the bounded login interval expires.
    uint32_t waited = 0U;
    while (!g_stop && network.getStatus() != NET_STAT_RUNNING && waited < DEFAULT_CONNECT_TIMEOUT_MS) {
        waitAndClock(network, 10U);
        waited += 10U;
    }
    if (network.getStatus() != NET_STAT_RUNNING) {
        LogError(LOG_NET, "FNE login did not complete within %u ms", DEFAULT_CONNECT_TIMEOUT_MS);
        network.close();
        LogFinalise();
        return EXIT_FAILURE;
    }

    // in simultaneous mode each repetition starts one call per talkgroup; the
    // calls retain independent RTP stream and sequence state while all socket
    // access is serialized through the single authenticated connection
    const uint32_t durationMs = options.durationSeconds * MILLISECONDS_PER_SECOND;
    const uint32_t callsPerRepetition = options.simultaneous ? static_cast<uint32_t>(options.talkgroups.size()) : 1U;
    const uint32_t totalCalls = options.callCount * callsPerRepetition;
    bool success = true;

    // iterate over the number of call repetitions, spawning and managing worker threads for each call
    for (uint32_t repetition = 0U; repetition < options.callCount && !g_stop; repetition++) {
        std::atomic<uint32_t> remaining(0U);
        std::vector<uint8_t> results(callsPerRepetition, 0U);
        std::vector<std::unique_ptr<CallWorker>> workers;
        workers.reserve(callsPerRepetition);

        // spawn worker threads for each call in the current repetition
        for (uint32_t member = 0U; member < callsPerRepetition; member++) {
            const uint32_t scheduleIndex = options.simultaneous ? member : repetition;
            const uint32_t callIndex = repetition * callsPerRepetition + member;
            const uint32_t dstId = options.talkgroups[scheduleIndex % options.talkgroups.size()];
            const uint8_t slot = options.slots[scheduleIndex % options.slots.size()];
            const uint32_t srcId = options.srcId + callIndex;

            workers.push_back(std::make_unique<CallWorker>(network, options, callIndex + 1U, totalCalls,
                srcId, dstId, slot, durationMs, results[member], remaining));
            remaining++;
            if (!workers.back()->run()) {
                remaining--;
                break;
            }
            workers.back()->setName("callsim-worker");
        }

        auto lastClock = std::chrono::steady_clock::now();
        while (remaining.load() > 0U) {
            const auto now = std::chrono::steady_clock::now();
            const uint32_t elapsed = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - lastClock).count());
            if (elapsed > 0U) {
                network.clockConnection(elapsed);
                lastClock = now;
            }

            Thread::sleep(1U);
        }

        // join all worker threads for the current repetition before proceeding
        for (const std::unique_ptr<CallWorker>& worker : workers)
            worker->wait();

        // determine if all calls in the current repetition were successful
        success = std::find(results.begin(), results.end(), 0U) == results.end();
        if (!success)
            break;

        if (repetition + 1U < options.callCount)
            waitAndClock(network, options.gapMs);
    }

    network.close();
    LogFinalise();

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
