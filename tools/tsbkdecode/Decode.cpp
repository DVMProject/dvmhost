// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - TSBK/CSBK Decoder
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "Decode.h"
#include "common/edac/CRC.h"
#include "common/edac/BPTC19696.h"
#include "common/p25/P25Utils.h"
#include "common/p25/lc/tsbk/TSBKFactory.h"
#include "common/dmr/lc/csbk/CSBKFactory.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>

// ---------------------------------------------------------------------------
//  Internal Functions
// ---------------------------------------------------------------------------

/**
 * @brief Prints a numeric field in decimal and hexadecimal.
 * @tparam T Numeric type.
 * @param name Field name.
 * @param value Numeric value.
 */
template<typename T>
void field(const char* name, T value)
{
    std::cout << "  " << name << ": " << +value << " (0x" << std::hex << +value << std::dec << ")\n";
}

/**
 * @brief Prints a boolean field.
 * @param name Field name.
 * @param value Boolean value.
 */
void field(const char* name, bool value)
{
    std::cout << "  " << name << ": " << (value ? "true" : "false") << '\n';
}

/**
 * @brief Dumps the decoded bytes and common block fields.
 * @param raw Pointer to the raw block bytes.
 */
void dump(const uint8_t* raw)
{
    std::cout << "Decoded bytes:";
    for (unsigned i = 0; i < 12; ++i)
        std::cout << ' ' << std::hex << std::setw(2) << std::setfill('0') << +raw[i];
    std::cout << std::dec << '\n';
    field("Opcode", raw[0] & 0x3f);
    field("Manufacturer / feature ID", raw[1]);
    field("Last block", (raw[0] & 0x80) != 0);
}

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/* Decodes a raw block or frame and prints its fields. */

bool decodeBlock(std::vector<uint8_t> bytes, bool csbk, bool frame, bool isp)
{
    const size_t expected = frame ? (csbk ? 33U : 45U) : 12U;
    if ((!frame && bytes.size() != 10U && bytes.size() != 12U) ||
        (frame && bytes.size() != expected)) {
        std::cerr << "Expected " << (frame ? (csbk ? "33" : "45") : "10 or 12") << " bytes.\n";
        return false;
    }

    std::array<uint8_t, 12> raw{};
    std::array<uint8_t, 33> burst{};

    if (frame) {
        if (csbk) {
            edac::BPTC19696 bptc;
            bptc.decode(bytes.data(), raw.data());
        }
        else {
            std::array<uint8_t, 25> fec{};
            p25::P25Utils::decode(bytes.data(), fec.data(), 114U, 318U);
            edac::Trellis trellis;
            if (!trellis.decode12(fec.data(), raw.data())) {
                std::cerr << "Trellis decoding failed.\n";
                return false;
            }
        }
    }
    else {
        std::copy(bytes.begin(), bytes.end(), raw.begin());
        if (bytes.size() == 10U) {
            edac::CRC::addCCITT162(raw.data(), 12U);
            if (csbk) {
                raw[10] ^= 0xa5;
                raw[11] ^= 0xa5;
            }
            std::cout << "CRC generated for 10-byte input.\n";
        }
    }

    auto checksum = raw;
    if (csbk) {
        checksum[10] ^= 0xa5;
        checksum[11] ^= 0xa5;
    }

    const bool validCRC = edac::CRC::checkCCITT162(checksum.data(), 12U);
    if (!validCRC)
        std::cerr << "Warning: CRC check failed; continuing decode.\n";

    dump(raw.data());

    p25::lc::TSBK::setWarnCRC(true);
    p25::lc::tsbk::TSBKFactory::setWarnCRC(true);
    dmr::lc::CSBK::setWarnCRC(true);
    dmr::lc::csbk::CSBKFactory::setWarnCRC(true);

    if (csbk) {
        edac::BPTC19696 bptc;
        bptc.encode(raw.data(), burst.data());
        auto block = dmr::lc::csbk::CSBKFactory::createCSBK(burst.data(), dmr::defines::DataType::CSBK);
        if (!block) {
            std::cerr << "Unsupported CSBK opcode.\n";
            return false;
        }

        std::cout << block->toString() << '\n';

        if (!block->getDecodedRaw()) {
            std::cerr << "CSBK decoder is not implemented in the common library.\n";
            return false;
        }

        if (auto typed = dynamic_cast<dmr::lc::csbk::CSBK_ACK_RSP*>(block.get())) {
            field("GI", typed->getGI());
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("Reason", typed->getReason());
        }

        if (auto typed = dynamic_cast<dmr::lc::csbk::CSBK_BROADCAST*>(block.get())) {
            field("ColorCode", typed->getColorCode());
            field("LogicalCh1", typed->getLogicalCh1());
            field("LogicalCh2", typed->getLogicalCh2());
            field("AnncType", typed->getAnncType());
            field("AnnWdCh1", typed->getAnnWdCh1());
            field("AnnWdCh2", typed->getAnnWdCh2());
            field("RequireReg", typed->getRequireReg());
            field("SystemId", typed->getSystemId());
            field("BackoffNo", typed->getBackoffNo());
        }

        if (auto typed = dynamic_cast<dmr::lc::csbk::CSBK_BSDWNACT*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("BSId", typed->getBSId());
        }

        if (auto typed = dynamic_cast<dmr::lc::csbk::CSBK_CALL_ALRT*>(block.get())) {
            field("GI", typed->getGI());
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
        }

        if (auto typed = dynamic_cast<dmr::lc::csbk::CSBK_EXT_FNCT*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("DataContent", typed->getDataContent());
            field("ExtendedFunction", typed->getExtendedFunction());
        }

        if (auto typed = dynamic_cast<dmr::lc::csbk::CSBK_MAINT*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("MaintKind", typed->getMaintKind());
        }

        if (auto typed = dynamic_cast<dmr::lc::csbk::CSBK_NACK_RSP*>(block.get())) {
            field("GI", typed->getGI());
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("Reason", typed->getReason());
            field("ServiceKind", typed->getServiceKind());
        }

        if (auto typed = dynamic_cast<dmr::lc::csbk::CSBK_PRECCSBK*>(block.get())) {
            field("GI", typed->getGI());
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("DataContent", typed->getDataContent());
            field("CBF", typed->getCBF());
        }

        if (auto typed = dynamic_cast<dmr::lc::csbk::CSBK_RAND*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("Proxy", typed->getProxy());
            field("ServiceOptions", typed->getServiceOptions());
            field("ServiceExtra", typed->getServiceExtra());
            field("ServiceKind", typed->getServiceKind());
        }

        if (auto typed = dynamic_cast<dmr::lc::csbk::CSBK_UU_ANS_RSP*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
        }

        if (auto typed = dynamic_cast<dmr::lc::csbk::CSBK_UU_V_REQ*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
        }
    }
    else {
        // These outbound opcodes overlap inbound authentication messages in the factory.
        const unsigned opcode = raw[0] & 0x3f;
        if (!isp && (opcode == p25::defines::TSBKO::OSP_NET_STS_BCAST ||
                     opcode == p25::defines::TSBKO::OSP_RFSS_STS_BCAST)) {
            field("LRA", raw[2]);
            if (opcode == p25::defines::TSBKO::OSP_NET_STS_BCAST) {
                std::cout << "Network Status Broadcast\n";
                field("NetId", (unsigned(raw[3]) << 12) | (unsigned(raw[4]) << 4) | (raw[5] >> 4));
                field("SysId", ((raw[5] & 0x0f) << 8) | raw[6]);
            }
            else {
                std::cout << "RFSS Status Broadcast\n";
                field("SysId", ((raw[3] & 0x0f) << 8) | raw[4]);
                field("RFSSId", raw[5]);
                field("SiteId", raw[6]);
            }
            field("ChannelId", raw[7] >> 4);
            field("ChannelNo", ((raw[7] & 0x0f) << 8) | raw[8]);
            field("ServiceClass", raw[9]);
            return true;
        }

        auto block = p25::lc::tsbk::TSBKFactory::createTSBK(raw.data(), true);
        if (!block) {
            std::cerr << "Unsupported TSBK opcode.\n";
            return false;
        }

        std::cout << block->toString(isp) << '\n';

        if (!block->getDecodedRaw()) {
            std::cerr << "TSBK decoder is not implemented in the common library.\n";
            return false;
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::IOSP_ACK_RSP*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("AIV", typed->getAIV());
            field("ExtendedAddr", typed->getEX());
            if (typed->getAIV() && typed->getEX()) {
                field("NetId", typed->getNetId());
                field("SysId", typed->getSysId());
            }
            field("Service", typed->getService());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::IOSP_CALL_ALRT*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::IOSP_EXT_FNCT*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("ExtendedFunction", typed->getExtendedFunction());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::IOSP_GRP_AFF*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("SysId", typed->getSysId());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::IOSP_GRP_VCH*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("GrpVchId", typed->getGrpVchId());
            field("GrpVchNo", typed->getGrpVchNo());
            field("Emergency", typed->getEmergency());
            field("Encrypted", typed->getEncrypted());
            field("Priority", typed->getPriority());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::IOSP_MSG_UPDT*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("Message", typed->getMessage());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::IOSP_RAD_MON*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("TxMult", typed->getTxMult());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::IOSP_STS_UPDT*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("Status", typed->getStatus());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::IOSP_UU_ANS*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("Response", typed->getResponse());
            field("Emergency", typed->getEmergency());
            field("Encrypted", typed->getEncrypted());
            field("Priority", typed->getPriority());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::IOSP_UU_VCH*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("GrpVchId", typed->getGrpVchId());
            field("GrpVchNo", typed->getGrpVchNo());
            field("Emergency", typed->getEmergency());
            field("Encrypted", typed->getEncrypted());
            field("Priority", typed->getPriority());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::IOSP_U_REG*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("NetId", typed->getNetId());
            field("SysId", typed->getSysId());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::ISP_AUTH_FNE_RST*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("AuthSuccess", typed->getAuthSuccess());
            field("AuthStandalone", typed->getAuthStandalone());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::ISP_AUTH_RESP*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("AuthStandalone", typed->getAuthStandalone());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::ISP_AUTH_SU_DMD*>(block.get())) {
            field("SrcId", typed->getSrcId());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::ISP_CAN_SRV_REQ*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("AIV", typed->getAIV());
            field("Service", typed->getService());
            field("Response", typed->getResponse());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::ISP_EMERG_ALRM_REQ*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("AIV", typed->getAIV());
            field("Service", typed->getService());
            field("Response", typed->getResponse());
            field("Emergency", typed->getEmergency());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::ISP_GRP_AFF_Q_RSP*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("AnnounceGroup", typed->getAnnounceGroup());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::ISP_LOC_REG_REQ*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("LRA", typed->getLRA());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::ISP_SNDCP_CH_REQ*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DataServiceOptions", typed->getDataServiceOptions());
            field("DataAccessControl", typed->getDataAccessControl());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::ISP_SNDCP_REC_REQ*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DataToSend", typed->getDataToSend());
            field("DataServiceOptions", typed->getDataServiceOptions());
            field("DataAccessControl", typed->getDataAccessControl());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::ISP_U_DEREG_REQ*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("NetId", typed->getNetId());
            field("SysId", typed->getSysId());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::OSP_ADJ_STS_BCAST*>(block.get())) {
            field("AdjSiteSysId", typed->getAdjSiteSysId());
            field("AdjSiteRFSSId", typed->getAdjSiteRFSSId());
            field("AdjSiteId", typed->getAdjSiteId());
            field("AdjSiteChnId", typed->getAdjSiteChnId());
            field("AdjSiteChnNo", typed->getAdjSiteChnNo());
            field("AdjSiteSvcClass", typed->getAdjSiteSvcClass());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::OSP_DENY_RSP*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("AIV", typed->getAIV());
            field("Service", typed->getService());
            field("Response", typed->getResponse());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::OSP_DVM_LC_CALL_TERM*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("GrpVchId", typed->getGrpVchId());
            field("GrpVchNo", typed->getGrpVchNo());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::OSP_GRP_VCH_GRANT_UPD*>(block.get())) {
            field("DstId", typed->getDstId());
            field("GrpVchId", typed->getGrpVchId());
            field("GrpVchNo", typed->getGrpVchNo());
            field("GrpVchIdB", typed->getGrpVchIdB());
            field("GrpVchNoB", typed->getGrpVchNoB());
            field("DstIdB", typed->getDstIdB());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::OSP_QUE_RSP*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("AIV", typed->getAIV());
            field("Service", typed->getService());
            field("Response", typed->getResponse());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::OSP_UU_VCH_GRANT_UPD*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("DstId", typed->getDstId());
            field("GrpVchId", typed->getGrpVchId());
            field("GrpVchNo", typed->getGrpVchNo());
        }

        if (auto typed = dynamic_cast<p25::lc::tsbk::OSP_U_DEREG_ACK*>(block.get())) {
            field("SrcId", typed->getSrcId());
            field("NetId", typed->getNetId());
            field("SysId", typed->getSysId());
        }
    }

    return true;
}
