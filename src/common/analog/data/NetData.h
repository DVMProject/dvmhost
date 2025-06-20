// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file NetData.h
 * @ingroup analog
 * @file NetData.cpp
 * @ingroup analog
 */
#if !defined(__ANALOG_DATA__NET_DATA_H__)
#define __ANALOG_DATA__NET_DATA_H__

#include "common/Defines.h"
#include "common/analog/AnalogDefines.h"

namespace analog
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents network analog data. When setting audio data, it is expected to be 20ms of 16-bit 
         *  audio at 8kHz, or 320 bytes in length.
         * @ingroup analog
         */
        class HOST_SW_API NetData {
        public:
            /**
             * @brief Initializes a new instance of the NetData class.
             * @param data Instance of NetData class to copy from.
             */
            NetData(const NetData& data);
            /**
             * @brief Initializes a new instance of the NetData class.
             */
            NetData();
            /**
             * @brief Finalizes a instance of the NetData class.
             */
            ~NetData();

            /**
             * @brief Equals operator.
             * @param data Instance of NetData class to copy from.
             */
            NetData& operator=(const NetData& data);

            /**
             * @brief Sets audio data.
             * @param[in] buffer Audio data buffer.
             */
            void setAudio(const uint8_t* buffer);
            /**
             * @brief Gets audio data.
             * @param[out] buffer Audio data buffer.
             */
            uint32_t getAudio(uint8_t* buffer) const;

        public:
            /**
             * @brief Source ID.
             */
            DECLARE_PROPERTY(uint32_t, srcId, SrcId);
            /**
             * @brief Destination ID.
             */
            DECLARE_PROPERTY(uint32_t, dstId, DstId);

            /**
             * @brief 
             */
            DECLARE_PROPERTY(uint8_t, control, Control);

            /**
             * @brief Sequence number.
             */
            DECLARE_PROPERTY(uint8_t, seqNo, SeqNo);

            /**
             * @brief Flag indicatin if this group audio or individual audio.
             */
            DECLARE_PROPERTY(bool, group, Group);

            /**
             * @brief Audio frame type.
             */
            DECLARE_PROPERTY(defines::AudioFrameType::E, frameType, FrameType);

        private:
            uint8_t* m_audio;
        };
    } // namespace data
} // namespace analog

#endif // __ANALOG_DATA__NET_DATA_H__
