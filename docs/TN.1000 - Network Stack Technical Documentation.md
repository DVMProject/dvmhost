# DVM Network Stack Technical Documentation

**Version:** 1.5
**Date:** November 22, 2026
**Author:** AI Assistant (updated based on current source code analysis)

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
            ┌──────────────────────┼───────────────────────────┐
            │                      │                           │
    ┌───────▼────────┐      ┌──────▼──────────┐      ┌─────────▼─────────┐
    │  dvmhost #1    │      │  FNE Regional   │      │    dvmbridge      │
    │  (Leaf Peer)   │      │  (Child FNE)    │      │    (Leaf Peer)    │
    └────────────────┘      └──────┬──────────┘      └───────────────────┘
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
                  │                                 │
      ┌───────────┼───────────┬──────────┐   (Takes over on failure)
      │           │           │          │
┌─────▼────┐ ┌────▼────┐ ┌────▼───┐ ┌────▼─────┐
│ dvmhost1 │ │dvmhost2 │ │Regional│ │dvmbridge │
│  (Leaf)  │ │ (Leaf)  │ │  FNE   │ │  (Leaf)  │
└──────────┘ └─────────┘ └────┬───┘ └──────────┘
                              │
                        ┌─────┴─────────┐
                        │               │
                   ┌────▼─────┐   ┌─────▼─────┐
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
- **Topology Cleanup**: Tree updates reconcile parents/children and remove disconnected peer state
- **Fast Reconnect Policy**: Optional `spanningTreeFastReconnect` handling permits the configured reconnect behavior
- **Mixed Children**: FNE nodes can have both child FNE nodes and leaf peers
- **Topology Exchange**: Neighbor FNE links exchange serialized tree state using `NET_TREE` messages
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

These classes provide the foundational networking functionality used by all DVM components (dvmhost, dvmbridge, dvmpatch, and dvmfne):

```
BaseNetwork (Abstract Base Class)
    │
    ├── Network (Peer Implementation - for dvmhost, dvmbridge, dvmpatch)

Core Supporting Classes:
    ├── FrameQueue (RTP Frame Management)
    │   └── RawFrameQueue (Raw UDP Socket Operations)
    │
    ├── RTPHeader (Standard RTP Header - RFC 3550)
    ├── RTPExtensionHeader (RTP Extension Base)
    │   └── RTPFNEHeader (FNE-Specific Extension Header)
    │
    ├── RTPStreamMultiplex (Multi-Stream Management)
    ├── PacketBuffer (Fragmentation/Reassembly)
    ├── NetRPC / RPCHeader (RPC Transport and Framing Helpers)
    ├── AdaptiveJitterBuffer (Per-Stream Reordering/Jitter Management)
    └── udp::Socket (UDP Transport Layer)
```

#### FNE-Specific Network Classes (dvmhost/src/fne/network/)

These classes extend the core functionality specifically for current FNE runtime operations:

```
TrafficNetwork (Extends BaseNetwork)
    │
    ├── MetadataNetwork (Metadata / Activity / Diagnostic Transfer Listener)
    ├── PeerNetwork (Extends Network for Upstream/Neighbor FNE Links)
    ├── FNEPeerConnection (Per-Peer Session State, RTPStreamMultiplex)
    ├── SpanningTree (Peer Topology State)
    │
    └── Call Handlers (Traffic Routing & Management):
        ├── TagDMRData (DMR Protocol Handler)
        ├── TagP25Data (P25 Protocol Handler)
        ├── TagP25P2Data (P25 Phase 2 Protocol Handler)
        ├── TagNXDNData (NXDN Protocol Handler)
        └── TagAnalogData (Analog Protocol Handler)
```

#### Class Usage by Component:

| Class | dvmhost | dvmbridge | dvmpatch | dvmfne |
|-------|---------|-----------|----------|--------------------|
| BaseNetwork | ✓ | ✓ | ✓ | ✓ |
| Network | ✓ | ✓ | ✓ | ✓ (via `PeerNetwork`) |
| TrafficNetwork | ✗ | ✗ | ✗ | ✓ |
| MetadataNetwork | ✗ | ✗ | ✗ | ✓ |
| PeerNetwork | ✗ | ✗ | ✗ | ✓ |
| FrameQueue | ✓ | ✓ | ✓ | ✓ |
| RTPHeader/FNEHeader | ✓ | ✓ | ✓ | ✓ |
| TagXXXData | ✗ | ✗ | ✗ | ✓ |
| FNEPeerConnection | ✗ | ✗ | ✗ | ✓ |
| SpanningTree | ✗ | ✗ | ✗ | ✓ |

### BaseNetwork Class

The `BaseNetwork` class provides core networking functionality shared by both peer and master implementations:

- **Ring Buffers**: Fixed-size circular buffers (4098 bytes each) for DMR, P25, P25 Phase 2, NXDN, and analog receive data
- **Protocol Writers**: Methods for writing DMR, P25 LDU1/LDU2/TDU/TSDU/TDULC, P25 Phase 2, NXDN, and analog frames
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
    RingBuffer m_rxP25P2Data;       // P25 Phase 2 receive buffer (4KB)
    RingBuffer m_rxNXDNData;        // NXDN receive buffer (4KB)
    RingBuffer m_rxAnalogData;      // Analog receive buffer (4KB)
    
    // Stream identifiers
    uint32_t* m_dmrStreamId;        // DMR stream IDs (2 slots)
    uint32_t m_p25StreamId;         // P25 stream ID
    uint32_t* m_p25P2StreamId;      // P25 Phase 2 stream IDs (2 slots)
    uint32_t m_nxdnStreamId;        // NXDN stream ID
    uint32_t m_analogStreamId;      // Analog stream ID
};
```

### Network Class (Peer)

The `Network` class implements peer-side functionality for connecting to FNE masters:

- **Connection State Machine**: Login, authorization, configuration, and running states
- **Authentication**: SHA256 digest authentication using FNE-provided per-login salt and shared password
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

### TrafficNetwork / MetadataNetwork / PeerNetwork Classes

Current FNE runtime networking is split across multiple classes rather than a single `FNENetwork` implementation:

- **TrafficNetwork**: Primary master-side traffic/control plane. Manages connected peers, routing, affiliations, ACL distribution, spanning tree state, HA state, and protocol call handlers.
- **MetadataNetwork**: Separate metadata/activity/diagnostic/status listener bound to `master.port + 1`, sharing the same peer/runtime context as `TrafficNetwork`.
- **PeerNetwork**: Upstream or neighbor FNE client link implementation used when this FNE also peers to other masters.

Representative current layout:

```cpp
class TrafficNetwork : public BaseNetwork {
private:
    std::unordered_map<uint32_t, PeerConnectionPtr> m_peers;
    concurrent::unordered_map<uint32_t,
        std::shared_ptr<fne_lookups::AffiliationLookup>> m_peerAffiliations;
    concurrent::shared_unordered_map<uint32_t, std::vector<uint32_t>> m_ccPeerMap;

    lookups::RadioIdLookup* m_ridLookup;
    lookups::TalkgroupRulesLookup* m_tidLookup;
    lookups::PeerListLookup* m_peerListLookup;

    SpanningTree* m_treeRoot;
    ThreadPool m_threadPool;
    Timer m_maintainenceTimer;

    bool m_enableSpanningTree;
};

class MetadataNetwork : public BaseNetwork {
private:
    TrafficNetwork* m_trafficNetwork;
    ThreadPool m_threadPool;
};

class PeerNetwork : public Network {
private:
    lookups::PeerListLookup* m_pidLookup;
    ThreadPool m_threadPool;
};
```

At the application level, `HostFNE` owns `m_network` (`TrafficNetwork`), `m_mdNetwork` (`MetadataNetwork`), and `m_peerNetworks` (`PeerNetwork` map). Startup order is `createMasterNetwork()`, then `initializeRESTAPI()`, then `createPeerNetworks()`.

---

## 3. Network Protocol Layers

The DVM network stack implements a layered protocol architecture:

### Layer 1: UDP Transport

- **Socket Operations**: Standard UDP datagram socket (IPv4/IPv6)
- **Socket Operations**: Datagram reads and writes through `udp::Socket`; address resolution supports IPv4 and IPv6
- **Application Processing**: FNE packet work can be dispatched through configured worker pools
- **Encryption**: Optional AES-256-CBC UDP payload wrapping with a preshared key, per-datagram IV, and `0xC0FE` packet leader

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
- **PT (Payload Type)**: `0x56` for DVM writes; the reader accepts `0x56` and `0x57`
- **Sequence Number**: Incrementing packet sequence (0-65535)
- **Timestamp**: RTP timestamp (8000 Hz clock rate)
- **SSRC**: Synchronization source identifier

### Layer 3: RTP Extension (FNE Header)

Custom FNE extension header:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Extension Profile (0x00FE)   | Extension Length (0x0004)     |
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
- **Extension Profile**: 0x00FE (`DVM_FRAME_START`), encoded as a 16-bit big-endian value
- **Extension Length**: Length of the 16-byte FNE body in 32-bit words (4)
- **CRC-16**: Checksum of payload data
- **Function**: Primary operation code (see NET_FUNC)
- **Sub-Function**: Secondary operation code (see NET_SUBFUNC)
- **Stream ID**: Unique identifier for call/session (32-bit)
- **Peer ID**: Source peer identifier
- **Message Length**: Length of payload following headers

### Layer 4: Protocol Payload

Protocol-specific data follows the RTP+FNE headers:

- **DMR**: 55-byte logical network message plus 8 transmitted padding bytes
- **P25**: DVM/DFSI application messages with DUID-specific lengths; these are not copied 242-byte air-interface LDUs
- **P25 Phase 2**: 66-byte logical message plus 8 transmitted padding bytes
- **NXDN**: 74-byte logical message plus 8 transmitted padding bytes
- **Analog**: 344-byte logical message containing 320 opaque audio bytes, plus 8 transmitted padding bytes

### Complete Packet Example

**DMR Voice Frame - Total Size: 95 bytes (UDP payload)**

```
Complete Packet Structure:
┌─────────────┬─────────────┬──────────────┐
│  RTP Header │ RTP Extension/FNE Header │ DMR Application Message │
│   12 bytes  │          20 bytes        │        63 bytes         │
└─────────────┴─────────────┴──────────────┘

Structural hexadecimal representation (95 bytes; symbolic fields are not a known-answer vector):

Offset 0-11 (RTP Header):
90 56 SS SS TT TT TT TT XX XX XX XX

Offset 12-31 (FNE Header):
00 FE 00 04 CC CC 00 00 12 34 56 78 00 00 4E 20 00 00 00 3F

Offset 32-94 (DMR application message):
44 4D 52 44 ...
["DMRD", DMR network fields, raw DMR frame, BER/RSSI, and 8 padding bytes]

Decoded values:
  RTP:
    - Version: 2, Extension: yes, Marker: no
    - Payload Type: 0x56
    - Sequence, timestamp, and SSRC: example-dependent
  
  FNE Header:
    - Extension Profile: 0x00FE
    - Extension Length: 4 words
    - CRC-16: calculated over all 63 application-message bytes
    - Function: 0x00 (PROTOCOL)
    - Sub-Function: 0x00 (DMR)
    - Stream ID: 0x12345678
    - Peer ID: 20000
    - Message Length: 63
  
  DMR Payload:
    - Tag: "DMRD"
    - Sequence, source ID, destination ID, slot/call type, and raw DMR frame
    - BER/RSSI trailer and eight zeroed padding bytes
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
90 56 00 2A 00 00 1F 40 00 00 4E 20
│  │  │  │  │        │  │        │
│  │  │  │  │        │  └────────┴─ SSRC: 0x00004E20 (20000)
│  │  │  │  └────────┴──────────── Timestamp: 0x00001F40 (8000)
│  │  └──┴───────────────────────── Sequence: 0x002A (42)
│  └─────────────────────────────── Payload Type: 0x56 (marker bit clear)
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
0-1         | Extension Profile | 2 bytes | 0x00FE (big-endian)
2-3         | Extension Length  | 2 bytes | 4 words (big-endian)
4-5         | CRC-16            | 2 bytes | Application-message checksum (big-endian)
6           | Function          | 1 byte  | NET_FUNC opcode
7           | Sub-Function      | 1 byte  | NET_SUBFUNC opcode
8-11        | Stream ID         | 4 bytes | Call/session identifier (big-endian)
12-15       | Peer ID           | 4 bytes | Source peer ID (big-endian)
16-19       | Message Length    | 4 bytes | Payload length (big-endian)
```

**Example FNE Header (hexadecimal):**
```
00 FE 00 04 A3 5C 00 00 12 34 56 78 00 00 4E 20 00 00 00 3F
│     │     │     │  │  │           │           │           │
│     │     │     │  │  │           │           └───────────┴─ Message Length: 63
│     │     │     │  │  │           └───────────────────────── Peer ID: 20000
│     │     │     │  │  └───────────────────────────────────── Stream ID: 0x12345678
│     │     │     └──┴──────────────────────────────────────── Function/Subfunction: PROTOCOL/DMR
│     │     └───────────────────────────────────────────────── CRC-16: example value
│     └─────────────────────────────────────────────────────── Extension Length: 4 words
└───────────────────────────────────────────────────────────── Extension Profile: 0x00FE
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
| 0x79 | GRANT_REL | Channel grant release |
| 0x7A | GRANT_REQ | Channel grant request |
| 0x7B | INCALL_CTRL | In-call control (busy/reject) |
| 0x7C | KEY_REQ | Encryption key request |
| 0x7D | KEY_RSP | Encryption key response |
| 0x7E | ACK | Acknowledgment |
| 0x7F | NAK | Negative acknowledgment |
| 0x80 | KEY_KEK_REQ | Encryption key LLA/UKEK request |
| 0x81 | KEY_KEK_RSP | Encryption key LLA/UKEK response |
| 0x84 | SUBSCRIBER_OP | Subscriber operation query/control |
| 0x85 | RADIO_ALIAS_SYNC | Radio alias synchronization |
| 0x8E | KEYS_INVENTORY | Encryption key inventory transfer |
| 0x8F | KEYS_UPDATE | Encryption key container update |
| 0x90 | TRANSFER | Activity/diagnostic/status transfer |
| 0x91 | ANNOUNCE | Affiliation/registration announcements |
| 0x92 | REPL | FNE replication (peer/TG/RID lists) |
| 0x93 | NET_TREE | Network spanning tree management |

### NAK Reason Codes

When an FNE master sends a NAK (Negative Acknowledgment), it includes a 16-bit reason code to indicate why the operation failed. These reason codes help peers diagnose connection problems and take appropriate action.

**NAK Message Format:**
```
Reserved (6 bytes) + Peer ID (4 bytes) + Reason Code (2 bytes)
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
| 9 | FNE_DUPLICATE_CONN | Warning | FNE detected this peer as a duplicate connection | Disconnect, clear remote peer state, and retry with duplicate-connection backoff |

**Severity Levels:**

- **Warning**: Temporary or correctable condition. Peer should retry or adjust configuration.
- **Fatal**: Permanent rejection. Peer should cease all network operations and disable itself.

**Special Handling:**

- `PEER_ACL` (Code 7): This is the only fatal NAK reason. When received, the peer must:
  - Log an error message
  - Transition to `NET_STAT_WAITING_LOGIN` state
  - Set `m_enabled = false` to disable all network operations
  - Stop attempting to reconnect (unless `neverDisableOnACLNAK` configuration flag is set)

- `FNE_MAX_CONN` (Code 8): The FNE has reached its permitted connection count. The peer returns to its configured retry/reconnect behavior.

- `FNE_DUPLICATE_CONN` (Code 9): The peer transitions to `NET_STAT_WAITING_CONNECT`, clears `m_remotePeerId`, sets the duplicate-connection flag, reduces retry count thresholds, and uses the extended duplicate-connection reconnect delay (`DUPLICATE_CONN_RETRY_TIME`) to avoid rapid reconnect storms.

### NET_SUBFUNC: Secondary Function Codes

#### Protocol Sub-Functions (PROTOCOL)

| Code | Name | Purpose |
|------|------|---------|
| 0x00 | PROTOCOL_SUBFUNC_DMR | DMR protocol data |
| 0x01 | PROTOCOL_SUBFUNC_P25 | P25 protocol data |
| 0x02 | PROTOCOL_SUBFUNC_NXDN | NXDN protocol data |
| 0x03 | PROTOCOL_SUBFUNC_P25_P2 | P25 Phase 2 protocol data |
| 0x0F | PROTOCOL_SUBFUNC_ANALOG | Analog audio data |

#### Master Sub-Functions (MASTER)

| Code | Name | Purpose |
|------|------|---------|
| 0x00 | MASTER_SUBFUNC_WL_RID | Whitelist RID update |
| 0x01 | MASTER_SUBFUNC_BL_RID | Blacklist RID update |
| 0x02 | MASTER_SUBFUNC_ACTIVE_TGS | Active talkgroup list |
| 0x03 | MASTER_SUBFUNC_DEACTIVE_TGS | Deactivate talkgroups |
| 0x04 | MASTER_SUBFUNC_TG_ADMIT | Talkgroup RID admission lists |
| 0xA3 | MASTER_HA_PARAMS | High availability parameters |

#### Transfer Sub-Functions (TRANSFER)

| Code | Name | Purpose |
|------|------|---------|
| 0x01 | TRANSFER_SUBFUNC_ACTIVITY | Activity log data |
| 0x02 | TRANSFER_SUBFUNC_DIAG | Diagnostic log data |
| 0x03 | TRANSFER_SUBFUNC_STATUS | Peer status JSON |
| 0x04 | TRANSFER_SUBFUNC_PATCH_STATUS | Console patch status transfer |

#### Subscriber Operation Sub-Functions (SUBSCRIBER_OP)

| Code | Name | Purpose |
|------|------|---------|
| 0x00 | SUBSCRIBER_OP_OPERATION | Subscriber operation result query |
| 0x01 | SUBSCRIBER_OP_P25_STATUS | P25 subscriber status query |
| 0x02 | SUBSCRIBER_OP_START | Start subscriber operation |

#### Announce Sub-Functions (ANNOUNCE)

| Code | Name | Purpose |
|------|------|---------|
| 0x00 | ANNC_SUBFUNC_GRP_AFFIL | Group affiliation |
| 0x01 | ANNC_SUBFUNC_UNIT_REG | Unit registration |
| 0x02 | ANNC_SUBFUNC_UNIT_DEREG | Unit deregistration |
| 0x03 | ANNC_SUBFUNC_GRP_UNAFFIL | Group unaffiliation |
| 0x90 | ANNC_SUBFUNC_AFFILS | Complete affiliation update |
| 0x91 | ANNC_SUBFUNC_UNIT_REGS | Complete unit registration update |
| 0x9A | ANNC_SUBFUNC_SITE_VC | Site voice channel list |

#### Replication Sub-Functions (REPL)

| Code | Name | Purpose |
|------|------|---------|
| 0x00 | REPL_TALKGROUP_LIST | Talkgroup rules replication |
| 0x01 | REPL_RID_LIST | Radio ID replication |
| 0x02 | REPL_PEER_LIST | Peer configuration list |
| 0x03 | REPL_RID_ALIAS_LIST | Radio alias list transfer |
| 0xA2 | REPL_ACT_PEER_LIST | Active peer list |
| 0xA3 | REPL_HA_PARAMS | HA configuration parameters |
| 0xA4 | REPL_PATCH_STATUS | Console patch status transfer |

#### Network Tree Sub-Functions (NET_TREE)

| Code | Name | Purpose |
|------|------|---------|
| 0x00 | NET_TREE_LIST | Spanning-tree topology list transfer |
| 0x01 | NET_TREE_DISC | Spanning-tree disconnect notification |

#### In-Call Control (INCALL_CTRL)

| Code | Name | Purpose |
|------|------|---------|
| 0x00 | BUSY_DENY | Reject call - channel busy |
| 0x01 | REJECT_TRAFFIC | Reject active traffic |
| 0xD2 | DMR_RC_CEASE_TRANSMIT | DMR reverse channel: cease transmit |
| 0xD3 | DMR_RC_REQUEST_CEASE_TRANSMIT | DMR reverse channel: request cease transmit |
| 0xD4 | DMR_RC_MAXIMUM_POWER | DMR reverse channel: maximum power |
| 0xD5 | DMR_RC_MINIMUM_POWER | DMR reverse channel: minimum power |
| 0xD6 | DMR_RC_POWER_INCREASE_ONE_STEP | DMR reverse channel: increase power one step |
| 0xD7 | DMR_RC_POWER_DECREASE_ONE_STEP | DMR reverse channel: decrease power one step |

---

## 6. Connection Management

### Connection States

The peer connection follows a state machine:

```
NET_STAT_WAITING_CONNECT
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
- Repeater login tag (`"RPTL"`)
- Peer ID

```cpp
bool Network::writeLogin() {
    uint8_t buffer[8U];
    ::memcpy(buffer + 0U, TAG_REPEATER_LOGIN, 4U);
    SET_UINT32(m_peerId, buffer, 4U);
    
    return writeMaster({ NET_FUNC::RPTL, NET_SUBFUNC::NOP }, 
                       buffer, 8U, pktSeq(true), m_loginStreamId);
}
```

**Raw RPTL Message Structure:**

```
[RTP Header: 12 bytes] + [FNE Header: 20 bytes] + [RPTL Payload: 8 bytes]

RPTL Payload (8 bytes):
Byte Offset | Field              | Size    | Description
------------|--------------------|---------|---------------------------------
0-3         | TAG_REPEATER_LOGIN | 4 bytes | ASCII "RPTL"
4-7         | Peer ID            | 4 bytes | Peer identifier (big-endian)
```

**Example RPTL Payload (hexadecimal):**
```
52 50 54 4C 00 00 4E 20
│        │  │        │
│        │  └────────┴─ Peer ID: 0x00004E20 (20000)
└────────┴──────────── TAG: "RPTL"
```

After receiving `RPTL`, the FNE generates a random 32-bit salt and returns it in the `ACK` payload extension bytes. The peer stores that salt (`m_salt`) and uses it for the digest in Step 2.

**Step 2: Authorization Digest (RPTK)**

Peer responds with SHA256 digest of `salt || password`:

```cpp
bool Network::writeAuthorisation() {
    size_t size = m_password.size();
    uint8_t* in = new uint8_t[size + sizeof(uint32_t)];
    ::memcpy(in, m_salt, sizeof(uint32_t));
    for (size_t i = 0U; i < size; i++)
        in[i + sizeof(uint32_t)] = m_password.at(i);

    uint8_t out[40U];
    ::memcpy(out + 0U, TAG_REPEATER_AUTH, 4U);
    SET_UINT32(m_peerId, out, 4U);

    edac::SHA256 sha256;
    sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out + 8U);
    delete[] in;

    return writeMaster({ NET_FUNC::RPTK, NET_SUBFUNC::NOP }, 
                       out, 40U, pktSeq(), m_loginStreamId);
}
```

**Raw RPTK Message Structure:**

```
[RTP Header: 12 bytes] + [FNE Header: 20 bytes] + [RPTK Payload: 40 bytes]

RPTK Payload (40 bytes):
Byte Offset | Field              | Size    | Description
------------|--------------------|---------|---------------------------------
0-3         | TAG_REPEATER_AUTH  | 4 bytes | ASCII "RPTK"
4-7         | Peer ID            | 4 bytes | Peer identifier (big-endian)
8-39        | SHA256 Hash        | 32 bytes| SHA256(salt + password)
```

**Example RPTK Payload (hexadecimal):**
```
52 50 54 4B 00 00 4E 20 [32 bytes SHA256(salt||password)]
│        │  │        │
│        │  └────────┴─ Peer ID: 20000
└────────┴──────────── TAG: "RPTK"
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
    
    json::object sysInfo = json::object();
    sysInfo["latitude"].set<float>(m_metadata->latitude);
    sysInfo["longitude"].set<float>(m_metadata->longitude);
    sysInfo["height"].set<int>(m_metadata->height);
    sysInfo["location"].set<std::string>(m_metadata->location);
    config["info"].set<json::object>(sysInfo);

    json::object channel = json::object();
    channel["txPower"].set<uint32_t>(m_metadata->power);
    channel["txOffsetMhz"].set<float>(m_metadata->txOffsetMhz);
    channel["chBandwidthKhz"].set<float>(m_metadata->chBandwidthKhz);
    channel["channelId"].set<uint8_t>(m_metadata->channelId);
    channel["channelNo"].set<uint32_t>(m_metadata->channelNo);
    config["channel"].set<json::object>(channel);

    json::object rcon = json::object();
    rcon["password"].set<std::string>(m_metadata->restApiPassword);
    rcon["port"].set<uint16_t>(m_metadata->restApiPort);
    config["rcon"].set<json::object>(rcon);

    config["peerClass"].set<uint32_t>(PEER_CONN_CLASS_STANDARD);
    config["conventionalPeer"].set<bool>(m_metadata->isConventional);
    config["software"].set<std::string>(std::string(__NETVER__));
    
    // Serialize to JSON string
    std::string json = json::object(config).serialize();

    DECLARE_CHAR_ARRAY(buffer, json.length() + 9U);
    ::memcpy(buffer + 0U, TAG_REPEATER_CONFIG, 4U);
    ::snprintf(buffer + 8U, json.length() + 1U, "%s", json.c_str());
    
    return writeMaster({ NET_FUNC::RPTC, NET_SUBFUNC::NOP },
                       (uint8_t*)buffer, json.length() + 8U,
                       RTP_END_OF_CALL_SEQ, m_loginStreamId);
}
```

**Raw RPTC Message Structure:**

```
[RTP Header: 12 bytes] + [FNE Header: 20 bytes] + [TAG: 4 bytes] + [Reserved: 4 bytes] + [JSON: variable]

RPTC Payload:
Byte Offset | Field              | Size      | Description
------------|--------------------|-----------|-----------------------------
0-3         | TAG_REPEATER_CONFIG| 4 bytes   | ASCII "RPTC"
4-7         | Reserved           | 4 bytes   | Reserved (zero-initialized)
8+          | JSON Configuration | Variable  | UTF-8 JSON string
```

**Example RPTC Payload (partial hexadecimal + ASCII):**
```
52 50 54 43 00 00 00 00 7B 22 69 64 65 6E 74 69 74 79 22 3A 22 4B 42 ...
│  │  │  │  │        │  │  
│  │  │  │  │        │  └─ JSON starts: {"identity":"KB...
│  │  │  │  └────────┴─── Reserved: 0x00000000
└──┴──┴──┴───────────── TAG: "RPTC"
```

**RPTC Configuration JSON Schema:**

The RPTC configuration uses a nested JSON structure with three main object groups: `info` (system information), `channel` (RF parameters), and `rcon` (remote control/REST API). Current peers also include the top-level `peerClass` field, which is the canonical way to describe the peer connection class.

```json
{
  "identity": "string",          // Peer identity/callsign (required)
  "rxFrequency": 0,              // RX frequency in Hz (required)
  "txFrequency": 0,              // TX frequency in Hz (required)
  
  "info": {                      // System information object (required)
    "latitude": 0.0,             // Latitude in decimal degrees
    "longitude": 0.0,            // Longitude in decimal degrees
    "height": 0,                 // Antenna height in meters
    "location": "string"         // Location description
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
 
  "peerClass": 2,                // Canonical peer connection class enum

  "conventionalPeer": false,     // Conventional (non-trunked) mode (optional)
  "software": "string"           // Software identifier (optional)
}
```

**PEER_CONN_CLASS Enumeration:**

- **0 (`PEER_CONN_CLASS_UNKNOWN`)**: Unknown class. The FNE normalizes this to `PEER_CONN_CLASS_STANDARD` when processing RPTC.
- **1 (`PEER_CONN_CLASS_NEIGHBOR`)**: Downstream neighbor FNE peer used for FNE-to-FNE interconnects.
- **2 (`PEER_CONN_CLASS_STANDARD`)**: Standard RF site peer. This is the default value emitted by current peer implementations.
- **3 (`PEER_CONN_CLASS_SYSVIEW`)**: SysView monitoring peer.
- **4 (`PEER_CONN_CLASS_CONSOLE`)**: Console client peer.
- **5 (`PEER_CONN_CLASS_INVALID`)**: Sentinel only. Values greater than or equal to this are treated as invalid and normalized to `PEER_CONN_CLASS_STANDARD` by the FNE.

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
    "location": "Philadelphia, PA"
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
 
  "peerClass": 2,

  "conventionalPeer": false,
  "software": "DVMHOST_R04A00"
}
```

**Configuration Field Details:**

**Top-Level Fields:**
- **identity**: Unique identifier for the peer (callsign, site name, etc.)
- **rxFrequency/txFrequency**: Operating frequencies in Hertz
- **peerClass**: Canonical peer connection class (`PEER_CONN_CLASS_STANDARD` is emitted by current peers)
- **conventionalPeer**: Indicates non-trunked operation mode (affects grant behavior)
- **software**: Software version string (e.g., `DVMHOST_R04A00`) for compatibility checking

**System Information Object (`info`):**
- **latitude/longitude**: Geographic coordinates in decimal degrees for mapping and adjacent site calculations
- **height**: Antenna height above ground level in meters (used for coverage calculations)
- **location**: Human-readable location description

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
- `peerClass` → Primary peer classification used to determine routing and peer behavior
- `conventionalPeer` → Affects talkgroup affiliation and grant behavior (conventional peers don't require grants)

If `peerClass` is absent, the FNE can still fall back to legacy booleans for backward compatibility with older peers. If `peerClass` is `PEER_CONN_CLASS_UNKNOWN` or greater than or equal to `PEER_CONN_CLASS_INVALID`, the FNE normalizes it to `PEER_CONN_CLASS_STANDARD`.

**Step 4: ACK/NAK Response**

Master responds with ACK (success) or NAK (failure):
- **ACK**: Peer transitions to RUNNING state
- **NAK**: Connection rejected (authentication failure, ACL deny, etc.)

### Heartbeat Mechanism

Once connected, peers and master exchange periodic PING/PONG messages:

**Peer → Master: PING**

```cpp
bool Network::writePing() {
    uint8_t buffer[1U];
    ::memset(buffer, 0x00U, 1U);
    
    return writeMaster({ NET_FUNC::PING, NET_SUBFUNC::NOP }, 
                       buffer, 1U, RTP_END_OF_CALL_SEQ,
                       createStreamId());
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

Code excerpts in this section are abbreviated to emphasize framing and flow.
They are not declaration-accurate substitutes for `BaseNetwork.h`; consult the
current method signatures and message builders when implementing or calling the
API.

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

**Logical DMR Message Size:** 55 bytes (`DMR_PACKET_LENGTH = 55U`). The current
writer includes the eight `PACKET_PAD` bytes in the FNE message length, so 63
application-message bytes are transmitted.

**Constants:**
- `DMR_PACKET_LENGTH = 55U` (defined in `BaseNetwork.h`)
- `DMR_FRAME_LENGTH_BYTES = 33U` (defined in `DMRDefines.h`)
- `PACKET_PAD = 8U` (padding for buffer allocation)
- Total transmitted application-message size: 55 + 8 = 63 bytes

**Raw DMR Packet Example (hexadecimal):**
```
[RTP Header: 12 bytes] [FNE Header: 20 bytes] [DMR Application Message: 63 bytes]

Complete Packet (95 bytes):
90 56 SS SS TT TT TT TT XX XX XX XX | 00 FE 00 04 CC CC 00 00 II II II II PP PP PP PP 00 00 00 3F | 44 4D 52 44 [DMR fields, frame, BER/RSSI, and padding]
├─────────── RTP (12) ────────────┤   ├─────────────────────── FNE (20) ────────────────────────┤   ├──────────── DMR + padding (63) ────────────────────┤

DMR Payload breakdown:
44 4D 52 44 05 00 AA BB 00 CC DD 00 00 00 01 02 00 00 00 00 [33 bytes DMR frame...] 00 7F
│        │  │  │     │  │     │   │     │  │  │  │       │                          │  │
│        │  │  │     │  │     │   │     │  │  │  └───────┴ Reserved                 │  │
│        │  │  │     │  │     │   │     │  │  └─────────── Slot/FLCO/DataType: 0x02 │  │
│        │  │  │     │  │     │   │     │  └────────────── Control: 0x01            │  │ 
│        │  │  │     │  │     │   └─────┴───────────────── Reserved                 │  │
│        │  │  │     │  └─────┴─────────────────────────── Dest ID: 0x00CCDD (52445)│  │
│        │  │  └─────┴──────────────────────────────────── Src ID: 0x00AABB (43707) │  │
│        │  └───────────────────────────────────────────── Sequence: 5              │  │
└────────┴──────────────────────────────────────────────── Tag: "DMRD" (0x444D5244) │  │
                                                                                    └─── BER: 0x00
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

**P25 Message Header Structure (24 bytes):**

**Control Byte (`CONTROL_BYTE`) Flags:**

The Control Byte at offset 14 is a bitmask defined by `enum CONTROL_BYTE` in `BaseNetwork.h`. It is passed as the `controlByte` parameter to `writeP25TDU()` and stored in the TDU packet to signal in-band network control to the receiving endpoint. Multiple flags may be OR'd together. A value of `0x00` means no control operation is requested.

| Bit Mask | Name | Description |
|----------|------|-------------|
| `0x80` | `NET_CTRL_GRANT_DEMAND` | Grant demand — peer is requesting a channel grant for an ongoing call |
| `0x40` | `NET_CTRL_GRANT_DENIAL` | Grant denial — FNE is denying a channel grant request |
| `0x20` | `NET_CTRL_SWITCH_OVER` | Call source RID switch over — the originating radio ID changed mid-call |
| `0x08` | `NET_CTRL_GRANT_ENCRYPT` | Encrypted grant — the associated channel grant is for an encrypted call |
| `0x01` | `NET_CTRL_U2U` | Unit-to-unit — this is a unit-to-unit (private) call, not a group call |

**P25 Message Header Structure (24 bytes):**

The message header created by `createP25_MessageHdr()` contains:

| Offset | Length | Field | Description |
|--------|--------|-------|-------------|
| 0-3 | 4 | P25 Tag | "P25D" (0x50323544) |
| 4 | 1 | LCO | Link Control Opcode |
| 5-7 | 3 | Source ID | Calling radio ID |
| 8-10 | 3 | Destination ID | Target talkgroup/radio |
| 11-12 | 2 | System ID | Site system ID |
| 13 | 1 | Reserved | Reserved (0x00) |
| 14 | 1 | Control Byte | Network control flags (`CONTROL_BYTE` bitmask) |
| 15 | 1 | MFId | Manufacturer ID |
| 16-18 | 3 | Network ID | FNE network/WACN ID |
| 19 | 1 | Slot / Reserved | Bit 7: slot number; bits 4–0: P2 DUID (if Phase 2, else 0x00) |
| 20 | 1 | LSD1 | Low Speed Data byte 1 |
| 21 | 1 | LSD2 | Low Speed Data byte 2 |
| 22 | 1 | DUID | Data Unit ID (0x05 LDU1, 0x0A LDU2, 0x03 TDU, 0x07 TSDU, 0x0F TDULC) |
| 23 | 1 | Frame Length | Payload data length in bytes (0x00 for LDU1/LDU2; set for TDU/TSDU/TDULC) |

```
P25 Message Header (first 24 bytes):
50 32 35 44 00 00 AA BB 00 CC DD 00 01 00 00 00 00 00 01 00 00 00 05 00
│        │  │  │     │  │     │  │  │  │  │  │  │     │  │  │  │  │  │
│        │  │  │     │  │     │  │  │  │  │  │  │     │  │  │  │  │  └─────── Frame Length: 0x00
│        │  │  │     │  │     │  │  │  │  │  │  │     │  │  │  │  └────────── DUID: 0x05 (LDU1)
│        │  │  │     │  │     │  │  │  │  │  │  │     │  │  │  └───────────── LSD2: 0x00
│        │  │  │     │  │     │  │  │  │  │  │  │     │  │  └──────────────── LSD1: 0x00
│        │  │  │     │  │     │  │  │  │  │  │  │     │  └─────────────────── Slot/Reserved: 0x00
│        │  │  │     │  │     │  │  │  │  │  │  └─────┴────────────────────── Net ID: 0x000001 (1)
│        │  │  │     │  │     │  │  │  │  │  └─────────────────────────────── MFId: 0x00 (Standard)
│        │  │  │     │  │     │  │  │  │  └────────────────────────────────── Control Byte: 0x00
│        │  │  │     │  │     │  │  │  └───────────────────────────────────── Reserved: 0x00
│        │  │  │     │  │     │  └──┴──────────────────────────────────────── Sys ID: 0x0001 (1)
│        │  │  │     │  └─────┴────────────────────────────────────────────── Dst ID: 0x00CCDD (52445)
│        │  │  └─────┴─────────────────────────────────────────────────────── Src ID: 0x0000AABB (43707)
│        │  └──────────────────────────────────────────────────────────────── LCO: 0x00 (Group Voice)
└────────┴─────────────────────────────────────────────────────────────────── Tag: "P25D" (0x50323544)
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

**Logical P25 LDU1 Size:** 193 bytes (`P25_LDU1_PACKET_LENGTH`); the FNE
message length is 201 bytes after `PACKET_PAD`.

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
[RTP: 12 bytes] [FNE: 20 bytes] [P25 LDU1 Application Message: 201 bytes]

[P25 Message Header 24 bytes]

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
- Total transmitted application-message size: 193 + 8 = 201 bytes

**P25 LDU2 Packet Structure:**

LDU2 has a similar structure to LDU1 but uses different DFSI frame types and contains Encryption Sync (ESS) data instead of Link Control in frames 3-8. Its logical size is 181 bytes (`P25_LDU2_PACKET_LENGTH = 181U`) and transmitted FNE message length is 189 bytes.

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
- Total transmitted application-message size: 181 + 8 = 189 bytes

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

**Raw TDU Packet Example (hexadecimal):**

```
[RTP: 12 bytes] [FNE: 20 bytes] [P25 TDU Application Message: 32 bytes]

Complete Packet (64 bytes):
90 56 SS SS TT TT TT TT XX XX XX XX | 00 FE 00 04 CC CC 00 01 II II II II PP PP PP PP 00 00 00 20 | 50 32 35 44 [24-byte P25 TDU header and 8 padding bytes]
├─────────── RTP (12) ────────────┤   ├────────────────────── FNE (20) ─────────────────────────┤   ├──────── P25 TDU + padding (32) ────────┤
```

**Constants:**
- `MSG_HDR_SIZE = 24U` (defined in `BaseNetwork.h`)
- `PACKET_PAD = 8U`
- Total allocated size: 24 + 8 = 32 bytes
- DUID value: `0x03` (TDU - Terminator Data Unit)

The unencrypted UDP payload ends at byte 63. There is no DUID-specific payload
after the 24-byte P25 header; the final eight bytes are `PACKET_PAD`.

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
[RTP: 12 bytes] [FNE: 20 bytes] [P25 TSDU Application Message: 77 bytes]

Complete Packet (109 bytes):
90 56 SS SS TT TT TT TT XX XX XX XX | 00 FE 00 04 CC CC 00 01 II II II II PP PP PP PP 00 00 00 4D | 50 32 35 44 [P25 TSDU header, 45-byte TSDU, and padding]
├─────────── RTP (12) ────────────┤   ├──────────────────────── FNE (20) ───────────────────────┤   ├──────────────────────────── P25 TSDU + padding (77) ────────────────────────────┤

TSDU Frame Data (45 bytes starting at offset 24):
55 75 F7 FF 5D 7F [NID: 2 bytes] [TSBK: 25 bytes FEC encoded] [Status: 12 bytes]
│              │  └────────────┴────────────────────┴──────────────────────── Raw over-the-air P25 TSDU frame
└──────────────┴───────────────────────────────────────────────────────────── Frame Sync pattern
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
[RTP: 12 bytes] [FNE: 20 bytes] [P25 TDULC Application Message: 86 bytes]

Complete Packet (118 bytes):
90 56 SS SS TT TT TT TT XX XX XX XX | 00 FE 00 04 CC CC 00 01 II II II II PP PP PP PP 00 00 00 56 | 50 32 35 44 [P25 TDULC header, 54-byte TDULC, and padding]
├─────────── RTP (12) ────────────┤   ├──────────────────────── FNE (20) ───────────────────────┤   ├─────────────────────────── P25 TDULC + padding (86) ───────────────────────────┤

TDULC Frame Data (54 bytes starting at offset 24):
55 75 F7 FF 5D 7F [NID: 2 bytes] [LC: 36 bytes FEC encoded] [Status: 10 bytes]
│              │  └────────────┴──────────────────────┴─────────────────────── Raw over-the-air P25 TDULC frame
└──────────────┴────────────────────────────────────────────────────────────── Frame Sync pattern
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

**P25 Phase 2 (TDMA Voice/Data) Network Format:**

P25 Phase 2 traffic is carried under its own network subfunction, `NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2 (0x03)`, rather than the classic `PROTOCOL_SUBFUNC_P25` path. `BaseNetwork::writeP25P2()` creates the encapsulated packet and `Network::clock()` handles it in a dedicated receive branch with separate per-slot receive stream tracking.

The encapsulated Phase 2 format is fixed-length:

| Offset | Length | Field | Description |
|--------|--------|-------|-------------|
| 0-23 | 24 | Message Header | Standard P25 network header created by `createP25_MessageHdr()` |
| 24-63 | 40 | P25 Phase 2 Frame Data | Raw Phase 2 payload (`P25_P2_FRAME_LENGTH_BYTES = 40U`) |
| 64-65 | 2 | Trailer | Reserved logical packet trailer |

**Logical P25 Phase 2 Size:** 66 bytes (`P25_P2_PACKET_LENGTH = 66U`); the
transmitted FNE message length is 74 bytes after padding.

**Constants:**
- `PROTOCOL_SUBFUNC_P25_P2 = 0x03U` (defined in `RTPFNEHeader.h`)
- `P25_P2_PACKET_LENGTH = 66U` (defined in `BaseNetwork.h`)
- `P25_P2_FRAME_LENGTH_BYTES = 40U` (defined in `P25Defines.h`)
- `MSG_HDR_SIZE = 24U`
- `PACKET_PAD = 8U`
- Total transmitted application-message size: 66 + 8 = 74 bytes

**Header Semantics:**

`createP25P2_Message()` starts from the normal P25 message header, then overlays Phase 2-specific metadata:

- The common P25 header is created with `DUID::PDU` and `FrameType::DATA_UNIT`.
- `buffer[14]` carries the DVM control byte.
- `buffer[19]` carries Phase 2 signaling:
    - Bit 7 is the slot marker inspected by the receive path.
    - The low bits carry the `p25::defines::P2_DUID` value.
- `buffer[23]` contains the encapsulated data count, which is `0x40` (64 bytes: 24-byte header + 40-byte Phase 2 payload).

**Receive-Side Behavior in `Network.cpp`:**

- `Network::clock()` routes `PROTOCOL_SUBFUNC_P25_P2` into `m_rxP25P2Data`, separate from classic `m_rxP25Data`.
- The receive path derives the slot from bit 7 of byte 19 and maintains one active receive stream per slot in `m_rxP25P2StreamId[2]`.
- Promiscuous peers validate multiplexed RTP sequencing per stream using `RTPStreamMultiplex::verifyStream()`.
- Non-promiscuous peers lock a slot to the first active Phase 2 stream until an end-of-call RTP sequence is observed.
- Oversized frames are logged when they exceed `P25_P2_PACKET_LENGTH + PACKET_PAD`.

**Raw P25 Phase 2 Packet Example (layout):**

```
[RTP: 12 bytes] [FNE: 20 bytes] [P25 Phase 2 Application Message: 74 bytes]

P25 Phase 2 payload breakdown:
"P25D" | common P25 header fields | controlByte @ 14 | slot/DUID @ 19 | count=0x40 @ 23 | 40 bytes raw P25 Phase 2 payload
```

**Implementation Note:**

P25 Phase 2 is a separate transport lane in DVM documentation because it has its own subfunction code, receive buffer, reset path (`resetP25P2(slotNo)`), and per-slot stream tracking even though it reuses the generic P25 message header builder.

#### NXDN Message Format

NXDN frames use a fixed-length network format created by `createNXDN_Message()`
in `BaseNetwork.cpp`. The logical packet size is 74 bytes as defined by
`NXDN_PACKET_LENGTH = 74U`.

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
| 15 | 1 | Group/Call Type Bits | Bit 6 (`0x40`) indicates private call when set; clear indicates group call |
| 16-22 | 7 | Reserved | Reserved for future use |
| 23 | 1 | Count | Total NXDN data length |
| 24-71 | 48 | NXDN Frame Data | Raw NXDN frame (`NXDN_FRAME_LENGTH_BYTES = 48U`) |

**Logical NXDN Message Size:** 74 bytes (`NXDN_PACKET_LENGTH`). This includes a
24-byte header, 48-byte CAI frame, and two-byte trailer. The writer transmits an
additional eight padding bytes, for an FNE message length of 82 bytes.

**Constants:**
- `NXDN_PACKET_LENGTH = 74U` (defined in `BaseNetwork.h`: 24 byte header + 48 byte frame + 2 byte trailer)
- `NXDN_FRAME_LENGTH_BYTES = 48U` (defined in `NXDNDefines.h`)
- `PACKET_PAD = 8U`
- Total transmitted application-message size: 74 + 8 = 82 bytes

**Raw NXDN Packet Example (hexadecimal):**

```
[RTP Header: 12 bytes] [FNE Header: 20 bytes] [NXDN Application Message: 82 bytes]

Complete Packet (114 bytes):
90 56 SS SS TT TT TT TT XX XX XX XX | 00 FE 00 04 CC CC 00 02 II II II II PP PP PP PP 00 00 00 52 | 4E 58 44 44 [NXDN header, CAI frame, BER/RSSI, and padding]
├─────────── RTP (12) ────────────┤   ├──────────────────────── FNE (20) ───────────────────────┤   ├──────────────────────────── NXDN + trailer + padding (82) ────────────────────────────┤

NXDN Payload breakdown:
4E 58 44 44 02 00 10 00 00 20 00 00 00 00 00 01 00 00 00 00 00 00 00 30 [48 bytes NXDN frame data...]
│        │  │  │     │  │     │  │     │  │  │  │                  │ └──── NXDN Count: 0x30 (48)
│        │  │  │     │  │     │  │     │  │  │  └──────────────────┴────── Reserved
│        │  │  │     │  │     │  │     │  │  └──────────────────────────── Group: 0x01 (group call)
│        │  │  │     │  │     │  │     │  └─────────────────────────────── Control: 0x00
│        │  │  │     │  │     │  └─────┴────────────────────────────────── Reserved
│        │  │  │     │  └─────┴─────────────────────────────────────────── Dest ID: 0x002000 (8192)
│        │  │  └─────┴──────────────────────────────────────────────────── Src ID: 0x001000 (4096)
│        │  └───────────────────────────────────────────────────────────── Msg Type: 0x02 (RTCH)
└────────┴──────────────────────────────────────────────────────────────── Tag: "NXDD" (0x4E584444)
```

**NXDN Message Types:**
- **RTCH (Radio Traffic Channel):** Voice/data channel frames (message type varies based on content)
- **RCCH (Radio Control Channel):** Control signaling frames

The actual NXDN frame data (48 bytes) contains the over-the-air NXDN frame structure with FSW (Frame Sync Word), LICH (Link Information Channel), SACCH (Slow Associated Control Channel), FACCH (Fast Associated Control Channel), or voice data depending on the frame type.

#### Analog Message Format

Analog audio frames are created by `createAnalog_Message()` in `BaseNetwork.cpp`. The current network frame size is 344 bytes as defined by `ANALOG_PACKET_LENGTH = 344U`.

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

The analog audio packet uses the `TAG_ANALOG_DATA` identifier (`"ANOD"`) and contains 320 bytes of audio payload (`AUDIO_SAMPLES_LENGTH_BYTES = 320U`). The inline comments in current analog code describe this as 20 ms of audio at 8 kHz, while `analog::data::NetData` still notes that the audio buffer is expected to be MuLaw encoded. The network builder itself follows the 320-byte payload constant.

| Offset | Length | Field | Description |
|--------|--------|-------|-------------|
| 0-3 | 4 | Analog Tag | "ANOD" (`TAG_ANALOG_DATA`) |
| 4 | 1 | Sequence Number | Packet sequence number for audio ordering (0-255) |
| 5-7 | 3 | Source ID | Radio ID of transmitting station |
| 8-10 | 3 | Destination ID | Target radio ID or conference ID |
| 11-13 | 3 | Reserved | Reserved for future use (0x000000) |
| 14 | 1 | Control Byte | Network control flags |
| 15 | 1 | Frame Type / Group | Bit 6 (`0x40`) indicates private call when set; clear indicates group call.<br/>Remaining bits carry `AudioFrameType` |
| 16-19 | 4 | Reserved | Reserved for future use (0x00000000) |
| 20-339 | 320 | Audio Data | Analog network audio payload (`AUDIO_SAMPLES_LENGTH_BYTES`) |
| 340-343 | 4 | Trailer | Reserved trailer bytes |

**Logical Analog Message Size:** 344 bytes (`ANALOG_PACKET_LENGTH`); the FNE
message length is 352 bytes after `PACKET_PAD`.

**Constants:**
- `ANALOG_PACKET_LENGTH = 344U` (defined in `BaseNetwork.h`: 20 byte header + `AUDIO_SAMPLES_LENGTH_BYTES` + 4 byte trailer)
- `AUDIO_SAMPLES_LENGTH_BYTES = 320U`
- `PACKET_PAD = 8U`
- Total transmitted application-message size: 344 + 8 = 352 bytes

**Audio Encoding Details:**
- **Payload Length:** 320 bytes
- **Sample Rate:** 8 kHz (8000 samples per second)
- **Implementation Note:** `AnalogDefines.h` describes this as 20 ms of 16-bit audio at 8 kHz; `NetData.h` describes the buffer passed into `setAudio()` as MuLaw encoded. The network packetizer currently uses the 320-byte constant and copies that payload verbatim.

**Raw Analog Packet Example (layout):**

```
[RTP Header: 12 bytes] [FNE Header: 20 bytes] [Analog Application Message: 352 bytes]

Complete Packet (384 bytes):
[RTP 12] [FNE 20] ["ANOD" + header bytes + 320 bytes audio + 4 byte trailer + 8 padding bytes]

Analog Payload breakdown:
"ANOD" | Seq | SrcID | DstID | Reserved | Control | FrameType/Group | Reserved | 320 bytes audio | 4 byte trailer

G.711 μ-law Audio Sample Example (first 16 bytes of audio data shown):
FF FE FD FC FB FA F9 F8 F7 F6 F5 F4 F3 F2 F1 F0 ...
│  │  │  │  │  │  │  │  │  │  │  │  │  │  │  │
└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴─── Each byte is one 8kHz audio sample (μ-law encoded)
```

The current analog network payload is sized for 320 bytes per frame, with the packetizer copying the application-provided buffer directly into the on-wire message.

G.711 μ-law details, if used by the source audio path, remain codec-layer behavior above the network framing described here.

### Ring Buffer Architecture

Each protocol uses a fixed-size ring buffer for received data:

```cpp
RingBuffer m_rxDMRData(NET_RING_BUF_SIZE, "DMR Net Buffer");     // 4098 bytes
RingBuffer m_rxP25Data(NET_RING_BUF_SIZE, "P25 Net Buffer");     // 4098 bytes
RingBuffer m_rxP25P2Data(NET_RING_BUF_SIZE, "P25 Phase 2 Net Buffer"); // 4098 bytes
RingBuffer m_rxNXDNData(NET_RING_BUF_SIZE, "NXDN Net Buffer");   // 4098 bytes
RingBuffer m_rxAnalogData(NET_RING_BUF_SIZE, "Analog Net Buffer"); // 4098 bytes
```

**Operations:**

- `addData(data, length)`: Add received data to buffer
- `dataSize()`: Query current buffer occupancy
- `getData()`: Read data from buffer
- `clear()`: Empty buffer

**Buffer Sizing:**

4098-byte buffers provide protocol-specific buffering based on on-wire packet sizes (plus `PACKET_PAD`):
- DMR (`55 + 8 = 63` bytes): ~65 frames
- P25 LDU1 (`193 + 8 = 201` bytes): ~20 frames
- P25 LDU2 (`181 + 8 = 189` bytes): ~21 frames
- P25 Phase 2 (`66 + 8 = 74` bytes): ~55 frames
- NXDN (`74 + 8 = 82` bytes): ~49 frames
- Analog (`344 + 8 = 352` bytes): ~11 frames

### Packet Fragmentation

`PacketBuffer` transports logical objects that are too large for one ordinary
DVM application message. Current users include replicated lookup files, active
peer and patch-status JSON, radio aliases, FNE topology, talkgroup RID admission
data, and encrypted key-container inventory/update data.

Packet buffering is an application layer above RTP/FNE. Each fragment is a
complete 544-byte DVM application message and receives its own RTP header, FNE
header, CRC-16, function/subfunction, peer ID, and common transfer stream ID:

```text
Logical object
    |
    +-- optional zlib compression
    |
    +-- PacketBuffer fragment 0 (544 bytes) -- RTP/FNE datagram 0
    +-- PacketBuffer fragment 1 (544 bytes) -- RTP/FNE datagram 1
    +-- ...
    +-- PacketBuffer fragment N (544 bytes) -- RTP/FNE datagram N

Unencrypted fragment datagram:
+----------------+----------------------+---------------------------+
| RTP header     | RTP extension/FNE    | PacketBuffer fragment     |
| 12 bytes       | 20 bytes             | 544 bytes                 |
+----------------+----------------------+---------------------------+
                         total UDP payload: 576 bytes
```

The function/subfunction identifies what the reassembled object means. The
PacketBuffer header only describes fragmentation and compression; it does not
contain an application type or transfer ID.

#### PacketBuffer constants and limits

| Constant | Value | Meaning |
|---|---:|---|
| `FRAG_SIZE` | 544 bytes | Fixed transmitted fragment size |
| `FRAG_HDR_SIZE` | 10 bytes | Legacy fragment header |
| `FRAG_BLOCK_SIZE` | 534 bytes | Legacy payload capacity per fragment |
| `EXTENDED_FRAG_HDR_SIZE` | 12 bytes | Extended fragment header |
| `EXTENDED_FRAG_BLOCK_SIZE` | 532 bytes | Extended payload capacity per fragment |
| `MAX_FRAGMENT_SIZE` | 8 MiB | Maximum compressed and uncompressed logical size |
| `MAX_BLOCK_CNT` | 256 | Maximum legacy block count |
| `MAX_EX_BLOCK_CNT` | 65,535 | Maximum extended block count |

The encoder selects the legacy format when compressed length is at most
`256 * 534 = 136,704` bytes. A larger compressed object uses the extended
format. Selection is implicit: the decoder makes the same decision from the
encoded compressed-length field. There is no separate version bit.

#### Legacy fragment format

The legacy format uses 8-bit block fields. Only block zero carries nonzero size
metadata; bytes 0-7 are zero in subsequent blocks.

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                 Uncompressed Length (block 0)                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  Compressed Length (block 0)                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Block Number  |  Last Block   |                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
|                                                               |
|                    Fragment Payload (534 bytes)                |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                         544 bytes total
```

`Last Block` is the zero-based final block number, not the number of blocks.
Therefore `blockCount = lastBlock + 1`. For example, a three-fragment transfer
uses block numbers 0, 1, and 2 and encodes `Last Block = 2`.

#### Extended fragment format

The extended format repeats both size fields in every fragment. This allows any
extended block to establish and validate the transfer metadata before block zero
arrives. Its block fields are 16-bit big-endian values.

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Uncompressed Length                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Compressed Length                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Block Number          |          Last Block           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                    Fragment Payload (532 bytes)                |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                         544 bytes total
```

The final fragment is zero-padded to 544 bytes on the wire. `Fragment::blockSize`
records the meaningful bytes generated for that block, but senders transmit
`FRAG_SIZE`; reassembly copies only the bytes remaining in `compressedLength`.

#### Encode operation

`PacketBuffer::encode(data, length)` performs the following operation:

```text
Input object and uncompressed length
              |
              v
       clear old fragment state
              |
              v
   compression enabled for this PacketBuffer?
       | yes                         | no
       v                             v
 zlib-compress                  copy input bytes
       +-------------+---------------+
                     v
 validate nonzero compressed length and both lengths <= 8 MiB
                     |
                     v
 compressed length > 136,704 bytes?
       | yes                         | no
       v                             v
 extended header/532 B          legacy header/534 B
       +-------------+---------------+
                     v
 blockCount = ceil(compressedLength / blockPayloadSize)
                     |
                     v
 allocate and zero one 544-byte Fragment for each block
                     |
                     v
 encode sizes, block number, last-block value, and payload slice
                     |
                     v
 store Fragment objects in `fragments`, indexed by block ID
```

`encode()` returns `true` after building the in-memory fragment map. It does not
send packets. The caller iterates `pkt.fragments`, sends each fragment with the
chosen function/subfunction and one shared stream ID, commonly sleeps 60 ms
between blocks to pace the transfer, and finally calls `clear()`.

All current FNE PacketBuffer call sites construct buffers with compression
enabled. The class also supports uncompressed operation; in that mode the
compressed-length field equals the uncompressed length and decode skips zlib.

#### Decode and reassembly operation

`decode(fragment, &message, &outLength)` returns `true` only when that call
completes the logical object. `false` means either that more blocks are required
or that the fragment was rejected; callers use logging and buffer state to
distinguish those cases.

```text
Receive one 544-byte fragment
              |
              v
read uncompressed/compressed lengths
              |
              v
select legacy or extended format from compressed length
              |
              v
decode block number and last block; calculate blockCount
              |
              v
validate block range, count, sizes, and format consistency
              |
              +-- invalid/duplicate --> log; reject (some errors clear assembly)
              |
              v
copy fixed payload region into a newly owned Fragment
              |
              v
insert by block number (arrival order need not be sequential)
              |
              v
fragments.size() == blockCount?
       | no                          | yes
       v                             v
 return false                 require block zero and size metadata
                                     |
                                     v
                           revalidate expected block count
                                     |
                                     v
                           copy blocks 0..N into compressed buffer
                                     |
                                     v
                           compression enabled?
                             | yes             | no
                             v                 v
                         decompress       require equal lengths
                             +--------+--------+
                                      v
                           verify final uncompressed length
                                      |
                                      v
                         allocate output with `new[]`, clear assembly,
                         set outLength, and return true
```

Validation includes:

- current block must be less than the declared block count;
- legacy count cannot exceed 256 and extended count cannot exceed 65,535;
- compressed and uncompressed sizes must be nonzero and no greater than 8 MiB;
- calculated block count must match `ceil(compressedLength / blockSize)`;
- legacy nonzero blocks must not claim size metadata;
- fragments in one assembly must agree on format and block count;
- extended fragments must also agree on both size fields;
- duplicate block numbers are ignored; and
- every block from zero through the declared final block must exist before copy.

The returned `message` is allocated with `new[]`; the successful caller owns it
and must release it with `delete[]`. `clear()` deletes every retained fragment
payload and fragment object and empties the map. Destruction calls `clear()`.

#### Transfer identity and concurrent receive state

PacketBuffer itself has no stream field. FNE metadata handlers associate an
assembly with both the authenticated peer and the RTP/FNE stream ID:

```text
PacketBufferMap
  key: peerId
  value: PacketBufferEntry
           +-- mutex
           +-- streamId
           +-- unique_ptr<PacketBuffer>
```

`MetadataNetwork` maintains separate maps for key updates, active-peer
replication, patch-status replication, and topology lists. A handler obtains the
entry for the peer, locks its mutex, and compares the incoming stream ID. A
stream mismatch clears or rejects the old assembly according to that operation.
The entry is erased only if the map still refers to the same shared entry, so a
stalled worker cannot erase a replacement transfer.

Peer-side `Network` and FNE `PeerNetwork` receivers instead keep a distinct
long-lived PacketBuffer member for each replicated dataset. The surrounding
packet handler resets that member on stream changes and clears it after success
or fatal validation/application errors.

#### Functions and subfunctions using PacketBuffer

The following table lists current PacketBuffer wire users. `NOP` means the
primary function alone identifies the operation.

| Function | Subfunction | Direction | Reassembled content and consumer |
|---|---|---|---|
| `MASTER` | `MASTER_SUBFUNC_TG_ADMIT` (`0x04`) | FNE -> peer | Binary talkgroup-to-admitted-RID dataset; decoded by the common peer `Master` handler |
| `RADIO_ALIAS_SYNC` | `NOP` | FNE -> requesting peer | Radio alias lookup data; decoded by the common peer radio-alias handler |
| `KEYS_INVENTORY` | `NOP` | FNE -> remote inventory requester | Serialized encrypted-key-container inventory response |
| `KEYS_UPDATE` | `NOP` | Remote requester -> FNE metadata listener | Authenticated encrypted key-container update data phase |
| `REPL` | `REPL_TALKGROUP_LIST` (`0x00`) | FNE -> neighbor `PeerNetwork` | Talkgroup-rules file; installed/reloaded by the receiving FNE peer link |
| `REPL` | `REPL_RID_LIST` (`0x01`) | FNE -> neighbor `PeerNetwork` | RID ACL/lookup file |
| `REPL` | `REPL_PEER_LIST` (`0x02`) | FNE -> neighbor `PeerNetwork` | Peer-list file |
| `REPL` | `REPL_RID_ALIAS_LIST` (`0x03`) | FNE -> neighbor `PeerNetwork` | Radio-alias file |
| `REPL` | `REPL_ACT_PEER_LIST` (`0xA2`) | Child/neighbor `PeerNetwork` -> FNE metadata listener | `REPL`-tagged JSON array of active peers |
| `REPL` | `REPL_PATCH_STATUS` (`0xA4`) | FNE <-> neighbor FNE | `REPL`-tagged JSON patch-status object |
| `NET_TREE` | `NET_TREE_LIST` (`0x00`) | Child/neighbor `PeerNetwork` -> FNE metadata listener | `REPL`-tagged serialized spanning-tree JSON array |

The ordinary RID, talkgroup, peer, and alias replication subfunctions are
decoded in `PeerNetwork` with dataset-specific PacketBuffer members. Active-peer,
patch-status, and tree-list uploads are decoded in `MetadataNetwork` with
per-peer `PacketBufferEntry` maps because worker tasks can process fragments from
multiple neighbor FNEs concurrently.

#### MASTER / TG admission transfer

```text
TrafficNetwork::writePeerTGIDAdmission
  build binary payload:
    repeated talkgroup records and admitted RID values
  -> PacketBuffer(compression=true)
  -> MASTER / MASTER_SUBFUNC_TG_ADMIT fragments
  -> peer Network::PacketHandler::master
  -> m_tgAdmissionPkt.decode()
  -> replace in-memory talkgroup RID admission map
```

All fragments share the generated stream ID. The peer tracks the admission
stream and clears partial state when a different stream supersedes it.

#### REPL lookup-list transfers

```text
FNE TrafficNetwork                        Neighbor FNE PeerNetwork
      |                                             |
      | serialize lookup file/data                  |
      | compress + fragment                         |
      |                                             |
      | REPL/TALKGROUP_LIST fragments ------------> | m_tgidPkt
      | REPL/RID_LIST fragments ------------------> | m_ridPkt
      | REPL/PEER_LIST fragments -----------------> | m_pidPkt
      | REPL/RID_ALIAS_LIST fragments ------------> | m_ridAliasPkt
      |                                             |
      |                                  decode complete object
      |                                  write temporary/target file
      |                                  reload corresponding lookup
```

The receiving `PeerNetwork` can use a randomized temporary filename when it is
not configured to persist the replicated ACL. After a successful reload it marks
the neighbor link replica-capable and invokes the replica callback when present.

#### Active-peer replication

```text
Child FNE PeerNetwork::writePeerLinkPeers
  JSON array -> prepend `REPL` application tag/header
  -> PacketBuffer -> REPL/REPL_ACT_PEER_LIST
  -> upstream MetadataNetwork
  -> validate connected neighbor + replica permission + source address
  -> per-peer/stream reassembly
  -> parse JSON array
  -> update replicated active-peer state
```

#### Patch-status replication

Patch status is bidirectional between eligible FNE neighbors. The sender
serializes a JSON object, prepends the `REPL` tag/header, fragments it, and sends
`REPL_PATCH_STATUS`. The receiver requires at least the eight-byte replication
prefix after reassembly, parses the remaining bytes as a JSON object, and passes
it to the local patch-status processor or callback.

#### Network-tree transfer

```text
PeerNetwork::writeSpanningTree
  SpanningTree::serializeTree(root) -> JSON array
  -> prepend `REPL` tag/header
  -> PacketBuffer
  -> NET_TREE / NET_TREE_LIST fragments on metadata path
  -> MetadataNetwork validates neighbor identity/address/class
  -> per-peer and stream-locked reassembly
  -> parse JSON array
  -> SpanningTree::deserializeTree under tree lock
  -> send NET_TREE_DISC for discovered duplicate peer paths
```

`NET_TREE_DISC` itself is a small control message and does not use PacketBuffer.

#### Radio-alias synchronization

The FNE answers a radio-alias synchronization request by encoding the alias data
with PacketBuffer and returning `RADIO_ALIAS_SYNC/NOP` fragments on one stream.
The peer's `m_radioAliasSyncPkt` reassembles the object, checks that lookup update
is enabled and an alias lookup exists, then installs/reloads the data. The
request/control frame itself is not a PacketBuffer object.

#### Key inventory and key update

Key inventory is a request/fragmented-response operation. The FNE serializes the
inventory, compresses and fragments it, and directly responds to the requester's
address with `KEYS_INVENTORY/NOP`; this special path can serve an authorized
remote requester that is not logged in as a standard peer.

Key update is a two-phase inbound operation:

```text
Remote requester                              FNE MetadataNetwork
       |                                                |
       | KEYS_UPDATE 80-byte authentication frame       |
       |-----------------------------------------------> |
       |                    verify remote salt/hash      |
       |                    reserve peer/stream entry    |
       |                                                |
       | KEYS_UPDATE PacketBuffer fragment 0            |
       |-----------------------------------------------> |
       | KEYS_UPDATE PacketBuffer fragment 1..N         |
       |-----------------------------------------------> |
       |                    reassemble/decompress        |
       |                    write temporary container    |
       |                    atomically rename/install    |
       |                    reload crypto lookup         |
```

The 80-byte authentication frame is not a PacketBuffer fragment. Data-phase
frames must be at least `FRAG_SIZE`, must use the authenticated stream ID, and
are rejected if no reserved update entry exists. A concurrent update for the
same peer receives `KEYS_UPDATE/KEYS_UPDATE_STATUS` with busy status. The
current implementation does not send a success-status frame after installation.
Installation uses a peer/stream-specific temporary file and atomic rename before
the crypto lookup is reloaded.

#### Verification coverage

`tests/network/PacketBuffer_Tests.cpp` exercises the wire and state-machine
properties described above. Its focused `[network][packetbuffer]` cases cover:

- uncompressed, compressed, and out-of-order reassembly;
- duplicate-fragment handling and rejection of fragments beyond the declared
  block count;
- rejection of oversized metadata and invalid zlib data;
- legacy encoding through the 256-block limit and transition to extended
  encoding above that threshold; and
- extended-format reassembly when block zero arrives last.

These tests validate PacketBuffer itself. Application handlers separately remain
responsible for authenticating the peer, binding fragments to the expected
stream, validating the reassembled file or JSON schema, applying it atomically
where required, and clearing operation-specific state on failure.

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

On an FNE, accepted DMR, P25 Phase 1, P25 Phase 2, and NXDN traffic is dispatched through the shared inbound worker pool with a per-stream ordering key. Frames for one peer/SSRC, stream, direction, and protocol lane execute FIFO, while unrelated calls can execute concurrently. Dispatch does not wait for a missing sequence number or call header; RTP validation and the protocol late-entry rules continue to handle those cases.

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

Configured call-state and hang timers prevent stale call records; the effective
timeout is owned by the current FNE configuration and call handler.

---

## 9. Security

### Authentication

SHA256-based digest authentication:

**Process:**

1. Peer sends `RPTL` login message (`TAG_REPEATER_LOGIN` + peer ID)
2. Master generates random 32-bit salt for that login exchange
3. Master returns the salt in the `ACK` payload for `RPTL`
4. Peer computes: `SHA256(salt || password)`
5. Peer sends digest in `RPTK` (`TAG_REPEATER_AUTH` + peer ID + digest)
6. Master computes the same digest and validates

**Implementation:**

```cpp
size_t size = m_password.size();
uint8_t* in = new uint8_t[size + sizeof(uint32_t)];
::memcpy(in, m_salt, sizeof(uint32_t));
for (size_t i = 0U; i < size; i++)
    in[i + sizeof(uint32_t)] = m_password.at(i);

edac::SHA256 sha256;
sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out + 8U);
```

**Security Properties:**

- Reduces replay risk by using per-login random salt
- Password never transmitted in cleartext
- A captured salt and digest can be used to test password guesses offline; use a strong, unique password

### Encryption

Optional AES-256-CBC encryption is implemented as UDP payload wrapping at the socket layer:

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

The implementation uses AES-256-CBC (Cipher Block Chaining mode):
- **Algorithm**: AES-256-CBC
- **Key Size**: 256 bits (32 bytes)
- **Block Size**: 128 bits (16 bytes)
- **IV**: A fresh 16-byte initialization vector is generated for each datagram and appended after the ciphertext
- **Padding**: Zero-padding to the AES block boundary
- **Wire Layout**: Two-byte `0xC0FE` leader, block-aligned ciphertext, then the 16-byte IV

**Runtime Behavior:** When preshared-key wrapping is enabled, receive logic drops datagrams that do not carry the `0xC0FE` leader. This is transport wrapping around the existing RTP/FNE payload, not SRTP.

This wrapper provides transport confidentiality but does not authenticate the ciphertext. Deployments requiring authenticated transport should use an additional protected network layer such as a VPN or IPsec.

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
- Shared-password challenge-response authentication for the UDP peer protocol; REST HTTPS certificates are a separate facility

**Configuration and Policy Controls:**

- RID and talkgroup lookup policy controls whether unknown or unauthorized sources are admitted.
- Talkgroup rules control affiliation requirements and peer inclusion/exclusion.
- Unit-to-unit routing is controlled by the current FNE call handlers, talkgroup/RID policy, unit-route registry, and per-peer permissions.
- Per-peer `allowUnitToUnit` policy can further restrict private-call ingress and egress.

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

Receiver stream state verifies the active stream ID and RTP sequence behavior.
When enabled, `AdaptiveJitterBuffer` separately records total, reordered,
dropped, and timed-out frames.

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

### Connection Retry Logic

Failed connections are retried on the configured retry timer. The standard
retry timeout is fixed; duplicate-connection handling temporarily selects the
longer duplicate retry timeout rather than applying exponential backoff:

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

The 4098-byte protocol ring buffers are receive queues. RTP reordering is
implemented separately by `AdaptiveJitterBuffer` on enabled FNE peer paths:

- in-order frames use a fast delivery path;
- out-of-order frames can be retained and released in sequence;
- missing-frame gaps are released after a configured wait; and
- total, reordered, dropped, and timed-out frame counters are maintained.

The supported depth range is 2-8 frames and wait range is 10,000-200,000
microseconds. Defaults are four frames and 40,000 microseconds.

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

1. **Bounded Copies**: Keep framing and routing copies limited to validated message lengths
2. **Thread Pool**: Asynchronous packet processing
3. **UDP Socket Tuning**: Platform socket buffers should be tuned from observed drops and queue behavior
4. **Stream Separation**: Keep concurrent stream state isolated per peer
5. **Direct Routing**: Minimize master-side processing

**Latency:**

No universal end-to-end latency is asserted. Network path, worker load,
encryption, protocol cadence, jitter-buffer wait, and downstream RF/audio
processing all contribute and should be measured in the target deployment.

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

**Illustrative Status Fields:**

The exact JSON object is produced by the calling application. The following is
an example shape, not a mandatory FNE wire schema:

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

### Metadata Network Port

Current FNE runtime uses a separate `MetadataNetwork` listener for activity log transfer, diagnostic transfer, status transfer, peer announcement and packet-buffered replication metadata:

```cpp
class MetadataNetwork : public BaseNetwork {
private:
    TrafficNetwork* m_trafficNetwork;
    uint16_t m_port;              // Bound to master port + 1
    ThreadPool m_threadPool;

public:
    void processNetwork();        // Process metadata / transfer packets
};
```

`HostFNE::createMasterNetwork()` creates `TrafficNetwork` on the configured master port and `MetadataNetwork` on `master.port + 1`.

**Benefits:**

- Isolate diagnostic traffic from operational traffic
- Different QoS/priority handling
- Optional firewall rules
- Reduced congestion on main traffic port

### Network Statistics

The FNE records subsystem-specific call, grant, activity, and packet-data events
through its metrics logging implementation. Adaptive jitter buffers expose
total, reordered, dropped, and timed-out frame counts. UDP sockets maintain
their implemented counters, and runtime maps expose connected-peer/call state to
the owning services. This does not imply that packet-loss percentage, average
latency, aggregate bandwidth, and every queue depth are universally calculated.

**Monitoring Tools:**

- Real-time web dashboard
- REST API for metrics
- Optional InfluxDB integration in `src/fne/network/influxdb`
- Additional external monitoring may consume logs or REST data, but Prometheus and SNMP exporters are not implemented by this stack

---

## 12. Performance Considerations

### Scalability

**FNE Master Scalability:**

The configured connection limit has a hard maximum of 250 peers. This is a
configuration bound, not a published benchmark result. Supported call count,
throughput, CPU load, and memory depend on hardware, enabled protocols, traffic
mix, logging, encryption, and topology. Numerical capacity claims require a
reproducible benchmark and build revision.

**Scaling Techniques:**

1. **Thread Pool**: Asynchronous packet processing
2. **Concurrent Collections**: Used for selected shared maps and runtime state
3. **Per-Peer State**: Separates sessions and stream multiplexing
4. **Metadata Listener**: Moves eligible auxiliary transfers off the traffic port
5. **Efficient Lookups**: Hash maps are used for peer and stream lookup

### Network Bandwidth

**Per-Call Bandwidth (Approximate):**

| Protocol | Codec | Bandwidth |
|----------|-------|-----------|
| DMR | AMBE+2 | ~7 kbps |
| P25 | IMBE | ~9 kbps |
| NXDN | AMBE+2 | ~7 kbps |
| Analog | Application-selected 8-bit audio | payload rate depends on packet cadence; 320 audio bytes per network message |

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

No reproducible profiling results are currently included with this note. RTP
encoding, CRC calculation, allocation/copying, worker dispatch, policy lookup,
and protocol routing should be profiled together under a representative workload.

### Memory Management

**Memory Allocation:**

- **Static Buffers**: Ring buffers allocated at initialization
- **Packet Buffers**: Fragmentation/reassembly objects retain fragment state for large logical transfers
- **Smart Pointers**: Automatic cleanup (UInt8Array)
- **Stack Allocation**: Prefer stack for small, temporary buffers

**Memory Footprint:**

No fixed memory-footprint figures are asserted. Object size alone does not
include ring-buffer storage, maps, queued datagrams, fragment buffers, lookup
data, call handlers, worker state, encryption buffers, or allocator overhead.

### Configuration Tuning

**UDP Socket Buffers:**

Socket buffer behavior is platform-dependent. Tune operating-system buffers
from observed drops and queue behavior rather than assuming a universal 512KB
project default.

**Thread Pool Sizing:**

```cpp
ThreadPool m_threadPool(workerCnt, "fne");
// Worker count is configured and clamped by the FNE loader.
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
    | P25 PROTOCOL (LDU1)       |                           |
    | Logical Data Unit 1       |                           |
    | [Voice + LC + LSD + ESS]  |                           |
    |-------------------------->|-------------------------->|
    |                           | (Route based on TG)       |
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
    | AlgID=0x84 (AES-256)   |                           |
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
- `0x81`: DES-OFB
- `0x84`: AES-256
- `0xAA`: ARC4
- Other values: Vendor-specific, implementation-specific, or reserved

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
- Network transport uses the optional AES-256-CBC UDP wrapper described in Section 9
- Keys are transmitted encrypted over the already-secured network connection
- Only authorized peers (verified during RPTK) can request keys
- Key material format is algorithm-specific (AES keys, DES keys, etc.)

### Network Management Examples

#### Example 6: Peer Login Sequence

```
Peer                        FNE Master
  |                             |
  | RPTL (login request)        |
  |---------------------------->|
  |                             | (Generate login salt)
  |                             |
  |                Salt + ACK   |
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
    id: 100000
    address: master.example.com
    port: 62031
    password: "SecurePassword123"

    encrypted: false
    presharedKey: "000102030405060708090A0B0C0D0E0F000102030405060708090A0B0C0D0E0F"
    kmfPresharedKey: "000102030405060708090A0B0C0D0E0F000102030405060708090A0B0C0D0E0F"

    jitter: 360
    disallowUnitToUnit: false
    slot1: true
    slot2: true

    updateLookups: false
    saveLookups: false
    allowActivityTransfer: true
    allowDiagnosticTransfer: true
    allowStatusTransfer: true

    packetDump: false
    debug: false
```

Protocol enablement and RF/site metadata are configured in their maintained
sections outside `network`; HA endpoints are learned through the FNE HA exchange,
not configured through `haEnabled` or `haAddresses` keys in this host section.

### Master Configuration (YAML)

```yaml
master:
    peerId: 9000100
    address: 0.0.0.0
    port: 62031           # Traffic port; metadata listener uses port + 1

    # Authentication
    password: "SecurePassword123"

    # Scaling
    workers: 16
    connectionLimit: 100

    # Transport security
    encrypted: false
    presharedKey: "000102030405060708090A0B0C0D0E0F000102030405060708090A0B0C0D0E0F"
  
    # Protocol traffic permissions
    allowDMRTraffic: true
    allowP25Traffic: true
    allowP25P2Traffic: false
    allowNXDNTraffic: true
    allowAnalogTraffic: false

    # High availability advertisements
    ha:
        enable: false
        advertisedWANAddress: 1.2.3.4
        advertisedWANPort: 62031

    # Adaptive jitter defaults
    jitterBuffer:
        enable: false
        defaultMaxSize: 4
        defaultMaxWait: 40000

    # Logging
    reportPeerPing: false
    logDenials: true
    logUpstreamCallStartEnd: true
```

These are illustrative excerpts, not complete configurations. Use
`configs/config.example.yml`, `configs/fne-config.example.yml`, their schemas,
and `tools/dvmcfggen` for complete current configuration.

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
- **Check**: RPTL/RPTK exchange timeout (salt ACK not received)
- **Solution**: Verify SHA256 hash calculation

### Debug Techniques

**Enable Packet Dumps:**

```cpp
m_debug = true;  // In Network, TrafficNetwork, or MetadataNetwork constructor/config
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
grep "NAK" /var/log/dvm/dvmhost.log

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
| 1.1 | May 8, 2026 | Update to match current state of the codebase |
| 1.2 | Jul 8, 2026 | Corrected connection/auth flow, opcode tables, RPTC schema, ping payload, bitfields, and buffer/stream details to match current implementation |
| 1.3 | Nov 22, 2026 | Updated UDP encryption to AES-256-CBC and replaced removed private-call policy flags |
| 1.4 | Nov 22, 2026 | Source audit corrected RTP/FNE framing, packet lengths/examples, opcode tables, security statements, configuration excerpts, and unsupported performance claims while preserving the original document structure. |
| 1.5 | Nov 22, 2026 | Added detailed PacketBuffer encode/decode, legacy and extended fragment formats, validation, concurrency, ownership, and function/subfunction usage documentation with transfer diagrams. |

---

**End of Document**
