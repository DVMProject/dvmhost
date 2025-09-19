// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/p25/kmm/KMMFactory.h"
#include "common/Log.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "network/FNENetwork.h"
#include "network/P25OTARService.h"
#include "HostFNE.h"

using namespace network;
using namespace network::callhandler;
using namespace network::callhandler::packetdata;
using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>
#include <chrono>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the P25OTARService class. */

P25OTARService::P25OTARService(FNENetwork* network, P25PacketData* packetData, bool debug) :
    m_network(network),
    m_packetData(packetData),
    m_debug(debug)
{
    assert(network != nullptr);
    assert(packetData != nullptr);
}

/* Finalizes a instance of the P25OTARService class. */

P25OTARService::~P25OTARService() = default;

/* Helper used to process KMM frames from PDU data. */

bool P25OTARService::processDLD(const uint8_t* data, uint32_t len, uint32_t llId, uint8_t n, bool encrypted)
{
    m_packetData->write_PDU_Ack_Response(PDUAckClass::ACK, PDUAckType::ACK, n, llId, false);

    uint32_t payloadSize = 0U;
    UInt8Array pduUserData = processKMM(data, len, llId, encrypted, &payloadSize);
    m_packetData->write_PDU_KMM(pduUserData.get(), payloadSize, llId, encrypted);

    if (pduUserData == nullptr)
        return false;

    return true;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Helper used to process KMM frames. */

UInt8Array P25OTARService::processKMM(const uint8_t* data, uint32_t len, uint32_t llId, bool encrypted, uint32_t* payloadSize)
{
    if (payloadSize != nullptr)
        *payloadSize = 0U;

    std::unique_ptr<KMMFrame> frame = KMMFactory::create(data);
    if (frame == nullptr) {
        LogWarning(LOG_NET, P25_PDU_STR ", undecodable KMM packet");
        return nullptr;
    }

    switch (frame->getMessageId()) {
        case KMM_MessageType::HELLO:
        {
            KMMHello* kmm = static_cast<KMMHello*>(frame.get());
            LogMessage(LOG_NET, P25_PDU_STR ", KMM Hello, llId = %u, flag = $%02X", llId,
                kmm->getFlag());

            // respond with No-Service if KMF services are disabled
//            if (!m_network->m_kmfServicesEnabled)
                return write_KMM_NoService(llId, kmm->getSrcLLId(), payloadSize);
        }
        break;

        default:
        break;
    } // switch (packet->getPDUType())

    return nullptr;
}

/* Helper used to return a No-Service KMM to the calling SU. */

UInt8Array P25OTARService::write_KMM_NoService(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize)
{
    if (payloadSize != nullptr)
        *payloadSize = KMM_NO_SERVICE_LENGTH;

    UInt8Array kmmFrame = std::make_unique<uint8_t[]>(KMM_NO_SERVICE_LENGTH);

    uint8_t buffer[KMM_NO_SERVICE_LENGTH];
    KMMNoService outKmm = KMMNoService();
    outKmm.setSrcLLId(WUID_FNE);
    outKmm.setDstLLId(kmmRSI);

    outKmm.encode(buffer);

    ::memcpy(kmmFrame.get(), buffer, KMM_NO_SERVICE_LENGTH);
    return kmmFrame;
}
