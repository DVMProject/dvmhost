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
 * @file CallSimNetwork.h
 * @brief Defines single-connection, multi-stream FNE networking for callsim.
 * @ingroup callsim
 */
#if !defined(__CALL_SIM_NETWORK_H__)
#define __CALL_SIM_NETWORK_H__

#include "common/network/Network.h"

#include <cstdint>
#include <mutex>

// ---------------------------------------------------------------------------
//  Structure Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Tracks the independent RTP state assigned to one simulated call.
 * @ingroup callsim
 */
struct CallStreamState {
    uint32_t streamId = 0U; //!< RTP stream ID; allocated when the first frame is written.
    uint16_t sequence = 0U; //!< Next ordinary RTP packet sequence number.
};

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Multiplexes independent simulated calls over one authenticated FNE connection.
 * @ingroup callsim
 */
class CallSimNetwork : public network::Network {
public:
    using network::Network::Network;

    /**
     * @brief Clocks the shared FNE connection while excluding concurrent writers.
     * @param ms Elapsed time in milliseconds.
     */
    void clockConnection(uint32_t ms);

    /**
     * @brief Writes a DMR frame using call-specific RTP state.
     * @param[in] data DMR network frame.
     * @param[in,out] stream RTP state for the simulated call.
     * @param end Flag indicating that this is the final call frame.
     * @return True when the frame was accepted for transmission.
     */
    bool writeDMRCall(const dmr::data::NetData& data, CallStreamState& stream, bool end = false);

    /**
     * @brief Writes a P25 LDU1 using call-specific RTP state.
     * @param[in] control P25 link control.
     * @param[in] lsd P25 low-speed data.
     * @param[in] data Raw P25 LDU data.
     * @param[in,out] stream RTP state for the simulated call.
     * @return True when the frame was accepted for transmission.
     */
    bool writeP25LDU1Call(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd,
        const uint8_t* data, CallStreamState& stream);

    /**
     * @brief Writes a P25 LDU2 using call-specific RTP state.
     * @param[in] control P25 link control.
     * @param[in] lsd P25 low-speed data.
     * @param[in] data Raw P25 LDU data.
     * @param[in,out] stream RTP state for the simulated call.
     * @return True when the frame was accepted for transmission.
     */
    bool writeP25LDU2Call(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd,
        const uint8_t* data, CallStreamState& stream);

    /**
     * @brief Writes a terminating P25 TDU using call-specific RTP state.
     * @param[in] control P25 link control.
     * @param[in] lsd P25 low-speed data.
     * @param[in,out] stream RTP state for the simulated call.
     * @return True when the frame was accepted for transmission.
     */
    bool writeP25TDUCall(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd,
        CallStreamState& stream);

    /**
     * @brief Writes an NXDN frame using call-specific RTP state.
     * @param[in] control NXDN link control.
     * @param[in] data Raw NXDN frame.
     * @param length Raw frame length in bytes.
     * @param[in,out] stream RTP state for the simulated call.
     * @param end Flag indicating that this is the final call frame.
     * @return True when the frame was accepted for transmission.
     */
    bool writeNXDNCall(const nxdn::lc::RTCH& control, const uint8_t* data, uint32_t length,
        CallStreamState& stream, bool end = false);

    /**
     * @brief Writes a group-call grant request while excluding concurrent frame writers.
     * @param mode DVM protocol mode.
     * @param srcId Source subscriber ID.
     * @param dstId Destination talkgroup ID.
     * @param slot DMR slot, or zero for FDMA protocols.
     * @return True when the request was accepted for transmission.
     */
    bool writeGroupGrantRequest(uint8_t mode, uint32_t srcId, uint32_t dstId, uint8_t slot);

private:
    std::mutex m_writeMutex; //!< Serializes socket writes and connection clock processing.

    /**
     * @brief Allocates an RTP stream ID on first use and returns the next sequence.
     * @param[in,out] stream RTP state for the simulated call.
     * @param end Flag indicating an end-of-call sequence marker.
     * @return RTP packet sequence value for the next write.
     */
    uint16_t nextSequence(CallStreamState& stream, bool end);
};

#endif // __CALL_SIM_NETWORK_H__
