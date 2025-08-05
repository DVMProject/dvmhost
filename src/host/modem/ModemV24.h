// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
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
#include "common/p25/lc/LC.h"
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
        STT_NO_DATA,                        //! No Data
        STT_NON_IMBE,                       //! Non-IMBE Data/Signalling Frame
        STT_NON_IMBE_NO_JITTER,             //! Non-IMBE Data/Signalling Frame with Jitter Disabled
        STT_IMBE                            //! IMBE Voice Frame
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
            VHDR1 = new uint8_t[P25DFSIDEF::DFSI_TIA_VHDR_LEN];
            VHDR2 = new uint8_t[P25DFSIDEF::DFSI_TIA_VHDR_LEN];
            LDULC = new uint8_t[P25DEF::P25_LDU_LC_FEC_LENGTH_BYTES];

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
            if (LDULC != nullptr)
                delete[] LDULC;
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
                ::memset(VHDR1, 0x00U, P25DFSIDEF::DFSI_TIA_VHDR_LEN);
            if (VHDR2 != nullptr)
                ::memset(VHDR2, 0x00U, P25DFSIDEF::DFSI_TIA_VHDR_LEN);

            if (LDULC != nullptr)
                ::memset(LDULC, 0x00U, P25DEF::P25_LDU_LC_FEC_LENGTH_BYTES);

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
         * @brief LDU LC.
         */
        uint8_t* LDULC;

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
     * \code{.unparsed}
     * This is the format of the Motorola V.24 DFSI Voice Headers:
     * 
     * Voice Header 1 (VHDR1):
     * Bit  7 6 5 4 3 2 1 0
     *     +-+-+-+-+-+-+-+-+
     *     |       FT      | 0
     *     +-+-+-+-+-+-+-+-+
     *     | Start of Strm | 1 - 9
     *     +-+-+-+-+-+-+-+-+
     *     |S0 |  G0       | 10
     *     +-+-+-+-+-+-+-+-+
     *     |S1 |  G1       | 11
     *     +-+-+-+-+-+-+-+-+
     *     |S2 |  G2       | 12
     *     +-+-+-+-+-+-+-+-+
     *     |S3 |  G3       | 13
     *     +-+-+-+-+-+-+-+-+
     *     |S4 |  G4       | 14
     *     +-+-+-+-+-+-+-+-+
     *     |S5 |  G5       | 15
     *     +-+-+-+-+-+-+-+-+
     *     |S6 |  G6       | 16
     *     +-+-+-+-+-+-+-+-+
     *     |S7 |  G7       | 17
     *     +-+-+-+-+-+-+-+-+
     *     |  S7...S0      | 18
     *     +-+-+-+-+-+-+-+-+
     *     |S8 |  G8       | 19
     *     +-+-+-+-+-+-+-+-+
     *     |S9 |  G9       | 20
     *     +-+-+-+-+-+-+-+-+
     *     |S10|  G10      | 21
     *     +-+-+-+-+-+-+-+-+
     *     |S11|  G11      | 22
     *     +-+-+-+-+-+-+-+-+
     *     |S12|  G12      | 23
     *     +-+-+-+-+-+-+-+-+
     *     |S13|  G13      | 24
     *     +-+-+-+-+-+-+-+-+
     *     |S14|  G14      | 25
     *     +-+-+-+-+-+-+-+-+
     *     |S15|  G15      | 26
     *     +-+-+-+-+-+-+-+-+
     *     |  S15...S8     | 27
     *     +-+-+-+-+-+-+-+-+
     *     |S16|  G16      | 26
     *     +-+-+-+-+-+-+-+-+
     *     |S17|  G17      | 28
     *     +-+-+-+-+-+-+-+-+
     *     |7|6| Rsvd  |Bsy| 29
     *     +-+-+-+-+-+-+-+-+
     *     |  Rsvd         | 30
     *     +-+-+-+-+-+-+-+-+
     *
     * Voice Header 2 (VHDR2):
     * Bit  7 6 5 4 3 2 1 0
     *     +-+-+-+-+-+-+-+-+
     *     |       FT      | 0
     *     +-+-+-+-+-+-+-+-+
     *     |S18|  G18      | 1
     *     +-+-+-+-+-+-+-+-+
     *     |S19|  G19      | 2
     *     +-+-+-+-+-+-+-+-+
     *     |S20|  G20      | 3
     *     +-+-+-+-+-+-+-+-+
     *     |S21|  G21      | 4
     *     +-+-+-+-+-+-+-+-+
     *     |S22|  G22      | 5
     *     +-+-+-+-+-+-+-+-+
     *     |S23|  G23      | 6
     *     +-+-+-+-+-+-+-+-+
     *     |S24|  G24      | 7
     *     +-+-+-+-+-+-+-+-+
     *     |S25|  G25      | 8
     *     +-+-+-+-+-+-+-+-+
     *     |  S25...S18    | 9
     *     +-+-+-+-+-+-+-+-+
     *     |S26|  G26      | 10
     *     +-+-+-+-+-+-+-+-+
     *     |S27|  G27      | 11
     *     +-+-+-+-+-+-+-+-+
     *     |S28|  G28      | 12
     *     +-+-+-+-+-+-+-+-+
     *     |S29|  G29      | 13
     *     +-+-+-+-+-+-+-+-+
     *     |S30|  G30      | 14
     *     +-+-+-+-+-+-+-+-+
     *     |S31|  G31      | 15
     *     +-+-+-+-+-+-+-+-+
     *     |S32|  G32      | 16
     *     +-+-+-+-+-+-+-+-+
     *     |S33|  G33      | 17
     *     +-+-+-+-+-+-+-+-+
     *     |  S33...S26    | 18
     *     +-+-+-+-+-+-+-+-+
     *     |S34|  G34      | 19
     *     +-+-+-+-+-+-+-+-+
     *     |S35|  G35      | 20
     *     +-+-+-+-+-+-+-+-+
     *     |5|4| Rsvd  |Bsy| 21
     *     +-+-+-+-+-+-+-+-+
     *
     * This is the format of the TIA-102.BAHA DFSI Voice Headers:
     * 
     * TIA-102.BAHA Voice Header 1 (VHDR1):
     * Bit  7 6 5 4 3 2 1 0
     *     +-+-+-+-+-+-+-+-+
     *     |       FT      | 0
     *     +-+-+-+-+-+-+-+-+
     *     |S0 |  G0       | 1
     *     +-+-+-+-+-+-+-+-+
     *     |S1 |  G1       | 2
     *     +-+-+-+-+-+-+-+-+
     *     |S2 |  G2       | 3
     *     +-+-+-+-+-+-+-+-+
     *     |S3 |  G3       | 4
     *     +-+-+-+-+-+-+-+-+
     *     |S4 |  G4       | 5
     *     +-+-+-+-+-+-+-+-+
     *     |S5 |  G5       | 6
     *     +-+-+-+-+-+-+-+-+
     *     |S6 |  G6       | 7
     *     +-+-+-+-+-+-+-+-+
     *     |S7 |  G7       | 8
     *     +-+-+-+-+-+-+-+-+
     *     |S8 |  G8       | 9
     *     +-+-+-+-+-+-+-+-+
     *     |S9 |  G9       | 10
     *     +-+-+-+-+-+-+-+-+
     *     |S10|  G10      | 11
     *     +-+-+-+-+-+-+-+-+
     *     |S11|  G11      | 12
     *     +-+-+-+-+-+-+-+-+
     *     |S12|  G12      | 13
     *     +-+-+-+-+-+-+-+-+
     *     |S13|  G13      | 14
     *     +-+-+-+-+-+-+-+-+
     *     |S14|  G14      | 15
     *     +-+-+-+-+-+-+-+-+
     *     |S15|  G15      | 16
     *     +-+-+-+-+-+-+-+-+
     *     |S16|  G16      | 17
     *     +-+-+-+-+-+-+-+-+
     *     |S17|  G17      | 18
     *     +-+-+-+-+-+-+-+-+
     *     | Rsvd      |7|6| 19
     *     +-+-+-+-+-+-+-+-+
     *     |  S15...S0     | 20
     *     +-+-+-+-+-+-+-+-+
     *     |  S15...S0     | 21
     *     +-+-+-+-+-+-+-+-+
     *
     * TIA-102.BAHA Voice Header 2 (VHDR2):
     * Bit  7 6 5 4 3 2 1 0
     *     +-+-+-+-+-+-+-+-+
     *     |       FT      | 0
     *     +-+-+-+-+-+-+-+-+
     *     |S18|  G18      | 1
     *     +-+-+-+-+-+-+-+-+
     *     |S19|  G19      | 2 
     *     +-+-+-+-+-+-+-+-+
     *     |S20|  G20      | 3
     *     +-+-+-+-+-+-+-+-+
     *     |S21|  G21      | 4
     *     +-+-+-+-+-+-+-+-+
     *     |S22|  G22      | 5
     *     +-+-+-+-+-+-+-+-+
     *     |S23|  G23      | 6
     *     +-+-+-+-+-+-+-+-+
     *     |S24|  G24      | 7
     *     +-+-+-+-+-+-+-+-+
     *     |S25|  G25      | 8
     *     +-+-+-+-+-+-+-+-+
     *     |S26|  G26      | 9
     *     +-+-+-+-+-+-+-+-+
     *     |S27|  G27      | 10
     *     +-+-+-+-+-+-+-+-+
     *     |S28|  G28      | 11
     *     +-+-+-+-+-+-+-+-+
     *     |S29|  G29      | 12
     *     +-+-+-+-+-+-+-+-+
     *     |S30|  G30      | 13
     *     +-+-+-+-+-+-+-+-+
     *     |S31|  G31      | 14
     *     +-+-+-+-+-+-+-+-+ 
     *     |S32|  G32      | 15
     *     +-+-+-+-+-+-+-+-+
     *     |S33|  G33      | 16
     *     +-+-+-+-+-+-+-+-+
     *     |S34|  G34      | 17
     *     +-+-+-+-+-+-+-+-+
     *     |S35|  G35      | 18
     *     +-+-+-+-+-+-+-+-+
     *     | Rsvd      |5|4| 19
     *     +-+-+-+-+-+-+-+-+
     *     |  S33...S18    | 20
     *     +-+-+-+-+-+-+-+-+
     *     |  S33...S18    | 21
     *     +-+-+-+-+-+-+-+-+
     * \endcode
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
         * @brief Helper to set the TIA-102 format DFSI frame flag.
         * @param set 
         */
        void setTIAFormat(bool set);

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
         * @brief Helper to test if the P25 ring buffer has free space.
         * @returns bool True, if the P25 ring buffer has free space, otherwise false.
         */
        bool hasP25Space(uint32_t length) const override;

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

        uint8_t m_superFrameCnt;

        p25::Audio m_audio;

        p25::NID* m_nid;

        RingBuffer<uint8_t> m_txP25Queue;

        DFSICallData* m_txCall;
        DFSICallData* m_rxCall;
        bool m_txCallInProgress;
        bool m_rxCallInProgress;
        uint64_t m_txLastFrameTime;
        uint64_t m_rxLastFrameTime;
        
        uint16_t m_callTimeout;

        uint16_t m_jitter;
        uint64_t m_lastP25Tx;

        edac::RS634717 m_rs;

        bool m_useTIAFormat;

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
        void convertToAirV24(const uint8_t *data, uint32_t length);
        /**
         * @brief Internal helper to convert from TIA-102 DFSI to TIA-102 air interface.
         * @param data Buffer containing data to convert.
         * @param length Length of buffer.
         */
        void convertToAirTIA(const uint8_t *data, uint32_t length);

        /**
         * @brief Helper to add a V.24 data frame to the P25 Tx queue with the proper timestamp and formatting.
         * @param data Buffer containing V.24 data frame to send.
         * @param len Length of buffer.
         * @param msgType Type of message to send (used for proper jitter clocking).
         */
        void queueP25Frame(uint8_t* data, uint16_t length, SERIAL_TX_TYPE msgType);

        /**
         * @brief Send a start of stream sequence (HDU, etc) to the connected serial V24 device.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         */
        void startOfStreamV24(const p25::lc::LC& control);
        /**
         * @brief Send an end of stream sequence (TDU, etc) to the connected serial V24 device.
         */
        void endOfStreamV24();

        /**
         * @brief Helper to generate the NID value.
         * @param duid P25 DUID.
         * @returns uint16_t P25 NID.
         */
        uint16_t generateNID(P25DEF::DUID::E duid = P25DEF::DUID::LDU1);

        /**
         * @brief Send a start of stream sequence (HDU, etc) to the connected UDP TIA-102 device.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         */
        void startOfStreamTIA(const p25::lc::LC& control);
        /**
         * @brief Send an end of stream sequence (TDU, etc) to the connected UDP TIA-102 device.
         */
        void endOfStreamTIA();
        /**
         * @brief Send a start of stream ACK.
         */
        void ackStartOfStreamTIA();

        /**
         * @brief Internal helper to convert from TIA-102 air interface to V.24/DFSI.
         * @param data Buffer containing data to convert.
         * @param length Length of buffer.
         */
        void convertFromAirV24(uint8_t* data, uint32_t length);
        /**
         * @brief Internal helper to convert from TIA-102 air interface to TIA-102 DFSI.
         * @param data Buffer containing data to convert.
         * @param length Length of buffer.
         */
        void convertFromAirTIA(uint8_t* data, uint32_t length);
    };
} // namespace modem

#endif // __MODEM_V24_H__
