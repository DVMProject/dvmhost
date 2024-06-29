// SPDX-License-Identifier: BSL-1.0
/*
 * Digital Voice Modem - Common Library
 * BSL-1.0 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (c) 2003-2013 Christopher M. Kohlhoff
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "network/rest/http/HTTPRequestHandler.h"
#include "network/rest/http/HTTPPayload.h"

using namespace network::rest::http;

#include <fstream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the HTTPRequestHandler class. */
HTTPRequestHandler::HTTPRequestHandler(const std::string& docRoot) :
    m_docRoot(docRoot)
{
    /* stub */
}

/* Handle a request and produce a reply. */
void HTTPRequestHandler::handleRequest(const HTTPPayload& request, HTTPPayload& reply)
{
    // decode url to path
    std::string requestPath;
    if (!urlDecode(request.uri, requestPath)) {
        reply = HTTPPayload::statusPayload(HTTPPayload::BAD_REQUEST);
        return;
    }

    // request path must be absolute and not contain "..".
    if (requestPath.empty() || requestPath[0] != '/' ||
        requestPath.find("..") != std::string::npos) {
        reply = HTTPPayload::statusPayload(HTTPPayload::BAD_REQUEST);
        return;
    }

    // if path ends in slash (i.e. is a directory) then add "index.html"
    if (requestPath[requestPath.size() - 1] == '/') {
        requestPath += "index.html";
    }

    // determine the file extension
    std::size_t lastSlashPos = requestPath.find_last_of("/");
    std::size_t lastDotPos = requestPath.find_last_of(".");
    std::string extension;
    if (lastDotPos != std::string::npos && lastDotPos > lastSlashPos) {
        extension = requestPath.substr(lastDotPos + 1);
    }

    // open the file to send back
    std::string fullPath = m_docRoot + requestPath;
    std::ifstream is(fullPath.c_str(), std::ios::in | std::ios::binary);
    if (!is) {
        reply = HTTPPayload::statusPayload(HTTPPayload::NOT_FOUND);
        return;
    }

    // fill out the reply to be sent to the client
    reply.status = HTTPPayload::OK;

    char buf[512];
    while (is.read(buf, sizeof(buf)).gcount() > 0)
        reply.content.append(buf, is.gcount());

    reply.headers.clearHeaders();
    reply.headers.add("Content-Length", std::to_string(reply.content.size()));
    reply.headers.add("Content-Type", "application/octet-stream");
}

// ---------------------------------------------------------------------------
//  Private Members
// ---------------------------------------------------------------------------

/* Perform URL-decoding on a string. Returns false if the encoding was invalid. */
bool HTTPRequestHandler::urlDecode(const std::string& in, std::string& out)
{
    out.clear();
    out.reserve(in.size());

    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%') {
            if (i + 3 <= in.size()) {
                int value = 0;
                std::istringstream is(in.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    out += static_cast<char>(value);
                    i += 2;
                }
                else {
                    return false;
                }
            }
            else {
                return false;
            }
        }
        else if (in[i] == '+') {
            out += ' ';
        }
        else {
            out += in[i];
        }
    }

    return true;
}
