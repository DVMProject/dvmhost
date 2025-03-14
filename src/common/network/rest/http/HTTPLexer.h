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
/**
 * @file HTTPLexer.h
 * @ingroup http
 * @file HTTPLexer.cpp
 * @ingroup http
 */
#if !defined(__REST_HTTP__HTTP_LEXER_H__)
#define __REST_HTTP__HTTP_LEXER_H__

#include "common/Defines.h"

#include <tuple>
#include <vector>

namespace network
{
    namespace rest
    {
        namespace http
        {
            // ---------------------------------------------------------------------------
            //  Class Prototypes
            // ---------------------------------------------------------------------------

            struct HTTPPayload;

            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief This class implements the lexer for incoming payloads.
             */
            class HTTPLexer {
            public:
                /**
                 * @brief Lexing result.
                 */
                enum ResultType { GOOD, BAD, INDETERMINATE, CONTINUE };

                /**
                 * @brief Initializes a new instance of the HTTPLexer class.
                 * @param clientLexer Flag indicating this lexer is used for a HTTP client.
                 */
                HTTPLexer(bool clientLexer);

                /**
                 * @brief Reset to initial parser state.
                 */
                void reset();

                /**
                 * @brief Parse some data. The enum return value is good when a complete request has
                 *  been parsed, bad if the data is invalid, indeterminate when more data is
                 *  required. The InputIterator return value indicates how much of the input
                 *  has been consumed.
                 * @tparam InputIterator
                 * @param payload HTTP request payload.
                 * @param begin 
                 * @param end 
                 * @returns std::tuple<ResultType, InputIterator> 
                 */
                template <typename InputIterator>
                std::tuple<ResultType, InputIterator> parse(HTTPPayload& payload, InputIterator begin, InputIterator end)
                {
                    while (begin != end) {
                        ResultType result = consume(payload, *begin++);
                        if (result == GOOD || result == BAD)
                            return std::make_tuple(result, begin);
                    }
                    return std::make_tuple(INDETERMINATE, begin);
                }

                /**
                 * @brief Returns flag indicating whether or not characters have been consumed from the payload.
                 * @returns True, if characters were consumed, otherwise false.
                 */
                uint32_t consumed() const { return m_consumed; }

            private:
                /**
                 * @brief Handle the next character of input.
                 * @param payload HTTP request payload.
                 * @param input Character.
                 */
                ResultType consume(HTTPPayload& payload, char input);

                /**
                 * @brief Check if a byte is an HTTP character.
                 * @param c Character.
                 * @returns bool True, if character is an HTTP character, otherwise false.
                 */
                static bool isChar(int c);
                /**
                 * @brief Check if a byte is an HTTP control character.
                 * @param c Character.
                 * @returns bool True, if character is an HTTP control character, otherwise false.
                 */
                static bool isControl(int c);
                /**
                 * @brief Check if a byte is an HTTP special character.
                 * @param c Character.
                 * @returns bool True, if character is an HTTP special character, otherwise false.
                 */
                static bool isSpecial(int c);
                /**
                 * @brief Check if a byte is an digit.
                 * @param c Character.
                 * @returns bool True, if character is a digit, otherwise false.
                 */
                static bool isDigit(int c);

                /**
                 * @brief Structure representing lexed HTTP headers.
                 */
                struct LexedHeader
                {
                    /**
                     * @brief Header name.
                     */
                    std::string name;
                    /**
                     * @brief Header value.
                     */
                    std::string value;

                    /**
                     * @brief Initializes a new instance of the LexedHeader class
                     */
                    LexedHeader() { /* stub */ }
                    /**
                     * @brief Initializes a new instance of the LexedHeader class
                     * @param n Header name.
                     * @param v Header value.
                     */
                    LexedHeader(const std::string& n, const std::string& v) : name(n), value(v) {}
                };

                std::vector<LexedHeader> m_headers;
                uint16_t m_status;
                bool m_clientLexer = false;
                uint32_t m_consumed;

                /**
                 * @brief Lexer machine state.
                 */
                enum state
                {
                    METHOD_START,               //! HTTP Method Start
                    METHOD,                     //! HTTP Method
                    URI,                        //! HTTP URI

                    HTTP_VERSION_H,             //! HTTP Version: H
                    HTTP_VERSION_T_1,           //! HTTP Version: T
                    HTTP_VERSION_T_2,           //! HTTP Version: T
                    HTTP_VERSION_P,             //! HTTP Version: P
                    HTTP_VERSION_SLASH,         //! HTTP Version: /
                    HTTP_VERSION_MAJOR_START,   //! HTTP Version Major Start
                    HTTP_VERSION_MAJOR,         //! HTTP Version Major
                    HTTP_VERSION_MINOR_START,   //! HTTP Version Minor Start
                    HTTP_VERSION_MINOR,         //! HTTP Version Minor

                    HTTP_STATUS_1,              //! Status Number 1
                    HTTP_STATUS_2,              //! Status Number 2
                    HTTP_STATUS_3,              //! Status Number 3
                    HTTP_STATUS_END,            //! Status End
                    HTTP_STATUS_MESSAGE_START,  //! Status Message Start
                    HTTP_STATUS_MESSAGE,        //! Status Message End

                    EXPECTING_NEWLINE_1,        //!

                    HEADER_LINE_START,          //! Header Line Start
                    HEADER_LWS,                 //!
                    HEADER_NAME,                //! Header Name
                    SPACE_BEFORE_HEADER_VALUE,  //!
                    HEADER_VALUE,               //! Header Value

                    EXPECTING_NEWLINE_2,        //!
                    EXPECTING_NEWLINE_3         //!
                } m_state;
            };
        } // namespace http
    } // namespace rest
} // namespace network

#endif // __REST_HTTP__HTTP_LEXER_H__
