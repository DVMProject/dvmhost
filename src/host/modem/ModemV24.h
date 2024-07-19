// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file ModemV24.h
 * @ingroup modem
 * @file ModemV24.cpp
 * @ingroup modem
 */
#if !defined(__MODEM_V24_H__)
#define __MODEM_V24_H__

#include "Defines.h"
#include "common/edac/RS634717.h"
#include "common/p25/dfsi/frames/MotVoiceHeader1.h"
#include "common/p25/dfsi/frames/MotVoiceHeader2.h"
#include "common/p25/Audio.h"
#include "common/p25/NID.h"
#include "modem/Modem.h"

namespace modem
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    /**
     * @addtogroup modem
     * @{
     */

    /**
     * @brief DFSI serial tx flags used to determine proper jitter handling of data in ringbuffer.
     * @ingroup modem
     */
    enum SERIAL_TX_TYPE {
        NONIMBE,                            //! Non-IMBE Data/Signalling Frame
        IMBE                                //! IMBE Voice Frame
    };

    /** @} */

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents DFSI call data.
     * @ingroup modem
     */
    class HOST_SW_API DFSICallData {
    public:
        auto operator=(DFSICallData&) -> DFSICallData& = delete;
        auto operator=(DFSICallData&&) -> DFSICallData& = delete;
        DFSICallData(DFSICallData&) = delete;

        /**
         * @brief Initializes a new instance of the DFSICallData class.
         */
        DFSICallData() :
            srcId(0U),
            dstId(0U),
            lco(0U),
            mfId(P25DEF::MFG_STANDARD),
            serviceOptions(0U),
            lsd1(0U),
            lsd2(0U),
            MI(nullptr),
            algoId(P25DEF::ALGO_UNENCRYPT),
            kId(0U),
            VHDR1(nullptr),
            VHDR2(nullptr),
            netLDU1(nullptr),
            netLDU2(nullptr)
        {
            MI = new uint8_t[P25DEF::MI_LENGTH_BYTES];
            VHDR1 = new uint8_t[p25::dfsi::frames::MotVoiceHeader1::HCW_LENGTH];
            VHDR2 = new uint8_t[p25::dfsi::frames::MotVoiceHeader2::HCW_LENGTH];

            netLDU1 = new uint8_t[9U * 25U];
            netLDU2 = new uint8_t[9U * 25U];

            ::memset(netLDU1, 0x00U, 9U * 25U);
            ::memset(netLDU2, 0x00U, 9U * 25U);

            resetCallData();
        }
        /**
         * @brief Finalizes a instance of the DFSICallData class.
         */
        ~DFSICallData()
        {
            if (MI != nullptr)
                delete[] MI;
            if (VHDR1 != nullptr)
                delete[] VHDR1;
            if (VHDR2 != nullptr)
                delete[] VHDR2;
            if (netLDU1 != nullptr)
                delete[] netLDU1;
            if (netLDU2 != nullptr)
                delete[] netLDU2;
        }

        /**
         * @brief Helper to reset the call data associated with this connection.
         */
        void resetCallData()
        {
            srcId = 0U;
            dstId = 0U;

            lco = 0U;
            mfId = P25DEF::MFG_STANDARD;
            serviceOptions = 0U;

            lsd1 = 0U;
            lsd2 = 0U;

            if (MI != nullptr)
                ::memset(MI, 0x00U, P25DEF::MI_LENGTH_BYTES);
            algoId = P25DEF::ALGO_UNENCRYPT;
            kId = 0U;

            if (VHDR1 != nullptr)
                ::memset(VHDR1, 0x00U, p25::dfsi::frames::MotVoiceHeader1::HCW_LENGTH);
            if (VHDR2 != nullptr)
                ::memset(VHDR2, 0x00U, p25::dfsi::frames::MotVoiceHeader2::HCW_LENGTH);

            if (netLDU1 != nullptr)
                ::memset(netLDU1, 0x00U, 9U * 25U);
            if (netLDU2 != nullptr)
                ::memset(netLDU2, 0x00U, 9U * 25U);

            n = 0U;
            seqNo = 0U;
        }

    public:
        /** @name Call Data */
        /** 
         * @brief Source Radio ID.
         */
        uint32_t srcId;
        /** 
         * @brief Destination ID.
         */
        uint32_t dstId;

        /**
         * @brief Link Control Opcode.
         */
        uint8_t lco;
        /**
         * @brief Manufacturer ID.
         */
        uint8_t mfId;
        /**
         * @brief Call Service Options.
         */
        uint8_t serviceOptions;

        /**
         * @brief Low Speed Data 1.
         */
        uint8_t lsd1;
        /**
         * @brief Low Speed Data 2.
         */
        uint8_t lsd2;

        /**
         * @brief Encryption Message Indicator.
         */
        uint8_t* MI;
        /**
         * @brief Encryption Algorithm ID.
         */
        uint8_t algoId;
        /**
         * @brief Encryption Key ID.
         */
        uint32_t kId;

        /**
         * @brief Voice Header 1.
         */
        uint8_t* VHDR1;
        /**
         * @brief Voice Header 2.
         */
        uint8_t* VHDR2;

        /**
         * @brief Sequence Number.
         */
        uint32_t seqNo;
        /**
         * @brief 
         */
        uint8_t n;

        /**
         * @brief LDU1 Buffer.
         */
        uint8_t* netLDU1;
        /**
         * @brief LDU2 Buffer.
         */
        uint8_t* netLDU2;
        /** @} */
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the core interface to the V.24 modem hardware.
     * @ingroup modem
     */
    class HOST_SW_API ModemV24 : public Modem {
    public:
        /**
         * @brief Initializes a new instance of the ModemV24 class.
         * @param port Port the air interface modem is connected to.
         * @param duplex Flag indicating the modem is operating in duplex mode.
         * @param p25QueueSize Modem P25 Rx frame buffer queue size (bytes).
         * @param p25TxQueueSize Modem P25 Tx frame buffer queue size (bytes).
         * @param rtrt Flag indicating whether or not RT/RT is enabled.
         * @param diu Flag indicating whether or not V.24 communications are to a DIU.
         * @param jitter 
         * @param dumpModemStatus Flag indicating whether the modem status is dumped to the log.
         * @param trace Flag indicating whether air interface modem trace is enabled.
         * @param debug Flag indicating whether air interface modem debug is enabled.
         */
        ModemV24(port::IModemPort* port, bool duplex, uint32_t p25QueueSize, uint32_t p25TxQueueSize,
            bool rtrt, bool diu, uint16_t jitter, bool dumpModemStatus, bool trace, bool debug);
        /**
         * @brief Finalizes a instance of the ModemV24 class.
         */
        ~ModemV24();

        /**
         * @brief Sets the call timeout.
         * @param timeout Timeout.
         */
        void setCallTimeout(uint16_t timeout);
        /**
         * @brief Sets the P25 NAC.
         * @param nac NAC.
         */
        void setP25NAC(uint32_t nac) override;

        /**
         * @brief Opens connection to the air interface modem.
         * @returns bool True, if connection to modem is made, otherwise false.
         */
        bool open() override;

        /**
         * @brief Updates the modem by the passed number of milliseconds.
         * @param ms Number of milliseconds.
         */
        void clock(uint32_t ms) override;

        /**
         * @brief Closes connection to the air interface modem.
         */
        void close() override;

        /**
         * @brief Writes raw data to the air interface modem.
         * @param data Data to write to modem.
         * @param length Length of data to write.
         * @returns int Actual length of data written.
         */
        int write(const uint8_t* data, uint32_t length) override;

    private:
        bool m_rtrt;
        bool m_diu;

        p25::Audio m_audio;

        p25::NID m_nid;

        RingBuffer<uint8_t> m_txP25Queue;

        DFSICallData* m_call;
        bool m_callInProgress;
        uint64_t m_lastFrameTime;
        uint16_t m_callTimeout;

        uint16_t m_jitter;
        uint64_t m_lastP25Tx;

        edac::RS634717 m_rs;

        /**
         * @brief Helper to write data from the P25 Tx queue to the serial interface.
         * @return int Actual number of bytes written to the serial interface.
         */
        int writeSerial();

        /**
         * @brief Helper to store converted Rx frames.
         * @param buffer Buffer containing converted Rx frame.
         * @param length Length of buffer.
         */
        void storeConvertedRx(const uint8_t* buffer, uint32_t length);
        /**
         * @brief Helper to generate a P25 TDU packet.
         * @param buffer Buffer to create TDU.
         */
        void create_TDU(uint8_t* buffer);

        /**
         * @brief Internal helper to convert from V.24/DFSI to TIA-102 air interface.
         * @param data Buffer containing data to convert.
         * @param length Length of buffer.
         */
        void convertToAir(const uint8_t *data, uint32_t length);

        /**
         * @brief Internal helper to convert from TIA-102 air interface to V.24/DFSI.
         * @param data Buffer containing data to convert.
         * @param length Length of buffer.
         * @returns SERIAL_TX_TYPE Transmit data type.
         */
        SERIAL_TX_TYPE convertFromAir(uint8_t* data, uint32_t length);
    };
} // namespace modem

#endif // __MODEM_V24_H__
