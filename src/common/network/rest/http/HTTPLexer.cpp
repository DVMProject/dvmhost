// SPDX-License-Identifier: BSL-1.0
/*
 * Digital Voice Modem - Common Library
 * BSL-1.0 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (c) 2003-2013 Christopher M. Kohlhoff
 *  Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "network/rest/http/HTTPLexer.h"
#include "network/rest/http/HTTPPayload.h"
#include "Log.h"

using namespace network::rest::http;

#include <cctype>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the HTTPLexer class. */
HTTPLexer::HTTPLexer(bool clientLexer) :
    m_headers(),
    m_clientLexer(clientLexer),
    m_consumed(0U),
    m_state(METHOD_START)
{
    if (m_clientLexer) {
        m_state = HTTP_VERSION_H;
    }
}

/* Reset to initial parser state. */
void HTTPLexer::reset()
{
    m_state = METHOD_START;
    if (m_clientLexer) {
        m_state = HTTP_VERSION_H;
    }

    m_headers = std::vector<LexedHeader>();
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Handle the next character of input. */
HTTPLexer::ResultType HTTPLexer::consume(HTTPPayload& req, char input)
{
    m_consumed++;
    switch (m_state)
    {
    /*
    ** HTTP Method
    */

    case METHOD_START:
        if (!isChar(input) || isControl(input) || isSpecial(input))
            return BAD;
        else {
            m_state = METHOD;
            req.method.push_back(input);
            return INDETERMINATE;
        }
    case METHOD:
        if (input == ' ') {
            m_state = URI;
            return INDETERMINATE;
        }
        else if (!isChar(input) || isControl(input) || isSpecial(input)) {
            return BAD;
        }
        else {
            req.method.push_back(input);
            return INDETERMINATE;
        }

    /*
    ** URI
    */

    case URI:
        if (input == ' ') {
            m_state = HTTP_VERSION_H;
            return INDETERMINATE;
        }
        else if (isControl(input)) {
            return BAD;
        }
        else {
            req.uri.push_back(input);
            return INDETERMINATE;
        }

    /*
    ** HTTP/1.0
    ** HTTP/1.0 200 OK
    */
    case HTTP_VERSION_H:
        if (input == 'H') {
            m_state = HTTP_VERSION_T_1;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case HTTP_VERSION_T_1:
        if (input == 'T') {
            m_state = HTTP_VERSION_T_2;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case HTTP_VERSION_T_2:
        if (input == 'T') {
            m_state = HTTP_VERSION_P;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case HTTP_VERSION_P:
        if (input == 'P') {
            m_state = HTTP_VERSION_SLASH;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case HTTP_VERSION_SLASH:
        if (input == '/') {
            req.httpVersionMajor = 0;
            req.httpVersionMinor = 0;
            m_state = HTTP_VERSION_MAJOR_START;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case HTTP_VERSION_MAJOR_START:
        if (isDigit(input)) {
            req.httpVersionMajor = req.httpVersionMajor * 10 + input - '0';
            m_state = HTTP_VERSION_MAJOR;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case HTTP_VERSION_MAJOR:
        if (input == '.') {
            m_state = HTTP_VERSION_MINOR_START;
            return INDETERMINATE;
        }
        else if (isDigit(input)) {
            req.httpVersionMajor = req.httpVersionMajor * 10 + input - '0';
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case HTTP_VERSION_MINOR_START:
        if (isDigit(input)) {
            req.httpVersionMinor = req.httpVersionMinor * 10 + input - '0';
            m_state = HTTP_VERSION_MINOR;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case HTTP_VERSION_MINOR:
        if (input == '\r') {
            m_state = EXPECTING_NEWLINE_1;
            if (m_clientLexer) {
                return BAD;
            }
            else {
                return INDETERMINATE;
            }
        }
        else if (input == ' ') {
            if (m_clientLexer) {
                m_state = HTTP_STATUS_1;
                return INDETERMINATE;
            }
            else {
                return BAD;
            }
        }
        else if (isDigit(input)) {
            req.httpVersionMinor = req.httpVersionMinor * 10 + input - '0';
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case HTTP_STATUS_1:
        if (isDigit(input)) {
            m_state = HTTP_STATUS_2;
            m_status = m_status * 10 + input - '0';
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case HTTP_STATUS_2:
        if (isDigit(input)) {
            m_state = HTTP_STATUS_3;
            m_status = m_status * 10 + input - '0';
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case HTTP_STATUS_3:
        if (isDigit(input)) {
            m_state = HTTP_STATUS_END;
            m_status = m_status * 10 + input - '0';
            req.status = (HTTPPayload::StatusType)m_status;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case HTTP_STATUS_END:
        if (input == ' ') {
            m_state = HTTP_STATUS_MESSAGE;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case HTTP_STATUS_MESSAGE_START:
        if (!isChar(input) || isControl(input) || isSpecial(input)) {
            return BAD;
        }
        else {
            m_state = HTTP_STATUS_MESSAGE;
            return INDETERMINATE;
        }
    case HTTP_STATUS_MESSAGE:
        if (input == '\r') {
            m_state = EXPECTING_NEWLINE_1;
            return INDETERMINATE;
        }
        else if (input == ' ') {
            m_state = HTTP_STATUS_MESSAGE;
            return INDETERMINATE;
        }
        else if (!isChar(input) || isControl(input) || isSpecial(input)) {
            return BAD;
        }
        else {
            return INDETERMINATE;
        }

    case EXPECTING_NEWLINE_1:
        if (input == '\n') {
            m_state = HEADER_LINE_START;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }

    /*
    ** Headers
    */

    case HEADER_LINE_START:
        if (input == '\r') {
            m_state = EXPECTING_NEWLINE_3;
            return INDETERMINATE;
        }
        else if (!req.headers.empty() && (input == ' ' || input == '\t')) {
            m_state = HEADER_LWS;
            return INDETERMINATE;
        }
        else if (!isChar(input) || isControl(input) || isSpecial(input)) {
            return BAD;
        }
        else {
            m_headers.push_back(LexedHeader());
            m_headers.back().name.push_back(std::tolower(input));
            m_state = HEADER_NAME;
            return INDETERMINATE;
        }

    case HEADER_LWS:
        if (input == '\r') {
            m_state = EXPECTING_NEWLINE_2;
            return INDETERMINATE;
        }
        else if (input == ' ' || input == '\t') {
            return INDETERMINATE;
        }
        else if (isControl(input)) {
            return BAD;
        }
        else {
            m_state = HEADER_VALUE;
            m_headers.back().value.push_back(input);
            return INDETERMINATE;
        }

    case HEADER_NAME:
        if (input == ':') {
            m_state = SPACE_BEFORE_HEADER_VALUE;
            return INDETERMINATE;
        }
        else if (!isChar(input) || isControl(input) || isSpecial(input)) {
            return BAD;
        }
        else
        {
            m_headers.back().name.push_back(std::tolower(input));
            return INDETERMINATE;
        }

    case SPACE_BEFORE_HEADER_VALUE:
        if (input == ' ') {
            m_state = HEADER_VALUE;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }

    case HEADER_VALUE:
        if (input == '\r') {
            m_state = EXPECTING_NEWLINE_2;
            return INDETERMINATE;
        }
        else if (isControl(input)) {
            return BAD;
        }
        else {
            m_headers.back().value.push_back(input);
            return INDETERMINATE;
        }

    case EXPECTING_NEWLINE_2:
        if (input == '\n') {
            m_state = HEADER_LINE_START;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }

    case EXPECTING_NEWLINE_3:
        if (input == '\n') {
            for (auto header : m_headers) {
                //::LogDebug(LOG_REST, "HTTPLexer::consume(), header = %s, value = %s", header.name.c_str(), header.value.c_str());
                req.headers.add(header.name, header.value);
            }

            return GOOD;
        } else {
            return BAD;
        }

    default:
        return BAD;
    }
}

/* Check if a byte is an HTTP character. */
bool HTTPLexer::isChar(int c)
{
    return c >= 0 && c <= 127;
}

/* Check if a byte is an HTTP control character. */
bool HTTPLexer::isControl(int c)
{
    return (c >= 0 && c <= 31) || (c == 127);
}

/* Check if a byte is an HTTP special character. */
bool HTTPLexer::isSpecial(int c)
{
    switch (c)
    {
    case '(': case ')': case '<': case '>': case '@':
    case ',': case ';': case ':': case '\\': case '"':
    case '/': case '[': case ']': case '?': case '=':
    case '{': case '}': case ' ': case '\t':
        return true;
    default:
        return false;
    }
}

/* Check if a byte is an digit. */
bool HTTPLexer::isDigit(int c)
{
    return c >= '0' && c <= '9';
}
