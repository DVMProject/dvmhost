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
#include "common/dmr/DMRDefines.h"
#include "common/Log.h"
#include "common/p25/P25Defines.h"
#include "common/json/json.h"
#include "network/MetadataNetwork.h"
#include "network/callhandler/TagDMRData.h"
#include "network/callhandler/TagP25Data.h"

using namespace network;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define SOP_DMR "dmr"
#define SOP_P25 "p25"
#define SOP_NXDN "nxdn"

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
    const bool found = true;
    const NET_SUBFUNC::ENUM subFunction = req->fneHeader.getSubFunction();

    // helper function to write the response back to the peer
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
    case NET_SUBFUNC::SUBSCRIBER_OP_START:
        {
            response["type"].set<std::string>("operation-start");
            if (!request["protocol"].is<std::string>() || !request["operation"].is<std::string>() ||
                !request["targetId"].is<uint32_t>()) {
                response["found"].set<bool>(notFound);
                response["error"].set<std::string>("invalid operation request");
                break;
            }

            const std::string protocolName = request["protocol"].get<std::string>();
            const std::string operationName = request["operation"].get<std::string>();
            const uint32_t targetId = request["targetId"].get<uint32_t>();
            const uint32_t requestedPeerId = request["peerId"].getDefault<uint32_t>(0U);
            const uint32_t operationPeerId = requestedPeerId != 0U ? requestedPeerId : network->findPeerUnitReg(targetId);

            if (targetId == 0U) {
                response["found"].set<bool>(notFound);
                response["error"].set<std::string>("invalid target ID");
                break;
            }

            SubscriberOperationStore::Protocol protocol;
            SubscriberOperationStore::Kind kind;
            uint16_t requestValue = 0U;
            if (protocolName == SOP_P25) {
                protocol = SubscriberOperationStore::Protocol::P25;

                // determine the kind of P25 operation requested
                if (operationName == "radio-check") {
                    kind = SubscriberOperationStore::Kind::RADIO_CHECK;
                } else if (operationName == "status-query") {
                    kind = SubscriberOperationStore::Kind::STATUS_QUERY;
                } else if (operationName == "message-update") {
                    if (!request["value"].is<uint16_t>() || operationPeerId == 0U) {
                        response["found"].set<bool>(notFound);
                        response["error"].set<std::string>("message update requires a value and known route or explicit peer ID");
                        break;
                    }
                    kind = SubscriberOperationStore::Kind::MESSAGE_UPDATE;
                    requestValue = request["value"].get<uint16_t>();
                } else if (operationName == "inhibit") {
                    kind = SubscriberOperationStore::Kind::INHIBIT;
                } else if (operationName == "uninhibit") {
                    kind = SubscriberOperationStore::Kind::UNINHIBIT;
                } else {
                    response["found"].set<bool>(notFound);
                    response["error"].set<std::string>("unsupported P25 operation");
                    break;
                }
            } else if (protocolName == SOP_DMR) {
                protocol = SubscriberOperationStore::Protocol::DMR;
                if (!request["slot"].is<uint8_t>()) {
                    response["found"].set<bool>(notFound);
                    response["error"].set<std::string>("invalid DMR slot");
                    break;
                }

                // extract and validate the DMR slot from the request
                const uint8_t slot = request["slot"].get<uint8_t>();
                if (slot == 0U || slot > 2U) {
                    response["found"].set<bool>(notFound);
                    response["error"].set<std::string>("invalid DMR slot");
                    break;
                }

                // determine the kind of DMR operation requested
                if (operationName == "radio-check") {
                    kind = SubscriberOperationStore::Kind::RADIO_CHECK;
                } else if (operationName == "inhibit") {
                    kind = SubscriberOperationStore::Kind::INHIBIT;
                } else if (operationName == "uninhibit") {
                    kind = SubscriberOperationStore::Kind::UNINHIBIT;
                } else {
                    response["found"].set<bool>(notFound);
                    response["error"].set<std::string>("unsupported DMR operation");
                    break;
                }
            } else {
                response["found"].set<bool>(notFound);
                response["error"].set<std::string>("unsupported protocol");
                break;
            }

            // start the subscriber operation and obtain its operation ID
            const uint64_t operationId = network->subscriberOperations().start(
                protocol, kind, targetId, operationPeerId, requestValue);
            if (operationId == 0U) {
                response["found"].set<bool>(notFound);
                response["error"].set<std::string>("an equivalent operation is already pending");
                break;
            }

            // send the appropriate protocol-specific message based on the operation kind
            if (protocol == SubscriberOperationStore::Protocol::P25) {
                switch (kind) {
                case SubscriberOperationStore::Kind::RADIO_CHECK:
                    network->p25TrafficHandler()->write_TSDU_Ext_Func(operationPeerId,
                        p25::defines::ExtendedFunctions::CHECK, p25::defines::WUID_FNE, targetId);
                    break;
                case SubscriberOperationStore::Kind::STATUS_QUERY:
                    network->p25TrafficHandler()->write_TSDU_Status_Query(operationPeerId, targetId);
                    break;
                case SubscriberOperationStore::Kind::MESSAGE_UPDATE:
                    network->p25TrafficHandler()->write_TSDU_Message_Update(operationPeerId, targetId, requestValue);
                    break;
                case SubscriberOperationStore::Kind::INHIBIT:
                    network->p25TrafficHandler()->write_TSDU_Ext_Func(operationPeerId,
                        p25::defines::ExtendedFunctions::INHIBIT, p25::defines::WUID_FNE, targetId);
                    break;
                case SubscriberOperationStore::Kind::UNINHIBIT:
                    network->p25TrafficHandler()->write_TSDU_Ext_Func(operationPeerId,
                        p25::defines::ExtendedFunctions::UNINHIBIT, p25::defines::WUID_FNE, targetId);
                    break;
                }
            } else {
                const uint8_t slot = request["slot"].get<uint8_t>();
                uint8_t function = dmr::defines::ExtendedFunctions::CHECK;
                uint32_t argument = dmr::defines::WUID_ALL;
                if (kind == SubscriberOperationStore::Kind::INHIBIT) {
                    function = dmr::defines::ExtendedFunctions::INHIBIT;
                    argument = dmr::defines::WUID_STUNI;
                } else if (kind == SubscriberOperationStore::Kind::UNINHIBIT) {
                    function = dmr::defines::ExtendedFunctions::UNINHIBIT;
                    argument = dmr::defines::WUID_STUNI;
                }

                network->dmrTrafficHandler()->write_Ext_Func(operationPeerId, slot, function, argument, targetId);
            }

            response["found"].set<bool>(found);
            response["operationId"].set<uint64_t>(operationId);
            response["protocol"].set<std::string>(SubscriberOperationStore::protocolName(protocol));
            response["operation"].set<std::string>(SubscriberOperationStore::kindName(kind));
            response["state"].set<std::string>("pending");
            response["targetId"].set<uint32_t>(targetId);
            response["peerId"].set<uint32_t>(operationPeerId);
            response["requestValue"].set<uint16_t>(requestValue);
        }
        break;

    case NET_SUBFUNC::SUBSCRIBER_OP_OPERATION:
        {
            response["type"].set<std::string>("operation");

            // check if the request contains an operation ID and validate it
            const bool hasOperationId = request.find("operationId") != request.end();
            if (hasOperationId && !request["operationId"].is<uint64_t>()) {
                response["found"].set<bool>(notFound);
                response["error"].set<std::string>("invalid operation ID");
                break;
            }

            // if an operation ID was provided, attempt to retrieve the corresponding operation from the store
            if (hasOperationId) {
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

            // retrieve the last 5 subscriber operations from the store and construct the JSON array
            json::array operationHistory = json::array();
            const std::vector<SubscriberOperationStore::Operation> operations = network->subscriberOperations().getLast(5U);
            for (const SubscriberOperationStore::Operation& historyOperation : operations) {
                json::object operationObj = json::object();
                operationObj["operationId"].set<uint64_t>(historyOperation.id);
                operationObj["protocol"].set<std::string>(SubscriberOperationStore::protocolName(historyOperation.protocol));
                operationObj["operation"].set<std::string>(SubscriberOperationStore::kindName(historyOperation.kind));
                operationObj["state"].set<std::string>(SubscriberOperationStore::stateName(historyOperation.state));
                operationObj["targetId"].set<uint32_t>(historyOperation.targetId);
                operationObj["peerId"].set<uint32_t>(historyOperation.peerId);
                operationObj["requestValue"].set<uint16_t>(historyOperation.requestValue);
                operationObj["resultValue"].set<uint16_t>(historyOperation.resultValue);
                operationObj["createdAt"].set<uint64_t>(historyOperation.createdAt);
                operationObj["completedAt"].set<uint64_t>(historyOperation.completedAt);
                operationHistory.push_back(json::value(operationObj));
            }

            // if no specific operation ID was requested, indicate whether any history was found
            if (!hasOperationId) {
                const bool historyFound = !operations.empty();
                response["found"].set<bool>(historyFound);
            }

            response["operations"].set<json::array>(operationHistory);
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

            // retrieve the subscriber ID from the request and fetch the corresponding P25 status from the store
            const uint32_t subscriberId = request["subscriberId"].get<uint32_t>();
            SubscriberOperationStore::Status status;
            const bool found = subscriberId != 0U && network->subscriberOperations().getP25Status(subscriberId, status);

            response["found"].set<bool>(found);
            response["protocol"].set<std::string>(SOP_P25);
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
