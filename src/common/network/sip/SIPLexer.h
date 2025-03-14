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
/**
 * @file SIPLexer.h
 * @ingroup sip
 * @file SIPLexer.cpp
 * @ingroup sip
 */
#if !defined(__SIP__SIP_LEXER_H__)
#define __SIP__SIP_LEXER_H__

#include "common/Defines.h"

#include <tuple>
#include <vector>

namespace network
{
    namespace sip
    {
        // ---------------------------------------------------------------------------
        //  Class Prototypes
        // ---------------------------------------------------------------------------

        struct SIPPayload;

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements the lexer for incoming payloads.
         */
        class SIPLexer {
        public:
            /**
             * @brief Lexing result.
             */
            enum ResultType { GOOD, BAD, INDETERMINATE, CONTINUE };

            /**
             * @brief Initializes a new instance of the SIPLexer class.
             * @param clientLexer Flag indicating this lexer is used for a SIP client.
             */
            SIPLexer(bool clientLexer);

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
             * @param payload SIP request payload.
             * @param begin 
             * @param end 
             * @returns std::tuple<ResultType, InputIterator> 
             */
            template <typename InputIterator>
            std::tuple<ResultType, InputIterator> parse(SIPPayload& payload, InputIterator begin, InputIterator end)
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
             * @param payload SIP request payload.
             * @param input Character.
             */
            ResultType consume(SIPPayload& payload, char input);

            /**
             * @brief Check if a byte is an SIP character.
             * @param c Character.
             * @returns bool True, if character is an SIP character, otherwise false.
             */
            static bool isChar(int c);
            /**
             * @brief Check if a byte is an SIP control character.
             * @param c Character.
             * @returns bool True, if character is an SIP control character, otherwise false.
             */
            static bool isControl(int c);
            /**
             * @brief Check if a byte is an SIP special character.
             * @param c Character.
             * @returns bool True, if character is an SIP special character, otherwise false.
             */
            static bool isSpecial(int c);
            /**
             * @brief Check if a byte is an digit.
             * @param c Character.
             * @returns bool True, if character is a digit, otherwise false.
             */
            static bool isDigit(int c);

            /**
             * @brief Structure representing lexed SIP headers.
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
                METHOD_START,               //! SIP Method Start
                METHOD,                     //! SIP Method
                URI,                        //! SIP URI

                SIP_VERSION_S,              //! SIP Version: S
                SIP_VERSION_I,              //! SIP Version: I
                SIP_VERSION_P,              //! SIP Version: P
                SIP_VERSION_SLASH,          //! SIP Version: /
                SIP_VERSION_MAJOR_START,    //! SIP Version Major Start
                SIP_VERSION_MAJOR,          //! SIP Version Major
                SIP_VERSION_MINOR_START,    //! SIP Version Minor Start
                SIP_VERSION_MINOR,          //! SIP Version Minor

                SIP_STATUS_1,               //! Status Number 1
                SIP_STATUS_2,               //! Status Number 2
                SIP_STATUS_3,               //! Status Number 3
                SIP_STATUS_END,             //! Status End
                SIP_STATUS_MESSAGE_START,   //! Status Message Start
                SIP_STATUS_MESSAGE,         //! Status Message End

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
    } // namespace sip
} // namespace network

#endif // __SIP__SIP_LEXER_H__
