// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "p25/lc/tsbk/TSBKFactory.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

#if FORCE_TSBK_CRC_WARN
bool TSBKFactory::m_warnCRC = true;
#else
bool TSBKFactory::m_warnCRC = false;
#endif

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the TSBKFactory class.
/// </summary>
TSBKFactory::TSBKFactory() = default;

/// <summary>
/// Finalizes a instance of TSBKFactory class.
/// </summary>
TSBKFactory::~TSBKFactory() = default;

/// <summary>
/// Create an instance of a TSBK.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
std::unique_ptr<TSBK> TSBKFactory::createTSBK(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    edac::Trellis trellis = edac::Trellis();

    if (rawTSBK) {
        ::memcpy(tsbk, data, P25_TSBK_LENGTH_BYTES);

        bool ret = edac::CRC::checkCCITT162(tsbk, P25_TSBK_LENGTH_BYTES);
        if (!ret) {
            if (m_warnCRC) {
                // if we're already warning instead of erroring CRC, don't announce invalid CRC in the 
                // case where no CRC is defined
                if ((tsbk[P25_TSBK_LENGTH_BYTES - 2U] != 0x00U) && (tsbk[P25_TSBK_LENGTH_BYTES - 1U] != 0x00U)) {
                    LogWarning(LOG_P25, "TSBKFactory::createTSBK(), failed CRC CCITT-162 check");
                }

                ret = true; // ignore CRC error
            }
            else {
                LogError(LOG_P25, "TSBKFactory::createTSBK(), failed CRC CCITT-162 check");
            }
        }
    }
    else {
        // deinterleave
        uint8_t raw[P25_TSBK_FEC_LENGTH_BYTES];
        P25Utils::decode(data, raw, 114U, 318U);

        // decode 1/2 rate Trellis & check CRC-CCITT 16
        try {
            bool ret = trellis.decode12(raw, tsbk);
            if (!ret) {
                LogError(LOG_P25, "TSBKFactory::createTSBK(), failed to decode Trellis 1/2 rate coding");
            }

            if (ret) {
                ret = edac::CRC::checkCCITT162(tsbk, P25_TSBK_LENGTH_BYTES);
                if (!ret) {
                    if (m_warnCRC) {
                        LogWarning(LOG_P25, "TSBKFactory::createTSBK(), failed CRC CCITT-162 check");
                        ret = true; // ignore CRC error
                    }
                    else {
                        LogError(LOG_P25, "TSBKFactory::createTSBK(), failed CRC CCITT-162 check");
                    }
                }
            }

            if (!ret)
                return nullptr;
        }
        catch (...) {
            Utils::dump(2U, "P25, decoding excepted with input data", tsbk, P25_TSBK_LENGTH_BYTES);
            return nullptr;
        }
    }

    uint8_t lco = tsbk[0U] & 0x3F;                                                  // LCO
    uint8_t mfId = tsbk[1U];                                                        // Mfg Id.

    // Motorola P25 vendor opcodes
    if (mfId == P25_MFG_MOT) {
        switch (lco) {
        case TSBK_IOSP_GRP_VCH:
        case TSBK_IOSP_UU_VCH:
        case TSBK_IOSP_UU_ANS:
        case TSBK_IOSP_TELE_INT_ANS:
        case TSBK_IOSP_STS_UPDT:
        case TSBK_IOSP_STS_Q:
        case TSBK_IOSP_MSG_UPDT:
        case TSBK_IOSP_CALL_ALRT:
        case TSBK_IOSP_ACK_RSP:
        case TSBK_IOSP_GRP_AFF:
        case TSBK_IOSP_U_REG:
        case TSBK_ISP_CAN_SRV_REQ:
        case TSBK_ISP_GRP_AFF_Q_RSP:
        case TSBK_OSP_DENY_RSP:
        case TSBK_OSP_QUE_RSP:
        case TSBK_ISP_U_DEREG_REQ:
        case TSBK_OSP_U_DEREG_ACK:
        case TSBK_ISP_LOC_REG_REQ:
            mfId = P25_MFG_STANDARD;
            break;
        default:
            LogError(LOG_P25, "TSBKFactory::createTSBK(), unknown TSBK LCO value, mfId = $%02X, lco = $%02X", mfId, lco);
            break;
        }

        if (mfId == P25_MFG_MOT) {
            return nullptr;
        }
        else {
            mfId = tsbk[1U];
        }
    }

    // internal / Omaha Communication Systems P25 vendor opcodes
    if (mfId == P25_MFG_DVM_OCS) {
        switch (lco) {
        case LC_CALL_TERM:
            return decode(new OSP_DVM_LC_CALL_TERM(), data, rawTSBK);
        default:
            mfId = P25_MFG_STANDARD;
            break;
        }

        if (mfId == P25_MFG_DVM_OCS) {
            return nullptr;
        }
        else {
            mfId = tsbk[1U];
        }
    }

    // standard P25 reference opcodes
    switch (lco) {
    case TSBK_IOSP_GRP_VCH:
        return decode(new IOSP_GRP_VCH(), data, rawTSBK);
    case TSBK_IOSP_UU_VCH:
        return decode(new IOSP_UU_VCH(), data, rawTSBK);
    case TSBK_IOSP_UU_ANS:
        return decode(new IOSP_UU_ANS(), data, rawTSBK);
    case TSBK_ISP_SNDCP_CH_REQ:
        return decode(new ISP_SNDCP_CH_REQ(), data, rawTSBK);
    case TSBK_IOSP_STS_UPDT:
        return decode(new IOSP_STS_UPDT(), data, rawTSBK);
    case TSBK_IOSP_MSG_UPDT:
        return decode(new IOSP_MSG_UPDT(), data, rawTSBK);
    case TSBK_IOSP_RAD_MON:
        return decode(new IOSP_RAD_MON(), data, rawTSBK);
    case TSBK_IOSP_CALL_ALRT:
        return decode(new IOSP_CALL_ALRT(), data, rawTSBK);
    case TSBK_IOSP_ACK_RSP:
        return decode(new IOSP_ACK_RSP(), data, rawTSBK);
    case TSBK_ISP_EMERG_ALRM_REQ:
        return decode(new ISP_EMERG_ALRM_REQ(), data, rawTSBK);
    case TSBK_IOSP_EXT_FNCT:
        return decode(new IOSP_EXT_FNCT(), data, rawTSBK);
    case TSBK_IOSP_GRP_AFF:
        return decode(new IOSP_GRP_AFF(), data, rawTSBK);
    case TSBK_IOSP_U_REG:
        return decode(new IOSP_U_REG(), data, rawTSBK);
    case TSBK_ISP_CAN_SRV_REQ:
        return decode(new ISP_CAN_SRV_REQ(), data, rawTSBK);
    case TSBK_ISP_GRP_AFF_Q_RSP:
        return decode(new ISP_GRP_AFF_Q_RSP(), data, rawTSBK);
    case TSBK_OSP_QUE_RSP:
        return decode(new OSP_QUE_RSP(), data, rawTSBK);
    case TSBK_ISP_U_DEREG_REQ:
        return decode(new ISP_U_DEREG_REQ(), data, rawTSBK);
    case TSBK_OSP_U_DEREG_ACK:
        return decode(new OSP_U_DEREG_ACK(), data, rawTSBK);
    case TSBK_ISP_LOC_REG_REQ:
        return decode(new ISP_LOC_REG_REQ(), data, rawTSBK);
    case TSBK_ISP_AUTH_RESP:
        return decode(new ISP_AUTH_RESP(), data, rawTSBK);
    case TSBK_ISP_AUTH_FNE_RST:
        return decode(new ISP_AUTH_FNE_RST(), data, rawTSBK);
    case TSBK_ISP_AUTH_SU_DMD:
        return decode(new ISP_AUTH_SU_DMD(), data, rawTSBK);
    case TSBK_OSP_ADJ_STS_BCAST:
        return decode(new OSP_ADJ_STS_BCAST(), data, rawTSBK);
    default:
        LogError(LOG_P25, "TSBKFactory::create(), unknown TSBK LCO value, mfId = $%02X, lco = $%02X", mfId, lco);
        break;
    }

    return nullptr;
}

/// <summary>
/// Create an instance of a AMBT.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="blocks"></param>
/// <returns></returns>
std::unique_ptr<AMBT> TSBKFactory::createAMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks)
{
    assert(blocks != nullptr);

    if (dataHeader.getFormat() != PDU_FMT_AMBT) {
        LogError(LOG_P25, "TSBKFactory::createAMBT(), PDU is not a AMBT PDU");
        return nullptr;
    }

    if (dataHeader.getBlocksToFollow() == 0U) {
        LogError(LOG_P25, "TSBKFactory::createAMBT(), PDU contains no data blocks");
        return nullptr;
    }

    uint8_t lco = dataHeader.getAMBTOpcode();                                       // LCO
    uint8_t mfId = dataHeader.getMFId();                                            // Mfg Id.

    // Motorola P25 vendor opcodes
    if (mfId == P25_MFG_MOT) {
        switch (lco) {
        case TSBK_IOSP_GRP_VCH:
        case TSBK_IOSP_UU_VCH:
        case TSBK_IOSP_UU_ANS:
        case TSBK_IOSP_TELE_INT_ANS:
        case TSBK_IOSP_STS_UPDT:
        case TSBK_IOSP_STS_Q:
        case TSBK_IOSP_MSG_UPDT:
        case TSBK_IOSP_CALL_ALRT:
        case TSBK_IOSP_ACK_RSP:
        case TSBK_IOSP_GRP_AFF:
        case TSBK_IOSP_U_REG:
        case TSBK_ISP_CAN_SRV_REQ:
        case TSBK_ISP_GRP_AFF_Q_RSP:
        case TSBK_OSP_DENY_RSP:
        case TSBK_OSP_QUE_RSP:
        case TSBK_ISP_U_DEREG_REQ:
        case TSBK_OSP_U_DEREG_ACK:
        case TSBK_ISP_LOC_REG_REQ:
            mfId = P25_MFG_STANDARD;
            break;
        default:
            LogError(LOG_P25, "TSBKFactory::createAMBT(), unknown TSBK LCO value, mfId = $%02X, lco = $%02X", mfId, lco);
            break;
        }

        if (mfId == P25_MFG_MOT) {
            return nullptr;
        }
        else {
            mfId = dataHeader.getMFId();
        }
    }

    // standard P25 reference opcodes
    switch (lco) {
    case TSBK_IOSP_STS_UPDT:
        return decode(new MBT_IOSP_STS_UPDT(), dataHeader, blocks);
    case TSBK_IOSP_MSG_UPDT:
        return decode(new MBT_IOSP_MSG_UPDT(), dataHeader, blocks);
    case TSBK_IOSP_CALL_ALRT:
        return decode(new MBT_IOSP_CALL_ALRT(), dataHeader, blocks);
    case TSBK_IOSP_ACK_RSP:
        return decode(new MBT_IOSP_ACK_RSP(), dataHeader, blocks);
    case TSBK_IOSP_GRP_AFF:
        return decode(new MBT_IOSP_GRP_AFF(), dataHeader, blocks);
    case TSBK_ISP_CAN_SRV_REQ:
        return decode(new MBT_ISP_CAN_SRV_REQ(), dataHeader, blocks);
    case TSBK_IOSP_EXT_FNCT:
        return decode(new MBT_IOSP_EXT_FNCT(), dataHeader, blocks);
    case TSBK_ISP_AUTH_RESP_M:
        return decode(new MBT_ISP_AUTH_RESP_M(), dataHeader, blocks);
    case TSBK_ISP_AUTH_SU_DMD:
        return decode(new MBT_ISP_AUTH_SU_DMD(), dataHeader, blocks);
    default:
        LogError(LOG_P25, "TSBKFactory::createAMBT(), unknown TSBK LCO value, mfId = $%02X, lco = $%02X", mfId, lco);
        break;
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <param name="tsbk"></param>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns></returns>
std::unique_ptr<TSBK> TSBKFactory::decode(TSBK* tsbk, const uint8_t* data, bool rawTSBK)
{
    assert(tsbk != nullptr);
    assert(data != nullptr);

    if (!tsbk->decode(data, rawTSBK)) {
        return nullptr;
    }

    return std::unique_ptr<TSBK>(tsbk);
}

/// <summary>
///
/// </summary>
/// <param name="ambt"></param>
/// <param name="dataHeader"></param>
/// <param name="blocks"></param>
/// <returns></returns>
std::unique_ptr<AMBT> TSBKFactory::decode(AMBT* ambt, const data::DataHeader& dataHeader, const data::DataBlock* blocks)
{
    assert(ambt != nullptr);
    assert(blocks != nullptr);

    if (!ambt->decodeMBT(dataHeader, blocks)) {
        return nullptr;
    }

    return std::unique_ptr<AMBT>(ambt);
}
