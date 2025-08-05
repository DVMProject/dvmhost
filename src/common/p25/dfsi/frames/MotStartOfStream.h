// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file MotStartVoiceFrame.h
 * @ingroup dfsi_frames
 * @file MotStartVoiceFrame.cpp
 * @ingroup dfsi_frames
 */
#if !defined(__MOT_START_OF_STREAM_H__)
#define __MOT_START_OF_STREAM_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "common/p25/dfsi/frames/FrameDefines.h"

namespace p25
{
    namespace dfsi
    {
        namespace frames
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Implements a P25 Motorola start of stream packet.
             * \code{.unparsed}
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |       FT      |   ICW Format  |  Opcode       |  Param 1      |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |  Argment 1    |   Param 2     |  Argument 2   |  Param 3      |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |  Argment 3    |
             *     +-+-+-+-+-+-+-+-+
             * \endcode
             * @ingroup dfsi_frames
             */
            class HOST_SW_API MotStartOfStream {
            public:
                /**
                 * @brief Initializes a copy instance of the MotStartOfStream class.
                 */
                MotStartOfStream();
                /**
                 * @brief Initializes a copy instance of the MotStartOfStream class.
                 * @param data Buffer to containing MotStartOfStream to decode.
                 */
                MotStartOfStream(uint8_t* data);
                /**
                 * @brief Finalizes a instance of the MotStartOfStream class.
                 */
                ~MotStartOfStream();

                /**
                 * @brief Decode a start of stream frame.
                 * @param[in] data Buffer to containing MotStartOfStream to decode.
                 */
                bool decode(const uint8_t* data);
                /**
                 * @brief Encode a start of stream frame.
                 * @param[out] data Buffer to encode a MotStartOfStream.
                 */
                void encode(uint8_t* data);

                /** @name Start of Stream Type 3 control word parameters. */
                /**
                 * @brief Helper to get parameter 1 of the control word.
                 * @return uint8_t Parameter 1 of the control word.
                 */
                uint8_t getParam1() const { return icw[0U]; }
                /**
                 * @brief Helper to get the argument for parameter 1 of the control word.
                 * @return uint8_t Argument 1 for parameter 1 of the control word.
                 */
                uint8_t getArgument1() const { return icw[1U]; }
                /**
                 * @brief Helper to set parameter 1 of the control word.
                 * @param value Parameter 1 of the control word.
                 */
                void setParam1(uint8_t value) { icw[0U] = value; }
                /**
                 * @brief Helper to set the argument for parameter 1 of the control word.
                 * @param value Argument 1 for parameter 1 of the control word.
                 */
                void setArgument1(uint8_t value) { icw[1U] = value; }

                /**
                 * @brief Helper to get parameter 2 of the control word.
                 * @return uint8_t Parameter 2 of the control word.
                 */
                uint8_t getParam2() const { return icw[2U]; }
                /**
                 * @brief Helper to get the argument for parameter 2 of the control word.
                 * @return uint8_t Argument 2 for parameter 2 of the control word.
                 */
                uint8_t getArgument2() const { return icw[3U]; }
                /**
                 * @brief Helper to set parameter 1 of the control word.
                 * @param value Parameter 1 of the control word.
                 */
                void setParam2(uint8_t value) { icw[2U] = value; }
                /**
                 * @brief Helper to set the argument for parameter 2 of the control word.
                 * @param value Argument for parameter 2 of the control word.
                 */
                void setArgument2(uint8_t value) { icw[3U] = value; }

                /**
                 * @brief Helper to get parameter 3 of the control word.
                 * @return uint8_t Parameter 3 of the control word.
                 */
                uint8_t getParam3() const { return icw[4U]; }
                /**
                 * @brief Helper to get argument 3 for parameter 3 of the control word.
                 * @return uint8_t Argument for parameter 3 of the control word.
                 */
                uint8_t getArgument3() const { return icw[5U]; }
                /**
                 * @brief Helper to set parameter 3 of the control word.
                 * @param value Parameter 3 of the control word.
                 */
                void setParam3(uint8_t value) { icw[4U] = value; }
                /**
                 * @brief Helper to set the argument for parameter 3 of the control word.
                 * @param value Argument for parameter 3 of the control word.
                 */
                void setArgument3(uint8_t value) { icw[5U] = value; }
                /** @} */

                /**
                 * @brief Get the raw ICW parameter/argument buffer.
                 * @return uint8_t* Raw ICW buffer.
                 * @note The buffer is 6 bytes long and contains the parameters and arguments for the
                 *      start of stream control word.
                 */ 
                uint8_t* getICW() const { return icw; }

            public:
                /**
                 * @brief Format.
                 */
                DECLARE_PROPERTY(uint8_t, format, Format);
                /**
                 * @brief Opcode.
                 */
                DECLARE_PROPERTY(MotStartStreamOpcode::E, opcode, Opcode);

            private:
                uint8_t* icw;
            };
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __MOT_START_OF_STREAM_H__