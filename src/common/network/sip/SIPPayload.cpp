// SPDX-License-Identifier: BSL-1.0
/*
 * Digital Voice Modem - Common Library
 * BSL-1.0 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (c) 2003-2013 Christopher M. Kohlhoff
 *  Copyright (C) 2023-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "network/sip/SIPPayload.h"
#include "Log.h"
#include "Utils.h"

using namespace network::sip;

#include <string>

namespace status_strings {
    const std::string trying = "SIP/2.0 100 Trying\r\n";
    const std::string ringing = "SIP/2.0 180 Ringing\r\n";
    const std::string ok = "SIP/2.0 200 OK\r\n";
    const std::string accepted = "SIP/2.0 202 Accepted\r\n";
    const std::string no_notify = "SIP/2.0 204 No Notification\r\n";
    const std::string multiple_choices = "SIP/2.0 300 Multiple Choices\r\n";
    const std::string moved_permanently = "SIP/2.0 301 Moved Permanently\r\n";
    const std::string moved_temporarily = "SIP/2.0 302 Moved Temporarily\r\n";
    const std::string bad_request = "SIP/2.0 400 Bad Request\r\n";
    const std::string unauthorized = "SIP/2.0 401 Unauthorized\r\n";
    const std::string forbidden = "SIP/2.0 403 Forbidden\r\n";
    const std::string not_found = "SIP/2.0 404 Not Found\r\n";
    const std::string internal_server_error = "SIP/2.0 500 Internal Server Error\r\n";
    const std::string not_implemented = "SIP/2.0 501 Not Implemented\r\n";
    const std::string bad_gateway = "SIP/2.0 502 Bad Gateway\r\n";
    const std::string service_unavailable = "SIP/2.0 503 Service Unavailable\r\n";
    const std::string busy_everywhere = "SIP/2.0 600 Busy Everywhere\r\n";
    const std::string decline = "SIP/2.0 603 Decline\r\n";

    asio::const_buffer toBuffer(SIPPayload::StatusType status)
    {
        switch (status)
        {
        case SIPPayload::TRYING:
            return asio::buffer(trying);
        case SIPPayload::RINGING:
            return asio::buffer(ringing);
        case SIPPayload::OK:
            return asio::buffer(ok);
        case SIPPayload::ACCEPTED:
            return asio::buffer(accepted);
        case SIPPayload::NO_NOTIFY:
            return asio::buffer(no_notify);
        case SIPPayload::MULTIPLE_CHOICES:
            return asio::buffer(multiple_choices);
        case SIPPayload::MOVED_PERMANENTLY:
            return asio::buffer(moved_permanently);
        case SIPPayload::MOVED_TEMPORARILY:
            return asio::buffer(moved_temporarily);
        case SIPPayload::BAD_REQUEST:
            return asio::buffer(bad_request);
        case SIPPayload::UNAUTHORIZED:
            return asio::buffer(unauthorized);
        case SIPPayload::FORBIDDEN:
            return asio::buffer(forbidden);
        case SIPPayload::NOT_FOUND:
            return asio::buffer(not_found);
        case SIPPayload::INTERNAL_SERVER_ERROR:
            return asio::buffer(internal_server_error);
        case SIPPayload::NOT_IMPLEMENTED:
            return asio::buffer(not_implemented);
        case SIPPayload::BAD_GATEWAY:
            return asio::buffer(bad_gateway);
        case SIPPayload::SERVICE_UNAVAILABLE:
            return asio::buffer(service_unavailable);
        case SIPPayload::BUSY_EVERYWHERE:
            return asio::buffer(busy_everywhere);
        case SIPPayload::DECLINE:
            return asio::buffer(decline);
        default:
            return asio::buffer(internal_server_error);
        }
    }
} // namespace status_strings

namespace misc_strings {
    const char name_value_separator[] = { ':', ' ' };
    const char request_method_separator[] = { ' ' };
    const char crlf[] = { '\r', '\n' };

    const char sip_default_version[] = { 'S', 'I', 'P', '/', '2', '.', '0' };
} // namespace misc_strings

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Convert the reply into a vector of buffers. The buffers do not own the underlying memory blocks, therefore the reply object must remain valid and not be changed until the write operation has completed. */

std::vector<asio::const_buffer> SIPPayload::toBuffers()
{
    std::vector<asio::const_buffer> buffers;
    if (isClientPayload) {
        // copy method and erase zero terminator
        method.erase(std::find(method.begin(), method.end(), '\0'), method.end());

        // copy URI and erase zero terminator
        uri.erase(std::find(uri.begin(), uri.end(), '\0'), uri.end());
#if DEBUG_SIP_PAYLOAD
        ::LogDebugEx(LOG_SIP, "SIPPayload::toBuffers()", "method = %s, uri = %s", method.c_str(), uri.c_str());
#endif
        buffers.push_back(asio::buffer(method));
        buffers.push_back(asio::buffer(misc_strings::request_method_separator));
        buffers.push_back(asio::buffer(uri));
        buffers.push_back(asio::buffer(misc_strings::request_method_separator));
        buffers.push_back(asio::buffer(misc_strings::sip_default_version));
        buffers.push_back(asio::buffer(misc_strings::crlf));
    }
    else {
        buffers.push_back(status_strings::toBuffer(status));
    }

    for (std::size_t i = 0; i < headers.size(); ++i) {
        SIPHeaders::Header& h = headers.m_headers[i];
#if DEBUG_SIP_PAYLOAD
        ::LogDebugEx(LOG_SIP, "SIPPayload::toBuffers()", "header = %s, value = %s", h.name.c_str(), h.value.c_str());
#endif

        buffers.push_back(asio::buffer(h.name));
        buffers.push_back(asio::buffer(misc_strings::name_value_separator));
        buffers.push_back(asio::buffer(h.value));
        buffers.push_back(asio::buffer(misc_strings::crlf));
    }

    buffers.push_back(asio::buffer(misc_strings::crlf));
    if (content.size() > 0)
        buffers.push_back(asio::buffer(content));

#if DEBUG_SIP_PAYLOAD
    ::LogDebugEx(LOG_SIP, "SIPPayload::toBuffers()", "content = %s", content.c_str());
    for (auto buffer : buffers)
        Utils::dump("SIPPayload::toBuffers(), buffer[]", (uint8_t*)buffer.data(), buffer.size());
#endif

    return buffers;
}

/* Prepares payload for transmission by finalizing status and content type. */

void SIPPayload::payload(json::object& obj, SIPPayload::StatusType s)
{
    json::value v = json::value(obj);
    std::string json = std::string(v.serialize());
    payload(json, s, "application/json");
}

/* Prepares payload for transmission by finalizing status and content type. */

void SIPPayload::payload(std::string& c, SIPPayload::StatusType s, const std::string& contentType)
{
    content = c;
    status = s;
    ensureDefaultHeaders(contentType);
}

// ---------------------------------------------------------------------------
//  Static Members
// ---------------------------------------------------------------------------

/* Get a status payload. */

SIPPayload SIPPayload::requestPayload(std::string method, std::string uri)
{
    SIPPayload rep;
    rep.isClientPayload = true;
    rep.method = ::strtoupper(method);
    rep.uri = std::string(uri);
    return rep;
}

/* Get a status payload. */

SIPPayload SIPPayload::statusPayload(SIPPayload::StatusType status, const std::string& contentType)
{
    SIPPayload rep;
    rep.isClientPayload = false;
    rep.status = status;
    rep.ensureDefaultHeaders(contentType);

    return rep;
}


/* Helper to attach a host TCP stream reader. */

void SIPPayload::attachHostHeader(const asio::ip::tcp::endpoint remoteEndpoint)
{
    headers.add("Host", std::string(remoteEndpoint.address().to_string() + ":" + std::to_string(remoteEndpoint.port())));
}

// ---------------------------------------------------------------------------
//  Private Members
// ---------------------------------------------------------------------------

/* Internal helper to ensure the headers are of a default for the given content type. */

void SIPPayload::ensureDefaultHeaders(const std::string& contentType)
{
    if (!isClientPayload) {
        headers.add("Content-Type", std::string(contentType));
        headers.add("Content-Length", std::to_string(content.size()));
        headers.add("Server", std::string(("DVM/" __VER__)));
    }
    else {
        headers.add("User-Agent", std::string(("DVM/" __VER__)));
        headers.add("Accept", "*/*");
        headers.add("Content-Type", std::string(contentType));
        headers.add("Content-Length", std::to_string(content.size()));
    }
}
