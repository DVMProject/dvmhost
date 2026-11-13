// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file IPConvergenceService.h
 * @ingroup fne_packetdata
 * @file IPConvergenceService.cpp
 * @ingroup fne_packetdata
 */
#if !defined(__P25_DATA__IPCONVERGENCESERVICE_H__)
#define  __P25_DATA__IPCONVERGENCESERVICE_H__

#include "common/Defines.h"
#include "common/p25/data/IPv4Packet.h"
#include "network/callhandler/packetdata/p25/PacketDataState.h"

#include <cstdint>

namespace network
{
    namespace callhandler
    {
        namespace packetdata
        {
            namespace p25data
            {
                // ---------------------------------------------------------------------------
                //  Constants
                // ---------------------------------------------------------------------------

                /**
                 * @addtogroup fne_packetdata
                 * @{
                 */

                /** 
                 * @brief Result of an IP convergence operation. 
                 */
                enum class ConvergenceResult : uint8_t {
                    OK,                 //!< Operation was successful.
                    MALFORMED,          //!< The IP datagram is malformed.
                    NO_BINDING,         //!< No binding exists for the IP datagram.
                    SOURCE_MISMATCH,    //!< The source address does not match the expected source.
                    MTU_EXCEEDED,       //!< The IP datagram exceeds the maximum transmission unit.
                    POLICY_REJECTED     //!< The IP datagram was rejected by policy.
                };

                /** 
                 * @brief LLC delivery mode selected for an IP datagram. 
                 */
                enum class DataDeliveryMode : uint8_t {
                    CONFIRMED,          //!< The IP datagram requires confirmed delivery.
                    UNCONFIRMED         //!< The IP datagram does not require confirmed delivery.
                };
                /* @} */

                // ---------------------------------------------------------------------------
                //  Struct Declaration
                // ---------------------------------------------------------------------------

                /** 
                 * @brief Result returned when decoding an IP datagram.
                 * @ingroup fne_packetdata
                 */
                struct IPDecodeResult {
                    /**
                     * @brief The result of the IP convergence operation.
                     */
                    ConvergenceResult result = ConvergenceResult::MALFORMED;
                    /**
                     * @brief The decoded IP packet.
                     */
                    ::p25::data::IPv4Packet packet;
                };

                // ---------------------------------------------------------------------------
                //  Struct Declaration
                // ---------------------------------------------------------------------------

                /** 
                 * @brief Result returned when routing an IP datagram for encoding.
                 * @ingroup fne_packetdata
                 */
                struct IPEncodeResult {
                    /**
                     * @brief The result of the IP convergence operation.
                     */
                    ConvergenceResult result = ConvergenceResult::MALFORMED;
                    /**
                     * @brief The logical link identifier for the encoded IP datagram.
                     */
                    uint32_t llId = 0U;
                    /**
                     * @brief The length of the encoded IP datagram.
                     */
                    uint16_t length = 0U;
                    /**
                     * @brief The delivery mode selected for the encoded IP datagram.
                     */
                    DataDeliveryMode delivery = DataDeliveryMode::CONFIRMED;
                };

                // ---------------------------------------------------------------------------
                //  Struct Declaration
                // ---------------------------------------------------------------------------

                /** 
                 * @brief Policy controlling SCEP routing and delivery.
                 * @ingroup fne_packetdata
                 */
                struct SCEPPolicy {
                    /**
                     * @brief The maximum transmission unit for the SCEP policy.
                     */
                    uint16_t mtu = 510U;
                    /**
                     * @brief Indicates whether broadcast transmission is allowed under the SCEP policy.
                     */
                    bool allowBroadcast = true;
                    /**
                     * @brief Indicates whether multicast transmission is allowed under the SCEP policy.
                     */
                    bool allowMulticast = false;
                    /**
                     * @brief The delivery mode selected for unicast transmissions under the SCEP policy.
                     */
                    DataDeliveryMode unicastDelivery = DataDeliveryMode::CONFIRMED;
                    /**
                     * @brief The delivery mode selected for broadcast transmissions under the SCEP policy.
                     */
                    DataDeliveryMode broadcastDelivery = DataDeliveryMode::UNCONFIRMED;
                };

                // ---------------------------------------------------------------------------
                //  Class Declaration
                // ---------------------------------------------------------------------------

                /**
                 * @brief Defines an IP convergence service for a P25 packet data bearer.
                 * @ingroup fne_packetdata
                 */
                class IPConvergenceService {
                public:
                    /**
                     * @brief Finalizes a instance of IPConvergenceService.
                     */
                    virtual ~IPConvergenceService() = default;

                    /**
                     * @brief Decodes an IP datagram from the provided data buffer.
                     * @param data Pointer to the buffer containing the IP datagram.
                     * @param length Length of the data buffer.
                     * @return The result of the IP datagram decoding operation.
                     */
                    virtual IPDecodeResult decode(const uint8_t* data, uint32_t length) const = 0;
                    /**
                     * @brief Encodes an IP datagram into the provided data buffer.
                     * @param data Pointer to the buffer where the IP datagram will be encoded.
                     * @param length Length of the data buffer.
                     * @return The result of the IP datagram encoding operation.
                     */
                    virtual IPEncodeResult encode(const uint8_t* data, uint32_t length) const = 0;
                    /**
                     * @brief Authorizes an uplink transmission for the specified logical link and IP packet.
                     * @param llId The logical link identifier.
                     * @param packet The IP packet to be transmitted.
                     * @return The result of the uplink authorization operation.
                     */
                    virtual ConvergenceResult authorizeUplink(uint32_t llId,
                        const ::p25::data::IPv4Packet& packet) const = 0;
                    /**
                     * @brief Routes a downlink transmission to the specified destination address.
                     * @param destinationAddress The destination IP address for the downlink transmission.
                     * @return The result of the downlink routing operation.
                     */
                    virtual uint32_t routeDownlink(uint32_t destinationAddress) const = 0;
                };

                // ---------------------------------------------------------------------------
                //  Class Declaration
                // ---------------------------------------------------------------------------

                /**
                 * @brief Implements the conventional SCEP IP convergence service.
                 * @ingroup fne_packetdata
                 */
                class SCEPService : public IPConvergenceService {
                public:
                    /**
                     * @brief Initializes an instance of the SCEPService class.
                     * @param bindings The data binding registry to be used by the service.
                     * @param policy The SCEP policy to be applied by the service.
                     */
                    explicit SCEPService(const DataBindingRegistry& bindings,
                        const SCEPPolicy& policy = SCEPPolicy());

                    /**
                     * @brief Decodes an IP datagram from the provided data buffer.
                     * @param data Pointer to the buffer containing the IP datagram.
                     * @param length Length of the data buffer.
                     * @return The result of the IP datagram decoding operation.
                     */
                    IPDecodeResult decode(const uint8_t* data, uint32_t length) const override;
                    /**
                     * @brief Encodes an IP datagram into the provided data buffer.
                     * @param data Pointer to the buffer containing the IP datagram.
                     * @param length Length of the data buffer.
                     * @return The result of the IP datagram encoding operation.
                     */
                    IPEncodeResult encode(const uint8_t* data, uint32_t length) const override;
                    /**
                     * @brief Authorizes an uplink transmission for the specified logical link ID and IP packet.
                     * @param llId The logical link ID for the uplink transmission.
                     * @param packet The IP packet to be transmitted.
                     * @return The result of the uplink authorization operation.
                     */
                    ConvergenceResult authorizeUplink(uint32_t llId, const ::p25::data::IPv4Packet& packet) const override;
                    /**
                     * @brief Routes a downlink transmission to the specified destination address.
                     * @param destinationAddress The destination address for the downlink transmission.
                     * @return The logical link ID to which the downlink transmission should be routed.
                     */
                    uint32_t routeDownlink(uint32_t destinationAddress) const override;

                    /**
                     * @brief Determines if the specified address is a broadcast address.
                     * @param address The address to be checked.
                     * @return True if the address is a broadcast address, false otherwise.
                     */
                    static bool isBroadcast(uint32_t address);
                    /**
                     * @brief Determines if the specified address is a multicast address.
                     * @param address The address to be checked.
                     * @return True if the address is a multicast address, false otherwise.
                     */
                    static bool isMulticast(uint32_t address);

                private:
                    const DataBindingRegistry& m_bindings;
                    SCEPPolicy m_policy;
                };
            } // namespace p25data
        } // namespace packetdata
    } // namespace callhandler
} // namespace network

#endif // __P25_DATA__IPCONVERGENCESERVICE_H__
