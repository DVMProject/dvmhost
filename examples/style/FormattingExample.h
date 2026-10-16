// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Formatting and Style Example
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 DVMProject Authors
 *
 */
/**
 * @defgroup style_example Formatting and Style Example
 * @brief Demonstrates the expected organization and documentation of project-owned C++ files.
 * @ingroup common
 *
 * @file FormattingExample.h
 * @ingroup style_example
 * @file FormattingExample.cpp
 * @ingroup style_example
 */
#if !defined(__FORMATTING_EXAMPLE_H__)
#define __FORMATTING_EXAMPLE_H__

#include "common/Defines.h"

#include <vector>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

namespace style_example { class FormattingObserver; }

namespace style_example
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    /**
     * @addtogroup style_example
     * @{
     */

    /** @brief Number of bytes in the fixed example-frame header. */
    const uint32_t EXAMPLE_HEADER_LENGTH_BYTES = 6U;

    /** @brief Maximum permitted example-frame payload length in bytes. */
    const uint32_t EXAMPLE_MAX_PAYLOAD_LENGTH_BYTES = 64U;

    /** @brief Protocol version encoded by this example. */
    const uint8_t EXAMPLE_PROTOCOL_VERSION = 1U;

    /** @} */

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents decoded metadata from an example frame.
     * @ingroup style_example
     */
    struct FormattingFrameInfo {
        uint16_t sessionId;    //!< Session identifier carried by the frame.
        uint16_t payloadLength; //!< Payload length in bytes.
        bool response;         //!< Flag indicating that the frame is a response.
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Encodes and decodes a small frame used to demonstrate project style.
     * @ingroup style_example
     */
    class FormattingExample {
    public:
        /**
         * @brief Initializes a new instance of the FormattingExample class.
         * @param[in] observer Optional non-owning observer associated with the codec.
         */
        explicit FormattingExample(FormattingObserver* observer = nullptr);
        /**
         * @brief Finalizes an instance of the FormattingExample class.
         */
        ~FormattingExample();

        /**
         * @brief Decodes an example frame and updates the current state.
         * @param[in] data Buffer containing the encoded frame.
         * @param length Available encoded-frame length in bytes.
         * @returns bool True if the complete frame was valid; otherwise, false.
         */
        bool decode(const uint8_t* data, uint32_t length);
        /**
         * @brief Encodes the current state as an example frame.
         * @param[out] length Encoded-frame length in bytes.
         * @returns UInt8Array Owned encoded frame, or nullptr when the current state is invalid.
         */
        UInt8Array encode(uint32_t* length) const;

        /**
         * @brief Replaces the current payload.
         * @param[in] data Buffer containing payload bytes.
         * @param length Payload length in bytes.
         * @returns bool True if the payload was accepted; otherwise, false.
         */
        bool setPayload(const uint8_t* data, uint32_t length);
        /**
         * @brief Clears all decoded and configured frame state.
         */
        void reset();

    public:
        /**
         * @brief Session identifier encoded in the example frame.
         */
        DECLARE_PROPERTY(uint16_t, sessionId, SessionId);
        /**
         * @brief Flag indicating whether the example frame is a response.
         */
        DECLARE_PROPERTY(bool, response, Response);
        /**
         * @brief Flag indicating whether the current frame state is valid.
         */
        DECLARE_RO_PROPERTY(bool, valid, Valid);

    private:
        /**
         * @brief Identifies the current processing state of the example codec.
         * @ingroup style_example
         */
        enum PROCESSING_STATE {
            STATE_IDLE = 0U,       //!< No valid frame is available.
            STATE_DECODED = 1U,    //!< A valid frame has been decoded.
            STATE_CONFIGURED = 2U  //!< Frame state was configured locally.
        };

        /**
         * @brief Represents the payload state owned by the example codec.
         * @ingroup style_example
         */
        struct PayloadState {
            std::vector<uint8_t> data; //!< Current payload bytes.
            uint32_t revision;         //!< Number of accepted payload replacements.
        };

        /** @brief Optional non-owning observer associated with this instance. */
        FormattingObserver* m_observer;
        /** @brief Current processing state. */
        PROCESSING_STATE m_state;
        /** @brief Current payload and its revision metadata. */
        PayloadState m_payload;

        /**
         * @brief Validates the fixed header and returns its decoded metadata.
         * @param[in] data Buffer containing the encoded frame.
         * @param length Available encoded-frame length in bytes.
         * @param[out] info Decoded fixed-header metadata.
         * @returns bool True if the header was valid; otherwise, false.
         */
        bool decodeHeader(const uint8_t* data, uint32_t length, FormattingFrameInfo& info) const;

        /**
         * @brief Declares the standard assignment operator and private copy helper.
         */
        DECLARE_COPY(FormattingExample);
    };
} // namespace style_example

#endif // __FORMATTING_EXAMPLE_H__
