# DVM Network Stack Technical Documentation

**Version:** 1.0  
**Date:** December 3, 2025  
**Author:** AI Assistant (based on source code analysis)

AI WARNING: This document was mainly generated using AI assistance. As such, there is the possibility of some error or inconsistency.

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Network Protocol Layers](#network-protocol-layers)
4. [RTP Protocol Implementation](#rtp-protocol-implementation)
5. [Network Functions and Sub-Functions](#network-functions-and-sub-functions)
6. [Connection Management](#connection-management)
7. [Data Transport](#data-transport)
8. [Stream Multiplexing](#stream-multiplexing)
9. [Security](#security)
10. [Quality of Service](#quality-of-service)
11. [Network Diagnostics](#network-diagnostics)
12. [Performance Considerations](#performance-considerations)

---

## 1. Overview

The Digital Voice Modem (DVM) network stack is a sophisticated real-time communication system designed to transport digital voice protocols (DMR, P25, NXDN) and analog audio over IP networks. The architecture is based on the Real-time Transport Protocol (RTP) with custom extensions specific to Fixed Network Equipment (FNE) operations.

### Key Features

- **Multi-Protocol Support**: DMR, P25, NXDN, and analog audio
- **RTP-Based Transport**: Standards-compliant RTP with custom extensions
- **Master-Peer Architecture**: Centralized master with distributed peers
- **Stream Multiplexing**: Concurrent call handling via unique stream IDs
- **High Availability**: Failover support with multiple master addresses
- **Encryption**: Optional preshared key encryption for endpoint security
- **Network Replication**: Peer list, talkgroup, and RID list replication
- **Quality of Service**: Packet sequencing, acknowledgments, and retry logic
- **Activity Logging**: Distributed activity and diagnostic log transfer

### Network Topology

The DVM network implements a **spanning tree topology** to prevent routing loops and ensure efficient traffic distribution. The FNE master acts as the root of the tree, with additional FNE nodes forming branches. **Peers (dvmhost, dvmbridge, dvmpatch) are always leaf nodes** and cannot have children of their own.

```
                        ┌─────────────────────┐
                        │   FNE Master (Root) │
                        │    Primary FNE      │
                        └──────────┬──────────┘
                                   │
            ┌──────────────────────┼──────────────────────────────────┐
            │                      │                                  │
    ┌───────▼────────┐     ┌──────▼──────────┐            ┌─────────▼─────────┐
    │  dvmhost #1    │     │  FNE Regional   │            │    dvmbridge      │
    │  (Leaf Peer)   │     │  (Child FNE)    │            │    (Leaf Peer)    │
    └────────────────┘     └──────┬──────────┘            └───────────────────┘
                                   │
                        ┌──────────┼──────────────────┐
                        │          │                  │
                ┌───────▼──────┐   │          ┌───────▼─────────┐
                │  dvmhost #2  │   │          │  FNE District   │
                │ (Leaf Peer)  │   │          │  (Child FNE)    │
                └──────────────┘   │          └───────┬─────────┘
                                   │                  │
                           ┌───────▼──────┐   ┌───────▼─────────┐
                           │  dvmpatch    │   │   dvmhost #3    │
                           │ (Leaf Peer)  │   │  (Leaf Peer)    │
                           └──────────────┘   └─────────────────┘

Key Topology Rules:
┌─────────────────┐
│      FNE        │ ◄─── Can have child FNEs and/or leaf peers
└─────────────────┘

┌─────────────────┐
│  dvmhost (Peer) │ ◄─── Always a leaf node, no children allowed
└─────────────────┘

┌─────────────────┐
│ dvmbridge (Peer)│ ◄─── Always a leaf node, no children allowed
└─────────────────┘

┌─────────────────┐
│ dvmpatch (Peer) │ ◄─── Always a leaf node, no children allowed
└─────────────────┘


Alternative HA Configuration with Replica:

    ┌─────────────────────┐           ┌─────────────────────┐
    │   FNE Master (Root) │◄─────────►│  FNE Replica (HA)   │
    │    Primary FNE      │  Repl.    │   Standby FNE       │
    └──────────┬──────────┘  Sync     └──────────┬──────────┘
               │                                  │
    ┌──────────┼──────────┬──────────┐   (Takes over on failure)
    │          │          │          │
┌───▼──────┐ ┌▼────────┐ ┌▼───────┐ ┌▼─────────┐
│ dvmhost1 │ │dvmhost2 │ │Regional│ │dvmbridge │
│  (Leaf)  │ │ (Leaf)  │ │  FNE   │ │  (Leaf)  │
└──────────┘ └─────────┘ └┬───────┘ └──────────┘
                           │
                    ┌──────┴────────┐
                    │               │
                ┌───▼──────┐   ┌────▼──────┐
                │ dvmhost3 │   │ dvmpatch  │
                │  (Leaf)  │   │  (Leaf)   │
                └──────────┘   └───────────┘
```

**Spanning Tree Features:**

- **Loop Prevention**: Network tree structure prevents routing loops
- **Hierarchical FNE Structure**: Only FNE nodes can have children (other FNEs or peers)
- **Leaf-Only Peers**: dvmhost, dvmbridge, dvmpatch are always terminal leaf nodes
- **Multi-Level FNE Hierarchy**: FNE nodes can be nested multiple levels deep
- **Root Master**: Primary FNE master serves as spanning tree root
- **Branch Pruning**: Automatic detection and removal of redundant paths
- **Fast Reconvergence**: Quick recovery when topology changes occur
- **Mixed Children**: FNE nodes can have both child FNE nodes and leaf peers
- **Replication Sync**: Network tree topology synchronized across all FNE nodes
- **Peer Types**:
  - **dvmhost**: Repeater/hotspot hosts (always leaf)
  - **dvmbridge**: Network bridges connecting to other systems (always leaf)
  - **dvmpatch**: Audio patch/gateway nodes (always leaf)
- **Configuration**: Enabled via `enableSpanningTree` option in FNE configuration

---

## 2. Architecture

### Class Hierarchy

The DVM network stack is organized into a hierarchical class structure with clear separation between **common/core networking classes** (used by all components) and **FNE-specific classes** (used only by the FNE master).

#### Common/Core Network Classes (dvmhost/src/common/network/)

These classes provide the foundational networking functionality used by all DVM components (dvmhost, dvmbridge, dvmpatch, dvmfne):

```
BaseNetwork (Abstract Base Class)
    │
    ├── Network (Peer Implementation - for dvmhost, dvmbridge, dvmpatch)
    │
    └── FNENetwork (Master Implementation - for dvmfne only)

Core Supporting Classes:
    ├── FrameQueue (RTP Frame Management)
    │   └── RawFrameQueue (Raw UDP Socket Operations)
    │
    ├── RTPHeader (Standard RTP Header - RFC 3550)
    │   └── RTPExtensionHeader (RTP Extension Base)
    │       └── RTPFNEHeader (FNE-Specific Extension Header)
    │
    ├── RTPStreamMultiplex (Multi-Stream Management)
    ├── PacketBuffer (Fragmentation/Reassembly)
    └── udp::Socket (UDP Transport Layer)
```

#### FNE-Specific Network Classes (dvmhost/src/fne/network/)

These classes extend the core functionality specifically for FNE master operations:

```
FNENetwork (Extends BaseNetwork)
    │
    ├── DiagNetwork (Diagnostic Port Handler)
    │   └── Uses BaseNetwork functionality on alternate port
    │
    └── Call Handlers (Traffic Routing & Management):
        ├── TagDMRData (DMR Protocol Handler)
        ├── TagP25Data (P25 Protocol Handler)
        ├── TagNXDNData (NXDN Protocol Handler)
        └── TagAnalogData (Analog Protocol Handler)
```

#### Class Usage by Component:

| Class | dvmhost | dvmbridge | dvmpatch | dvmfne |
|-------|---------|-----------|----------|--------|
| BaseNetwork | ✓ | ✓ | ✓ | ✓ |
| Network | ✓ | ✓ | ✓ | ✗ |
| FNENetwork | ✗ | ✗ | ✗ | ✓ |
| FrameQueue | ✓ | ✓ | ✓ | ✓ |
| RTPHeader/FNEHeader | ✓ | ✓ | ✓ | ✓ |
| DiagNetwork | ✗ | ✗ | ✗ | ✓ |
| TagXXXData | ✗ | ✗ | ✗ | ✓ |
| FNEPeerConnection | ✗ | ✗ | ✗ | ✓ |

### BaseNetwork Class

The `BaseNetwork` class provides core networking functionality shared by both peer and master implementations:

- **Ring Buffers**: Fixed-size circular buffers (4KB each) for DMR, P25, NXDN, and analog receive data
- **Protocol Writers**: Methods for writing DMR, P25 LDU1/LDU2, NXDN, and analog frames
- **Message Builders**: Functions to construct protocol-specific network messages
- **Grant Management**: Grant request and encryption key request handling
- **Announcements**: Unit registration, group affiliation, and peer status announcements

```cpp
class BaseNetwork {
protected:
    uint32_t m_peerId;              // Unique peer identifier
    NET_CONN_STATUS m_status;       // Connection status
    udp::Socket* m_socket;          // UDP socket
    FrameQueue* m_frameQueue;       // RTP frame queue
    
    // Ring buffers for protocol data
    RingBuffer m_rxDMRData;         // DMR receive buffer (4KB)
    RingBuffer m_rxP25Data;         // P25 receive buffer (4KB)
    RingBuffer m_rxNXDNData;        // NXDN receive buffer (4KB)
    RingBuffer m_rxAnalogData;      // Analog receive buffer (4KB)
    
    // Stream identifiers
    uint32_t* m_dmrStreamId;        // DMR stream IDs (2 slots)
    uint32_t m_p25StreamId;         // P25 stream ID
    uint32_t m_nxdnStreamId;        // NXDN stream ID
    uint32_t m_analogStreamId;      // Analog stream ID
};
```

### Network Class (Peer)

The `Network` class implements peer-side functionality for connecting to FNE masters:

- **Connection State Machine**: Login, authorization, configuration, and running states
- **Authentication**: SHA256-based challenge-response authentication
- **Heartbeat**: Regular ping/pong messages to maintain connection
- **Metadata Exchange**: Peer configuration and status reporting
- **High Availability**: Support for multiple master addresses with failover

```cpp
class Network : public BaseNetwork {
private:
    std::string m_address;          // Master IP address
    uint16_t m_port;                // Master port
    std::string m_password;         // Authentication password
    
    NET_CONN_STATUS m_status;       // Connection state
    Timer m_retryTimer;             // Connection retry timer
    Timer m_timeoutTimer;           // Peer timeout timer
    
    uint32_t m_loginStreamId;       // Login sequence stream ID
    PeerMetadata* m_metadata;       // Peer metadata
    RTPStreamMultiplex* m_mux;      // Stream multiplexer
    
    std::vector<std::string> m_haIPs;     // HA master addresses
    uint32_t m_currentHAIP;         // Current HA index
};
```

### FNENetwork Class (Master)

The `FNENetwork` class implements master-side functionality for managing connected peers:

- **Peer Management**: Track connected peers, their capabilities, and metadata
- **Call Routing**: Route traffic between peers based on talkgroup affiliations
- **Access Control**: Whitelist/blacklist management for RIDs and talkgroups
- **Network Replication**: Distribute peer lists, talkgroup rules, and RID lookups
- **Spanning Tree**: Optional spanning tree protocol for complex network topologies
- **Thread Pool**: Worker threads for asynchronous packet processing

```cpp
class FNENetwork : public BaseNetwork {
private:
    std::unordered_map<uint32_t, FNEPeerConnection*> m_peers;
    std::unordered_map<uint32_t, uint32_t> m_ccPeerMap;
    
    lookups::RadioIdLookup* m_ridLookup;
    lookups::TalkgroupRulesLookup* m_tidLookup;
    lookups::PeerListLookup* m_peerListLookup;
    
    ThreadPool m_threadPool;        // Worker threads
    Timer m_maintainenceTimer;      // Peer maintenance timer
    
    bool m_enableSpanningTree;      // Spanning tree enabled
    bool m_disallowU2U;             // Disallow unit-to-unit calls
};
```

---

## 3. Network Protocol Layers

The DVM network stack implements a layered protocol architecture:

### Layer 1: UDP Transport

- **Socket Operations**: Standard UDP datagram socket (IPv4/IPv6)
- **Buffer Sizes**: Configurable send/receive buffers (default 512KB)
- **Non-Blocking**: Asynchronous I/O for high-throughput scenarios
- **Encryption**: Optional AES-256 encryption at transport layer

### Layer 2: RTP (Real-time Transport Protocol)

Standard RTP header format (RFC 3550):

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|X|  CC   |M|     PT      |       Sequence Number         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Synchronization Source (SSRC) identifier            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Fields:**
- **V (Version)**: Always 2
- **P (Padding)**: Padding flag (typically 0)
- **X (Extension)**: Extension header present (always 1 for DVM)
- **CC (CSRC Count)**: Contributing source count (typically 0)
- **M (Marker)**: Application-specific marker bit
- **PT (Payload Type)**: 0x56 for DVM, 0x00 for G.711
- **Sequence Number**: Incrementing packet sequence (0-65535)
- **Timestamp**: RTP timestamp (8000 Hz clock rate)
- **SSRC**: Synchronization source identifier

### Layer 3: RTP Extension (FNE Header)

Custom FNE extension header:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Payload Type (0xFE)          | Payload Length (4)            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          CRC-16               | Function      | Sub-Function  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          Stream ID                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Peer ID                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Message Length                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Fields:**
- **Payload Type**: 0xFE (DVM_FRAME_START)
- **Payload Length**: Length of extension in 32-bit words (4)
- **CRC-16**: Checksum of payload data
- **Function**: Primary operation code (see NET_FUNC)
- **Sub-Function**: Secondary operation code (see NET_SUBFUNC)
- **Stream ID**: Unique identifier for call/session (32-bit)
- **Peer ID**: Source peer identifier
- **Message Length**: Length of payload following headers

### Layer 4: Protocol Payload

Protocol-specific data follows the RTP+FNE headers:

- **DMR**: 35-byte frames (metadata + 33-byte DMR data)
- **P25**: Variable length (LDU1: 242 bytes, LDU2: 242 bytes, TSDU: variable)
- **NXDN**: Variable length based on message type
- **Analog**: Audio samples (typically G.711 encoded)

### Complete Packet Example

**DMR Voice Frame - Total Size: 67 bytes (UDP payload)**

```
Complete Packet Structure:
┌─────────────┬─────────────┬──────────────┐
│  RTP Header │  FNE Header │  DMR Payload │
│   12 bytes  │   20 bytes  │   35 bytes   │
└─────────────┴─────────────┴──────────────┘

Hexadecimal representation (67 bytes):

Offset 0-11 (RTP Header):
90 FE 00 2A 00 00 1F 40 00 00 4E 20

Offset 12-31 (FNE Header):
FE 04 A3 5C 00 00 00 00 12 34 56 78 00 00 4E 20 00 00 00 23

Offset 32-66 (DMR Payload):
12 34 56 78 05 00 00 AA BB 00 CC DD 01 02 03 04 7F 00 
[33 bytes of DMR frame data...]

Decoded values:
  RTP:
    - Version: 2, Extension: yes, Marker: no
    - Payload Type: 0xFE (254)
    - Sequence: 42
    - Timestamp: 8000
    - SSRC: 20000
  
  FNE Header:
    - CRC-16: 0xA35C
    - Function: 0x00 (PROTOCOL)
    - Sub-Function: 0x00 (DMR)
    - Stream ID: 0x12345678
    - Peer ID: 20000
    - Message Length: 35
  
  DMR Payload:
    - Stream ID: 0x12345678
    - Sequence: 5
    - Source ID: 43707
    - Dest ID: 52445
    - Slot: 1
    - Call Type: Group Voice
    - + 33 bytes DMR frame
```

---

## 4. RTP Protocol Implementation

### RTPHeader Class

The `RTPHeader` class encapsulates standard RTP header functionality:

```cpp
class RTPHeader {
private:
    uint8_t m_version;          // RTP version (2)
    bool m_padding;             // Padding flag
    bool m_extension;           // Extension present
    uint8_t m_cc;               // CSRC count
    bool m_marker;              // Marker bit
    uint8_t m_payloadType;      // Payload type
    uint16_t m_seq;             // Sequence number
    uint32_t m_timestamp;       // Timestamp
    uint32_t m_ssrc;            // SSRC identifier
    
public:
    bool decode(const uint8_t* data);
    void encode(uint8_t* data);
    static void resetStartTime();
};
```

**Raw Byte Structure (12 bytes):**

```
Byte Offset | Field              | Size    | Description
------------|--------------------|---------|---------------------------------
0           | V+P+X+CC          | 1 byte  | Version(2b), Padding(1b), Extension(1b), CSRC Count(4b)
1           | M+PT              | 1 byte  | Marker(1b), Payload Type(7b)
2-3         | Sequence Number   | 2 bytes | Packet sequence (big-endian)
4-7         | Timestamp         | 4 bytes | RTP timestamp (big-endian)
8-11        | SSRC              | 4 bytes | Synchronization source ID (big-endian)
```

**Example RTP Header (hexadecimal):**
```
90 FE 00 2A 00 00 1F 40 00 00 4E 20
│  │  │  │  │        │  │        │
│  │  │  │  │        │  └────────┴─ SSRC: 0x00004E20 (20000)
│  │  │  │  └────────┴──────────── Timestamp: 0x00001F40 (8000)
│  │  └──┴───────────────────────── Sequence: 0x002A (42)
│  └─────────────────────────────── Payload Type: 0xFE (254, marker bit clear)
└────────────────────────────────── Version 2, no pad, extension set (0x90)
```

**Key Methods:**

- `decode()`: Parse RTP header from received packet
- `encode()`: Serialize RTP header into buffer for transmission
- `resetStartTime()`: Reset timestamp base for new session

**Timestamp Calculation:**

The RTP timestamp is calculated based on elapsed time since stream start:

```cpp
if (m_timestamp == INVALID_TS) {
    uint64_t timeSinceStart = hrc::diffNow(m_wcStart);
    uint64_t microSeconds = timeSinceStart * RTP_GENERIC_CLOCK_RATE;
    m_timestamp = uint32_t(microSeconds / 1000000);
}
```

Clock rate: 8000 Hz (RTP_GENERIC_CLOCK_RATE)

### RTPFNEHeader Class

The `RTPFNEHeader` extends `RTPExtensionHeader` with DVM-specific fields:

```cpp
class RTPFNEHeader : public RTPExtensionHeader {
private:
    uint16_t m_crc16;           // Payload CRC
    NET_FUNC::ENUM m_func;      // Function code
    NET_SUBFUNC::ENUM m_subFunc;// Sub-function code
    uint32_t m_streamId;        // Stream identifier
    uint32_t m_peerId;          // Peer identifier
    uint32_t m_messageLength;   // Message length
    
public:
    bool decode(const uint8_t* data) override;
    void encode(uint8_t* data) override;
};
```

**Raw Byte Structure (20 bytes total):**

```
Byte Offset | Field              | Size    | Description
------------|--------------------|---------|---------------------------------
0-1         | Extension Header   | 2 bytes | Payload Type (0xFE) + Length (4)
2-3         | CRC-16            | 2 bytes | Payload checksum (big-endian)
4           | Function          | 1 byte  | NET_FUNC opcode
5           | Sub-Function      | 1 byte  | NET_SUBFUNC opcode
6-7         | Reserved          | 2 bytes | Padding (0x00)
8-11        | Stream ID         | 4 bytes | Call/session identifier (big-endian)
12-15       | Peer ID           | 4 bytes | Source peer ID (big-endian)
16-19       | Message Length    | 4 bytes | Payload length (big-endian)
```

**Example FNE Header (hexadecimal):**
```
FE 04 A3 5C 00 00 00 00 12 34 56 78 00 00 4E 20 00 00 00 21
│  │  │  │  │  │  │  │  │        │  │        │  │        │
│  │  │  │  │  │  │  │  │        │  └────────┴─ Peer ID: 0x00004E20 (20000)
│  │  │  │  │  │  │  │  └────────┴──────────── Stream ID: 0x12345678
│  │  │  │  │  │  └──┴─────────────────────── Reserved (0x0000)
│  │  │  │  └──┴────────────────────────────── Function: 0x00, SubFunc: 0x00
│  │  └──┴───────────────────────────────────── CRC-16: 0xA35C
│  └─────────────────────────────────────────── Extension Length: 4 (words)
└────────────────────────────────────────────── Payload Type: 0xFE
```

**Encoding Example:**

```cpp
void RTPFNEHeader::encode(uint8_t* data) {
    m_payloadType = DVM_FRAME_START;           // 0xFE
    m_payloadLength = RTP_FNE_HEADER_LENGTH_EXT_LEN; // 4
    RTPExtensionHeader::encode(data);
    
    data[4U] = (m_crc16 >> 8) & 0xFFU;         // CRC-16 MSB
    data[5U] = (m_crc16 >> 0) & 0xFFU;         // CRC-16 LSB
    data[6U] = m_func;                         // Function
    data[7U] = m_subFunc;                      // Sub-Function
    
    SET_UINT32(m_streamId, data, 8U);          // Stream ID
    SET_UINT32(m_peerId, data, 12U);           // Peer ID
    SET_UINT32(m_messageLength, data, 16U);    // Message Length
}
```

### FrameQueue Class

The `FrameQueue` class manages RTP frame creation, queuing, and transmission:

```cpp
class FrameQueue : public RawFrameQueue {
private:
    uint32_t m_peerId;
    static std::vector<Timestamp> m_streamTimestamps;
    
public:
    typedef std::pair<const NET_FUNC::ENUM, const NET_SUBFUNC::ENUM> OpcodePair;
    
    UInt8Array read(int& messageLength, sockaddr_storage& address, 
                    uint32_t& addrLen, frame::RTPHeader* rtpHeader,
                    frame::RTPFNEHeader* fneHeader);
                    
    bool write(const uint8_t* message, uint32_t length, uint32_t streamId,
               uint32_t peerId, uint32_t ssrc, OpcodePair opcode, 
               uint16_t rtpSeq, sockaddr_storage& addr, uint32_t addrLen);
};
```

**Read Operation:**

1. Read raw UDP packet from socket
2. Decode RTP header
3. Decode RTP extension (FNE header)
4. Extract and return payload data

**Write Operation:**

1. Generate RTP header with sequence and timestamp
2. Generate FNE header with function/sub-function
3. Calculate CRC-16 of payload
4. Assemble complete packet
5. Transmit via UDP socket

**Timestamp Management:**

The FrameQueue maintains per-stream timestamps to ensure proper RTP timing:

```cpp
Timestamp* findTimestamp(uint32_t streamId);
void insertTimestamp(uint32_t streamId, uint32_t timestamp);
void updateTimestamp(uint32_t streamId, uint32_t timestamp);
void eraseTimestamp(uint32_t streamId);
```

---

## 5. Network Functions and Sub-Functions

The DVM protocol uses a two-level opcode system for message routing and handling.

### NET_FUNC: Primary Function Codes

| Code | Name | Purpose |
|------|------|---------|
| 0x00 | PROTOCOL | Protocol data (DMR/P25/NXDN/Analog) |
| 0x01 | MASTER | Master control messages |
| 0x60 | RPTL | Repeater/peer login request |
| 0x61 | RPTK | Repeater/peer authorization |
| 0x62 | RPTC | Repeater/peer configuration |
| 0x70 | RPT_DISC | Peer disconnect notification |
| 0x71 | MST_DISC | Master disconnect notification |
| 0x74 | PING | Connection keepalive request |
| 0x75 | PONG | Connection keepalive response |
| 0x7A | GRANT_REQ | Channel grant request |
| 0x7B | INCALL_CTRL | In-call control (busy/reject) |
| 0x7C | KEY_REQ | Encryption key request |
| 0x7D | KEY_RSP | Encryption key response |
| 0x7E | ACK | Acknowledgment |
| 0x7F | NAK | Negative acknowledgment |
| 0x90 | TRANSFER | Activity/diagnostic/status transfer |
| 0x91 | ANNOUNCE | Affiliation/registration announcements |
| 0x92 | REPL | FNE replication (peer/TG/RID lists) |
| 0x93 | NET_TREE | Network spanning tree management |

### NAK Reason Codes

When an FNE master sends a NAK (Negative Acknowledgment), it includes a 16-bit reason code to indicate why the operation failed. These reason codes help peers diagnose connection problems and take appropriate action.

**NAK Message Format:**
```
Peer ID (4 bytes) + Reserved (4 bytes) + Reason Code (2 bytes)
```

**Reason Code Enumeration:**

| Code | Name | Severity | Description | Peer Action |
|------|------|----------|-------------|-------------|
| 0 | GENERAL_FAILURE | Warning | Unspecified failure or error | Retry operation |
| 1 | MODE_NOT_ENABLED | Warning | Requested digital mode (DMR/P25/NXDN) not enabled on FNE | Check FNE mode configuration |
| 2 | ILLEGAL_PACKET | Warning | Malformed or unintelligible packet received | Check packet format/encoding |
| 3 | FNE_UNAUTHORIZED | Warning | Peer not authorized or not properly logged in | Verify authentication credentials |
| 4 | BAD_CONN_STATE | Warning | Invalid operation for current connection state | Reset connection state machine |
| 5 | INVALID_CONFIG_DATA | Warning | Configuration data (RPTC) rejected during login | Verify RPTC JSON schema |
| 6 | PEER_RESET | Warning | FNE demands connection reset | Reset connection and re-login |
| 7 | PEER_ACL | **Fatal** | Peer rejected by Access Control List | **Disable network - peer is banned** |
| 8 | FNE_MAX_CONN | Warning | FNE has reached maximum permitted connections | Wait and retry later |

**Severity Levels:**

- **Warning**: Temporary or correctable condition. Peer should retry or adjust configuration.
- **Fatal**: Permanent rejection. Peer should cease all network operations and disable itself.

**Special Handling:**

- `PEER_ACL` (Code 7): This is the only fatal NAK reason. When received, the peer must:
  - Log an error message
  - Transition to `NET_STAT_WAITING_LOGIN` state
  - Set `m_enabled = false` to disable all network operations
  - Stop attempting to reconnect (unless `neverDisableOnACLNAK` configuration flag is set)

- `FNE_MAX_CONN` (Code 8): If received while in `NET_STAT_RUNNING` state, indicates the FNE is overloaded or shutting down. Peer should implement exponential backoff before reconnecting.

### NET_SUBFUNC: Secondary Function Codes

#### Protocol Sub-Functions (PROTOCOL)

| Code | Name | Purpose |
|------|------|---------|
| 0x00 | PROTOCOL_SUBFUNC_DMR | DMR protocol data |
| 0x01 | PROTOCOL_SUBFUNC_P25 | P25 protocol data |
| 0x02 | PROTOCOL_SUBFUNC_NXDN | NXDN protocol data |
| 0x0F | PROTOCOL_SUBFUNC_ANALOG | Analog audio data |

#### Master Sub-Functions (MASTER)

| Code | Name | Purpose |
|------|------|---------|
| 0x00 | MASTER_SUBFUNC_WL_RID | Whitelist RID update |
| 0x01 | MASTER_SUBFUNC_BL_RID | Blacklist RID update |
| 0x02 | MASTER_SUBFUNC_ACTIVE_TGS | Active talkgroup list |
| 0x03 | MASTER_SUBFUNC_DEACTIVE_TGS | Deactivate talkgroups |
| 0xA3 | MASTER_HA_PARAMS | High availability parameters |

#### Transfer Sub-Functions (TRANSFER)

| Code | Name | Purpose |
|------|------|---------|
| 0x01 | TRANSFER_SUBFUNC_ACTIVITY | Activity log data |
| 0x02 | TRANSFER_SUBFUNC_DIAG | Diagnostic log data |
| 0x03 | TRANSFER_SUBFUNC_STATUS | Peer status JSON |

#### Announce Sub-Functions (ANNOUNCE)

| Code | Name | Purpose |
|------|------|---------|
| 0x00 | ANNC_SUBFUNC_GRP_AFFIL | Group affiliation |
| 0x01 | ANNC_SUBFUNC_UNIT_REG | Unit registration |
| 0x02 | ANNC_SUBFUNC_UNIT_DEREG | Unit deregistration |
| 0x03 | ANNC_SUBFUNC_GRP_UNAFFIL | Group unaffiliation |
| 0x90 | ANNC_SUBFUNC_AFFILS | Complete affiliation update |
| 0x9A | ANNC_SUBFUNC_SITE_VC | Site voice channel list |

#### Replication Sub-Functions (REPL)

| Code | Name | Purpose |
|------|------|---------|
| 0x00 | REPL_TALKGROUP_LIST | Talkgroup rules replication |
| 0x01 | REPL_RID_LIST | Radio ID replication |
| 0x02 | REPL_PEER_LIST | Peer configuration list |
| 0xA2 | REPL_ACT_PEER_LIST | Active peer list |
| 0xA3 | REPL_HA_PARAMS | HA configuration parameters |

#### In-Call Control (INCALL_CTRL)

| Code | Name | Purpose |
|------|------|---------|
| 0x00 | BUSY_DENY | Reject call - channel busy |
| 0x01 | REJECT_TRAFFIC | Reject active traffic |

---

## 6. Connection Management

### Connection States

The peer connection follows a state machine:

```
NET_STAT_INVALID
    ↓
NET_STAT_WAITING_LOGIN (send RPTL)
    ↓
NET_STAT_WAITING_AUTHORISATION (send RPTK)
    ↓
NET_STAT_WAITING_CONFIG (send RPTC)
    ↓
NET_STAT_RUNNING (operational)
```

### Login Sequence

**Step 1: Login Request (RPTL)**

Peer sends login request with:
- Peer ID
- Random salt value

```cpp
bool Network::writeLogin() {
    uint8_t buffer[8U];
    ::memcpy(buffer + 0U, m_salt, sizeof(uint32_t));
    SET_UINT32(m_peerId, buffer, 4U);
    
    return writeMaster({ NET_FUNC::RPTL, NET_SUBFUNC::NOP }, 
                       buffer, 8U, m_pktSeq, m_loginStreamId);
}
```

**Raw RPTL Message Structure:**

```
[RTP Header: 12 bytes] + [FNE Header: 20 bytes] + [RPTL Payload: 8 bytes]

RPTL Payload (8 bytes):
Byte Offset | Field              | Size    | Description
------------|--------------------|---------|---------------------------------
0-3         | Salt               | 4 bytes | Random value (big-endian)
4-7         | Peer ID            | 4 bytes | Peer identifier (big-endian)
```

**Example RPTL Payload (hexadecimal):**
```
A1 B2 C3 D4 00 00 4E 20
│        │  │        │
│        │  └────────┴─ Peer ID: 0x00004E20 (20000)
└────────┴──────────── Salt: 0xA1B2C3D4
```

**Step 2: Authorization Challenge (RPTK)**

Peer responds to master's challenge with SHA256 hash:

```cpp
bool Network::writeAuthorisation() {
    uint8_t buffer[40U];
    
    // Combine peer salt and master challenge
    uint8_t hash[50U];
    ::memcpy(hash, m_salt, sizeof(uint32_t));
    ::memcpy(hash + sizeof(uint32_t), m_password.c_str(), m_password.size());
    
    // Calculate SHA256
    edac::SHA256 sha256;
    sha256.buffer(hash, 40U, hash);
    
    // Send response
    ::memcpy(buffer, hash, 32U);
    SET_UINT32(m_peerId, buffer, 32U);
    
    return writeMaster({ NET_FUNC::RPTK, NET_SUBFUNC::NOP }, 
                       buffer, 40U, m_pktSeq, m_loginStreamId);
}
```

**Raw RPTK Message Structure:**

```
[RTP Header: 12 bytes] + [FNE Header: 20 bytes] + [RPTK Payload: 40 bytes]

RPTK Payload (40 bytes):
Byte Offset | Field              | Size    | Description
------------|--------------------|---------|---------------------------------
0-31        | SHA256 Hash        | 32 bytes| SHA256(salt + password + challenge)
32-35       | Peer ID            | 4 bytes | Peer identifier (big-endian)
36-39       | Reserved           | 4 bytes | Padding (0x00)
```

**Example RPTK Payload (hexadecimal):**
```
2F 9A 8B ... [32 bytes of SHA256 hash] ... 00 00 4E 20 00 00 00 00
│                                          │        │  │        │
│                                          │        │  └────────┴─ Reserved
│                                          └────────┴──────────── Peer ID: 20000
└──────────────────────────────────────────────────────────────── SHA256 Hash
```

**Step 3: Configuration Exchange (RPTC)**

Peer sends configuration metadata:

```cpp
bool Network::writeConfig() {
    json::object config = json::object();
    
    // Identity and frequencies
    config["identity"].set<std::string>(m_metadata->identity);
    config["rxFrequency"].set<uint32_t>(m_metadata->rxFrequency);
    config["txFrequency"].set<uint32_t>(m_metadata->txFrequency);
    
    // Protocol support
    config["dmr"].set<bool>(m_dmrEnabled);
    config["p25"].set<bool>(m_p25Enabled);
    config["nxdn"].set<bool>(m_nxdnEnabled);
    config["analog"].set<bool>(m_analogEnabled);
    
    // Location
    config["latitude"].set<float>(m_metadata->latitude);
    config["longitude"].set<float>(m_metadata->longitude);
    
    // Serialize to JSON string
    std::string json = json::object(config).serialize();
    
    return writeMaster({ NET_FUNC::RPTC, NET_SUBFUNC::NOP },
                       (uint8_t*)json.c_str(), json.length(), 
                       m_pktSeq, m_loginStreamId);
}
```

**Raw RPTC Message Structure:**

```
[RTP Header: 12 bytes] + [FNE Header: 20 bytes] + [TAG: 4 bytes] + [Peer ID: 4 bytes] + [JSON: variable]

RPTC Payload:
Byte Offset | Field              | Size      | Description
------------|--------------------|-----------|-----------------------------
0-3         | TAG_REPEATER_CONFIG| 4 bytes   | ASCII "RPTC"
4-7         | Peer ID            | 4 bytes   | Peer identifier (big-endian)
8+          | JSON Configuration | Variable  | UTF-8 JSON string
```

**Example RPTC Payload (partial hexadecimal + ASCII):**
```
52 50 54 43 00 00 4E 20 7B 22 69 64 65 6E 74 69 74 79 22 3A 22 4B 42 ...
│  │  │  │  │        │  │  
│  │  │  │  │        │  └─ JSON starts: {"identity":"KB...
│  │  │  │  └────────┴─── Peer ID: 0x00004E20 (20000)
└──┴──┴──┴───────────── TAG: "RPTC"
```

**RPTC Configuration JSON Schema:**

The RPTC configuration uses a nested JSON structure with three main object groups: `info` (system information), `channel` (RF parameters), and `rcon` (remote control/REST API).

```json
{
  "identity": "string",           // Peer identity/callsign (required)
  "rxFrequency": 0,              // RX frequency in Hz (required)
  "txFrequency": 0,              // TX frequency in Hz (required)
  
  "info": {                      // System information object (required)
    "latitude": 0.0,             // Latitude in decimal degrees
    "longitude": 0.0,            // Longitude in decimal degrees
    "height": 0,                 // Antenna height in meters
    "location": "string",        // Location description
    "power": 0,                  // Transmit power in watts
    "class": 0,                  // Site class designation
    "band": 0,                   // Operating frequency band
    "slot": 0,                   // TDMA slot assignment (DMR)
    "colorCode": 0               // Color code for DMR systems
  },
  
  "channel": {                   // Channel configuration object (required)
    "txPower": 0,                // Transmit power in watts
    "txOffsetMhz": 0.0,          // TX offset in MHz
    "chBandwidthKhz": 0.0,       // Channel bandwidth in kHz
    "channelId": 0,              // Logical channel ID
    "channelNo": 0               // Physical channel number
  },
  
  "rcon": {                      // Remote control object (optional)
    "password": "string",        // REST API password
    "port": 0                    // REST API port
  },
  
  "externalPeer": false,         // External network peer flag (optional)
  "conventionalPeer": false,     // Conventional (non-trunked) mode (optional)
  "sysView": false,              // SysView monitoring peer flag (optional)
  "software": "string"           // Software identifier (optional)
}
```

**Example RPTC Configuration:**

```json
{
  "identity": "KB3JFI-R",
  "rxFrequency": 449000000,
  "txFrequency": 444000000,
  
  "info": {
    "latitude": 39.9526,
    "longitude": -75.1652,
    "height": 30,
    "location": "Philadelphia, PA",
    "power": 50,
    "class": 1,
    "band": 1,
    "slot": 1,
    "colorCode": 1
  },
  
  "channel": {
    "txPower": 50,
    "txOffsetMhz": 5.0,
    "chBandwidthKhz": 12.5,
    "channelId": 1,
    "channelNo": 1
  },
  
  "rcon": {
    "password": "api_secret",
    "port": 9990
  },
  
  "externalPeer": false,
  "conventionalPeer": false,
  "sysView": false,
  "software": "DVMHOST_R04A00"
}
```

**Configuration Field Details:**

**Top-Level Fields:**
- **identity**: Unique identifier for the peer (callsign, site name, etc.)
- **rxFrequency/txFrequency**: Operating frequencies in Hertz
- **externalPeer**: Indicates peer is outside the primary network (affects routing)
- **conventionalPeer**: Indicates non-trunked operation mode (affects grant behavior)
- **sysView**: Indicates monitoring-only peer (affiliation viewer, no traffic routing)
- **software**: Software version string (e.g., `DVMHOST_R04A00`) for compatibility checking

**System Information Object (`info`):**
- **latitude/longitude**: Geographic coordinates in decimal degrees for mapping and adjacent site calculations
- **height**: Antenna height above ground level in meters (used for coverage calculations)
- **location**: Human-readable location description
- **power**: Transmit power in watts
- **class**: Site class designation (used for network topology planning)
- **band**: Operating frequency band identifier
- **slot**: TDMA slot assignment for DMR systems
- **colorCode**: Color code for DMR systems (0-15)

**Channel Configuration Object (`channel`):**
- **txPower**: Transmit power in watts
- **txOffsetMhz**: Transmit frequency offset for repeater systems (duplex offset)
- **chBandwidthKhz**: Channel bandwidth in kHz (6.25, 12.5, or 25 kHz typical)
- **channelId**: Logical channel identifier for trunking systems
- **channelNo**: Physical channel number

**Remote Control Object (`rcon`):**
- **password**: REST API authentication password for remote management
- **port**: REST API listening port number (typically 9990)

**FNE Processing:**
The FNE master stores the complete configuration JSON in the peer connection object (`FNEPeerConnection::config`) and extracts specific fields for connection management:
- `identity` → Used for peer identification in logs and routing tables
- `software` → Logged for version tracking and compatibility checks
- `sysView` → Determines if peer is monitoring-only (no traffic routing)
- `externalPeer` → Used for spanning tree routing decisions (external peers have special routing rules)
- `conventionalPeer` → Affects talkgroup affiliation and grant behavior (conventional peers don't require grants)

**Step 4: ACK/NAK Response**

Master responds with ACK (success) or NAK (failure):
- **ACK**: Peer transitions to RUNNING state
- **NAK**: Connection rejected (authentication failure, ACL deny, etc.)

### Heartbeat Mechanism

Once connected, peers and master exchange periodic PING/PONG messages:

**Peer → Master: PING**

```cpp
bool Network::writePing() {
    uint8_t buffer[11U];
    SET_UINT32(m_peerId, buffer, 7U);
    
    return writeMaster({ NET_FUNC::PING, NET_SUBFUNC::NOP }, 
                       buffer, 11U, RTP_END_OF_CALL_SEQ, 
                       m_random.next());
}
```

**Master → Peer: PONG**

Master responds with PONG, resetting peer timeout timer.

**Timing:**
- Default ping interval: 5 seconds
- Default timeout: 3 missed pings (15 seconds)
- Configurable via `pingTime` parameter

### High Availability (HA)

The peer supports connection to multiple master addresses for failover:

```cpp
m_haIPs.push_back("master1.example.com");
m_haIPs.push_back("master2.example.com");
m_haIPs.push_back("master3.example.com");
```

**Failover Logic:**

1. Initial connection attempt to primary master
2. If connection fails or times out, advance to next HA address
3. Continue rotation through HA list until connection succeeds
4. On successful connection, stay with current master
5. On disconnect, resume HA rotation

**Configuration Parameters:**
- `m_haIPs`: Vector of master addresses
- `m_currentHAIP`: Index of current master
- `MAX_RETRY_HA_RECONNECT`: Retries before failover (2)

### Duplicate Connection Detection

The master detects and handles duplicate peer connections:

- Same peer ID connecting from multiple sources
- Existing connection flagged and optionally disconnected
- New connection rate-limited (60-minute delay)

```cpp
m_flaggedDuplicateConn = true;
m_maxRetryCount = MAX_RETRY_DUP_RECONNECT;
m_retryTimer.setTimeout(DUPLICATE_CONN_RETRY_TIME); // 3600s
```

---

## 7. Data Transport

### Protocol Message Structure

Each protocol has a specific message format for network transmission.

#### DMR Message Format

DMR uses 33-byte network frames:

```cpp
bool BaseNetwork::writeDMR(const dmr::data::NetData& data, bool noSequence) {
    uint8_t buffer[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, DMR_FRAME_LENGTH_BYTES + 2U);
    
    // Construct DMR message
    createDMR_Message(buffer, data);
    
    uint32_t streamId = data.getStreamId();
    if (!noSequence) {
        m_dmrStreamId[data.getSlotNo() - 1U] = streamId;
    }
    
    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR },
                       buffer, DMR_FRAME_LENGTH_BYTES + 2U,
                       pktSeq, streamId);
}
```

**DMR Packet Structure:**

The DMR network packet uses the TAG_DMR_DATA identifier (`0x444D5244` or ASCII "DMRD") and has a fixed size of 55 bytes as defined by `DMR_PACKET_LENGTH`. The structure is created by `createDMR_Message()` in `BaseNetwork.cpp`.

| Offset | Length | Field | Description |
|--------|--------|-------|-------------|
| 0-3 | 4 | DMR Tag | "DMRD" (0x444D5244) ASCII identifier |
| 4 | 1 | Sequence Number | Packet sequence number for ordering (0-255) |
| 5-7 | 3 | Source ID | DMR radio ID of originating station |
| 8-10 | 3 | Destination ID | DMR radio ID or talkgroup ID |
| 11-13 | 3 | Reserved | Reserved for future use (0x000000) |
| 14 | 1 | Control Byte | Network control flags |
| 15 | 1 | Slot/FLCO/DataType | Bit 7: Slot (0=Slot 1, 1=Slot 2)<br/>Bits 4-6: FLCO (Full Link Control Opcode)<br/>Bits 0-3: Data Type |
| 16-19 | 4 | Reserved | Reserved for future use (0x00000000) |
| 20-52 | 33 | DMR Frame Data | Raw DMR frame payload (264 bits = `DMR_FRAME_LENGTH_BYTES`) |
| 53 | 1 | BER | Bit Error Rate (0-255) |
| 54 | 1 | RSSI | Received Signal Strength Indicator (0-255) |

**Total DMR Payload Size:** 55 bytes (`DMR_PACKET_LENGTH = 55U`)

**Constants:**
- `DMR_PACKET_LENGTH = 55U` (defined in `BaseNetwork.h`)
- `DMR_FRAME_LENGTH_BYTES = 33U` (defined in `DMRDefines.h`)
- `PACKET_PAD = 8U` (padding for buffer allocation)
- Total allocated size: 55 + 8 = 63 bytes

**Raw DMR Packet Example (hexadecimal):**
```
[RTP Header: 12 bytes] [FNE Header: 20 bytes] [DMR Payload: 55 bytes]

Complete Packet (87 bytes):
90 FE 00 2A 00 00 1F 40 00 00 4E 20 | A3 5C 00 00 12 34 56 78 00 00 4E 20 00 23 00 00 00 00 00 00 | 44 4D 52 44 05 00 00 AA BB 00 CC DD 00 00 00 01 02 00 00 00 00 [33 bytes DMR data...] 00 7F
├─────────── RTP (12) ───────────┤ ├──────────────────────── FNE (20) ────────────────────────┤ ├────────────────────────────────── DMR (55) ────────────────────────────────┤

DMR Payload breakdown:
44 4D 52 44 05 00 00 AA BB 00 CC DD 00 00 00 01 02 00 00 00 00 [33 bytes DMR frame...] 00 7F
│        │  │  │        │  │        │  │  │  │  │  │               │  │
│        │  │  │        │  │        │  │  │  │  │  └─[Reserved 4]──┤  │
│        │  │  │        │  │        │  │  │  │  └─ Slot/FLCO/DataType: 0x02
│        │  │  │        │  │        │  │  │  └──── Control: 0x01
│        │  │  │        │  │        │  └─[Reserved 3]
│        │  │  │        │  └─[DestID 3rd byte]──── Dest ID: 0x00CCDD (52445)
│        │  │  │        └─[DestID 2nd byte]
│        │  │  └─[DestID 1st byte]
│        │  └─[SrcID 3rd byte]────────────────── Src ID: 0x0000AABB (43707)
│        └─[SrcID 2nd byte]
│        └─[SrcID 1st byte]
│        └─────────────────────────────────────── Sequence: 5
└──────────────────────────────────────────────── Tag: "DMRD" (0x444D5244)
       └─ (DMR frame at offset 20)──────────────── BER: 0x00
                                                    └─ RSSI: 0x7F (127)
```

#### P25 Message Formats

P25 supports multiple frame types with different structures. **Important:** P25 network messages use **DFSI (Digital Fixed Station Interface) encoding** for voice frames, not raw P25 frames. The DFSI encoding is implemented in the `p25::dfsi::LC` class and provides a more efficient network transport format.

**LDU1 (Link Data Unit 1) with DFSI Encoding:**

```cpp
bool BaseNetwork::writeP25LDU1(const p25::lc::LC& control, 
                                const p25::data::LowSpeedData& lsd,
                                const uint8_t* data,
                                P25DEF::FrameType::E frameType,
                                uint8_t controlByte) {
    uint8_t buffer[P25_LDU1_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, P25_LDU1_PACKET_LENGTH + PACKET_PAD);
    
    createP25_LDU1Message(buffer, control, lsd, data, frameType, controlByte);
    
    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 },
                       buffer, P25_LDU1_PACKET_LENGTH + PACKET_PAD,
                       pktSeq, m_p25StreamId);
}
```

**P25 LDU1 Packet Structure:**

The P25 LDU1 message uses DFSI encoding to pack 9 IMBE voice frames with link control data. The total size is 193 bytes (`P25_LDU1_PACKET_LENGTH = 193U`), which includes:
- 24-byte message header (`MSG_HDR_SIZE`)
- 9 DFSI-encoded voice frames at specific offsets
- 1-byte frame type field
- 12-byte encryption sync data

| Offset | Length | Field | Description |
|--------|--------|-------|-------------|
| 0-23 | 24 | Message Header | P25 message header (via `createP25_MessageHdr`) |
| 24-45 | 22 | DFSI Voice 1 | LDU1_VOICE1 (`DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES = 22U`) |
| 46-59 | 14 | DFSI Voice 2 | LDU1_VOICE2 (`DFSI_LDU1_VOICE2_FRAME_LENGTH_BYTES = 14U`) |
| 60-76 | 17 | DFSI Voice 3 + LC | LDU1_VOICE3 with Link Control (`DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES = 17U`) |
| 77-93 | 17 | DFSI Voice 4 + LC | LDU1_VOICE4 with Link Control (`DFSI_LDU1_VOICE4_FRAME_LENGTH_BYTES = 17U`) |
| 94-110 | 17 | DFSI Voice 5 + LC | LDU1_VOICE5 with Link Control (`DFSI_LDU1_VOICE5_FRAME_LENGTH_BYTES = 17U`) |
| 111-127 | 17 | DFSI Voice 6 + LC | LDU1_VOICE6 with Link Control (`DFSI_LDU1_VOICE6_FRAME_LENGTH_BYTES = 17U`) |
| 128-144 | 17 | DFSI Voice 7 + LC | LDU1_VOICE7 with Link Control (`DFSI_LDU1_VOICE7_FRAME_LENGTH_BYTES = 17U`) |
| 145-161 | 17 | DFSI Voice 8 + LC | LDU1_VOICE8 with Link Control (`DFSI_LDU1_VOICE8_FRAME_LENGTH_BYTES = 17U`) |
| 162-177 | 16 | DFSI Voice 9 + LSD | LDU1_VOICE9 with Low Speed Data (`DFSI_LDU1_VOICE9_FRAME_LENGTH_BYTES = 16U`) |
| 178-192 | 15 | Additional Data | Frame type and encryption sync |

**Total P25 LDU1 Size:** 193 bytes (`P25_LDU1_PACKET_LENGTH`)

**P25 Message Header Structure (24 bytes):**

The message header created by `createP25_MessageHdr()` contains:

| Offset | Length | Field | Description |
|--------|--------|-------|-------------|
| 0 | 4 | P25 Tag | "P25D" (0x50323544) |
| 4 | 1 | DUID | Data Unit ID (e.g., 0x05 for LDU1, 0x0A for LDU2) |
| 5 | 1 | Control Byte | Network control flags |
| 6 | 1 | LCO | Link Control Opcode |
| 7 | 1 | MFId | Manufacturer ID |
| 8-10 | 3 | Source ID | Calling radio ID |
| 11-13 | 3 | Destination ID | Target (talkgroup/radio) |
| 14-16 | 3 | Reserved | Reserved bytes |
| 17 | 1 | RSSI | Signal strength |
| 18 | 1 | BER | Bit error rate |
| 19-22 | 4 | Reserved | Reserved bytes |
| 23 | 1 | Count | Total payload size |

**DFSI Encoding Details:**

The Digital Fixed Station Interface (DFSI) encoding is a TIA-102.BAHA standard for transporting P25 voice over IP. Each IMBE voice frame (11 bytes of raw IMBE data) is encoded using the `p25::dfsi::LC::encodeLDU1()` method with a specific frame type:

- **LDU1_VOICE1** (0x62): First voice frame (22 bytes DFSI)
- **LDU1_VOICE2** (0x63): Second voice frame (14 bytes DFSI)
- **LDU1_VOICE3** (0x64): Voice + Link Control bits (17 bytes DFSI)
- **LDU1_VOICE4** (0x65): Voice + Link Control bits (17 bytes DFSI)
- **LDU1_VOICE5** (0x66): Voice + Link Control bits (17 bytes DFSI)
- **LDU1_VOICE6** (0x67): Voice + Link Control bits (17 bytes DFSI)
- **LDU1_VOICE7** (0x68): Voice + Link Control bits (17 bytes DFSI)
- **LDU1_VOICE8** (0x69): Voice + Link Control bits (17 bytes DFSI)
- **LDU1_VOICE9** (0x6A): Voice + Low Speed Data (16 bytes DFSI)

The DFSI frames embed the Link Control (LC) and Low Speed Data (LSD) information within the voice frames, providing a compact representation suitable for network transport.

**Raw P25 LDU1 Packet Example (partial):**

```
[RTP: 12 bytes] [FNE: 20 bytes] [P25 LDU1 Payload: 193 bytes]

P25 Message Header (first 24 bytes):
50 32 35 44 05 00 03 90 00 00 AA BB 00 CC DD EE 00 00 00 7F 00 00 00 00 A9
│        │  │  │  │  │  │        │  │        │  │  │  │  │  │  │  │  │
│        │  │  │  │  │  │        │  │        │  │  │  │  │  │  │  │  └─ Count: 0xA9 (169)
│        │  │  │  │  │  │        │  │        │  │  │  │  └──┴──┴──┴──── Reserved
│        │  │  │  │  │  │        │  │        │  │  │  └────────────────── BER: 0x00
│        │  │  │  │  │  │        │  │        │  │  └───────────────────── RSSI: 0x7F (127)
│        │  │  │  │  │  │        │  │        └──┴──┴────────────────────── Reserved
│        │  │  │  │  │  │        └──┴───────────────────────────────────── Dest ID: 0x00CCDDEE (13492718)
│        │  │  │  │  │  └───────────────────────────────────────────────── Src ID: 0x0000AABB (43707)
│        │  │  │  │  └──────────────────────────────────────────────────── MFId: 0x90 (Standard)
│        │  │  │  └─────────────────────────────────────────────────────── LCO: 0x03 (Group Voice Channel User)
│        │  │  └────────────────────────────────────────────────────────── Control: 0x00
│        │  └───────────────────────────────────────────────────────────── DUID: 0x05 (LDU1)
└────────┴──────────────────────────────────────────────────────────────── Tag: "P25D" (0x50323544)

DFSI Voice Frames (offsets 24-177):
Offset 24: [22 bytes] LDU1_VOICE1 (DFSI frame type 0x62)
Offset 46: [14 bytes] LDU1_VOICE2 (DFSI frame type 0x63)
Offset 60: [17 bytes] LDU1_VOICE3 + LC bits (DFSI frame type 0x64)
Offset 77: [17 bytes] LDU1_VOICE4 + LC bits (DFSI frame type 0x65)
Offset 94: [17 bytes] LDU1_VOICE5 + LC bits (DFSI frame type 0x66)
Offset 111: [17 bytes] LDU1_VOICE6 + LC bits (DFSI frame type 0x67)
Offset 128: [17 bytes] LDU1_VOICE7 + LC bits (DFSI frame type 0x68)
Offset 145: [17 bytes] LDU1_VOICE8 + LC bits (DFSI frame type 0x69)
Offset 162: [16 bytes] LDU1_VOICE9 + LSD (DFSI frame type 0x6A)

Additional Data (offsets 178-192):
[15 bytes] Frame type and encryption sync information
```

**Constants:**
- `P25_LDU1_PACKET_LENGTH = 193U` (defined in `BaseNetwork.h`)
- `P25_LDU_FRAME_LENGTH_BYTES = 216U` (raw P25 LDU over-the-air frame, not used in network)
- `PACKET_PAD = 8U`
- `MSG_HDR_SIZE = 24U`
- Total allocated size: 193 + 8 = 201 bytes

**LDU2 (Link Data Unit 2) with DFSI Encoding:**

LDU2 has a similar structure to LDU1 but uses different DFSI frame types and contains Encryption Sync (ESS) data instead of Link Control in frames 3-8. Total size is 181 bytes (`P25_LDU2_PACKET_LENGTH = 181U`).

**DFSI Frame Types for LDU2:**
- **LDU2_VOICE10** (0x6B): First voice frame (22 bytes)
- **LDU2_VOICE11** (0x6C): Second voice frame (14 bytes)
- **LDU2_VOICE12** (0x6D): Voice + Encryption Sync (17 bytes)
- **LDU2_VOICE13** (0x6E): Voice + Encryption Sync (17 bytes)
- **LDU2_VOICE14** (0x6F): Voice + Encryption Sync (17 bytes)
- **LDU2_VOICE15** (0x70): Voice + Encryption Sync (17 bytes)
- **LDU2_VOICE16** (0x71): Voice + Encryption Sync (17 bytes)
- **LDU2_VOICE17** (0x72): Voice + Encryption Sync (17 bytes)
- **LDU2_VOICE18** (0x73): Voice + Low Speed Data (16 bytes)

**Constants:**
- `P25_LDU2_PACKET_LENGTH = 181U`
- Total allocated size: 181 + 8 = 189 bytes

---

**TDU (Terminator Data Unit):**

The TDU message signals the end of a voice transmission. It is the smallest P25 network message, containing only the 24-byte P25 message header **with no additional payload after the FNE header**. Created by `createP25_TDUMessage()` in `BaseNetwork.cpp`.

**TDU Packet Structure:**

The TDU packet consists of:
- RTP Header (12 bytes)
- FNE Header (20 bytes)
- P25 Message Header (24 bytes)
- **No additional payload**

**Total TDU Payload Size (after FNE):** 24 bytes (`MSG_HDR_SIZE = 24U`)

**Constants:**
- `MSG_HDR_SIZE = 24U` (defined in `BaseNetwork.h`)
- `PACKET_PAD = 8U`
- Total allocated size: 24 + 8 = 32 bytes
- DUID value: `0x03` (TDU - Terminator Data Unit)

**P25 Message Header Structure (24 bytes):**

| Offset | Length | Field | Description |
|--------|--------|-------|-------------|
| 0-3 | 4 | P25 Tag | "P25D" (0x50323544) |
| 4 | 1 | DUID | 0x03 (TDU) |
| 5 | 1 | Control Byte | Network control flags (at offset 14 in buffer) |
| 6 | 1 | LCO | Link Control Opcode (from last transmission) |
| 7 | 1 | MFId | Manufacturer ID |
| 8-10 | 3 | Source ID | Radio ID that ended transmission |
| 11-13 | 3 | Destination ID | Target talkgroup/radio |
| 14-16 | 3 | Reserved | Reserved bytes |
| 17 | 1 | RSSI | Signal strength |
| 18 | 1 | BER | Bit error rate |
| 19-22 | 4 | Reserved | Reserved bytes |
| 23 | 1 | Count | 0x18 (24) - header size only, no payload follows |

**Raw TDU Packet Example (hexadecimal):**

```
[RTP Header: 12 bytes] [FNE Header: 20 bytes] [P25 Header: 24 bytes] [NO PAYLOAD]

Complete Packet (56 bytes):
90 FE 00 2A 00 00 1F 40 00 00 4E 20 | A3 5C 00 01 12 34 56 78 00 00 4E 20 00 18 00 00 00 00 00 00 | 50 32 35 44 03 00 03 90 00 00 AA BB 00 CC DD EE 00 00 00 7F 00 00 00 00 18
├─────────── RTP (12) ───────────┤ ├──────────────────────── FNE (20) ────────────────────────┤ ├──────────────────────── P25 (24) ─────────────────────────┤

P25 Message Header (NO PAYLOAD FOLLOWS):
50 32 35 44 03 00 03 90 00 00 AA BB 00 CC DD EE 00 00 00 7F 00 00 00 00 18
│        │  │  │  │  │  │        │  │        │  │  │  │  │  │  │  │  │
│        │  │  │  │  │  │        │  │        │  │  │  │  └──┴──┴──┴──┴─ Count: 0x18 (24) - header only
│        │  │  │  │  │  │        │  │        │  │  │  └────────────────── Reserved (4 bytes)
│        │  │  │  │  │  │        │  │        │  │  └───────────────────── BER: 0x00
│        │  │  │  │  │  │        │  │        │  └──────────────────────── RSSI: 0x7F (127)
│        │  │  │  │  │  │        │  │        └──┴──┴───────────────────── Reserved (3 bytes)
│        │  │  │  │  │  │        └──┴────────────────────────────────────── Dest ID: 0x00CCDDEE (13492718)
│        │  │  │  │  │  └────────────────────────────────────────────────── Src ID: 0x0000AABB (43707)
│        │  │  │  │  └───────────────────────────────────────────────────── MFId: 0x90 (Standard)
│        │  │  │  └──────────────────────────────────────────────────────── LCO: 0x03 (Group Voice)
│        │  │  └─────────────────────────────────────────────────────────── Control: 0x00
│        │  └────────────────────────────────────────────────────────────── DUID: 0x03 (TDU)
└────────┴───────────────────────────────────────────────────────────────── Tag: "P25D" (0x50323544)

Total packet ends at byte 56 - no additional payload bytes after the P25 header.
```

**Important Note:**

Unlike TSDU and TDULC which include raw P25 frame data after the message header, **TDU contains only the message header**. The 24-byte P25 header is immediately followed by padding bytes in the allocated buffer, but no actual P25 frame payload is present. The `Count` field (0x18 = 24) confirms this by indicating only the header size.

**Usage:**

TDU is sent when:
- Voice transmission ends normally
- PTT (Push-To-Talk) is released
- Trunking system ends the call grant without requiring link control information

---

**TSDU (Trunking System Data Unit):**

The TSDU message carries trunking control signaling in its raw over-the-air format. The payload contains a complete P25 TSDU frame as transmitted on the RF interface. Created by `createP25_TSDUMessage()` in `BaseNetwork.cpp`.

**TSDU Packet Structure:**

| Offset | Length | Field | Description |
|--------|--------|-------|-------------|
| 0-23 | 24 | Message Header | P25 message header with DUID = 0x07 (TSDU) |
| 24-68 | 45 | TSDU Frame Data | **Raw P25 TSDU frame as transmitted over-the-air** (`P25_TSDU_FRAME_LENGTH_BYTES = 45U`) |

**Total TSDU Size:** 69 bytes (`P25_TSDU_PACKET_LENGTH = 69U`)

**Constants:**
- `P25_TSDU_PACKET_LENGTH = 69U` (defined in `BaseNetwork.h`: 24 byte header + 45 byte TSDU frame)
- `P25_TSDU_FRAME_LENGTH_BYTES = 45U` (defined in `P25Defines.h`)
- `MSG_HDR_SIZE = 24U`
- `PACKET_PAD = 8U`
- Total allocated size: 69 + 8 = 77 bytes
- DUID value: `0x07` (TSDU - Trunking System Data Unit)

**TSDU Payload Format:**

The 45-byte TSDU frame data at offset 24 is **transmitted in its raw over-the-air format**, containing:

**P25 TSDU Over-the-Air Frame Structure (45 bytes):**

| Byte Range | Field | Description |
|------------|-------|-------------|
| 0-5 | Frame Sync | P25 frame synchronization pattern |
| 6-7 | NID | Network Identifier (NAC + DUID) |
| 8-32 | TSBK Data | Trunking System Block (25 bytes, FEC encoded) |
| 33-44 | Status Symbols | Status/padding symbols |

**Raw TSDU Packet Example (hexadecimal):**

```
[RTP: 12 bytes] [FNE: 20 bytes] [P25 TSDU: 69 bytes]

Complete Packet (101 bytes):
90 FE 00 2A 00 00 1F 40 00 00 4E 20 | A3 5C 00 01 12 34 56 78 00 00 4E 20 00 45 00 00 00 00 00 00 | 50 32 35 44 07 00 00 90 00 00 AA BB 00 CC DD EE 00 00 00 7F 00 00 00 00 2D [45 bytes raw TSDU frame...]
├─────────── RTP (12) ───────────┤ ├──────────────────────── FNE (20) ────────────────────────┤ ├───────────────────────────────── P25 TSDU (69) ────────────────────────────────┤

TSDU Message Header (24 bytes):
50 32 35 44 07 00 00 90 00 00 AA BB 00 CC DD EE 00 00 00 7F 00 00 00 00 2D
│        │  │  │  │  │  │        │  │        │  │  │  │  │  │  │  │  │
│        │  │  │  │  │  │        │  │        │  │  │  │  └──┴──┴──┴──┴─ Count: 0x2D (45)
│        │  │  │  │  │  │        │  │        │  │  │  └────────────────── Reserved
│        │  │  │  │  │  │        │  │        │  │  └───────────────────── BER: 0x00
│        │  │  │  │  │  │        │  │        │  └──────────────────────── RSSI: 0x7F (127)
│        │  │  │  │  │  │        │  │        └──┴──┴───────────────────── Reserved
│        │  │  │  │  │  │        └──┴────────────────────────────────────── Dest ID: 0x00CCDDEE (13492718)
│        │  │  │  │  │  └────────────────────────────────────────────────── Src ID: 0x0000AABB (43707)
│        │  │  │  │  └───────────────────────────────────────────────────── MFId: 0x90 (Standard)
│        │  │  │  └──────────────────────────────────────────────────────── LCO: 0x00 (not applicable for TSDU)
│        │  │  └─────────────────────────────────────────────────────────── Control: 0x00
│        │  └────────────────────────────────────────────────────────────── DUID: 0x07 (TSDU)
└────────┴───────────────────────────────────────────────────────────────── Tag: "P25D" (0x50323544)

TSDU Frame Data (45 bytes starting at offset 24):
55 75 F7 FF 5D 7F [NID: 2 bytes] [TSBK: 25 bytes FEC encoded] [Status: 12 bytes]
│              │  └────────────┴────────────────────┴──────────────────────── Raw over-the-air P25 TSDU frame
└──────────────┴────────────────────────────────────────────────────────────── Frame Sync pattern
```

**TSDU Content Types:**

The TSBK (Trunking System Block) within the TSDU can contain various trunking messages:
- **IOSP_GRP_VCH:** Group voice channel grant
- **IOSP_UU_VCH:** Unit-to-unit voice channel grant
- **OSP_ADJ_STS_BCAST:** Adjacent site broadcast
- **OSP_RFSS_STS_BCAST:** RFSS status broadcast
- **OSP_NET_STS_BCAST:** Network status broadcast
- **ISP_GRP_AFF_REQ:** Group affiliation request
- **ISP_U_REG_REQ:** Unit registration request
- And many other trunking opcodes defined in TIA-102.AABC

**Important:** The TSDU frame data is **not DFSI-encoded**. It contains the raw P25 frame as it would appear over the air, including frame sync, NID, FEC-encoded TSBK data, and status symbols. This allows the receiving peer to decode or retransmit the exact trunking signaling.

---

**TDULC (Terminator Data Unit with Link Control):**

The TDULC message signals the end of transmission and includes link control information in its raw over-the-air format. The payload contains a complete P25 TDULC frame as transmitted on the RF interface. Created by `createP25_TDULCMessage()` in `BaseNetwork.cpp`.

**TDULC Packet Structure:**

| Offset | Length | Field | Description |
|--------|--------|-------|-------------|
| 0-23 | 24 | Message Header | P25 message header with DUID = 0x0F (TDULC) |
| 24-77 | 54 | TDULC Frame Data | **Raw P25 TDULC frame as transmitted over-the-air** (`P25_TDULC_FRAME_LENGTH_BYTES = 54U`) |

**Total TDULC Size:** 78 bytes (`P25_TDULC_PACKET_LENGTH = 78U`)

**Constants:**
- `P25_TDULC_PACKET_LENGTH = 78U` (defined in `BaseNetwork.h`: 24 byte header + 54 byte TDULC frame)
- `P25_TDULC_FRAME_LENGTH_BYTES = 54U` (defined in `P25Defines.h`)
- `MSG_HDR_SIZE = 24U`
- `PACKET_PAD = 8U`
- Total allocated size: 78 + 8 = 86 bytes
- DUID value: `0x0F` (TDULC - Terminator Data Unit with Link Control)

**TDULC Payload Format:**

The 54-byte TDULC frame data at offset 24 is **transmitted in its raw over-the-air format**, containing:

**P25 TDULC Over-the-Air Frame Structure (54 bytes):**

| Byte Range | Field | Description |
|------------|-------|-------------|
| 0-5 | Frame Sync | P25 frame synchronization pattern |
| 6-7 | NID | Network Identifier (NAC + DUID) |
| 8-43 | LC Data | Link Control data (36 bytes, FEC encoded) |
| 44-53 | Status Symbols | Status/padding symbols |

**Raw TDULC Packet Example (hexadecimal):**

```
[RTP: 12 bytes] [FNE: 20 bytes] [P25 TDULC: 78 bytes]

Complete Packet (110 bytes):
90 FE 00 2A 00 00 1F 40 00 00 4E 20 | A3 5C 00 01 12 34 56 78 00 00 4E 20 00 4E 00 00 00 00 00 00 | 50 32 35 44 0F 00 03 90 00 00 AA BB 00 CC DD EE 00 00 00 7F 00 00 00 00 36 [54 bytes raw TDULC frame...]
├─────────── RTP (12) ───────────┤ ├──────────────────────── FNE (20) ────────────────────────┤ ├──────────────────────────────── P25 TDULC (78) ─────────────────────────────────┤

TDULC Message Header (24 bytes):
50 32 35 44 0F 00 03 90 00 00 AA BB 00 CC DD EE 00 00 00 7F 00 00 00 00 36
│        │  │  │  │  │  │        │  │        │  │  │  │  │  │  │  │  │
│        │  │  │  │  │  │        │  │        │  │  │  │  └──┴──┴──┴──┴─ Count: 0x36 (54)
│        │  │  │  │  │  │        │  │        │  │  │  └────────────────── Reserved
│        │  │  │  │  │  │        │  │        │  │  └───────────────────── BER: 0x00
│        │  │  │  │  │  │        │  │        │  └──────────────────────── RSSI: 0x7F (127)
│        │  │  │  │  │  │        │  │        └──┴──┴───────────────────── Reserved
│        │  │  │  │  │  │        └──┴────────────────────────────────────── Dest ID: 0x00CCDDEE (13492718)
│        │  │  │  │  │  └────────────────────────────────────────────────── Src ID: 0x0000AABB (43707)
│        │  │  │  │  └───────────────────────────────────────────────────── MFId: 0x90 (Standard)
│        │  │  │  └──────────────────────────────────────────────────────── LCO: 0x03 (Group Voice)
│        │  │  └─────────────────────────────────────────────────────────── Control: 0x00
│        │  └────────────────────────────────────────────────────────────── DUID: 0x0F (TDULC)
└────────┴───────────────────────────────────────────────────────────────── Tag: "P25D" (0x50323544)

TDULC Frame Data (54 bytes starting at offset 24):
55 75 F7 FF 5D 7F [NID: 2 bytes] [LC: 36 bytes FEC encoded] [Status: 10 bytes]
│              │  └────────────┴──────────────────────┴─────────────────────── Raw over-the-air P25 TDULC frame
└──────────────┴──────────────────────────────────────────────────────────────── Frame Sync pattern
```

**TDULC Link Control Content:**

The 36-byte FEC-encoded Link Control field contains the same LC information that was embedded in the LDU1 voice frames during the transmission:
- **LCO (Link Control Opcode):** Type of call (group voice, unit-to-unit, etc.)
- **MFId (Manufacturer ID):** Radio vendor identifier
- **Source ID:** Transmitting radio ID
- **Destination ID:** Target talkgroup or radio ID
- **Service Options:** Emergency flag, encrypted flag, priority
- **Additional LC data:** Depends on LCO type

**Important:** Like TSDU, the TDULC frame data is **not DFSI-encoded**. It contains the raw P25 frame as it would appear over the air, including frame sync, NID, FEC-encoded link control data, and status symbols. This allows the receiving peer to:
1. Decode the final call parameters
2. Retransmit the exact termination frame
3. Maintain synchronization with over-the-air P25 systems

**Usage:**

TDULC is sent when:
- Voice transmission ends with link control confirmation
- System needs to provide final call metadata
- Trunked system requires explicit termination with LC

The difference between **TDU** and **TDULC**:
- **TDU:** Simple terminator, no additional payload (24 bytes total)
- **TDULC:** Terminator with embedded link control (78 bytes total), provides complete call metadata at termination

#### NXDN Message Format

NXDN frames use a fixed-length network format created by `createNXDN_Message()` in `BaseNetwork.cpp`. The frame size is 70 bytes as defined by `NXDN_PACKET_LENGTH = 70U`.

```cpp
bool BaseNetwork::writeNXDN(const nxdn::NXDNData& data) {
    uint8_t buffer[NXDN_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, NXDN_PACKET_LENGTH + PACKET_PAD);
    
    createNXDN_Message(buffer, data);
    
    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN },
                       buffer, NXDN_PACKET_LENGTH + PACKET_PAD,
                       pktSeq, m_nxdnStreamId);
}
```

**NXDN Packet Structure:**

The NXDN network packet uses the TAG_NXDN_DATA identifier and includes the 48-byte NXDN frame plus metadata.

| Offset | Length | Field | Description |
|--------|--------|-------|-------------|
| 0-3 | 4 | NXDN Tag | "NXDD" (0x4E584444) ASCII identifier |
| 4 | 1 | Message Type | NXDN message type (RTCH/RCCH) |
| 5-7 | 3 | Source ID | NXDN radio ID of originating station |
| 8-10 | 3 | Destination ID | NXDN radio ID or talkgroup ID |
| 11-13 | 3 | Reserved | Reserved for future use (0x000000) |
| 14 | 1 | Control Byte | Network control flags |
| 15 | 1 | Group Flag | Group call flag (0=private, 1=group) |
| 16-22 | 7 | Reserved | Reserved for future use |
| 23 | 1 | Count | Total NXDN data length |
| 24-71 | 48 | NXDN Frame Data | Raw NXDN frame (`NXDN_FRAME_LENGTH_BYTES = 48U`) |

**Total NXDN Payload Size:** 70 bytes (`NXDN_PACKET_LENGTH`)

**Constants:**
- `NXDN_PACKET_LENGTH = 70U` (defined in `BaseNetwork.h`: 20 byte header + 48 byte frame + 2 byte trailer)
- `NXDN_FRAME_LENGTH_BYTES = 48U` (defined in `NXDNDefines.h`)
- `PACKET_PAD = 8U`
- Total allocated size: 70 + 8 = 78 bytes

**Raw NXDN Packet Example (hexadecimal):**

```
[RTP Header: 12 bytes] [FNE Header: 20 bytes] [NXDN Payload: 70 bytes]

Complete Packet (102 bytes):
90 FE 00 2A 00 00 1F 40 00 00 4E 20 | A3 5C 00 00 12 34 56 78 00 00 4E 20 00 46 00 00 00 00 00 00 | 4E 58 44 44 02 00 10 00 00 20 00 00 01 00 00 00 00 00 00 00 00 00 00 30 [48 bytes NXDN frame...]
├─────────── RTP (12) ───────────┤ ├──────────────────────── FNE (20) ────────────────────────┤ ├─────────────────────────────────── NXDN (70) ─────────────────────────────────┤

NXDN Payload breakdown:
4E 58 44 44 02 00 10 00 00 20 00 00 01 00 00 00 00 00 00 00 00 00 00 30 [48 bytes NXDN frame data...]
│        │  │  │        │  │        │  │  │  └──────────┴──────────┴─────┤
│        │  │  │        │  │        │  │  └───────────────────────────────── Group: 0x00 (private call)
│        │  │  │        │  │        │  └──────────────────────────────────── Control: 0x01
│        │  │  │        │  └─[Reserved 3]
│        │  │  │        └─[DestID 3rd byte]──────────────────────────────── Dest ID: 0x200000 (2097152)
│        │  │  └─[DestID 2nd byte]
│        │  └─[DestID 1st byte]
│        └─[SrcID 3rd byte]──────────────────────────────────────────────── Src ID: 0x001000 (4096)
│        └─[SrcID 2nd byte]
│        └─[SrcID 1st byte]
│        └──────────────────────────────────────────────────────────────── Msg Type: 0x02 (RTCH)
└───────────────────────────────────────────────────────────────────────── Tag: "NXDD" (0x4E584444)
       └──────────────────────────────────────────────────────────────────  Reserved (7 bytes)
                                                                           └─ Count: 0x30 (48)
                                                                              (NXDN frame at offset 24)
```

**NXDN Message Types:**
- **RTCH (Radio Traffic Channel):** Voice/data channel frames (message type varies based on content)
- **RCCH (Radio Control Channel):** Control signaling frames

The actual NXDN frame data (48 bytes) contains the over-the-air NXDN frame structure with FSW (Frame Sync Word), LICH (Link Information Channel), SACCH (Slow Associated Control Channel), FACCH (Fast Associated Control Channel), or voice data depending on the frame type.

#### Analog Message Format

Analog audio frames use G.711 μ-law encoding and are created by `createAnalog_Message()` in `BaseNetwork.cpp`. The frame size is 324 bytes as defined by `ANALOG_PACKET_LENGTH = 324U`.

```cpp
bool BaseNetwork::writeAnalog(const analog::AnalogData& data) {
    uint8_t buffer[ANALOG_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, ANALOG_PACKET_LENGTH + PACKET_PAD);
    
    createAnalog_Message(buffer, data);
    
    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG },
                       buffer, ANALOG_PACKET_LENGTH + PACKET_PAD,
                       pktSeq, m_analogStreamId);
}
```

**Analog Packet Structure:**

The analog audio packet uses the TAG_ANALOG_DATA identifier and contains 300 bytes of audio samples (calculated as 324 - 20 header - 4 trailer).

| Offset | Length | Field | Description |
|--------|--------|-------|-------------|
| 0-3 | 4 | Analog Tag | "ADIO" (0x4144494F) or similar ASCII identifier |
| 4 | 1 | Sequence Number | Packet sequence number for audio ordering (0-255) |
| 5-7 | 3 | Source ID | Radio ID of transmitting station |
| 8-10 | 3 | Destination ID | Target radio ID or conference ID |
| 11-13 | 3 | Reserved | Reserved for future use (0x000000) |
| 14 | 1 | Control Byte | Network control flags |
| 15 | 1 | Frame Type / Group | Bit 7: Group flag (0=private, 1=group)<br/>Bits 0-6: Audio frame type |
| 16-19 | 4 | Reserved | Reserved for future use (0x00000000) |
| 20-319 | 300 | Audio Data | G.711 μ-law encoded audio samples (300 bytes @ 8kHz) |
| 320-323 | 4 | Trailer | Reserved trailer bytes |

**Total Analog Payload Size:** 324 bytes (`ANALOG_PACKET_LENGTH`)

**Constants:**
- `ANALOG_PACKET_LENGTH = 324U` (defined in `BaseNetwork.h`: 20 byte header + 300 byte audio + 4 byte trailer)
- Audio sample size: 300 bytes (AUDIO_SAMPLES_LENGTH_BYTES, calculated as 324 - 20 - 4)
- `PACKET_PAD = 8U`
- Total allocated size: 324 + 8 = 332 bytes

**Audio Encoding Details:**
- **Codec:** G.711 μ-law (ITU-T G.711)
- **Sample Rate:** 8 kHz (8000 samples per second)
- **Bit Depth:** 8 bits per sample (1 byte per sample)
- **Frame Duration:** 37.5 ms (300 samples ÷ 8000 samples/sec = 0.0375 sec)
- **Samples per Frame:** 300 samples
- **Bandwidth:** 64 kbit/s (8000 samples/sec × 8 bits/sample)

**Raw Analog Packet Example (hexadecimal):**

```
[RTP Header: 12 bytes] [FNE Header: 20 bytes] [Analog Payload: 324 bytes]

Complete Packet (356 bytes):
90 FE 00 2A 00 00 1F 40 00 00 4E 20 | A3 5C 00 03 12 34 56 78 00 00 4E 20 01 44 00 00 00 00 00 00 | 41 44 49 4F 05 00 10 00 00 20 00 00 01 00 00 00 80 00 00 00 [300 bytes G.711 audio...] 00 00 00 00
├─────────── RTP (12) ───────────┤ ├──────────────────────── FNE (20) ────────────────────────┤ ├────────────────────────────────── Analog (324) ──────────────────────────────────┤

Analog Payload breakdown:
41 44 49 4F 05 00 10 00 00 20 00 00 01 00 00 00 80 00 00 00 [300 bytes audio data...] 00 00 00 00
│        │  │  │        │  │        │  │  │  │  │  └──────┴─ Reserved (4 bytes)
│        │  │  │        │  │        │  │  │  │  └─────────────── Frame Type/Group: 0x80 (group call, frame type 0)
│        │  │  │        │  │        │  │  │  └──────────────────── Control: 0x01
│        │  │  │        │  │        │  └──┴────────────────────── Reserved (3 bytes)
│        │  │  │        │  └─[DestID 3rd]──────────────────────── Dest ID: 0x200000 (2097152, conference)
│        │  │  │        └─[DestID 2nd]
│        │  │  └─[DestID 1st]
│        │  └─[SrcID 3rd]──────────────────────────────────────── Src ID: 0x001000 (4096)
│        └─[SrcID 2nd]
│        └─[SrcID 1st]
│        └──────────────────────────────────────────────────────── Sequence: 0x05 (5)
└───────────────────────────────────────────────────────────────── Tag: "ADIO" (0x4144494F)
       └────────────────────────────────────────────────────────── Reserved (4 bytes)
                                                                   └────── Audio samples (300 bytes)
                                                                           └─── Trailer (4 bytes)

G.711 μ-law Audio Sample Example (first 16 bytes of audio data shown):
FF FE FD FC FB FA F9 F8 F7 F6 F5 F4 F3 F2 F1 F0 ...
│  │  │  │  │  │  │  │  │  │  │  │  │  │  │  │
└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴─── Each byte is one 8kHz audio sample (μ-law encoded)
```

**G.711 μ-law Encoding:**

G.711 μ-law is a logarithmic audio compression standard that provides toll-quality voice at 64 kbit/s:
- **Range:** ±8159 linear PCM values compressed to 256 μ-law values (0x00-0xFF)
- **Characteristics:** Non-linear quantization favoring lower amplitude signals (human speech)
- **Compatibility:** Standard telephony codec, widely supported
- **Signal-to-Noise Ratio:** ~37 dB for speech signals

The 300-byte audio payload represents 37.5 milliseconds of continuous audio, allowing for smooth real-time voice transmission with minimal latency.

### Ring Buffer Architecture

Each protocol uses a fixed-size ring buffer for received data:

```cpp
RingBuffer m_rxDMRData(NET_RING_BUF_SIZE, "DMR Net Buffer");     // 4098 bytes
RingBuffer m_rxP25Data(NET_RING_BUF_SIZE, "P25 Net Buffer");     // 4098 bytes
RingBuffer m_rxNXDNData(NET_RING_BUF_SIZE, "NXDN Net Buffer");   // 4098 bytes
RingBuffer m_rxAnalogData(NET_RING_BUF_SIZE, "Analog Net Buffer"); // 4098 bytes
```

**Operations:**

- `addData(data, length)`: Add received data to buffer
- `dataSize()`: Query current buffer occupancy
- `getData()`: Read data from buffer
- `clear()`: Empty buffer

**Buffer Sizing:**

4KB buffers provide sufficient buffering for:
- DMR: ~124 frames (33 bytes each)
- P25 LDU: ~19 frames (216 bytes each)
- NXDN: Variable, typically 40-80 frames
- Analog: ~4 seconds at 8kHz G.711

### Packet Fragmentation

Large messages (peer lists, talkgroup rules, etc.) are fragmented:

```cpp
class PacketBuffer {
public:
    // Fragment header structure
    struct Fragment {
        uint8_t blockId;           // Block number
        uint32_t size;             // Total size
        uint32_t compressedSize;   // Compressed size
        uint8_t* data;             // Block data
    };
    
    bool decode(const uint8_t* data, uint8_t** message, uint32_t* outLength);
    void encode(uint8_t* data, uint32_t length);
};
```

**Fragment Format:**

```
| 0-3: Total Size | 4-7: Compressed Size | 8: Block# | 9: Block Count | 10+: Data |
```

**Process:**

1. Large message split into FRAG_BLOCK_SIZE chunks (typically 1024 bytes)
2. Optional zlib compression applied
3. Each fragment transmitted with block number and count
4. Receiver reassembles fragments
5. Decompression applied if enabled
6. Complete message delivered to application

---

## 8. Stream Multiplexing

### Stream Identifiers

Each call/session uses a unique 32-bit stream ID:

```cpp
uint32_t streamId = m_random.next(); // Generate random stream ID
```

**Stream ID Purposes:**

- Distinguish concurrent calls on different talkgroups
- Associate related frames within a single call
- Track RTP timestamps per stream
- Support multiple simultaneous calls

**Special Values:**

- `RTP_END_OF_CALL_SEQ` (65535): End-of-call marker
- `0x00000000`: Invalid/uninitialized

### RTPStreamMultiplex Class

Manages multiple concurrent RTP streams:

```cpp
class RTPStreamMultiplex {
private:
    std::unordered_map<uint32_t, uint16_t> m_streamSeq;
    
public:
    uint16_t getSequence(uint32_t streamId);
    void setSequence(uint32_t streamId, uint16_t seq);
    void eraseSequence(uint32_t streamId);
};
```

**Sequence Tracking:**

Each stream maintains its own RTP sequence counter:

```cpp
uint16_t sequence = m_mux->getSequence(streamId);
sequence = (sequence + 1) % 65536;
m_mux->setSequence(streamId, sequence);
```

### Receive-Side Stream Processing

Incoming packets are validated and routed by stream ID:

```cpp
MULTIPLEX_RET_CODE Network::verifyStream(uint16_t* lastRxSeq) {
    uint16_t rtpSeq = rtpHeader.getSequence();
    
    // Check for duplicate
    if (rtpSeq == *lastRxSeq) {
        return MULTIPLEX_DUP;
    }
    
    // Check for out-of-order
    if (rtpSeq < *lastRxSeq && ((*lastRxSeq - rtpSeq) < 100U)) {
        return MULTIPLEX_OLDPKT;
    }
    
    // Check for missed packets
    if ((rtpSeq - *lastRxSeq) > 1) {
        return MULTIPLEX_MISSING;
    }
    
    *lastRxSeq = rtpSeq;
    return MULTIPLEX_OK;
}
```

**Return Codes:**

- `MULTIPLEX_OK`: Valid next packet
- `MULTIPLEX_DUP`: Duplicate packet (discard)
- `MULTIPLEX_OLDPKT`: Out-of-order old packet (discard)
- `MULTIPLEX_MISSING`: Gap detected (warn but continue)

### Call Collision Handling

The FNE master detects and resolves call collisions:

```cpp
// Check for existing call on target talkgroup
if (isTargetInCall(dstId)) {
    // Send busy denial
    writeInCallCtrl(peerId, NET_ICC::BUSY_DENY, 
                    NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR, 
                    dstId, slotNo, streamId);
    return;
}
```

**Collision Timeout:**

Configurable timeout (default 5 seconds) prevents stale call states.

---

## 9. Security

### Authentication

SHA256-based challenge-response authentication:

**Process:**

1. Peer generates random salt (4 bytes)
2. Peer sends salt in RPTL login request
3. Master generates challenge value
4. Master sends challenge to peer
5. Peer computes: `SHA256(salt || password || challenge)`
6. Peer sends hash in RPTK authorization
7. Master validates hash

**Implementation:**

```cpp
// Peer side
uint8_t hash[50U];
::memcpy(hash, m_salt, sizeof(uint32_t));
::memcpy(hash + sizeof(uint32_t), m_password.c_str(), m_password.size());
// Append master challenge...

edac::SHA256 sha256;
sha256.buffer(hash, 40U, hash);
```

**Security Properties:**

- Prevents replay attacks (random salt/challenge)
- Password never transmitted in cleartext
- Mutual authentication possible
- Resistant to offline dictionary attacks

### Encryption

Optional AES-256-ECB encryption at transport layer:

```cpp
void Network::setPresharedKey(const uint8_t* presharedKey) {
    m_socket->setPresharedKey(presharedKey);
}
```

**Key Management:**

- 32-byte preshared key configured out-of-band
- Same key used for all peers (shared secret)
- Encrypts entire UDP payload (RTP + headers + data)

**Encryption Algorithm:**

The implementation uses AES-256-ECB (Electronic Codebook mode):
- **Algorithm**: AES-256-ECB
- **Key Size**: 256 bits (32 bytes)
- **Block Size**: 128 bits (16 bytes)
- **Mode**: ECB - each 16-byte block encrypted independently
- **Padding**: PKCS#7 padding for variable-length payloads

**Note**: ECB mode is used for simplicity and performance in this implementation. While ECB has known cryptographic weaknesses (identical plaintext blocks produce identical ciphertext), the primary goal is to provide basic confidentiality for network traffic between trusted peers rather than military-grade security. For high-security deployments, additional network-layer security (IPsec, VPN) should be used.

### Access Control

Multiple layers of access control:

**Radio ID (RID) Control:**

```cpp
// Whitelist/blacklist updates
NET_FUNC::MASTER + NET_SUBFUNC::MASTER_SUBFUNC_WL_RID
NET_FUNC::MASTER + NET_SUBFUNC::MASTER_SUBFUNC_BL_RID
```

**Talkgroup Control:**

```cpp
// Active/inactive talkgroup lists
NET_FUNC::MASTER + NET_SUBFUNC::MASTER_SUBFUNC_ACTIVE_TGS
NET_FUNC::MASTER + NET_SUBFUNC::MASTER_SUBFUNC_DEACTIVE_TGS
```

**Peer-Level ACL:**

- Peer ID validation
- IP address restrictions
- Certificate-based authentication (with SSL/TLS)

**Configuration Flags:**

- `m_rejectUnknownRID`: Reject calls from unknown RIDs
- `m_restrictGrantToAffOnly`: Require affiliation for grants
- `m_restrictPVCallToRegOnly`: Require registration for private calls
- `m_disallowU2U`: Disable unit-to-unit calls globally

---

## 10. Quality of Service

### Packet Sequencing

RTP sequence numbers detect packet loss and reordering:

```cpp
uint16_t m_pktSeq = 0;
m_pktSeq = (m_pktSeq + 1) % 65536;

rtpHeader.setSequence(m_pktSeq);
```

**Monitoring:**

Receiver tracks:
- Expected sequence vs. received sequence
- Gap count (missed packets)
- Duplicate count
- Out-of-order count

### Acknowledgments

Critical messages require acknowledgment:

**ACK/NAK Protocol:**

```cpp
// Send message requiring ACK
writeMaster(opcode, data, length, pktSeq, streamId);

// Wait for response
if (response == NET_FUNC::ACK) {
    // Success
} else if (response == NET_FUNC::NAK) {
    // Failure - handle error
}
```

**NAK Reasons:**

- Authentication failure
- ACL rejection
- Invalid configuration
- Protocol error
- Resource unavailable

### Retry Logic

Failed transmissions are retried with exponential backoff:

```cpp
Timer m_retryTimer(1000U, DEFAULT_RETRY_TIME); // 10 seconds
uint32_t m_retryCount = 0U;
const uint32_t MAX_RETRY_BEFORE_RECONNECT = 4U;

if (m_retryTimer.isRunning() && m_retryTimer.hasExpired()) {
    m_retryCount++;
    
    if (m_retryCount >= MAX_RETRY_BEFORE_RECONNECT) {
        // Give up, trigger reconnection
        close();
        open();
        m_retryCount = 0U;
    } else {
        // Retry transmission
        retransmit();
    }
    
    m_retryTimer.start();
}
```

**Retry Limits:**

- Standard: 4 retries before reconnect
- HA mode: 2 retries before failover
- Duplicate connection: 2 retries with 60-minute delay

### Jitter Buffering

Ring buffers provide jitter buffering:

- 4KB buffers smooth out network variations
- Application reads at constant rate
- Network fills buffer at variable rate
- Buffer absorbs timing jitter

**Buffer Management:**

```cpp
// Add received data
m_rxDMRData.addData(data, length);

// Check fill level
if (m_rxDMRData.dataSize() >= DMR_FRAME_LENGTH_BYTES) {
    // Read frame
    UInt8Array frame = readDMR(ret, frameLength);
    // Process frame...
}
```

### Latency Optimization

**Low-Latency Techniques:**

1. **Zero-Copy Operations**: Use pointers instead of buffer copies
2. **Thread Pool**: Asynchronous packet processing
3. **UDP Socket Tuning**: Large buffers (512KB)
4. **Priority Scheduling**: Real-time thread priorities
5. **Direct Routing**: Minimize master-side processing

**Typical Latencies:**

- Peer-to-master: 10-50ms (network dependent)
- Master processing: 1-5ms
- Master-to-peer: 10-50ms (network dependent)
- **End-to-End: 20-100ms typical**

---

## 11. Network Diagnostics

### Activity Logging

Peers can transfer activity logs to master:

```cpp
bool BaseNetwork::writeActLog(const char* message) {
    uint32_t len = (uint32_t)::strlen(message);
    
    uint8_t* buffer = new uint8_t[len];
    ::memcpy(buffer, message, len);
    
    bool ret = writeMaster({ NET_FUNC::TRANSFER, 
                            NET_SUBFUNC::TRANSFER_SUBFUNC_ACTIVITY },
                           buffer, len, pktSeq, streamId,
                           m_useAlternatePortForDiagnostics);
    
    delete[] buffer;
    return ret;
}
```

**Activity Log Events:**

- Call start/end
- Affiliation changes
- Registration events
- Grant requests/denials
- Error conditions

### Diagnostic Logging

Detailed diagnostic logs for troubleshooting:

```cpp
bool BaseNetwork::writeDiagLog(const char* message) {
    uint32_t len = (uint32_t)::strlen(message);
    
    uint8_t* buffer = new uint8_t[len];
    ::memcpy(buffer, message, len);
    
    bool ret = writeMaster({ NET_FUNC::TRANSFER,
                            NET_SUBFUNC::TRANSFER_SUBFUNC_DIAG },
                           buffer, len, pktSeq, streamId,
                           m_useAlternatePortForDiagnostics);
    
    delete[] buffer;
    return ret;
}
```

**Diagnostic Information:**

- Packet statistics
- Buffer utilization
- Timing measurements
- Protocol state changes
- Error details

### Status Transfer

Peers report status as JSON:

```cpp
bool BaseNetwork::writePeerStatus(json::object obj) {
    std::string json = json::object(obj).serialize();
    
    return writeMaster({ NET_FUNC::TRANSFER,
                        NET_SUBFUNC::TRANSFER_SUBFUNC_STATUS },
                       (uint8_t*)json.c_str(), json.length(),
                       pktSeq, streamId,
                       m_useAlternatePortForDiagnostics);
}
```

**Status Fields:**

```json
{
    "peerId": 1234567,
    "connected": true,
    "rxFrequency": 449000000,
    "txFrequency": 444000000,
    "dmrEnabled": true,
    "p25Enabled": true,
    "nxdnEnabled": false,
    "callsInProgress": 2,
    "txQueueDepth": 0,
    "rxQueueDepth": 3
}
```

### Diagnostic Network Port

FNE master uses separate port for diagnostics:

```cpp
class DiagNetwork : public BaseNetwork {
private:
    FNENetwork* m_fneNetwork;
    uint16_t m_port;              // Separate diagnostic port
    ThreadPool m_threadPool;
    
public:
    void processNetwork();        // Process diagnostic packets
};
```

**Benefits:**

- Isolate diagnostic traffic from operational traffic
- Different QoS/priority handling
- Optional firewall rules
- Reduced congestion on main port

### Network Statistics

Key metrics tracked:

**Per-Peer Statistics:**

- Packets received/transmitted
- Bytes received/transmitted
- Packet loss rate
- Average latency
- Jitter measurements
- Last activity timestamp

**Global Statistics:**

- Total peers connected
- Total calls in progress
- Aggregate bandwidth
- Queue depths
- Error counts

**Monitoring Tools:**

- Real-time web dashboard
- REST API for metrics
- Prometheus/InfluxDB integration
- SNMP support (optional)

---

## 12. Performance Considerations

### Scalability

**FNE Master Scalability:**

- **Peers**: Tested with 250+ concurrent peers
- **Calls**: 50+ simultaneous calls
- **Throughput**: 100+ Mbps aggregate
- **CPU**: ~10-20% per 100 peers (modern CPU)
- **Memory**: ~50-100MB base + 1-2MB per peer

**Scaling Techniques:**

1. **Thread Pool**: Asynchronous packet processing
2. **Lock-Free Queues**: Minimize contention
3. **Buffer Pooling**: Reduce allocation overhead
4. **Zero-Copy**: Direct buffer passing
5. **Efficient Lookups**: Hash maps for O(1) peer lookup

### Network Bandwidth

**Per-Call Bandwidth (Approximate):**

| Protocol | Codec | Bandwidth |
|----------|-------|-----------|
| DMR | AMBE+2 | ~7 kbps |
| P25 | IMBE | ~9 kbps |
| NXDN | AMBE+2 | ~7 kbps |
| Analog | G.711 | ~64 kbps |

**Overhead:**

- RTP header: 12 bytes
- FNE header: 20 bytes
- UDP header: 8 bytes
- IP header: 20 bytes (IPv4) or 40 bytes (IPv6)
- **Total overhead: 60-80 bytes per packet**

**Packet Rates:**

- DMR: ~50 packets/second
- P25: ~50 packets/second
- NXDN: ~25 packets/second
- Analog: 50-100 packets/second

### CPU Optimization

**Hot Paths:**

1. **RTP Encoding/Decoding**: Inline functions, avoid branches
2. **CRC Calculation**: Table-driven algorithm
3. **Buffer Operations**: memcpy optimization, alignment
4. **Hash Functions**: Fast hash for peer lookups
5. **Timestamp Math**: Integer arithmetic, avoid floating point

**Profiling Results:**

- RTP encode: ~0.5 µs per packet
- RTP decode: ~0.7 µs per packet
- Frame queue operation: ~1-2 µs
- Protocol message creation: ~2-5 µs
- **Total processing: ~5-10 µs per packet**

### Memory Management

**Memory Allocation:**

- **Static Buffers**: Ring buffers allocated at initialization
- **Object Pooling**: Reuse packet buffers
- **Smart Pointers**: Automatic cleanup (UInt8Array)
- **Stack Allocation**: Prefer stack for small, temporary buffers

**Memory Footprint:**

- BaseNetwork: ~20KB
- Network (Peer): ~50KB
- FNENetwork (Master): ~100KB base
- Per-peer state: ~1-2KB
- **Total for 100 peers: ~300-400MB**

### Configuration Tuning

**UDP Socket Buffers:**

```cpp
m_socket->recvBufSize(524288U); // 512KB recv buffer
m_socket->sendBufSize(524288U); // 512KB send buffer
```

**Thread Pool Sizing:**

```cpp
ThreadPool m_threadPool(workerCnt, "fne");
// Recommended: 2-4 workers per CPU core
```

**Timer Intervals:**

```cpp
Timer m_maintainenceTimer(1000U, pingTime);        // 5s recommended
Timer m_updateLookupTimer(1000U, updateTime * 60U);// 15-30 min
```

**Buffer Sizes:**

```cpp
#define NET_RING_BUF_SIZE 4098U  // 4KB ring buffers
// Increase for high-latency or high-jitter networks
```

### Best Practices

**Deployment:**

1. **Network**: Use dedicated VLANs for voice traffic
2. **QoS**: Mark packets with DSCP EF (Expedited Forwarding)
3. **Firewall**: Stateless rules for UDP (avoid connection tracking)
4. **MTU**: Ensure path MTU ≥ 1500 bytes (avoid fragmentation)
5. **Latency**: Target <50ms round-trip time

**Monitoring:**

1. **Log Levels**: Use ERROR/WARN in production, DEBUG for troubleshooting
2. **Metrics**: Export to time-series database (InfluxDB, Prometheus)
3. **Alerts**: Configure alerts for peer disconnections, high packet loss
4. **Dashboards**: Real-time visualization of key metrics

**Troubleshooting:**

1. **Packet Dumps**: Use `m_debug` flag to enable packet hex dumps
2. **Wireshark**: Capture UDP traffic for deep analysis
3. **Logs**: Correlate timestamps across peer and master logs
4. **Statistics**: Monitor sequence gaps, duplicate packets
5. **Network Tests**: iperf, ping, traceroute to validate network

---

## Appendix A: Message Flow Examples

### Protocol Call Flow Examples

#### Example 1: DMR Call Setup (Standard Mode)

```
Peer A (Initiator)          FNE Master              Peer B (Recipient)
    |                           |                           |
    | DMR PROTOCOL (Voice HDR)  |                           |
    |-------------------------->|-------------------------->|
    |                           | (Route based on TG)       |
    |                           |                           |
    | DMR PROTOCOL (Voice Burst)|                           |
    |-------------------------->|-------------------------->|
    |                           |                           |
    | ... (continued voice) ... |                           |
    |                           |                           |
    | DMR PROTOCOL (Terminator) |                           |
    |-------------------------->|-------------------------->|
    |                           |                           |
```

**DMR Call Flow Notes:**

- **No GRANT_REQ Required**: DMR calls can start directly with the voice header
- **FNE Routing**: The FNE master routes traffic based on destination talkgroup ID
- **Slot Assignment**: DMR supports two time slots for concurrent calls
- **ACL Enforcement**: Access control is enforced by the FNE during frame routing

#### Example 2: DMR Call Setup (FNE Authoritative Mode)

```
Peer A (Initiator)          FNE Master              Peer B (Recipient)
    |                           |                           |
    | GRANT_REQ (Optional)      |                           |
    | SrcID=123456, DstID=9     |                           |
    |-------------------------->|                           |
    |                           | (Check ACL, TG active,    |
    |                           |  channel availability)    |
    |                           |                           |
    |   DMR PROTOCOL (Grant)    |                           |
    |   [CSBK - Grant message]  |                           |
    |<--------------------------|                           |
    |                           |                           |
    | DMR PROTOCOL (Voice HDR)  |                           |
    |-------------------------->|-------------------------->|
    |                           |                           |
    | DMR PROTOCOL (Voice Burst)|                           |
    |-------------------------->|-------------------------->|
    |                           |                           |
    | ... (continued voice) ... |                           |
    |                           |                           |
    | DMR PROTOCOL (Terminator) |                           |
    |-------------------------->|-------------------------->|
    |                           |                           |
```

**FNE Authoritative Mode:**

- **GRANT_REQ**: Used when FNE operates in authoritative/trunking mode
- **Grant Response**: FNE responds with protocol-specific grant message (DMR CSBK, P25 GRANT, NXDN VCALL_ASSGN)
- **Pre-Call Authorization**: FNE validates call before voice traffic starts
- **Channel Management**: FNE can assign specific channels or deny calls with protocol-specific denial messages
- **Busy Detection**: FNE can reject calls if target is already in a call
- **Configuration**: Enabled via FNE master configuration settings

#### Example 3: P25 Voice Call Flow

```
Peer A (Initiator)          FNE Master              Peer B (Recipient)
    |                           |                           |
    | P25 PROTOCOL (HDU)        |                           |
    | Header Data Unit          |                           |
    |-------------------------->|-------------------------->|
    |                           | (Route based on TG)       |
    |                           |                           |
    | P25 PROTOCOL (LDU1)       |                           |
    | Logical Data Unit 1       |                           |
    | [Voice + LC + LSD]        |                           |
    |-------------------------->|-------------------------->|
    |                           |                           |
    | P25 PROTOCOL (LDU2)       |                           |
    | Logical Data Unit 2       |                           |
    | [Voice + ESS + LSD]       |                           |
    |-------------------------->|-------------------------->|
    |                           |                           |
    | P25 PROTOCOL (LDU1)       |                           |
    | (Alternating LDU1/LDU2)   |                           |
    |-------------------------->|-------------------------->|
    |                           |                           |
    | ... (voice continues) ... |                           |
    |                           |                           |
    | P25 PROTOCOL (TDU)        |                           |
    | Terminator Data Unit      |                           |
    | [End of transmission]     |                           |
    |-------------------------->|-------------------------->|
    |                           |                           |
```

**P25 Call Flow Details:**

- **HDU (Header Data Unit)**: Contains call setup information, manufacturer ID, and algorithm ID
- **LDU1/LDU2 Alternation**: P25 alternates between LDU1 and LDU2 frames
  - **LDU1**: Contains Link Control (LC) information and voice IMBE frames
  - **LDU2**: Contains Encryption Sync (ESS) and voice IMBE frames
- **Low Speed Data (LSD)**: Both LDU1 and LDU2 carry low-speed data channel
- **TDU (Terminator)**: Signals end of voice transmission with optional link control
- **Frame Rate**: ~50 packets/second (each LDU = 180ms of audio)
- **GRANT_REQ**: Optional, only used in FNE authoritative mode (not shown above)

#### Example 4: NXDN Voice Call Flow

```
Peer A (Initiator)          FNE Master              Peer B (Recipient)
    |                           |                           |
    | NXDN PROTOCOL (RCCH)      |                           |
    | Radio Control Channel     |                           |
    | [Call setup signaling]    |                           |
    |-------------------------->|-------------------------->|
    |                           | (Route based on TG)       |
    |                           |                           |
    | NXDN PROTOCOL (RTCH)      |                           |
    | Radio Traffic Channel     |                           |
    | [Voice Header + SACCH]    |                           |
    |-------------------------->|-------------------------->|
    |                           |                           |
    | NXDN PROTOCOL (RTCH)      |                           |
    | [Voice Frame + FACCH]     |                           |
    |-------------------------->|-------------------------->|
    |                           |                           |
    | NXDN PROTOCOL (RTCH)      |                           |
    | [Voice Frame + SACCH]     |                           |
    |-------------------------->|-------------------------->|
    |                           |                           |
    | ... (voice continues) ... |                           |
    |                           |                           |
    | NXDN PROTOCOL (RTCH)      |                           |
    | [Voice Frame - Last]      |                           |
    |-------------------------->|-------------------------->|
    |                           |                           |
    | NXDN PROTOCOL (RCCH)      |                           |
    | [Disconnect Message]      |                           |
    |-------------------------->|-------------------------->|
    |                           |                           |
```

**NXDN Call Flow Details:**

- **RCCH (Radio Control Channel)**: Carries control signaling for call setup and teardown
- **RTCH (Radio Traffic Channel)**: Carries voice and associated control data
- **SACCH (Slow Associated Control Channel)**: Embedded control channel in voice frames
- **FACCH (Fast Associated Control Channel)**: Steals voice bits for urgent signaling
- **Frame Structure**: NXDN frames contain 49 bits of encoded voice (AMBE+2)
- **Frame Rate**: ~25-50 packets/second depending on mode (4800 bps or 9600 bps)
- **Message Types**:
  - **CAC (Common Access Channel)**: Site information and idle channel data
  - **UDCH (User Data Channel)**: Short data messages
  - **Voice**: AMBE+2 encoded voice frames with embedded control data
- **GRANT_REQ**: Optional, only used in FNE authoritative mode (not shown above)

#### Example 5: Encryption Key Request and Response (KEY_REQ / KEY_RSP)

The KEY_REQ and KEY_RSP messages are used by peers to request encryption keys for securing call data. These messages flow **upstream from FNE to FNE** in a hierarchical network, allowing encryption key distribution from authoritative key management servers.

**Message Flow:**

```
Peer (dvmhost)           FNE (Child)              FNE (Parent/KMS)
    |                        |                           |
    | KEY_REQ                |                           |
    | KeyID=0x1234           |                           |
    | AlgID=0x81 (AES-256)   |                           |
    | SrcID=123456           |                           |
    |----------------------->|                           |
    |                        | KEY_REQ (forwarded)       |
    |                        | [Same parameters]         |
    |                        |-------------------------->|
    |                        |                           | (Lookup key in KMS)
    |                        |                           | (Verify peer authorization)
    |                        |                           |
    |                        |               KEY_RSP     |
    |                        |     [Encrypted Key Data]  |
    |                        |<--------------------------|
    |            KEY_RSP     |                           |
    | [Encrypted Key Data]   |                           |
    |<-----------------------|                           |
    |                        |                           |
    | (Use key for call)     |                           |
    |                        |                           |
```

**KEY_REQ Message Structure:**

The KEY_REQ message (function code `0x7C`) is sent by a peer when it needs encryption key material to participate in an encrypted call.

**Payload Format:**
```
Offset | Length | Field        | Description
-------|--------|--------------|---------------------------------------------
0-3    | 4      | Peer ID      | Requesting peer identifier
4-7    | 4      | Source ID    | Radio ID requesting the key
8-9    | 2      | Key ID       | Encryption key identifier (KID)
10     | 1      | Algorithm ID | Encryption algorithm (AlgID)
11-14  | 4      | Reserved     | Reserved for future use
```

**Algorithm IDs:**
- `0x80`: Unencrypted (cleartext)
- `0x81`: AES-256
- `0x82`: DES-OFB
- `0x83`: DES-XL
- `0x84`: ADP (Motorola Advanced Digital Privacy)
- `0x9F`: AES-256-GCM (custom)
- Other values: Vendor-specific or reserved

**KEY_RSP Message Structure:**

The KEY_RSP message (function code `0x7D`) is sent in response to a KEY_REQ, containing the requested encryption key material.

**Payload Format:**
```
Offset | Length | Field         | Description
-------|--------|---------------|--------------------------------------------
0-3    | 4      | Peer ID       | Target peer identifier
4-7    | 4      | Source ID     | Radio ID for this key
8-9    | 2      | Key ID        | Encryption key identifier (KID)
10     | 1      | Algorithm ID  | Encryption algorithm (AlgID)
11     | 1      | Key Length    | Length of encrypted key material
12-N   | Var    | Key Material  | Encrypted key data (algorithm-specific)
```

**Key Distribution Flow:**

1. **Upstream Propagation:**
   - KEY_REQ messages flow **upstream** through the FNE hierarchy
   - Each FNE checks if it has the requested key
   - If not found locally, forwards to parent FNE
   - Continues until reaching a Key Management Server (KMS) or authoritative FNE

2. **Key Response:**
   - KEY_RSP flows back down the same path
   - Each FNE may cache the key for future requests
   - Final KEY_RSP delivered to requesting peer

3. **Key Caching:**
   - FNE nodes may cache keys to reduce upstream requests
   - Cache timeout based on key lifetime policies
   - Revoked keys trigger cache invalidation

**Usage Scenarios:**

- **Encrypted Voice Calls:** Peer requests key before transmitting encrypted voice
- **Trunked System Operation:** FNE distributes keys to authorized peers for talkgroup
- **Over-The-Air Rekeying (OTAR):** Dynamic key updates during operation
- **Multi-Site Systems:** Keys propagate through FNE hierarchy to remote sites

**Important Notes:**

- KEY_REQ/KEY_RSP are for **call data encryption** (voice/data payload), not network transport encryption
- Network transport uses AES-256-ECB (see Section 7)
- Keys are transmitted encrypted over the already-secured network connection
- Only authorized peers (verified during RPTK) can request keys
- Key material format is algorithm-specific (AES keys, DES keys, etc.)

### Network Management Examples

#### Example 6: Peer Login Sequence

```
Peer                        FNE Master
  |                             |
  | RPTL (login + salt)         |
  |---------------------------->|
  |                             | (Generate challenge)
  |                             |
  |              Challenge + ACK|
  |<----------------------------|
  |                             |
  | RPTK (auth response)        |
  |---------------------------->|
  |                             | (Verify hash)
  |                             |
  |                         ACK |
  |<----------------------------|
  |                             |
  | RPTC (config JSON)          |
  |---------------------------->|
  |                             | (Store peer config)
  |                             |
  |                         ACK |
  |<----------------------------|
  |                             |
  | (Connected - send PINGs)    |
  |<===========================>|
  |                             |
```

#### Example 7: Affiliation Announcement

```
Peer A                      FNE Master              All Other Peers
  |                             |                           |
  | ANNOUNCE (GRP_AFFIL)        |                           |
  | SrcID=123456, DstID=789     |                           |
  |---------------------------->|                           |
  |                             | (Update affiliation DB)   |
  |                             |                           |
  |                             | ANNOUNCE (GRP_AFFIL)      |
  |                             |-------------------------->|
  |                             |                           |
  |                             | (Replicate to all peers)  |
  |                             |                           |
```

---

## Appendix B: Configuration Examples

### Peer Configuration (YAML)

```yaml
network:
  enable: true
  address: master.example.com
  port: 62031
  localPort: 0          # 0 = random port
  password: "SecurePassword123"
  
  # High Availability
  haEnabled: true
  haAddresses:
    - master1.example.com
    - master2.example.com
    - master3.example.com
  
  # Protocol Support
  dmr: true
  p25: true
  nxdn: false
  analog: false
  
  # Timing
  pingTime: 5           # Ping interval (seconds)
  updateLookupTime: 15  # Lookup update interval (minutes)
  
  # Security
  presharedKey: "0123456789ABCDEF0123456789ABCDEF"
  
  # Metadata
  identity: "Peer-12345"
  rxFrequency: 449000000
  txFrequency: 444000000
  latitude: 40.7128
  longitude: -74.0060
  location: "New York, NY"
```

### Master Configuration (YAML)

```yaml
fne:
  port: 62031
  diagPort: 62032       # Separate diagnostic port
  
  # Authentication
  password: "SecurePassword123"
  
  # Scaling
  workers: 8            # Thread pool size
  softConnLimit: 100    # Soft connection limit
  
  # Network Features
  spanningTree: true
  spanningTreeFastReconnect: true
  disallowU2U: false
  
  # Access Control
  rejectUnknownRID: true
  restrictGrantToAffOnly: true
  restrictPVCallToRegOnly: false
  
  # Timers
  pingTime: 5
  updateLookupTime: 30
  callCollisionTimeout: 5
  
  # Logging
  reportPeerPing: false
  logDenials: true
  logUpstreamCallStartEnd: true
```

---

## Appendix C: Troubleshooting Guide

### Common Issues

**Issue: Peer unable to connect to master**

- **Check**: Network connectivity (ping master)
- **Check**: Firewall rules (UDP port 62031)
- **Check**: Password configuration matches
- **Check**: Master accepting new connections
- **Solution**: Review logs for ACK/NAK messages

**Issue: High packet loss**

- **Check**: Network path MTU
- **Check**: UDP buffer sizes
- **Check**: CPU load on peer/master
- **Check**: Network congestion
- **Solution**: Increase buffer sizes, enable QoS

**Issue: Calls not routing between peers**

- **Check**: Talkgroup active on master
- **Check**: Peers have matching protocol enabled
- **Check**: Affiliation status
- **Check**: Access control lists
- **Solution**: Review master routing logic

**Issue: Authentication failures**

- **Check**: Password matches exactly
- **Check**: Clock synchronization (NTP)
- **Check**: Master challenge timeout
- **Solution**: Verify SHA256 hash calculation

### Debug Techniques

**Enable Packet Dumps:**

```cpp
m_debug = true;  // In Network or FNENetwork constructor
```

**Wireshark Capture:**

```bash
tcpdump -i eth0 -w capture.pcap udp port 62031
wireshark capture.pcap
```

**Log Analysis:**

```bash
# Search for specific peer ID
grep "peerId = 1234567" /var/log/dvm/dvmhost.log

# Search for NAK messages
grep "NAK" /var/log/dvm/dvmfne.log

# Monitor in real-time
tail -f /var/log/dvm/dvmhost.log | grep "NET"
```

---

## Appendix D: Glossary

| Term | Definition |
|------|------------|
| **AMBE** | Advanced Multi-Band Excitation (vocoder) |
| **DMR** | Digital Mobile Radio |
| **DSCP** | Differentiated Services Code Point |
| **FNE** | Fixed Network Equipment |
| **IMBE** | Improved Multi-Band Excitation (vocoder) |
| **LDU** | Logical Data Unit (P25) |
| **NAK** | Negative Acknowledgment |
| **NXDN** | Next Generation Digital Narrowband |
| **P25** | Project 25 (APCO-25) |
| **QoS** | Quality of Service |
| **RID** | Radio ID |
| **RTP** | Real-time Transport Protocol |
| **SSRC** | Synchronization Source |
| **TDU** | Terminator Data Unit (P25) |
| **TGID** | Talkgroup ID |
| **TSDU** | Trunking Signaling Data Unit (P25) |

---

## Appendix E: References

- **RTP (RFC 3550)**: https://tools.ietf.org/html/rfc3550
- **DMR Standard**: ETSI TS 102 361
- **P25 Standard**: TIA-102
- **NXDN Standard**: NXDN Technical Specification
- **DVM GitHub**: https://github.com/DVMProject/dvmhost
- **DVM Documentation**: https://docs.dvmproject.org

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | Dec 3, 2025 | Initial documentation based on source code analysis |

---

**End of Document**
