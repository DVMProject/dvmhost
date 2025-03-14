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
#include "network/sip/SIPLexer.h"
#include "network/sip/SIPPayload.h"
#include "Log.h"

using namespace network::sip;

#include <cctype>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the SIPLexer class. */

SIPLexer::SIPLexer(bool clientLexer) :
    m_headers(),
    m_clientLexer(clientLexer),
    m_consumed(0U),
    m_state(METHOD_START)
{
    if (m_clientLexer) {
        m_state = SIP_VERSION_S;
    }
}

/* Reset to initial parser state. */

void SIPLexer::reset()
{
    m_state = METHOD_START;
    if (m_clientLexer) {
        m_state = SIP_VERSION_S;
    }

    m_headers = std::vector<LexedHeader>();
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Handle the next character of input. */

SIPLexer::ResultType SIPLexer::consume(SIPPayload& req, char input)
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
            m_state = SIP_VERSION_S;
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
    ** SIP/2.0
    ** SIP/2.0 200 OK
    */
    case SIP_VERSION_S:
        if (input == 'S') {
            m_state = SIP_VERSION_I;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case SIP_VERSION_I:
        if (input == 'I') {
            m_state = SIP_VERSION_P;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case SIP_VERSION_P:
        if (input == 'P') {
            m_state = SIP_VERSION_SLASH;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case SIP_VERSION_SLASH:
        if (input == '/') {
            req.sipVersionMajor = 0;
            req.sipVersionMinor = 0;
            m_state = SIP_VERSION_MAJOR_START;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case SIP_VERSION_MAJOR_START:
        if (isDigit(input)) {
            req.sipVersionMajor = req.sipVersionMajor * 10 + input - '0';
            m_state = SIP_VERSION_MAJOR;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case SIP_VERSION_MAJOR:
        if (input == '.') {
            m_state = SIP_VERSION_MINOR_START;
            return INDETERMINATE;
        }
        else if (isDigit(input)) {
            req.sipVersionMajor = req.sipVersionMajor * 10 + input - '0';
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case SIP_VERSION_MINOR_START:
        if (isDigit(input)) {
            req.sipVersionMinor = req.sipVersionMinor * 10 + input - '0';
            m_state = SIP_VERSION_MINOR;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case SIP_VERSION_MINOR:
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
                m_state = SIP_STATUS_1;
                return INDETERMINATE;
            }
            else {
                return BAD;
            }
        }
        else if (isDigit(input)) {
            req.sipVersionMinor = req.sipVersionMinor * 10 + input - '0';
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case SIP_STATUS_1:
        if (isDigit(input)) {
            m_state = SIP_STATUS_2;
            m_status = m_status * 10 + input - '0';
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case SIP_STATUS_2:
        if (isDigit(input)) {
            m_state = SIP_STATUS_3;
            m_status = m_status * 10 + input - '0';
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case SIP_STATUS_3:
        if (isDigit(input)) {
            m_state = SIP_STATUS_END;
            m_status = m_status * 10 + input - '0';
            req.status = (SIPPayload::StatusType)m_status;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case SIP_STATUS_END:
        if (input == ' ') {
            m_state = SIP_STATUS_MESSAGE;
            return INDETERMINATE;
        }
        else {
            return BAD;
        }
    case SIP_STATUS_MESSAGE_START:
        if (!isChar(input) || isControl(input) || isSpecial(input)) {
            return BAD;
        }
        else {
            m_state = SIP_STATUS_MESSAGE;
            return INDETERMINATE;
        }
    case SIP_STATUS_MESSAGE:
        if (input == '\r') {
            m_state = EXPECTING_NEWLINE_1;
            return INDETERMINATE;
        }
        else if (input == ' ') {
            m_state = SIP_STATUS_MESSAGE;
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
                //::LogDebugEx(LOG_SIP, "SIPLexer::consume()", "header = %s, value = %s", header.name.c_str(), header.value.c_str());
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

/* Check if a byte is an SIP character. */

bool SIPLexer::isChar(int c)
{
    return c >= 0 && c <= 127;
}

/* Check if a byte is an SIP control character. */

bool SIPLexer::isControl(int c)
{
    return (c >= 0 && c <= 31) || (c == 127);
}

/* Check if a byte is an SIP special character. */

bool SIPLexer::isSpecial(int c)
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

bool SIPLexer::isDigit(int c)
{
    return c >= '0' && c <= '9';
}
