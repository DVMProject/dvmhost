// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 */
/**
 * @file SubscriberOperation.cpp
 * @ingroup fne_network
 */
#include "fne/Defines.h"
#include "common/Log.h"
#include "common/json/json.h"
#include "network/MetadataNetwork.h"

using namespace network;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t SUBSCRIBER_OPERATION_HEADER_LENGTH = 11U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::SUBSCRIBER_OP packets. */

void MetadataNetwork::PacketHandler::subscriberOperation(TrafficNetwork* network,
    MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc,
    uint32_t streamId)
{
    (void)ssrc;

    TrafficNetwork::PeerConnectionPtr connection = network->findPeer(peerId);
    const std::string sourceAddress = udp::Socket::address(req->address);

    // validate the peer connection and source address before processing the subscriber operation request
    if (connection == nullptr || !connection->connected() || connection->peerClass() != PEER_CONN_CLASS_CONSOLE ||
        (!connection->address().empty() && connection->address() != sourceAddress)) {
        network->writePeerNAK(peerId, network->createStreamId(), "SUBSCRIBER_OP", NET_CONN_NAK_FNE_UNAUTHORIZED);
        return;
    }

    // prepare the response object and helper for writing the response
    json::object response = json::object();
    const bool notFound = false;
    const NET_SUBFUNC::ENUM subFunction = req->fneHeader.getSubFunction();
    const auto writeResponse = [network, mdNetwork, connection, subFunction, streamId](json::object responseObj) {
        json::value responseValue(responseObj);
        const std::string responsePayload = responseValue.serialize();
        const uint32_t responseLength = static_cast<uint32_t>(responsePayload.length());

        // check if the response payload fits within the allowed data packet length
        if (responseLength + SUBSCRIBER_OPERATION_HEADER_LENGTH > DATA_PACKET_LENGTH) {
            LogError(LOG_MASTER, "PEER %u subscriber operation response too large, len = %u",
                connection->id(), responseLength);
            return false;
        }

        uint8_t buffer[DATA_PACKET_LENGTH];
        ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);
        ::memcpy(buffer + SUBSCRIBER_OPERATION_HEADER_LENGTH, responsePayload.data(), responseLength);

        return mdNetwork->writePeerMetadata(connection.get(), network->m_peerId, { NET_FUNC::SUBSCRIBER_OP, subFunction }, buffer,
            responseLength + SUBSCRIBER_OPERATION_HEADER_LENGTH, RTP_END_OF_CALL_SEQ, streamId);
    };

    // validate the request length against the expected header and data packet limits
    if (req->length <= static_cast<int>(SUBSCRIBER_OPERATION_HEADER_LENGTH) || req->length > static_cast<int>(DATA_PACKET_LENGTH)) {
        response["found"].set<bool>(notFound);
        response["error"].set<std::string>("invalid request");
        writeResponse(response);
        return;
    }

    // calculate the payload length by subtracting the header length from the total request length
    const size_t payloadLength = static_cast<size_t>(req->length) - SUBSCRIBER_OPERATION_HEADER_LENGTH;
    const std::string payload(reinterpret_cast<const char*>(req->buffer + SUBSCRIBER_OPERATION_HEADER_LENGTH), payloadLength);
    json::value value;
    const std::string parseError = json::parse(value, payload);
    if (!parseError.empty() || !value.is<json::object>()) {
        response["found"].set<bool>(notFound);
        response["error"].set<std::string>("invalid request");
        writeResponse(response);
        return;
    }

    json::object& request = value.get<json::object>();
    switch (subFunction) {
    case NET_SUBFUNC::SUBSCRIBER_OP_OPERATION:
        {
            response["type"].set<std::string>("operation");
            if (!request["operationId"].is<uint64_t>()) {
                response["found"].set<bool>(notFound);
                response["error"].set<std::string>("invalid operation ID");
                break;
            }

            const uint64_t operationId = request["operationId"].get<uint64_t>();
            SubscriberOperationStore::Operation operation;
            const bool found = operationId != 0U && network->subscriberOperations().get(operationId, operation);
            response["found"].set<bool>(found);
            response["operationId"].set<uint64_t>(operationId);
            if (found) {
                response["protocol"].set<std::string>(SubscriberOperationStore::protocolName(operation.protocol));
                response["operation"].set<std::string>(SubscriberOperationStore::kindName(operation.kind));
                response["state"].set<std::string>(SubscriberOperationStore::stateName(operation.state));
                response["targetId"].set<uint32_t>(operation.targetId);
                response["peerId"].set<uint32_t>(operation.peerId);
                response["requestValue"].set<uint16_t>(operation.requestValue);
                response["resultValue"].set<uint16_t>(operation.resultValue);
                response["createdAt"].set<uint64_t>(operation.createdAt);
                response["completedAt"].set<uint64_t>(operation.completedAt);
            }
        }
        break;

    case NET_SUBFUNC::SUBSCRIBER_OP_P25_STATUS:
        {
            response["type"].set<std::string>("p25-status");
            if (!request["subscriberId"].is<uint32_t>()) {
                response["found"].set<bool>(notFound);
                response["error"].set<std::string>("invalid subscriber ID");
                break;
            }

            const uint32_t subscriberId = request["subscriberId"].get<uint32_t>();
            SubscriberOperationStore::Status status;
            const bool found = subscriberId != 0U &&
                network->subscriberOperations().getP25Status(subscriberId, status);
            response["found"].set<bool>(found);
            response["protocol"].set<std::string>("p25");
            response["subscriberId"].set<uint32_t>(subscriberId);
            if (found) {
                response["peerId"].set<uint32_t>(status.peerId);
                response["status"].set<uint16_t>(status.value);
                response["solicited"].set<bool>(status.solicited);
                response["observedAt"].set<uint64_t>(status.observedAt);
            }
        }
        break;

    default:
        response["found"].set<bool>(notFound);
        response["error"].set<std::string>("unsupported query");
        break;
    }

    writeResponse(response);
}
