// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - TG Patch
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file HostPatch.h
 * @ingroup patch
 * @file HostPatch.cpp
 * @ingroup patch
 */
#if !defined(__HOST_PATCH_H__)
#define __HOST_PATCH_H__

#include "Defines.h"
#include "common/dmr/data/EmbeddedData.h"
#include "common/dmr/lc/LC.h"
#include "common/dmr/lc/PrivacyLC.h"
#include "common/p25/lc/LC.h"
#include "common/p25/Crypto.h"
#include "common/network/udp/Socket.h"
#include "common/yaml/Yaml.h"
#include "common/Timer.h"
#include "network/PeerNetwork.h"
#include "mmdvm/P25Network.h"

#include <string>
#include <mutex>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t TX_MODE_DMR = 1U;
const uint8_t TX_MODE_P25 = 2U;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the core service logic.
 * @ingroup bridge
 */
class HOST_SW_API HostPatch {
public:
    /**
     * @brief Initializes a new instance of the HostPatch class.
     * @param confFile Full-path to the configuration file.
     */
    HostPatch(const std::string& confFile);
    /**
     * @brief Finalizes a instance of the HostPatch class.
     */
    ~HostPatch();

    /**
     * @brief Executes the main host processing loop.
     * @returns int Zero if successful, otherwise error occurred.
     */
    int run();

private:
    const std::string& m_confFile;
    yaml::Node m_conf;

    network::PeerNetwork* m_network;

    uint32_t m_srcTGId;
    uint8_t m_srcSlot;
    uint32_t m_dstTGId;
    uint8_t m_dstSlot;
    bool m_twoWayPatch;

    bool m_mmdvmP25Reflector;
    mmdvm::P25Network* m_mmdvmP25Net;

    RPT_NET_STATE m_netState;
    p25::lc::LC m_netLC;
    bool m_gotNetLDU1;
    uint8_t* m_netLDU1;
    bool m_gotNetLDU2;
    uint8_t* m_netLDU2;

    std::string m_identity;

    uint8_t m_digiMode;

    dmr::data::EmbeddedData m_dmrEmbeddedData;

    bool m_grantDemand;

    bool m_callInProgress;
    uint8_t m_callAlgoId;
    uint64_t m_rxStartTime;
    uint32_t m_rxStreamId;

    uint8_t m_tekSrcAlgoId;
    uint16_t m_tekSrcKeyId;
    uint8_t m_tekDstAlgoId;
    uint16_t m_tekDstKeyId;
    bool m_requestedSrcTek;
    bool m_requestedDstTek;

    p25::crypto::P25Crypto* m_p25SrcCrypto;
    p25::crypto::P25Crypto* m_p25DstCrypto;

    uint32_t m_netId;
    uint32_t m_sysId;

    bool m_running;
    bool m_trace;
    bool m_debug;

    static std::mutex m_networkMutex;

    /**
     * @brief Reads basic configuration parameters from the INI.
     * @returns bool True, if configuration was read successfully, otherwise false.
     */
    bool readParams();
    /**
     * @brief Initializes network connectivity.
     * @returns bool True, if network connectivity was initialized, otherwise false.
     */
    bool createNetwork();
    /**
     * @brief Initializes MMDVM network connectivity.
     * @returns bool True, if network connectivity was initialized, otherwise false.
     */
    bool createMMDVMP25Network();

    /**
     * @brief Helper to process DMR network traffic.
     * @param buffer 
     * @param length 
     */
    void processDMRNetwork(uint8_t* buffer, uint32_t length);

    /**
     * @brief Helper to process P25 network traffic.
     * @param buffer 
     * @param length 
     */
    void processP25Network(uint8_t* buffer, uint32_t length);

    /**
     * @brief Helper to cross encrypt P25 network traffic audio frames.
     * @param ldu 
     * @param reverseEncrypt Flag indicating whether or not to reverse the encryption (i.e. use destination TEK vs source TEK).
     * @param p25N 
     */
    void cryptP25AudioFrame(uint8_t* ldu, bool reverseEncrypt, uint8_t p25N);

    /**
     * @brief Helper to process a FNE KMM TEK response.
     * @param ki Key Item.
     * @param algId Algorithm ID.
     * @param keyLength Length of key in bytes.
     */
    void processTEKResponse(p25::kmm::KeyItem* ki, uint8_t algId, uint8_t keyLength);

    /**
     * @brief Helper to check for an unflushed LDU1 packet.
     */
    void checkNet_LDU1();
    /**
     * @brief Helper to write a network P25 LDU1 packet.
     * @param toFNE Flag indicating whether or not the packet is being written to the DVM FNE.
     */
    void writeNet_LDU1(bool toFNE);
    /**
     * @brief Helper to check for an unflushed LDU2 packet.
     */
    void checkNet_LDU2();
    /**
     * @brief Helper to write a network P25 LDU2 packet.
     * @param toFNE Flag indicating whether or not the packet is being written to the DVM FNE.
     */
    void writeNet_LDU2(bool toFNE);

    /**
     * @brief Entry point to network processing thread.
     * @param arg Instance of the thread_t structure.
     * @returns void* (Ignore)
     */
    static void* threadNetworkProcess(void* arg);
    /**
     * @brief Entry point to MMDVM network processing thread.
     * @param arg Instance of the thread_t structure.
     * @returns void* (Ignore)
     */
    static void* threadMMDVMProcess(void* arg);

    /**
     * @brief Helper to reset IMBE buffer with null frames.
     * @param data Buffer containing frame data.
     * @param encrypted Flag indicating whether or not the data is encrypted.
     */
    void resetWithNullAudio(uint8_t* data, bool encrypted);
};

#endif // __HOST_PATCH_H__
