// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2012 Ian Wraith
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2021,2023,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "dmr/DMRDefines.h"
#include "dmr/data/DataHeader.h"
#include "edac/BPTC19696.h"
#include "edac/CRC.h"
#include "Utils.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::data;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t UDTF_NMEA = 0x05U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the DataHeader class. */

DataHeader::DataHeader() :
    m_GI(false),
    m_A(false),
    m_DPF(DPF::UDT),
    m_sap(0U),
    m_fsn(0U),
    m_Ns(0U),
    m_padCount(0U),
    m_F(false),
    m_S(false),
    m_dataFormat(0U),
    m_srcId(0U),
    m_dstId(0U),
    m_blocks(0U),
    m_rspClass(PDUResponseClass::NACK),
    m_rspType(PDUResponseType::NACK_ILLEGAL),
    m_rspStatus(0U),
    m_srcPort(0U),
    m_dstPort(0U),
    m_data(nullptr),
    m_SF(false),
    m_PF(false),
    m_UDTO(0U)
{
    m_data = new uint8_t[DMR_LC_HEADER_LENGTH_BYTES];
}

/* Finalizes a instance of the DataHeader class. */

DataHeader::~DataHeader()
{
    delete[] m_data;
}

/* Equals operator. */

DataHeader& DataHeader::operator=(const DataHeader& header)
{
    if (&header != this) {
        m_GI = header.m_GI;
        m_DPF = header.m_DPF;
        m_sap = header.m_sap;
        m_fsn = header.m_fsn;
        m_Ns = header.m_Ns;
        m_padCount = header.m_padCount;
        m_F = header.m_F;
        m_S = header.m_S;
        m_dataFormat = header.m_dataFormat;
        m_srcId = header.m_srcId;
        m_dstId = header.m_dstId;
        m_blocks = header.m_blocks;
        m_rspClass = header.m_rspClass;
        m_rspType = header.m_rspType;
        m_rspStatus = header.m_rspStatus;
        m_srcPort = header.m_srcPort;
        m_dstPort = header.m_dstPort;

        ::memcpy(m_data, header.m_data, DMR_LC_HEADER_LENGTH_BYTES);
        m_A = header.m_A;
        m_SF = header.m_SF;
        m_PF = header.m_PF;
        m_UDTO = header.m_UDTO;
    }

    return *this;
}

/* Decodes a DMR data header. */

bool DataHeader::decode(const uint8_t* data)
{
    assert(data != nullptr);

    // decode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.decode(data, m_data);

    // make sure the CRC-CCITT 16 was actually included (the network tends to zero the CRC)
    if (m_data[10U] != 0x00U && m_data[11U] != 0x00U) {
        // validate the CRC-CCITT 16
        m_data[10U] ^= DATA_HEADER_CRC_MASK[0U];
        m_data[11U] ^= DATA_HEADER_CRC_MASK[1U];

        bool valid = edac::CRC::checkCCITT162(m_data, DMR_LC_HEADER_LENGTH_BYTES);
        if (!valid)
            return false;

        // restore the checksum
        m_data[10U] ^= DATA_HEADER_CRC_MASK[0U];
        m_data[11U] ^= DATA_HEADER_CRC_MASK[1U];
    }

    m_GI = (m_data[0U] & 0x80U) == 0x80U;                                       // Group/Individual Flag
    m_A = (m_data[0U] & 0x40U) == 0x40U;

    m_DPF = (DPF::E)(m_data[0U] & 0x0FU);                                       // Data Packet Format
    if (m_DPF == DPF::PROPRIETARY)
        return true;

    m_dstId = m_data[2U] << 16 | m_data[3U] << 8 | m_data[4U];                  // Destination ID
    m_srcId = m_data[5U] << 16 | m_data[6U] << 8 | m_data[7U];                  // Source ID

    switch (m_DPF) {
    case DPF::UDT:
#if DEBUG_DMR_PDU_DATA
        Utils::dump(1U, "DMR, DataHeader::decode(), Unified Data Transport Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
#endif
        m_sap = ((m_data[1U] & 0xF0U) >> 4);                                    // Service Access Point
        m_dataFormat = (m_data[1U] & 0x0FU);                                    // UDT Format
        m_blocks = (m_data[8U] & 0x03U) + 1U;                                   // Blocks To Follow
        m_padCount = (m_data[8U] & 0xF8U) >> 3;                                 // Pad Nibble
        m_SF = (m_data[9U] & 0x80U) == 0x80U;                                   // Supplemental Flag
        m_PF = (m_data[9U] & 0x40U) == 0x40U;                                   // Protect Flag
        m_UDTO = m_data[9U] & 0x3FU;                                            // UDT Opcode
        break;

    case DPF::UNCONFIRMED_DATA:
#if DEBUG_DMR_PDU_DATA
        Utils::dump(1U, "DMR, DataHeader::decode(), Unconfirmed Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
#endif
        m_sap = ((m_data[1U] & 0xF0U) >> 4);                                    // Service Access Point
        m_padCount = (m_data[0U] & 0x10U) + (m_data[1U] & 0x0FU);               // Octet Pad Count
        m_F = (m_data[8U] & 0x80U) == 0x80U;                                    // Full Message Flag
        m_blocks = m_data[8U] & 0x7FU;                                          // Blocks To Follow
        m_fsn = m_data[9U] & 0x0FU;                                             // Fragment Sequence Number
        break;

    case DPF::CONFIRMED_DATA:
#if DEBUG_DMR_PDU_DATA
        Utils::dump(1U, "DMR, DataHeader::decode(), Confirmed Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
#endif
        m_sap = ((m_data[1U] & 0xF0U) >> 4);                                    // Service Access Point
        m_padCount = (m_data[0U] & 0x10U) + (m_data[1U] & 0x0FU);               // Octet Pad Count
        m_F = (m_data[8U] & 0x80U) == 0x80U;                                    // Full Message Flag
        m_blocks = m_data[8U] & 0x7FU;                                          // Blocks To Follow
        m_S = (m_data[9U] & 0x80U) == 0x80U;                                    // Synchronize Flag
        m_Ns = (m_data[9U] >> 4) & 0x07U;                                       // Send Sequence Number
        m_fsn = m_data[9U] & 0x0FU;                                             // Fragement Sequence Number
        break;

    case DPF::RESPONSE:
#if DEBUG_DMR_PDU_DATA
        Utils::dump(1U, "DMR, DataHeader::decode(), Response Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
#endif
        m_sap = ((m_data[1U] & 0xF0U) >> 4);                                    // Service Access Point
        m_blocks = m_data[8U] & 0x7FU;                                          // Blocks To Follow
        m_rspClass = (m_data[9U] >> 6) & 0x03U;                                 // Response Class
        m_rspType = (m_data[9U] >> 3) & 0x07U;                                  // Response Type
        m_rspStatus = m_data[9U] & 0x07U;                                       // Response Status
        break;

    case DPF::DEFINED_SHORT:
#if DEBUG_DMR_PDU_DATA
        Utils::dump(1U, "DMR, DataHeader::decode(), Defined Short Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
#endif
        m_sap = ((m_data[1U] & 0xF0U) >> 4);                                    // Service Access Point
        m_blocks = (m_data[0U] & 0x30U) + (m_data[1U] & 0x0FU);                 // Blocks To Follow
        m_F = (m_data[8U] & 0x01U) == 0x01U;                                    // Full Message Flag
        m_S = (m_data[8U] & 0x02U) == 0x02U;                                    // Synchronize Flag
        m_dataFormat = (m_data[8U] & 0xFCU) >> 2;                               // Defined Data Format
        m_padCount = m_data[9U];                                                // Bit Padding
        break;

    case DPF::DEFINED_RAW:
#if DEBUG_DMR_PDU_DATA
        Utils::dump(1U, "DMR, DataHeader::decode(), Raw Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
#endif
        m_sap = ((m_data[1U] & 0xF0U) >> 4);                                    // Service Access Point
        m_blocks = (m_data[0U] & 0x30U) + (m_data[1U] & 0x0FU);                 // Blocks To Follow
        m_F = (m_data[8U] & 0x01U) == 0x01U;                                    // Full Message Flag
        m_S = (m_data[8U] & 0x02U) == 0x02U;                                    // Synchronize Flag
        m_dstPort = (m_data[8U] & 0x1CU) >> 2;                                  // Destination Port
        m_srcPort = (m_data[8U] & 0xE0U) >> 5;                                  // Source Port
        break;

    default:
        Utils::dump("DMR, Unknown Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
        break;
    }

    return true;
}

/* Encodes a DMR data header. */

void DataHeader::encode(uint8_t* data) const
{
    assert(data != nullptr);

    // perform no processing other then regenerating FEC
    if (m_DPF == DPF::PROPRIETARY) {
        m_data[10U] = m_data[11U] = 0x00U;

        // compute CRC-CCITT 16
        m_data[10U] ^= DATA_HEADER_CRC_MASK[0U];
        m_data[11U] ^= DATA_HEADER_CRC_MASK[1U];

        edac::CRC::addCCITT162(m_data, DMR_LC_HEADER_LENGTH_BYTES);

        // restore the checksum
        m_data[10U] ^= DATA_HEADER_CRC_MASK[0U];
        m_data[11U] ^= DATA_HEADER_CRC_MASK[1U];

        // encode BPTC (196,96) FEC
        edac::BPTC19696 bptc;
        bptc.encode(m_data, data);
        return;
    }
    else {
        ::memset(m_data, 0x00U, DMR_LC_HEADER_LENGTH_BYTES);
    }

    m_data[0U] = (m_GI ? 0x80U : 0x00U) +                                       // Group/Individual Flag
        (m_A ? 0x40U : 0x00U) +
        (m_DPF & 0x0F);                                                         // Data Packet Format

    m_data[2U] = (m_dstId >> 16) & 0xFFU;                                       // Destination ID
    m_data[3U] = (m_dstId >> 8) & 0xFFU;
    m_data[4U] = (m_dstId >> 0) & 0xFFU;
    m_data[5U] = (m_srcId >> 16) & 0xFFU;                                       // Source ID
    m_data[6U] = (m_srcId >> 8) & 0xFFU;
    m_data[7U] = (m_srcId >> 0) & 0xFFU;

    switch (m_DPF) {
    case DPF::UDT:
        m_data[1U] = ((m_sap & 0x0FU) << 4) +                                   // Service Access Point
            (m_dataFormat & 0x0FU);                                             // UDT Format
        m_data[8U] = ((m_padCount & 0x1FU) << 3) +                              // Pad Nibble
            (m_blocks - 1U);                                                    // Blocks To Follow
        m_data[9U] = (m_SF ? 0x80U : 0x00U) +                                   // Supplemental Flag
            (m_PF ? 0x40U : 0x00U) +                                            // Protect Flag
            (m_UDTO & 0x3F);                                                    // UDT Opcode
#if DEBUG_DMR_PDU_DATA
        Utils::dump(1U, "DMR, DataHeader::decode(), Unified Data Transport Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
#endif
        break;

    case DPF::UNCONFIRMED_DATA:
        m_data[0U] = m_data[0U] + (m_padCount & 0x10U);                         // Octet Pad Count MSB
        m_data[1U] = ((m_sap & 0x0FU) << 4) +                                   // Service Access Point
            (m_padCount & 0x0FU);                                               // Octet Pad Count LSB
        m_data[8U] = (m_F ? 0x80U : 0x00U) +                                    // Full Message Flag
            (m_blocks & 0x7FU);                                                 // Blocks To Follow
        m_data[9U] = m_fsn;                                                     // Fragment Sequence Number
#if DEBUG_DMR_PDU_DATA
        Utils::dump(1U, "DMR, DataHeader::decode(), Unconfirmed Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
#endif
        break;

    case DPF::CONFIRMED_DATA:
        m_data[0U] = m_data[0U] + (m_padCount & 0x10U);                         // Octet Pad Count MSB
        m_data[1U] = ((m_sap & 0x0FU) << 4) +                                   // Service Access Point
            (m_padCount & 0x0FU);                                               // Octet Pad Count LSB
        m_data[8U] = (m_F ? 0x80U : 0x00U) +                                    // Full Message Flag
            (m_blocks & 0x7FU);                                                 // Blocks To Follow
        m_data[9U] = (m_S ? 0x80U : 0x00U) +                                    // Synchronize Flag
            ((m_Ns & 0x07U) << 4) +                                             // Send Sequence Number
            (m_fsn & 0x0FU);                                                    // Fragment Sequence Number
#if DEBUG_DMR_PDU_DATA
        Utils::dump(1U, "DMR, DataHeader::decode(), Confirmed Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
#endif
        break;

    case DPF::RESPONSE:
        m_data[1U] = ((m_sap & 0x0FU) << 4);                                    // Service Access Point
        m_data[8U] = m_blocks & 0x7FU;                                          // Blocks To Follow
        m_data[9U] = ((m_rspClass & 0x03U) << 6) +                              // Response Class
            ((m_rspType & 0x07U) << 3) +                                        // Response Type
            ((m_rspStatus & 0x07U));                                            // Response Status
#if DEBUG_DMR_PDU_DATA
        Utils::dump(1U, "DMR, DataHeader::decode(), Response Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
#endif
        break;

    case DPF::DEFINED_SHORT:
        m_data[0U] = m_data[0U] + (m_blocks & 0x30U);                           // Blocks To Follow MSB
        m_data[1U] = ((m_sap & 0x0FU) << 4) +                                   // Service Access Point
            (m_blocks & 0x0FU);                                                 // Blocks To Follow LSB
        m_data[8U] = (m_F ? 0x01U : 0x00U) +                                    // Full Message Flag
            (m_S ? 0x02U : 0x00U) +                                             // Synchronize Flag
            ((m_dataFormat & 0xFCU) << 2);                                      // Defined Data Format
        m_data[9U] = m_padCount;                                                // Bit Padding
#if DEBUG_DMR_PDU_DATA
        Utils::dump(1U, "DMR, DataHeader::decode(), Defined Short Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
#endif
        break;

    case DPF::DEFINED_RAW:
        m_data[0U] = m_data[0U] + (m_blocks & 0x30U);                           // Blocks To Follow MSB
        m_data[1U] = ((m_sap & 0x0FU) << 4) +                                   // Service Access Point
            (m_blocks & 0x0FU);                                                 // Blocks To Follow LSB
        m_data[8U] = (m_F ? 0x01U : 0x00U) +                                    // Full Message Flag
            (m_S ? 0x02U : 0x00U) +                                             // Synchronize Flag
            ((m_dstPort & 0x07U) << 2) +                                        // Destination Port
            ((m_srcPort & 0x07U) << 5);                                         // Source Port
#if DEBUG_DMR_PDU_DATA
        Utils::dump(1U, "DMR, DataHeader::decode(), Raw Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
#endif
        break;

    default:
        Utils::dump("DMR, Unknown Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
        break;
    }

    if (m_DPF == DPF::UDT) {
        m_data[9U] &= 0xFEU;

        edac::CRC::addCCITT162(m_data, DMR_LC_HEADER_LENGTH_BYTES);

        // restore the checksum
        m_data[10U] ^= DATA_HEADER_CRC_MASK[0U];
        m_data[11U] ^= DATA_HEADER_CRC_MASK[1U];
    }
    else {
        // compute CRC-CCITT 16
        m_data[10U] ^= DATA_HEADER_CRC_MASK[0U];
        m_data[11U] ^= DATA_HEADER_CRC_MASK[1U];

        edac::CRC::addCCITT162(m_data, DMR_LC_HEADER_LENGTH_BYTES);

        // restore the checksum
        m_data[10U] ^= DATA_HEADER_CRC_MASK[0U];
        m_data[11U] ^= DATA_HEADER_CRC_MASK[1U];
    }

    // encode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.encode(m_data, data);
}
