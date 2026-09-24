// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2017-2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/Log.h"
#include "common/nxdn/NXDNDefines.h"
#include "network/Network.h"

using namespace network;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::PROTOCOL packets. */

bool Network::PacketHandler::protocol(Network* network, uint32_t peerId, uint32_t streamId, uint64_t now,
    const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader, const uint8_t* buffer, int length)
{
    (void)now;

    // are protocol messages being user handled?
    if (network->m_userHandleProtocol) {
        network->userPacketHandler(fneHeader.getPeerId(), { fneHeader.getFunction(), fneHeader.getSubFunction() },
            buffer, length, fneHeader.getStreamId(), fneHeader, rtpHeader);
        return false;
    }

    // process incoming message subfunction opcodes
    switch (fneHeader.getSubFunction()) {
    case NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR:                 // Encapsulated DMR data frame
        {
            if (network->m_enabled && network->m_dmrEnabled) {
                uint32_t slotNo = (buffer[15U] & 0x80U) == 0x80U ? 1U : 0U; // this is the raw index for the stream ID array

                if (network->m_debug) {
                    LogDebug(LOG_NET, "DMR Slot %u, peer = %u, len = %u, pktSeq = %u, streamId = %u",
                        slotNo + 1U, peerId, length, rtpHeader.getSequence(), streamId);
                }

                if (network->m_promiscuousPeer) {
                    network->m_rxDMRStreamId[slotNo] = streamId;
                    network->m_pktLastSeq = network->m_pktSeq;

                    uint16_t lastRxSeq = 0U;

                    MULTIPLEX_RET_CODE ret = network->m_mux->verifyStream(streamId, rtpHeader.getSequence(), fneHeader.getFunction(), &lastRxSeq);
                    if (ret == MUX_LOST_FRAMES) {
                        LogError(LOG_NET, "PEER %u stream %u possible lost frames; got %u, expected %u", peerId,
                            streamId, rtpHeader.getSequence(), lastRxSeq, rtpHeader.getSequence());
                    }
                    else if (ret == MUX_OUT_OF_ORDER) {
                        LogError(LOG_NET, "PEER %u stream %u out-of-order; got %u, expected >%u", peerId,
                            streamId, rtpHeader.getSequence(), lastRxSeq);
                    }
#if DEBUG_RTP_MUX
                    else {
                        LogDebugEx(LOG_NET, "Network::clock()", "PEER %u valid mux, seq = %u, streamId = %u", peerId, rtpHeader.getSequence(), streamId);
                    }
#endif
                }
                else {
                    if (network->m_rxDMRStreamId[slotNo] == 0U) {
                        if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                            network->m_rxDMRStreamId[slotNo] = 0U;
                        }
                        else {
                            network->m_rxDMRStreamId[slotNo] = streamId;
                        }

                        network->m_pktLastSeq = network->m_pktSeq;
                    }
                    else {
                        if (network->m_rxDMRStreamId[slotNo] == streamId) {
                            uint16_t lastRxSeq = 0U;

                            MULTIPLEX_RET_CODE ret = network->verifyStream(&lastRxSeq);
                            if (ret == MUX_LOST_FRAMES) {
                                LogWarning(LOG_NET, "DMR Slot %u stream %u possible lost frames; got %u, expected %u",
                                    slotNo, streamId, network->m_pktSeq, lastRxSeq);
                            }
                            else if (ret == MUX_OUT_OF_ORDER) {
                                LogWarning(LOG_NET, "DMR Slot %u stream %u out-of-order; got %u, expected %u",
                                    slotNo, streamId, network->m_pktSeq, lastRxSeq);
                            }
#if DEBUG_RTP_MUX
                            else {
                                LogDebugEx(LOG_NET, "Network::clock()", "DMR Slot %u valid seq, seq = %u, streamId = %u", slotNo, rtpHeader.getSequence(), streamId);
                            }
#endif
                            if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                                network->m_rxDMRStreamId[slotNo] = 0U;
                            }
                        }
                    }

                    // check if we need to skip this stream -- a non-zero stream ID means the network client is locked
                    // to receiving a specific stream; a zero stream ID means the network is promiscuously
                    // receiving streams sent to this peer
                    if (network->m_rxDMRStreamId[slotNo] != 0U && network->m_rxDMRStreamId[slotNo] != streamId &&
                        rtpHeader.getSequence() != RTP_END_OF_CALL_SEQ) {
                        break;
                    }
                }

                if (network->m_packetDump)
                    Utils::dump(1U, "Network::clock(), Network Rx, DMR", buffer, length);
                if (length > (int)(DMR_PACKET_LENGTH + PACKET_PAD))
                    LogError(LOG_NET, "DMR Stream %u, frame oversized? this shouldn't happen, pktSeq = %u, len = %u", streamId, network->m_pktSeq, length);

                uint8_t len = length;
                network->m_rxDMRData.addData(&len, 1U);
                network->m_rxDMRData.addData(buffer, len);
            }
        }
        break;

    case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25:                 // Encapsulated P25 data frame
        {
            if (network->m_enabled && network->m_p25Enabled) {
                if (network->m_debug) {
                    LogDebug(LOG_NET, "P25, peer = %u, len = %u, pktSeq = %u, streamId = %u",
                        peerId, length, rtpHeader.getSequence(), streamId);
                }

                if (network->m_promiscuousPeer) {
                    network->m_rxP25StreamId = streamId;
                    network->m_pktLastSeq = network->m_pktSeq;

                    uint16_t lastRxSeq = 0U;

                    MULTIPLEX_RET_CODE ret = network->m_mux->verifyStream(streamId, rtpHeader.getSequence(), fneHeader.getFunction(), &lastRxSeq);
                    if (ret == MUX_LOST_FRAMES) {
                        LogError(LOG_NET, "PEER %u stream %u possible lost frames; got %u, expected %u", peerId,
                            streamId, rtpHeader.getSequence(), lastRxSeq, rtpHeader.getSequence());
                    }
                    else if (ret == MUX_OUT_OF_ORDER) {
                        LogError(LOG_NET, "PEER %u stream %u out-of-order; got %u, expected >%u", peerId,
                            streamId, rtpHeader.getSequence(), lastRxSeq);
                    }
#if DEBUG_RTP_MUX
                    else {
                        LogDebugEx(LOG_NET, "Network::clock()", "PEER %u valid mux, seq = %u, streamId = %u", peerId, rtpHeader.getSequence(), streamId);
                    }
#endif
                }
                else {
                    if (network->m_rxP25StreamId == 0U) {
                        if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                            network->m_rxP25StreamId = 0U;
                        }
                        else {
                            network->m_rxP25StreamId = streamId;
                        }

                        network->m_pktLastSeq = network->m_pktSeq;
                    }
                    else {
                        if (network->m_rxP25StreamId == streamId) {
                            uint16_t lastRxSeq = 0U;

                            MULTIPLEX_RET_CODE ret = network->verifyStream(&lastRxSeq);
                            if (ret == MUX_LOST_FRAMES) {
                                LogWarning(LOG_NET, "P25 stream %u possible lost frames; got %u, expected %u",
                                    streamId, network->m_pktSeq, lastRxSeq);
                            }
                            else if (ret == MUX_OUT_OF_ORDER) {
                                LogWarning(LOG_NET, "P25 stream %u out-of-order; got %u, expected %u",
                                    streamId, network->m_pktSeq, lastRxSeq);
                            }
#if DEBUG_RTP_MUX
                            else {
                                LogDebugEx(LOG_NET, "Network::clock()", "P25 valid seq, seq = %u, streamId = %u", rtpHeader.getSequence(), streamId);
                            }
#endif
                            if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                                network->m_rxP25StreamId = 0U;
                            }
                        }
                    }

                    // check if we need to skip this stream -- a non-zero stream ID means the network client is locked
                    // to receiving a specific stream; a zero stream ID means the network is promiscuously
                    // receiving streams sent to this peer
                    if (network->m_rxP25StreamId != 0U && network->m_rxP25StreamId != streamId &&
                        rtpHeader.getSequence() != RTP_END_OF_CALL_SEQ) {
                        break;
                    }
                }

                if (network->m_packetDump)
                    Utils::dump(1U, "Network::clock(), Network Rx, P25", buffer, length);
                if (length > 512)
                    LogError(LOG_NET, "P25 Stream %u, frame oversized? this shouldn't happen, pktSeq = %u, len = %u", streamId, network->m_pktSeq, length);

                // P25 frames can be up to 512 bytes, but we need to handle the case where the frame is larger than 255 bytes
                uint8_t len = length;
                if (length > 254) {
                    len = 254U;
                    network->m_rxP25Data.addData(&len, 1U);
                    len = length - 254U;
                    network->m_rxP25Data.addData(&len, 1U);
                }
                else {
                    network->m_rxP25Data.addData(&len, 1U);
                }

                network->m_rxP25Data.addData(buffer, length);
            }
        }
        break;

    case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2:              // Encapsulated P25 Phase 2 data frame
        {
            if (network->m_enabled && network->m_p25Enabled) {
                uint32_t slotNo = (buffer[19U] & 0x80U) == 0x80U ? 1U : 0U; // this is the raw index for the stream ID array

                if (network->m_debug) {
                    LogDebug(LOG_NET, "P25 Phase 2 Slot %u, peer = %u, len = %u, pktSeq = %u, streamId = %u",
                        slotNo + 1U, peerId, length, rtpHeader.getSequence(), streamId);
                }

                if (network->m_promiscuousPeer) {
                    network->m_rxP25P2StreamId[slotNo] = streamId;
                    network->m_pktLastSeq = network->m_pktSeq;

                    uint16_t lastRxSeq = 0U;

                    MULTIPLEX_RET_CODE ret = network->m_mux->verifyStream(streamId, rtpHeader.getSequence(), fneHeader.getFunction(), &lastRxSeq);
                    if (ret == MUX_LOST_FRAMES) {
                        LogError(LOG_NET, "PEER %u stream %u possible lost frames; got %u, expected %u", peerId,
                            streamId, rtpHeader.getSequence(), lastRxSeq, rtpHeader.getSequence());
                    }
                    else if (ret == MUX_OUT_OF_ORDER) {
                        LogError(LOG_NET, "PEER %u stream %u out-of-order; got %u, expected >%u", peerId,
                            streamId, rtpHeader.getSequence(), lastRxSeq);
                    }
#if DEBUG_RTP_MUX
                    else {
                        LogDebugEx(LOG_NET, "Network::clock()", "PEER %u valid mux, seq = %u, streamId = %u", peerId, rtpHeader.getSequence(), streamId);
                    }
#endif
                }
                else {
                    if (network->m_rxP25P2StreamId[slotNo] == 0U) {
                        if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                            network->m_rxP25P2StreamId[slotNo] = 0U;
                        }
                        else {
                            network->m_rxP25P2StreamId[slotNo] = streamId;
                        }

                        network->m_pktLastSeq = network->m_pktSeq;
                    }
                    else {
                        if (network->m_rxP25P2StreamId[slotNo] == streamId) {
                            uint16_t lastRxSeq = 0U;

                            MULTIPLEX_RET_CODE ret = network->verifyStream(&lastRxSeq);
                            if (ret == MUX_LOST_FRAMES) {
                                LogWarning(LOG_NET, "DMR Slot %u stream %u possible lost frames; got %u, expected %u",
                                    slotNo, streamId, network->m_pktSeq, lastRxSeq);
                            }
                            else if (ret == MUX_OUT_OF_ORDER) {
                                LogWarning(LOG_NET, "DMR Slot %u stream %u out-of-order; got %u, expected %u",
                                    slotNo, streamId, network->m_pktSeq, lastRxSeq);
                            }
#if DEBUG_RTP_MUX
                            else {
                                LogDebugEx(LOG_NET, "Network::clock()", "P25 Phase 2 Slot %u valid seq, seq = %u, streamId = %u", slotNo, rtpHeader.getSequence(), streamId);
                            }
#endif
                            if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                                network->m_rxP25P2StreamId[slotNo] = 0U;
                            }
                        }
                    }

                    // check if we need to skip this stream -- a non-zero stream ID means the network client is locked
                    // to receiving a specific stream; a zero stream ID means the network is promiscuously
                    // receiving streams sent to this peer
                    if (network->m_rxP25P2StreamId[slotNo] != 0U && network->m_rxP25P2StreamId[slotNo] != streamId &&
                        rtpHeader.getSequence() != RTP_END_OF_CALL_SEQ) {
                        break;
                    }
                }

                if (network->m_packetDump)
                    Utils::dump(1U, "Network::clock(), Network Rx, P25 Phase 2", buffer, length);
                if (length > (int)(P25_P2_PACKET_LENGTH + PACKET_PAD))
                    LogError(LOG_NET, "P25 Phase 2 Stream %u, frame oversized? this shouldn't happen, pktSeq = %u, len = %u", streamId, network->m_pktSeq, length);

                uint8_t len = length;
                network->m_rxP25P2Data.addData(&len, 1U);
                network->m_rxP25P2Data.addData(buffer, len);
            }
        }
        break;

    case NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN:                // Encapsulated NXDN data frame
        {
            if (network->m_enabled && network->m_nxdnEnabled) {
                if (network->m_debug) {
                    LogDebug(LOG_NET, "NXDN, peer = %u, len = %u, pktSeq = %u, streamId = %u",
                        peerId, length, rtpHeader.getSequence(), streamId);
                }

                if (network->m_promiscuousPeer) {
                    network->m_rxNXDNStreamId = streamId;
                    network->m_pktLastSeq = network->m_pktSeq;

                    uint16_t lastRxSeq = 0U;

                    MULTIPLEX_RET_CODE ret = network->m_mux->verifyStream(streamId, rtpHeader.getSequence(), fneHeader.getFunction(), &lastRxSeq);
                    if (ret == MUX_LOST_FRAMES) {
                        LogError(LOG_NET, "PEER %u stream %u possible lost frames; got %u, expected %u", peerId,
                            streamId, rtpHeader.getSequence(), lastRxSeq, rtpHeader.getSequence());
                    }
                    else if (ret == MUX_OUT_OF_ORDER) {
                        LogError(LOG_NET, "PEER %u stream %u out-of-order; got %u, expected >%u", peerId,
                            streamId, rtpHeader.getSequence(), lastRxSeq);
                    }
#if DEBUG_RTP_MUX
                    else {
                        LogDebugEx(LOG_NET, "Network::clock()", "PEER %u valid mux, seq = %u, streamId = %u", peerId, rtpHeader.getSequence(), streamId);
                    }
#endif
                }
                else {
                    if (network->m_rxNXDNStreamId == 0U) {
                        if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                            network->m_rxNXDNStreamId = 0U;
                        }
                        else {
                            network->m_rxNXDNStreamId = streamId;
                        }

                        network->m_pktLastSeq = network->m_pktSeq;
                    }
                    else {
                        if (network->m_rxNXDNStreamId == streamId) {
                            uint16_t lastRxSeq = 0U;

                            MULTIPLEX_RET_CODE ret = network->verifyStream(&lastRxSeq);
                            if (ret == MUX_LOST_FRAMES) {
                                LogWarning(LOG_NET, "NXDN stream %u possible lost frames; got %u, expected %u",
                                    streamId, network->m_pktSeq, lastRxSeq);
                            }
                            else if (ret == MUX_OUT_OF_ORDER) {
                                LogWarning(LOG_NET, "NXDN stream %u out-of-order; got %u, expected %u",
                                    streamId, network->m_pktSeq, lastRxSeq);
                            }
#if DEBUG_RTP_MUX
                            else {
                                LogDebugEx(LOG_NET, "Network::clock()", "NXDN valid seq, seq = %u, streamId = %u", rtpHeader.getSequence(), streamId);
                            }
#endif
                            if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                                network->m_rxNXDNStreamId = 0U;
                            }
                        }
                    }

                    // check if we need to skip this stream -- a non-zero stream ID means the network client is locked
                    // to receiving a specific stream; a zero stream ID means the network is promiscuously
                    // receiving streams sent to this peer
                    if (network->m_rxNXDNStreamId != 0U && network->m_rxNXDNStreamId != streamId &&
                        rtpHeader.getSequence() != RTP_END_OF_CALL_SEQ) {
                        break;
                    }
                }

                if (network->m_packetDump)
                    Utils::dump(1U, "Network::clock(), Network Rx, NXDN", buffer, length);
                if (length > (int)(NXDN_PACKET_LENGTH + PACKET_PAD)) {
                    LogWarning(LOG_NET, "NXDN Stream %u, rejecting oversized frame, pktSeq = %u, len = %u",
                        streamId, network->m_pktSeq, length);
                    break;
                }

                uint8_t len = length;
                network->m_rxNXDNData.addData(&len, 1U);
                network->m_rxNXDNData.addData(buffer, len);
            }
        }
        break;

    case NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG:              // Encapsulated Analog data frame
        {
            if (network->m_enabled && network->m_analogEnabled) {
                if (network->m_debug) {
                    LogDebug(LOG_NET, "Analog, peer = %u, len = %u, pktSeq = %u, streamId = %u",
                        peerId, length, rtpHeader.getSequence(), streamId);
                }

                if (network->m_promiscuousPeer) {
                    network->m_rxAnalogStreamId = streamId;
                    network->m_pktLastSeq = network->m_pktSeq;

                    uint16_t lastRxSeq = 0U;

                    MULTIPLEX_RET_CODE ret = network->m_mux->verifyStream(streamId, rtpHeader.getSequence(), fneHeader.getFunction(), &lastRxSeq);
                    if (ret == MUX_LOST_FRAMES) {
                        LogError(LOG_NET, "PEER %u stream %u possible lost frames; got %u, expected %u", peerId,
                            streamId, rtpHeader.getSequence(), lastRxSeq, rtpHeader.getSequence());
                    }
                    else if (ret == MUX_OUT_OF_ORDER) {
                        LogError(LOG_NET, "PEER %u stream %u out-of-order; got %u, expected >%u", peerId,
                            streamId, rtpHeader.getSequence(), lastRxSeq);
                    }
#if DEBUG_RTP_MUX
                    else {
                        LogDebugEx(LOG_NET, "Network::clock()", "PEER %u valid mux, seq = %u, streamId = %u", peerId, rtpHeader.getSequence(), streamId);
                    }
#endif
                }
                else {
                    if (network->m_rxAnalogStreamId == 0U) {
                        if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                            network->m_rxAnalogStreamId = 0U;
                        }
                        else {
                            network->m_rxAnalogStreamId = streamId;
                        }

                        network->m_pktLastSeq = network->m_pktSeq;
                    }
                    else {
                        if (network->m_rxAnalogStreamId == streamId) {
                            uint16_t lastRxSeq = 0U;

                            MULTIPLEX_RET_CODE ret = network->verifyStream(&lastRxSeq);
                            if (ret == MUX_LOST_FRAMES) {
                                LogWarning(LOG_NET, "Analog stream %u possible lost frames; got %u, expected %u",
                                    streamId, network->m_pktSeq, lastRxSeq);
                            }
                            else if (ret == MUX_OUT_OF_ORDER) {
                                LogWarning(LOG_NET, "Analog stream %u out-of-order; got %u, expected %u",
                                    streamId, network->m_pktSeq, lastRxSeq);
                            }
#if DEBUG_RTP_MUX
                            else {
                                LogDebugEx(LOG_NET, "Network::clock()", "Analog valid seq, seq = %u, streamId = %u", rtpHeader.getSequence(), streamId);
                            }
#endif
                            if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                                network->m_rxAnalogStreamId = 0U;
                            }
                        }
                    }

                    // check if we need to skip this stream -- a non-zero stream ID means the network client is locked
                    // to receiving a specific stream; a zero stream ID means the network is promiscuously
                    // receiving streams sent to this peer
                    if (network->m_rxAnalogStreamId != 0U && network->m_rxAnalogStreamId != streamId &&
                        rtpHeader.getSequence() != RTP_END_OF_CALL_SEQ) {
                        break;
                    }
                }

                if (network->m_packetDump)
                    Utils::dump(1U, "Network::clock(), Network Rx, Analog", buffer, length);
                if (length < (int)ANALOG_PACKET_LENGTH) {
                    LogError(LOG_NET, "Analog Stream %u, frame too short? this shouldn't happen, pktSeq = %u, len = %u", streamId, network->m_pktSeq, length);
                }
                else {
                    if (length > 512)
                        LogError(LOG_NET, "Analog Stream %u, frame oversized? this shouldn't happen, pktSeq = %u, len = %u", streamId, network->m_pktSeq, length);

                    // Analog frames are larger then 254 bytes, but we need to handle the case where the frame is larger than 255 bytes
                    uint8_t len = length - 254U;
                    network->m_rxAnalogData.addData(&len, 1U);

                    network->m_rxAnalogData.addData(buffer, length);
                }
            }
        }
        break;

    default:
        Utils::dump("unknown protocol opcode from the master", buffer, length);
        break;
    }

    return false;
}
