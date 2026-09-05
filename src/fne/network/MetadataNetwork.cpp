// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/edac/SHA256.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "network/MetadataNetwork.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"

using namespace network;
using namespace network::callhandler;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MetadataNetwork class. */

MetadataNetwork::MetadataNetwork(HostFNE* host, TrafficNetwork* trafficNetwork, const std::string& address, uint16_t port, uint16_t workerCnt) :
    BaseNetwork(trafficNetwork->m_peerId, true, trafficNetwork->m_debug, true, true, trafficNetwork->m_allowActivityTransfer, trafficNetwork->m_allowDiagnosticTransfer),
    m_trafficNetwork(trafficNetwork),
    m_host(host),
    m_address(address),
    m_port(port),
    m_status(NET_STAT_INVALID),
    m_peerReplicaActPkt(),
    m_peerTreeListPkt(),
    m_threadPool(workerCnt, "meta")
{
    assert(trafficNetwork != nullptr);
    assert(host != nullptr);
    assert(!address.empty());
    assert(port > 0U);
}

/* Finalizes a instance of the MetadataNetwork class. */

MetadataNetwork::~MetadataNetwork() = default;

/* Sets endpoint preshared encryption key. */

void MetadataNetwork::setPresharedKey(const uint8_t* presharedKey)
{
    m_socket->setPresharedKey(presharedKey);
}

/* Process a data frames from the network. */

void MetadataNetwork::processNetwork()
{
    if (m_status != NET_STAT_MST_RUNNING) {
        return;
    }

    sockaddr_storage address;
    uint32_t addrLen;
    frame::RTPHeader rtpHeader;
    frame::RTPFNEHeader fneHeader;
    int length = 0U;

    // read message
    UInt8Array buffer = m_frameQueue->read(length, address, addrLen, &rtpHeader, &fneHeader);
    if (length > 0) {
        if (m_debug)
            Utils::dump(1U, "MetadataNetwork::processNetwork(), Network Message", buffer.get(), length);

        uint32_t peerId = fneHeader.getPeerId();

        NetPacketRequest* req = new NetPacketRequest();
        req->obj = m_trafficNetwork;
        req->metadataObj = this;
        req->peerId = peerId;

        req->address = address;
        req->addrLen = addrLen;
        req->rtpHeader = rtpHeader;
        req->fneHeader = fneHeader;

        req->length = length;
        req->buffer = new uint8_t[length];
        ::memcpy(req->buffer, buffer.get(), length);

        if (!m_threadPool.enqueue(new_pooltask(taskNetworkRx, req))) {
            LogError(LOG_NET, "Failed to task enqueue network packet request, peerId = %u, %s:%u", peerId, 
                udp::Socket::address(address).c_str(), udp::Socket::port(address));
            if (req != nullptr) {
                if (req->buffer != nullptr)
                    delete[] req->buffer;
                delete req;
            }
        }
    }
}

/* Updates the timer by the passed number of milliseconds. */

void MetadataNetwork::clock(uint32_t ms)
{
    if (m_status != NET_STAT_MST_RUNNING) {
        return;
    }
}

/* Opens connection to the network. */

bool MetadataNetwork::open()
{
    if (m_debug)
        LogInfoEx(LOG_DIAG, "Opening Network");

    m_threadPool.start();

    m_status = NET_STAT_MST_RUNNING;

    m_socket = new udp::Socket(m_address, m_port);

    // reinitialize the frame queue
    if (m_frameQueue != nullptr) {
        delete m_frameQueue;
        m_frameQueue = new FrameQueue(m_socket, m_peerId, false);
    }

    bool ret = m_socket->open();
    if (!ret) {
        m_socket->recvBufSize(524288U); // 512K recv buffer
        m_socket->sendBufSize(524288U); // 512K send buffer
        m_status = NET_STAT_INVALID;
    }

    return ret;
}

/* Closes connection to the network. */

void MetadataNetwork::close()
{
    if (m_debug)
        LogInfoEx(LOG_DIAG, "Closing Network");

    m_threadPool.stop();
    m_threadPool.wait();
    
    m_socket->close();

    m_status = NET_STAT_INVALID;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Finds a packet buffer entry in the map. */

MetadataNetwork::PacketBufferEntryPtr MetadataNetwork::findPacketBufferEntry(PacketBufferMap& pktMap, uint32_t peerId)
{
    pktMap.lock(false);

    auto it = pktMap.find(peerId);
    PacketBufferEntryPtr pkt = (it != pktMap.end()) ? it->second : nullptr;

    pktMap.unlock();
    return pkt;
}

/* Finds or creates a packet buffer entry in the map. */

MetadataNetwork::PacketBufferEntryPtr MetadataNetwork::findOrCreatePacketBufferEntry(PacketBufferMap& pktMap, uint32_t peerId, const char* name, uint32_t streamId, bool* created)
{
    if (created != nullptr)
        *created = false;
    pktMap.lock(false);

    auto& entries = pktMap.get();
    auto it = entries.find(peerId);
    if (it == entries.end()) {
        PacketBufferEntryPtr pkt = std::make_shared<PacketBufferEntry>();
        pkt->buffer = std::make_unique<PacketBuffer>(true, name);
        pkt->streamId = streamId;

        it = entries.insert({ peerId, pkt }).first;
        if (created != nullptr)
            *created = true;
    }

    PacketBufferEntryPtr pkt = it->second;

    pktMap.unlock();
    return pkt;
}

/* Erases a packet buffer entry from the map. */

void MetadataNetwork::erasePacketBufferEntry(PacketBufferMap& pktMap, uint32_t peerId, const PacketBufferEntryPtr& pkt)
{
    pktMap.lock(false);

    auto& entries = pktMap.get();
    auto it = entries.find(peerId);
    if (it != entries.end() && it->second == pkt)
        entries.erase(it);

    pktMap.unlock();
}

/*
** Packet Processing
*/

/* Process a data frames from the network. */

void MetadataNetwork::taskNetworkRx(NetPacketRequest* req)
{
    if (req != nullptr) {
        TrafficNetwork* network = static_cast<TrafficNetwork*>(req->obj);
        if (network == nullptr) {
            if (req != nullptr) {
                if (req->buffer != nullptr)
                    delete[] req->buffer;
                delete req;
            }

            return;
        }

        MetadataNetwork* mdNetwork = static_cast<MetadataNetwork*>(req->metadataObj);
        if (mdNetwork == nullptr) {
            if (req != nullptr) {
                if (req->buffer != nullptr)
                    delete[] req->buffer;
                delete req;
            }

            return;
        }

        if (req == nullptr)
            return;

        if (req->length > 0) {
            uint32_t peerId = req->fneHeader.getPeerId();
            uint32_t ssrc = req->rtpHeader.getSSRC();
            uint32_t streamId = req->fneHeader.getStreamId();

            static const std::unordered_map<uint8_t, PacketHandlerFunc> handlers = {
                { NET_FUNC::TRANSFER, &MetadataNetwork::PacketHandler::transfer },
                { NET_FUNC::ANNOUNCE, &MetadataNetwork::PacketHandler::announce },

                { NET_FUNC::KEYS_INVENTORY, &MetadataNetwork::PacketHandler::keysInventory },
                { NET_FUNC::KEYS_UPDATE, &MetadataNetwork::PacketHandler::keysUpdate },

                { NET_FUNC::RADIO_ALIAS_SYNC, &MetadataNetwork::PacketHandler::radioAliasSync },

                { NET_FUNC::REPL, &MetadataNetwork::PacketHandler::replication },
                { NET_FUNC::NET_TREE, &MetadataNetwork::PacketHandler::networkTree }
            };

            // dispatch to the appropriate handler based on the function opcode
            const uint8_t func = req->fneHeader.getFunction();
            const uint8_t subFunc = req->fneHeader.getSubFunction();

            // validate transfer packet header length
            if (func == NET_FUNC::TRANSFER && (subFunc == NET_SUBFUNC::TRANSFER_SUBFUNC_ACTIVITY || subFunc == NET_SUBFUNC::TRANSFER_SUBFUNC_DIAG) &&
                !(req->length > TRANSFER_PCKT_HDR_LEN && req->length <= static_cast<int>(DATA_PACKET_LENGTH))) {
                LogWarning(LOG_MASTER, "PEER %u malformed FNE transfer packet, subfunc = $%02X, length = %d", peerId, subFunc, req->length);
                if (req->buffer != nullptr)
                    delete[] req->buffer;
                delete req;
                return;
            }

            auto it = handlers.find(func);
            if (it != handlers.end()) {
                it->second(network, mdNetwork, req, peerId, ssrc, streamId);
            }
        }

        if (req->buffer != nullptr)
            delete[] req->buffer;
        delete req;
    }
}
