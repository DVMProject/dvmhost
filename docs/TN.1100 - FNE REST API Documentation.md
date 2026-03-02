# DVM FNE REST API Technical Documentation

**Version:** 1.1  
**Date:** December 6, 2025  
**Author:** AI Assistant (based on source code analysis)

AI WARNING: This document was mainly generated using AI assistance. As such, there is the possibility of some error or inconsistency. Examples in Section 14 and Appendix C are *strictly* examples only for how the API *could* be used.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Authentication](#2-authentication)
3. [Common Endpoints](#3-common-endpoints)
4. [Peer Management](#4-peer-management)
5. [Radio ID (RID) Management](#5-radio-id-rid-management)
6. [Talkgroup (TGID) Management](#6-talkgroup-tgid-management)
7. [Peer List Management](#7-peer-list-management)
8. [Adjacent Site Map Management](#8-adjacent-site-map-management)
9. [System Operations](#9-system-operations)
10. [Protocol-Specific Operations](#10-protocol-specific-operations)
11. [Response Formats](#11-response-formats)
12. [Error Handling](#12-error-handling)
13. [Security Considerations](#13-security-considerations)
14. [Examples](#14-examples)

---

## 1. Overview

The DVM (Digital Voice Modem) FNE (Fixed Network Equipment) REST API provides a comprehensive interface for managing and monitoring FNE nodes in a distributed network. The API supports HTTP and HTTPS protocols and uses JSON for request and response payloads.

### 1.1 Base Configuration

**Default Ports:**
- HTTP: User-configurable (typically 9990)
- HTTPS: User-configurable (typically 9443)

**Transport:**
- Protocol: HTTP/1.1 or HTTPS
- Content-Type: `application/json`
- Character Encoding: UTF-8

**SSL/TLS Support:**
- Optional HTTPS with certificate-based security
- Configurable via `keyFile` and `certFile` parameters
- Uses OpenSSL when `ENABLE_SSL` is defined

### 1.2 API Architecture

The REST API is built on:
- **Request Dispatcher:** Routes HTTP requests to appropriate handlers
- **HTTP/HTTPS Server:** Handles network connections
- **Authentication Layer:** Token-based authentication using SHA-256
- **Lookup Tables:** Radio ID, Talkgroup Rules, Peer List, Adjacent Site Map

---

## 2. Authentication

All API endpoints (except `/auth`) require authentication using a token-based system.

### 2.1 Authentication Flow

1. Client sends password hash to `/auth` endpoint
2. Server validates password and returns authentication token
3. Client includes token in `X-DVM-Auth-Token` header for subsequent requests
4. Tokens are bound to client IP/host and remain valid for the session

### 2.2 Endpoint: PUT /auth

**Method:** `PUT`

**Description:** Authenticate with the FNE REST API and obtain an authentication token.

**Request Headers:**
```
Content-Type: application/json
```

**Request Body:**
```json
{
  "auth": "sha256_hash_of_password_in_hex"
}
```

**Password Hash Format:**
- Algorithm: SHA-256
- Encoding: Hexadecimal string (64 characters)
- Example: `"5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8"` (hash of "password")

**Response (Success):**
```json
{
  "status": 200,
  "token": "12345678901234567890"
}
```

**Response (Failure):**
```json
{
  "status": 400,
  "message": "invalid password"
}
```

**Error Conditions:**
- `400 Bad Request`: Invalid password, malformed auth string, or invalid characters
- `401 Unauthorized`: Authentication failed

**Notes:**
- Password must be pre-hashed with SHA-256 on client side
- Token is a 64-bit unsigned integer represented as a string
- Tokens are invalidated when:
  - Client authenticates again
  - Server explicitly invalidates the token
  - Server restarts

**Example (bash with curl):**
```bash
# Generate SHA-256 hash of password
PASSWORD="your_password_here"
HASH=$(echo -n "$PASSWORD" | sha256sum | cut -d' ' -f1)

# Authenticate
TOKEN=$(curl -X PUT http://fne.example.com:9990/auth \
  -H "Content-Type: application/json" \
  -d "{\"auth\":\"$HASH\"}" | jq -r '.token')

echo "Token: $TOKEN"
```

---

## 3. Common Endpoints

### 3.1 Endpoint: GET /version

**Method:** `GET`

**Description:** Retrieve FNE software version information.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200,
  "version": "Digital Voice Modem (DVM) Converged FNE 4.0.0 (built Dec 03 2025 12:00:00)"
}
```

**Notes:**
- Returns program name, version, and build timestamp
- Useful for compatibility checks and diagnostics

---

### 3.2 Endpoint: GET /status

**Method:** `GET`

**Description:** Retrieve current FNE system status and configuration.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200,
  "state": 1,
  "dmrEnabled": true,
  "p25Enabled": true,
  "nxdnEnabled": false,
  "peerId": 10001
}
```

**Response Fields:**
- `state`: Current FNE state (1 = running)
- `dmrEnabled`: Whether DMR protocol is enabled
- `p25Enabled`: Whether P25 protocol is enabled
- `nxdnEnabled`: Whether NXDN protocol is enabled
- `peerId`: This FNE's peer ID

---

## 4. Peer Management

### 4.1 Endpoint: GET /peer/query

**Method:** `GET`

**Description:** Query all connected peers and their status.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200,
  "peers": [
    {
      "peerId": 10001,
      "address": "192.168.1.100",
      "port": 54321,
      "connected": true,
      "connectionState": 4,
      "pingsReceived": 120,
      "lastPing": 1701619200,
      "controlChannel": 0,
      "config": {
        "identity": "Site 1 Repeater",
        "software": "dvmhost 4.0.0",
        "sysView": false,
        "externalPeer": false,
        "masterPeerId": 0,
        "conventionalPeer": false
      },
      "voiceChannels": [10002, 10003]
    }
  ]
}
```

**Response Fields:**
- `peerId`: Unique peer identifier
- `address`: IP address of peer
- `port`: Network port of peer
- `connected`: Connection status (true/false)
- `connectionState`: Connection state value (0=INVALID, 1=WAITING_LOGIN, 2=WAITING_AUTH, 3=WAITING_CONFIG, 4=RUNNING)
- `pingsReceived`: Number of pings received from peer
- `lastPing`: Unix timestamp of last ping received
- `controlChannel`: Control channel peer ID (0 if this peer is a control channel, or peer ID of associated control channel)
- `config`: Peer configuration object
  - `identity`: Peer description/name
  - `software`: Peer software version string
  - `sysView`: Whether peer is a SysView monitoring peer
  - `externalPeer`: Whether peer is a downstream neighbor FNE peer
  - `masterPeerId`: Master peer ID (for neighbor FNE peers)
  - `conventionalPeer`: Whether peer is a conventional (non-trunked) peer
- `voiceChannels`: Array of voice channel peer IDs associated with this control channel (empty if not a control channel)

---

### 4.2 Endpoint: GET /peer/count

**Method:** `GET`

**Description:** Get count of connected peers.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200,
  "peerCount": 5
}
```

---

### 4.3 Endpoint: PUT /peer/reset

**Method:** `PUT`

**Description:** Reset (disconnect and reconnect) a specific peer.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "peerId": 10001
}
```

**Response (Success):**
```json
{
  "status": 200
}
```

**Response (Failure - Invalid Request):**
```json
{
  "status": 400,
  "message": "peerId was not a valid integer"
}
```

**Notes:**
- Forces peer disconnect and requires re-authentication
- Useful for recovering from stuck connections
- Peer will need to complete RPTL/RPTK/RPTC sequence again
- Returns 200 OK even if the peer ID does not exist (check server logs for actual result)

---

### 4.4 Endpoint: PUT /peer/connreset

**Method:** `PUT`

**Description:** Reset the FNE's upstream peer connection (if FNE is operating as a child node).

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "peerId": 10001
}
```

**Response (Success):**
```json
{
  "status": 200
}
```

**Response (Failure - Invalid Request):**
```json
{
  "status": 400,
  "message": "peerId was not a valid integer"
}
```

**Notes:**
- Only applicable when FNE is configured as a child node with upstream peer connections
- Disconnects from specified upstream peer and attempts reconnection
- Used for recovering from upstream connection issues
- The `peerId` must match an upstream peer connection configured in the FNE

---

## 5. Radio ID (RID) Management

The Radio ID (RID) management endpoints allow dynamic modification of the radio ID whitelist/blacklist.

### 5.1 Endpoint: GET /rid/query

**Method:** `GET`

**Description:** Query all radio IDs in the lookup table.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200,
  "rids": [
    {
      "id": 123456,
      "enabled": true,
      "alias": "Unit 1"
    },
    {
      "id": 789012,
      "enabled": false,
      "alias": "Unit 2"
    }
  ]
}
```

**Response Fields:**
- `id`: Radio ID (subscriber ID)
- `enabled`: Whether radio is enabled (whitelisted)
- `alias`: Radio alias/name

**Notes:**
- Returns all radio IDs in the lookup table (no filtering available)
- Empty `alias` field will be returned as empty string if not set

---

### 5.2 Endpoint: PUT /rid/add

**Method:** `PUT`

**Description:** Add or update a radio ID in the lookup table.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "rid": 123456,
  "enabled": true,
  "alias": "Unit 1"
}
```

**Request Fields:**
- `rid` (required): Radio ID (subscriber ID)
- `enabled` (required): Whether radio is enabled (whitelisted)
- `alias` (optional): Radio alias/name

**Response (Success):**
```json
{
  "status": 200
}
```

**Response (Failure - Invalid Request):**
```json
{
  "status": 400,
  "message": "rid was not a valid integer"
}
```
or
```json
{
  "status": 400,
  "message": "enabled was not a valid boolean"
}
```

**Notes:**
- Changes are in-memory only until `/rid/commit` is called
- If radio ID already exists, it will be updated
- `enabled: false` effectively blacklists the radio
- `alias` field is optional and defaults to empty string if not provided

---

### 5.3 Endpoint: PUT /rid/delete

**Method:** `PUT`

**Description:** Remove a radio ID from the lookup table.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "rid": 123456
}
```

**Response (Success):**
```json
{
  "status": 200
}
```

**Response (Failure - Invalid Request):**
```json
{
  "status": 400,
  "message": "rid was not a valid integer"
}
```

**Response (Failure - RID Not Found):**
```json
{
  "status": 400,
  "message": "failed to find specified RID to delete"
}
```

**Notes:**
- Changes are in-memory only until `/rid/commit` is called
- Returns error if the specified RID does not exist in the lookup table

---

### 5.4 Endpoint: GET /rid/commit

**Method:** `GET`

**Description:** Commit all radio ID changes to disk.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200
}
```

**Notes:**
- Writes current in-memory state to configured RID file
- Changes persist across FNE restarts after commit
- Recommended workflow: Add/Delete multiple RIDs, then commit once

---

## 6. Talkgroup (TGID) Management

Talkgroup management endpoints control talkgroup rules, affiliations, and routing.

### 6.1 Endpoint: GET /tg/query

**Method:** `GET`

**Description:** Query all talkgroup rules.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200,
  "tgs": [
    {
      "name": "TAC 1",
      "alias": "Tactical 1",
      "invalid": false,
      "source": {
        "tgid": 1,
        "slot": 1
      },
      "config": {
        "active": true,
        "affiliated": false,
        "parrot": false,
        "inclusion": [],
        "exclusion": [],
        "rewrite": [],
        "always": [],
        "preferred": [],
        "permittedRids": []
      }
    }
  ]
}
```

**Response Fields:**

**Top-Level Fields:**
- `name`: Talkgroup name/description
- `alias`: Short alias for talkgroup
- `invalid`: Whether talkgroup is marked invalid (disabled)

**Source Object:**
- `tgid`: Talkgroup ID number
- `slot`: TDMA slot (1 or 2 for DMR, typically 1 for P25/NXDN)

**Config Object:**
- `active`: Whether talkgroup is currently active
- `affiliated`: Requires affiliation before use
- `parrot`: Echo mode (transmit back to source)
- `inclusion`: Array of peer IDs that should receive this talkgroup
- `exclusion`: Array of peer IDs that should NOT receive this talkgroup
- `rewrite`: Array of rewrite rules (source peer → destination TGID mappings)
- `always`: Array of peer IDs that always receive this talkgroup
- `preferred`: Array of preferred peer IDs for this talkgroup
- `permittedRids`: Array of radio IDs permitted to use this talkgroup

**Rewrite Rule Format:**
```json
{
  "peerid": 10001,
  "tgid": 2,
  "slot": 1
}
```

**Notes:**
- Returns all talkgroup rules (no filtering available)

---

### 6.2 Endpoint: PUT /tg/add

**Method:** `PUT`

**Description:** Add or update a talkgroup rule.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "name": "TAC 1",
  "alias": "Tactical 1",
  "source": {
    "tgid": 1,
    "slot": 1
  },
  "config": {
    "active": true,
    "affiliated": false,
    "parrot": false,
    "inclusion": [10001, 10002],
    "exclusion": [],
    "rewrite": [],
    "always": [],
    "preferred": [],
    "permittedRids": []
  }
}
```

**Request Fields:**
- `name` (required): Talkgroup name/description
- `alias` (required): Short alias for talkgroup
- `source` (required): Source object containing:
  - `tgid` (required): Talkgroup ID number
  - `slot` (required): TDMA slot
- `config` (required): Configuration object containing:
  - `active` (required): Whether talkgroup is active
  - `affiliated` (required): Requires affiliation
  - `parrot` (required): Echo mode
  - `inclusion` (required): Array of peer IDs (can be empty)
  - `exclusion` (required): Array of peer IDs (can be empty)
  - `rewrite` (optional): Array of rewrite rules (can be empty)
  - `always` (optional): Array of peer IDs (can be empty)
  - `preferred` (optional): Array of peer IDs (can be empty)
  - `permittedRids` (optional): Array of radio IDs (can be empty)

**Response (Success):**
```json
{
  "status": 200
}
```

**Response (Failure - Invalid Request):**
```json
{
  "status": 400,
  "message": "TG \"name\" was not a valid string"
}
```
or other validation error messages such as:
- `"TG \"alias\" was not a valid string"`
- `"TG \"source\" was not a valid JSON object"`
- `"TG source \"tgid\" was not a valid number"`
- `"TG source \"slot\" was not a valid number"`
- `"TG \"config\" was not a valid JSON object"`
- `"TG configuration \"active\" was not a valid boolean"`
- `"TG configuration \"affiliated\" was not a valid boolean"`
- `"TG configuration \"parrot\" slot was not a valid boolean"`
- `"TG configuration \"inclusion\" was not a valid JSON array"`
- And similar for other config arrays

**Notes:**
- Changes are in-memory only until `/tg/commit` is called
- If talkgroup already exists (same tgid+slot), it will be updated
- All fields are validated and errors are returned immediately if validation fails

---

### 6.3 Endpoint: PUT /tg/delete

**Method:** `PUT`

**Description:** Remove a talkgroup rule.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "tgid": 1,
  "slot": 1
}
```

**Response (Success):**
```json
{
  "status": 200
}
```

**Response (Failure - Invalid Request):**
```json
{
  "status": 400,
  "message": "tgid was not a valid integer"
}
```
or
```json
{
  "status": 400,
  "message": "slot was not a valid char"
}
```

**Response (Failure - TGID Not Found):**
```json
{
  "status": 400,
  "message": "failed to find specified TGID to delete"
}
```

**Notes:**
- Changes are in-memory only until `/tg/commit` is called
- Returns error if the specified talkgroup (tgid+slot combination) does not exist

---

### 6.4 Endpoint: GET /tg/commit

**Method:** `GET`

**Description:** Commit all talkgroup changes to disk.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response (Success):**
```json
{
  "status": 200
}
```

**Response (Failure - Write Error):**
```json
{
  "status": 400,
  "message": "failed to write new TGID file"
}
```

**Notes:**
- Writes current in-memory state to configured talkgroup rules file
- Changes persist across FNE restarts after commit
- Returns error if file write operation fails

---

## 7. Peer List Management

Peer list management controls the authorized peer database for spanning tree configuration.

### 7.1 Endpoint: GET /peer/list

**Method:** `GET`

**Description:** Query authorized peer list.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200,
  "peers": [
    {
      "peerId": 10001,
      "peerAlias": "Site 1 Repeater",
      "peerPassword": true,
      "peerReplica": false,
      "canRequestKeys": false,
      "canIssueInhibit": false,
      "hasCallPriority": false
    }
  ]
}
```

**Response Fields:**
- `peerId`: Unique peer identifier
- `peerAlias`: Peer description/name/alias
- `peerPassword`: Whether peer has a password configured (true/false)
- `peerReplica`: Whether peer participates in peer replication
- `canRequestKeys`: Whether peer can request encryption keys
- `canIssueInhibit`: Whether peer can issue radio inhibit commands
- `hasCallPriority`: Whether peer has call priority (can preempt other calls)

---

### 7.2 Endpoint: PUT /peer/add

**Method:** `PUT`

**Description:** Add or update an authorized peer.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "peerId": 10001,
  "peerAlias": "Site 1 Repeater",
  "peerPassword": "secretpass",
  "peerReplica": false,
  "canRequestKeys": false,
  "canIssueInhibit": false,
  "hasCallPriority": false
}
```

**Request Fields:**
- `peerId` (required): Unique peer identifier
- `peerAlias` (optional): Peer description/name/alias
- `peerPassword` (optional): Peer authentication password (string, not boolean)
- `peerReplica` (optional): Whether peer participates in peer replication (default: false)
- `canRequestKeys` (optional): Whether peer can request encryption keys (default: false)
- `canIssueInhibit` (optional): Whether peer can issue radio inhibit commands (default: false)
- `hasCallPriority` (optional): Whether peer has call priority (default: false)

**Response (Success):**
```json
{
  "status": 200
}
```

**Response (Failure - Invalid Request):**
```json
{
  "status": 400,
  "message": "peerId was not a valid integer"
}
```
or
```json
{
  "status": 400,
  "message": "peerAlias was not a valid string"
}
```
or
```json
{
  "status": 400,
  "message": "peerPassword was not a valid string"
}
```
or
```json
{
  "status": 400,
  "message": "peerReplica was not a valid boolean"
}
```
or similar validation errors for `canRequestKeys`, `canIssueInhibit`, or `hasCallPriority`

**Notes:**
- Changes are in-memory only until `/peer/commit` is called
- `peerPassword` in the request is a string (the actual password), but in the GET response it's a boolean indicating whether a password is set
- If peer already exists, it will be updated

---

### 7.3 Endpoint: PUT /peer/delete

**Method:** `PUT`

**Description:** Remove an authorized peer.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "peerId": 10001
}
```

**Response (Success):**
```json
{
  "status": 200
}
```

**Response (Failure - Invalid Request):**
```json
{
  "status": 400,
  "message": "peerId was not a valid integer"
}
```

**Notes:**
- Changes are in-memory only until `/peer/commit` is called
- Returns 200 OK even if the peer ID does not exist (no validation of existence)

---

### 7.4 Endpoint: GET /peer/commit

**Method:** `GET`

**Description:** Commit all peer list changes to disk.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200
}
```

**Notes:**
- Writes current in-memory state to configured peer list file
- Changes persist across FNE restarts after commit

---

## 8. Adjacent Site Map Management

Adjacent site map configuration controls peer-to-peer adjacency relationships for network topology.

### 8.1 Endpoint: GET /adjmap/list

**Method:** `GET`

**Description:** Query adjacent site mappings (peer neighbor relationships).

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200,
  "peers": [
    {
      "peerId": 10002,
      "neighbors": [10001, 10003, 10004]
    }
  ]
}
```

**Response Fields:**
- `peerId`: Peer ID for this entry
- `neighbors`: Array of peer IDs that are adjacent/neighboring to this peer

**Notes:**
- Returns all peer adjacency mappings
- Each entry defines which peers are neighbors of a given peer

---

### 8.2 Endpoint: PUT /adjmap/add

**Method:** `PUT`

**Description:** Add or update an adjacent site mapping (peer neighbor relationship).

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "peerId": 10002,
  "neighbors": [10001, 10003, 10004]
}
```

**Request Fields:**
- `peerId` (required): Peer ID for this entry
- `neighbors` (required): Array of peer IDs that are adjacent/neighboring to this peer

**Response (Success):**
```json
{
  "status": 200
}
```

**Response (Failure - Invalid Request):**
```json
{
  "status": 400,
  "message": "peerId was not a valid integer"
}
```
or
```json
{
  "status": 400,
  "message": "Peer \"neighbors\" was not a valid JSON array"
}
```
or
```json
{
  "status": 400,
  "message": "Peer neighbor value was not a valid number"
}
```

**Notes:**
- Changes are in-memory only until `/adjmap/commit` is called
- If adjacency entry for peer already exists, it will be updated
- Empty neighbors array is valid (peer has no neighbors)

---

### 8.3 Endpoint: PUT /adjmap/delete

**Method:** `PUT`

**Description:** Remove an adjacent site mapping.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "peerId": 10002
}
```

**Response (Success):**
```json
{
  "status": 200
}
```

**Response (Failure - Invalid Request):**
```json
{
  "status": 400,
  "message": "peerId was not a valid integer"
}
```

**Notes:**
- Changes are in-memory only until `/adjmap/commit` is called
- Returns 200 OK even if the peer ID does not exist (no validation of existence)

---

### 8.4 Endpoint: GET /adjmap/commit

**Method:** `GET`

**Description:** Commit all adjacent site map changes to disk.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200
}
```

**Notes:**
- Writes current in-memory state to configured adjacent site map file
- Changes persist across FNE restarts after commit

---

## 9. System Operations

### 9.1 Endpoint: GET /force-update

**Method:** `GET`

**Description:** Force immediate update of all connected peers with current configuration.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200
}
```

**Notes:**
- Triggers immediate REPL (replication) messages to all peers
- Sends updated talkgroup rules, radio ID lists, and peer lists
- Useful after making configuration changes

---

### 9.2 Endpoint: GET /reload-tgs

**Method:** `GET`

**Description:** Reload talkgroup rules from disk.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200
}
```

**Notes:**
- Discards in-memory changes
- Reloads from configured talkgroup rules file
- Useful for reverting uncommitted changes

---

### 9.3 Endpoint: GET /reload-rids

**Method:** `GET`

**Description:** Reload radio IDs from disk.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200
}
```

**Notes:**
- Discards in-memory changes
- Reloads from configured radio ID file
- Useful for reverting uncommitted changes

---

### 9.4 Endpoint: GET /reload-peers

**Method:** `GET`

**Description:** Reload authorized peer list from disk.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200
}
```

**Notes:**
- Discards in-memory changes to peer list
- Reloads from configured peer list file
- Useful for reverting uncommitted changes to authorized peers

---

### 9.5 Endpoint: GET /reload-crypto

**Method:** `GET`

**Description:** Reload cryptographic keys from disk.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200
}
```

**Notes:**
- Reloads encryption keys from configured crypto key file
- Used to update encryption keys without restarting the FNE
- Applies to DMR, P25, and NXDN encryption key tables

---

### 9.6 Endpoint: GET /stats

**Method:** `GET`

**Description:** Get FNE statistics and metrics including peer status, table load times, and call counts.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200,
  "peerStats": [
    {
      "peerId": 10001,
      "masterId": 10001,
      "address": "192.168.1.100",
      "port": 54321,
      "lastPing": "Fri Dec  6 10:30:45 2025",
      "pingsReceived": 1234,
      "missedMetadataUpdates": 0,
      "isNeighbor": false,
      "isReplica": false
    }
  ],
  "tableLastLoad": {
    "ridLastLoadTime": "Fri Dec  6 08:15:30 2025",
    "tgLastLoadTime": "Fri Dec  6 08:15:30 2025",
    "peerListLastLoadTime": "Fri Dec  6 08:15:30 2025",
    "adjSiteMapLastLoadTime": "Fri Dec  6 08:15:30 2025",
    "cryptoKeyLastLoadTime": "Fri Dec  6 08:15:30 2025"
  },
  "totalCallsProcessed": 5678,
  "ridTotalEntries": 150,
  "tgTotalEntries": 45,
  "peerListTotalEntries": 8,
  "adjSiteMapTotalEntries": 6,
  "cryptoKeyTotalEntries": 12
}
```

**Response Fields:**

**peerStats[]** - Array of peer statistics:
- `peerId`: Unique peer identifier
- `masterId`: Master peer ID
- `address`: IP address of peer
- `port`: Network port of peer
- `lastPing`: Last ping timestamp (human-readable format)
- `pingsReceived`: Total pings received from this peer
- `missedMetadataUpdates`: Number of missed metadata updates
- `isNeighbor`: Whether this is a neighbor FNE peer
- `isReplica`: Whether this peer participates in replication

**tableLastLoad** - Lookup table load timestamps:
- `ridLastLoadTime`: Radio ID table last load time (human-readable format)
- `tgLastLoadTime`: Talkgroup table last load time (human-readable format)
- `peerListLastLoadTime`: Peer list table last load time (human-readable format)
- `adjSiteMapLastLoadTime`: Adjacent site map table last load time (human-readable format)
- `cryptoKeyLastLoadTime`: Crypto key table last load time (human-readable format)

**Statistics Totals:**
- `totalCallsProcessed`: Total number of calls processed since FNE startup
- `ridTotalEntries`: Total entries in radio ID lookup table
- `tgTotalEntries`: Total entries in talkgroup rules table
- `peerListTotalEntries`: Total entries in authorized peer list
- `adjSiteMapTotalEntries`: Total entries in adjacent site map
- `cryptoKeyTotalEntries`: Total encryption keys loaded

**Notes:**
- Statistics are reset on FNE restart
- Timestamp fields use `ctime` format (e.g., "Fri Dec  6 10:30:45 2025")
- Useful for monitoring FNE health, performance, and peer connectivity
- `peerStats` array contains one entry per connected peer
- Table load times help identify when configuration files were last reloaded

---

### 9.7 Endpoint: GET /report-affiliations

**Method:** `GET`

**Description:** Get current radio affiliations across all peers.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200,
  "affiliations": [
    {
      "peerId": 10001,
      "affiliations": [
        {
          "srcId": 123456,
          "dstId": 1
        },
        {
          "srcId": 789012,
          "dstId": 2
        }
      ]
    },
    {
      "peerId": 10002,
      "affiliations": [
        {
          "srcId": 345678,
          "dstId": 1
        }
      ]
    }
  ]
}
```

**Response Fields:**
- `affiliations[]`: Array of peer affiliation records
  - `peerId`: Peer ID where affiliations are registered
  - `affiliations[]`: Array of affiliation records for this peer
    - `srcId`: Radio ID (subscriber ID)
    - `dstId`: Talkgroup ID the radio is affiliated to

**Notes:**
- Affiliations are grouped by peer ID
- Each peer may have multiple radio affiliations
- Empty peers (no affiliations) are not included in the response

---

### 9.8 Endpoint: GET /spanning-tree

**Method:** `GET`

**Description:** Get current network spanning tree topology.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200,
  "masterTree": [
    {
      "id": 1,
      "masterId": 1,
      "identity": "Master FNE",
      "children": [
        {
          "id": 10001,
          "masterId": 10001,
          "identity": "Site 1 FNE",
          "children": [
            {
              "id": 20001,
              "masterId": 20001,
              "identity": "Site 1 Repeater",
              "children": []
            }
          ]
        },
        {
          "id": 10002,
          "masterId": 10002,
          "identity": "Site 2 FNE",
          "children": []
        }
      ]
    }
  ]
}
```

**Response Fields:**
- `masterTree[]`: Array containing the root tree node (typically one element)
  - `id`: Peer ID of this node
  - `masterId`: Master peer ID (usually same as id for FNE nodes)
  - `identity`: Peer identity string
  - `children[]`: Array of child tree nodes with same structure (recursive)

**Notes:**
- Shows hierarchical network structure as a tree
- The root node represents the master FNE at the top of the tree
- Each node can have multiple children forming a hierarchical structure
- Leaf peers (dvmhost, dvmbridge) have empty children arrays
- FNE nodes with connected peers will have non-empty children arrays
- Useful for visualizing network topology and detecting duplicate connections

---

## 10. Protocol-Specific Operations

### 10.1 DMR Operations

#### 10.1.1 Endpoint: PUT /dmr/rid

**Method:** `PUT`

**Description:** Execute DMR-specific radio ID operations.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "peerId": 10001,
  "command": "check",
  "dstId": 123456,
  "slot": 1
}
```

**Request Parameters:**
- `peerId` (optional, integer): Target peer ID. Defaults to 0 (broadcast to all peers)
- `command` (required, string): Command to execute (see supported commands below)
- `dstId` (required, integer): Target radio ID
- `slot` (required, integer): DMR TDMA slot number (1 or 2)

**Supported Commands:**
- `page`: Send call alert (page) to radio
- `check`: Radio check
- `inhibit`: Radio inhibit
- `uninhibit`: Radio un-inhibit

**Response:**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Error Responses:**
- `400 Bad Request`: If command, dstId, or slot is missing or invalid
  ```json
  {
    "status": 400,
    "message": "command was not valid"
  }
  ```
- `400 Bad Request`: If slot is 0 or greater than 2
  ```json
  {
    "status": 400,
    "message": "invalid DMR slot number (slot == 0 or slot > 3)"
  }
  ```
- `400 Bad Request`: If command is not recognized
  ```json
  {
    "status": 400,
    "message": "invalid command"
  }
  ```

**Notes:**
- Commands are sent to specified peer or broadcast to all peers if peerId is 0
- `slot` parameter must be 1 or 2 for DMR TDMA slots
- Radio must be registered/affiliated on peer

---

### 10.2 P25 Operations

#### 10.2.1 Endpoint: PUT /p25/rid

**Method:** `PUT`

**Description:** Execute P25-specific radio ID operations.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "peerId": 10001,
  "command": "check",
  "dstId": 123456
}
```

**Request Parameters:**
- `peerId` (optional, integer): Target peer ID. Defaults to 0 (broadcast to all peers)
- `command` (required, string): Command to execute (see supported commands below)
- `dstId` (required, integer): Target radio ID
- `tgId` (required for `dyn-regrp` only, integer): Target talkgroup ID for dynamic regroup

**Supported Commands:**
- `page`: Send call alert (page) to radio
- `check`: Radio check
- `inhibit`: Radio inhibit
- `uninhibit`: Radio un-inhibit
- `dyn-regrp`: Dynamic regroup (requires `tgId` parameter)
- `dyn-regrp-cancel`: Cancel dynamic regroup
- `dyn-regrp-lock`: Lock dynamic regroup
- `dyn-regrp-unlock`: Unlock dynamic regroup
- `group-aff-req`: Group affiliation query
- `unit-reg`: Unit registration request

**Example with tgId (dynamic regroup):**
```json
{
  "peerId": 10001,
  "command": "dyn-regrp",
  "dstId": 123456,
  "tgId": 1
}
```

**Response:**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Error Responses:**
- `400 Bad Request`: If command or dstId is missing or invalid
  ```json
  {
    "status": 400,
    "message": "command was not valid"
  }
  ```
- `400 Bad Request`: If tgId is missing for `dyn-regrp` command
  ```json
  {
    "status": 400,
    "message": "talkgroup ID was not valid"
  }
  ```
- `400 Bad Request`: If command is not recognized
  ```json
  {
    "status": 400,
    "message": "invalid command"
  }
  ```

**Notes:**
- Commands are sent via P25 TSDU (Trunking System Data Unit) messages
- Commands are sent to specified peer or broadcast to all peers if peerId is 0
- Radio must be registered on the P25 system
- The `dyn-regrp` command requires the `tgId` parameter to specify target talkgroup
- No `slot` parameter is used for P25 (unlike DMR)

---

## 11. Response Formats

### 11.1 Standard Success Response

All successful API calls return HTTP 200 with a JSON object containing at minimum:

```json
{
  "status": 200
}
```

Some endpoints include an additional message field:

```json
{
  "status": 200,
  "message": "OK"
}
```

Data-returning endpoints add additional fields based on the endpoint (e.g., `version`, `peers`, `talkgroups`, `affiliations`, etc.).

### 11.2 Standard Error Response

Error responses include HTTP status code and JSON error object:

```json
{
  "status": 400,
  "message": "descriptive error message"
}
```

**HTTP Status Codes:**
- `200 OK`: Request successful
- `400 Bad Request`: Invalid request format or parameters
- `401 Unauthorized`: Missing or invalid authentication token
- `404 Not Found`: Endpoint does not exist (not commonly used by FNE)
- `500 Internal Server Error`: Server-side error (rare)

---

## 12. Error Handling

### 12.1 Authentication Errors

**Missing Token:**
```json
{
  "status": 401,
  "message": "no authentication token"
}
```

**Invalid Token (wrong token value for host):**
```json
{
  "status": 401,
  "message": "invalid authentication token"
}
```

**Illegal Token (host not authenticated):**
```json
{
  "status": 401,
  "message": "illegal authentication token"
}
```

**Notes:**
- Tokens are bound to the client's hostname/IP address
- An invalid token for a known host will devalidate that host's token
- An illegal token means the host hasn't authenticated yet or token expired

### 12.2 Validation Errors

**Invalid JSON:**
```json
{
  "status": 400,
  "message": "JSON parse error: unexpected character at position X"
}
```

**Invalid Content-Type:**

When Content-Type is not `application/json`, the server returns a plain text error response:
```
HTTP/1.1 400 Bad Request
Content-Type: text/plain

Invalid Content-Type. Expected: application/json
```

**Not a JSON Object:**
```json
{
  "status": 400,
  "message": "Request was not a valid JSON object."
}
```

**Missing or Invalid Required Fields:**

Examples of field validation errors:
```json
{
  "status": 400,
  "message": "command was not valid"
}
```
```json
{
  "status": 400,
  "message": "destination ID was not valid"
}
```
```json
{
  "status": 400,
  "message": "TG \"name\" was not a valid string"
}
```
```json
{
  "status": 400,
  "message": "TG source \"tgid\" was not a valid number"
}
```

### 12.3 Resource Errors

**Peer Not Found:**
```json
{
  "status": 400,
  "message": "cannot find peer"
}
```

**Talkgroup Not Found:**
```json
{
  "status": 400,
  "message": "cannot find talkgroup"
}
```

**Radio ID Not Found:**
```json
{
  "status": 400,
  "message": "cannot find RID"
}
```

**Invalid Command:**
```json
{
  "status": 400,
  "message": "invalid command"
}
```

### 12.4 Error Handling Best Practices

1. **Always check the `status` field** in responses, not just HTTP status code
2. **Parse the `message` field** for human-readable error descriptions
3. **Handle 401 errors** by re-authenticating with a new token
4. **Validate inputs** client-side to minimize 400 errors
5. **Log error responses** for debugging and audit trails

---

## 13. Security Considerations

### 13.1 Password Security

- **Never send plaintext passwords:** Always hash with SHA-256 before transmission
- **Use HTTPS in production:** Prevents token interception
- **Rotate passwords regularly:** Change FNE password periodically
- **Strong passwords:** Use complex passwords (minimum 16 characters recommended)

### 13.2 Token Management

- **Tokens are session-based:** Bound to client IP/hostname
- **Token invalidation:** Tokens are invalidated on:
  - Re-authentication
  - Explicit invalidation
  - Server restart
- **Token format:** 64-bit unsigned integer (not cryptographically secure by itself)

### 13.3 Network Security

- **Use HTTPS:** Enable SSL/TLS for production deployments
- **Firewall rules:** Restrict REST API access to trusted networks
- **Rate limiting:** Consider implementing rate limiting for brute-force protection
- **Audit logging:** Enable debug logging to track API access

### 13.4 SSL/TLS Configuration

When using HTTPS, ensure:
- Valid SSL certificates (not self-signed for production)
- Strong cipher suites enabled
- TLS 1.2 or higher
- Certificate expiration monitoring

---

## 14. Examples

### 14.1 Complete Authentication and Query Flow

```bash
#!/bin/bash

# Configuration
FNE_HOST="fne.example.com"
FNE_PORT="9990"
PASSWORD="your_password_here"

# Step 1: Generate password hash
echo "Generating password hash..."
HASH=$(echo -n "$PASSWORD" | sha256sum | cut -d' ' -f1)

# Step 2: Authenticate
echo "Authenticating..."
AUTH_RESPONSE=$(curl -s -X PUT "http://${FNE_HOST}:${FNE_PORT}/auth" \
  -H "Content-Type: application/json" \
  -d "{\"auth\":\"$HASH\"}")

TOKEN=$(echo "$AUTH_RESPONSE" | jq -r '.token')

if [ "$TOKEN" == "null" ] || [ -z "$TOKEN" ]; then
  echo "Authentication failed!"
  echo "$AUTH_RESPONSE"
  exit 1
fi

echo "Authenticated successfully. Token: $TOKEN"

# Step 3: Get version
echo -e "\nGetting version..."
curl -s -X GET "http://${FNE_HOST}:${FNE_PORT}/version" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

# Step 4: Get status
echo -e "\nGetting status..."
curl -s -X GET "http://${FNE_HOST}:${FNE_PORT}/status" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

# Step 5: Query peers
echo -e "\nQuerying peers..."
curl -s -X GET "http://${FNE_HOST}:${FNE_PORT}/peer/query" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq
```

### 14.2 Add Talkgroup with Inclusion List

```bash
#!/bin/bash

TOKEN="your_token_here"
FNE_HOST="fne.example.com"
FNE_PORT="9990"

# Add talkgroup
curl -X PUT "http://${FNE_HOST}:${FNE_PORT}/tg/add" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Emergency Services",
    "alias": "EMERG",
    "source": {
      "tgid": 9999,
      "slot": 1
    },
    "config": {
      "active": true,
      "affiliated": true,
      "parrot": false,
      "inclusion": [10001, 10002, 10003],
      "exclusion": [],
      "rewrite": [],
      "always": [10001],
      "preferred": [],
      "permittedRids": [100, 101, 102, 103]
    }
  }' | jq

# Commit changes
curl -X GET "http://${FNE_HOST}:${FNE_PORT}/tg/commit" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

# Force update to all peers
curl -X GET "http://${FNE_HOST}:${FNE_PORT}/force-update" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq
```

### 14.3 Radio ID Whitelist Management

```bash
#!/bin/bash

TOKEN="your_token_here"
FNE_HOST="fne.example.com"
FNE_PORT="9990"

# Add multiple radio IDs
RADIO_IDS=(123456 234567 345678 456789)

for RID in "${RADIO_IDS[@]}"; do
  echo "Adding RID: $RID"
  curl -X PUT "http://${FNE_HOST}:${FNE_PORT}/rid/add" \
    -H "X-DVM-Auth-Token: $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"rid\":$RID,\"enabled\":true}" | jq
done

# Commit all changes
echo "Committing changes..."
curl -X GET "http://${FNE_HOST}:${FNE_PORT}/rid/commit" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

# Query to verify
echo "Verifying..."
curl -X GET "http://${FNE_HOST}:${FNE_PORT}/rid/query" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq
```

### 14.4 P25 Radio Operations

```bash
#!/bin/bash

TOKEN="your_token_here"
FNE_HOST="fne.example.com"
FNE_PORT="9990"

# Send radio check to radio 123456 via peer 10001
curl -X PUT "http://${FNE_HOST}:${FNE_PORT}/p25/rid" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "peerId": 10001,
    "command": "check",
    "dstId": 123456
  }' | jq

# Send page to radio 123456
curl -X PUT "http://${FNE_HOST}:${FNE_PORT}/p25/rid" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "peerId": 10001,
    "command": "page",
    "dstId": 123456
  }' | jq

# Dynamic regroup radio 123456 to talkgroup 5000
curl -X PUT "http://${FNE_HOST}:${FNE_PORT}/p25/rid" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "peerId": 10001,
    "command": "dyn-regrp",
    "dstId": 123456,
    "tgId": 5000
  }' | jq

# Cancel dynamic regroup for radio 123456
curl -X PUT "http://${FNE_HOST}:${FNE_PORT}/p25/rid" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "peerId": 10001,
    "command": "dyn-regrp-cancel",
    "dstId": 123456
  }' | jq

# Send group affiliation query to radio 123456
curl -X PUT "http://${FNE_HOST}:${FNE_PORT}/p25/rid" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "peerId": 10001,
    "command": "group-aff-req",
    "dstId": 123456
  }' | jq
```

### 14.5 Python Example with Requests Library

```python
#!/usr/bin/env python3

import requests
import hashlib
import json

class DVMFNEClient:
    def __init__(self, host, port, password, use_https=False):
        self.base_url = f"{'https' if use_https else 'http'}://{host}:{port}"
        self.password = password
        self.token = None
        
    def authenticate(self):
        """Authenticate and get token"""
        password_hash = hashlib.sha256(self.password.encode()).hexdigest()
        
        response = requests.put(
            f"{self.base_url}/auth",
            json={"auth": password_hash}
        )
        
        if response.status_code == 200:
            data = response.json()
            self.token = data.get('token')
            return True
        else:
            print(f"Authentication failed: {response.text}")
            return False
    
    def _headers(self):
        """Get headers with auth token"""
        return {
            "X-DVM-Auth-Token": self.token,
            "Content-Type": "application/json"
        }
    
    def get_version(self):
        """Get FNE version"""
        response = requests.get(
            f"{self.base_url}/version",
            headers=self._headers()
        )
        return response.json()
    
    def get_peers(self):
        """Get connected peers"""
        response = requests.get(
            f"{self.base_url}/peer/query",
            headers=self._headers()
        )
        return response.json()
    
    def add_talkgroup(self, tgid, name, slot=1, active=True, affiliated=False):
        """Add a talkgroup"""
        data = {
            "name": name,
            "alias": name[:8],
            "source": {
                "tgid": tgid,
                "slot": slot
            },
            "config": {
                "active": active,
                "affiliated": affiliated,
                "parrot": False,
                "inclusion": [],
                "exclusion": [],
                "rewrite": [],
                "always": [],
                "preferred": [],
                "permittedRids": []
            }
        }
        
        response = requests.put(
            f"{self.base_url}/tg/add",
            headers=self._headers(),
            json=data
        )
        return response.json()
    
    def commit_talkgroups(self):
        """Commit talkgroup changes"""
        response = requests.get(
            f"{self.base_url}/tg/commit",
            headers=self._headers()
        )
        return response.json()
    
    def get_affiliations(self):
        """Get current affiliations"""
        response = requests.get(
            f"{self.base_url}/report-affiliations",
            headers=self._headers()
        )
        return response.json()

# Example usage
if __name__ == "__main__":
    # Create client
    client = DVMFNEClient("fne.example.com", 9990, "your_password_here")
    
    # Authenticate
    if client.authenticate():
        print("Authenticated successfully!")
        
        # Get version
        version = client.get_version()
        print(f"FNE Version: {version['version']}")
        
        # Get peers
        peers = client.get_peers()
        print(f"Connected peers: {len(peers.get('peers', []))}")
        
        # Add talkgroup
        result = client.add_talkgroup(100, "Test TG", slot=1, active=True)
        print(f"Add talkgroup result: {result}")
        
        # Commit
        result = client.commit_talkgroups()
        print(f"Commit result: {result}")
        
        # Get affiliations
        affs = client.get_affiliations()
        print(f"Affiliations: {json.dumps(affs, indent=2)}")
    else:
        print("Authentication failed!")
```

---

## Appendix A: Endpoint Summary Table

| Method | Endpoint | Description | Auth Required |
|--------|----------|-------------|---------------|
| PUT | /auth | Authenticate and get token | No |
| GET | /version | Get FNE version | Yes |
| GET | /status | Get FNE status | Yes |
| GET | /peer/query | Query connected peers | Yes |
| GET | /peer/count | Get peer count | Yes |
| PUT | /peer/reset | Reset peer connection | Yes |
| PUT | /peer/connreset | Reset upstream connection | Yes |
| GET | /rid/query | Query radio IDs | Yes |
| PUT | /rid/add | Add radio ID | Yes |
| PUT | /rid/delete | Delete radio ID | Yes |
| GET | /rid/commit | Commit radio ID changes | Yes |
| GET | /tg/query | Query talkgroups | Yes |
| PUT | /tg/add | Add talkgroup | Yes |
| PUT | /tg/delete | Delete talkgroup | Yes |
| GET | /tg/commit | Commit talkgroup changes | Yes |
| GET | /peer/list | Query peer list | Yes |
| PUT | /peer/add | Add authorized peer | Yes |
| PUT | /peer/delete | Delete authorized peer | Yes |
| GET | /peer/commit | Commit peer list changes | Yes |
| GET | /adjmap/list | Query adjacent site map | Yes |
| PUT | /adjmap/add | Add adjacent site | Yes |
| PUT | /adjmap/delete | Delete adjacent site | Yes |
| GET | /adjmap/commit | Commit adjacent site changes | Yes |
| GET | /force-update | Force peer updates | Yes |
| GET | /reload-tgs | Reload talkgroups from disk | Yes |
| GET | /reload-rids | Reload radio IDs from disk | Yes |
| GET | /reload-peers | Reload peer list from disk | Yes |
| GET | /reload-crypto | Reload crypto keys from disk | Yes |
| GET | /stats | Get FNE statistics | Yes |
| GET | /report-affiliations | Get affiliations | Yes |
| GET | /spanning-tree | Get network topology | Yes |
| PUT | /dmr/rid | DMR radio operations | Yes |
| PUT | /p25/rid | P25 radio operations | Yes |

---

## Appendix B: Configuration File Reference

### REST API Configuration (YAML)

```yaml
restApi:
  # Enable REST API
  enable: true
  
  # Bind address (0.0.0.0 = all interfaces)
  address: 0.0.0.0
  
  # Port number
  port: 9990
  
  # SHA-256 hashed password (pre-hash before putting in config)
  password: "your_secure_password"
  
  # SSL/TLS Configuration (optional)
  ssl:
    enable: false
    keyFile: /path/to/private.key
    certFile: /path/to/certificate.crt
  
  # Enable debug logging
  debug: false
```

---

## Appendix C: Common Use Cases

### C.1 Automated Peer Management

Monitor peer connections and automatically reset stuck peers:

```bash
# Get peer status
PEERS=$(curl -s -X GET "http://fne:9990/peer/query" \
  -H "X-DVM-Auth-Token: $TOKEN")

# Check for peers with old lastPing
CURRENT_TIME=$(date +%s)
echo "$PEERS" | jq -r '.peers[] | select(.lastPing < ('$CURRENT_TIME' - 300)) | .peerId' | while read PEER_ID; do
  echo "Resetting peer $PEER_ID (stale connection)"
  curl -X PUT "http://fne:9990/peer/reset" \
    -H "X-DVM-Auth-Token: $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"peerId\":$PEER_ID}"
done
```

### C.2 Dynamic Talkgroup Provisioning

Automatically create talkgroups from external source:

```bash
# Read talkgroups from CSV file
# Format: TGID,Name,Slot,Affiliated
while IFS=',' read -r TGID NAME SLOT AFFILIATED; do
  curl -X PUT "http://fne:9990/tg/add" \
    -H "X-DVM-Auth-Token: $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{
      \"name\": \"$NAME\",
      \"alias\": \"${NAME:0:8}\",
      \"source\": {\"tgid\": $TGID, \"slot\": $SLOT},
      \"config\": {
        \"active\": true,
        \"affiliated\": $AFFILIATED,
        \"parrot\": false,
        \"inclusion\": [],
        \"exclusion\": [],
        \"rewrite\": [],
        \"always\": [],
        \"preferred\": [],
        \"permittedRids\": []
      }
    }"
done < talkgroups.csv

# Commit all changes
curl -X GET "http://fne:9990/tg/commit" \
  -H "X-DVM-Auth-Token: $TOKEN"
```

### C.3 Affiliation Monitoring

Monitor and alert on specific affiliations:

```python
#!/usr/bin/env python3
import time
import requests

def monitor_affiliations(fne_host, port, token, watch_tgid):
    """Monitor affiliations for specific talkgroup"""
    url = f"http://{fne_host}:{port}/report-affiliations"
    headers = {"X-DVM-Auth-Token": token}
    
    known_affiliations = set()
    
    while True:
        try:
            response = requests.get(url, headers=headers)
            if response.status_code == 200:
                data = response.json()
                
                # Flatten nested structure: affiliations is array of {peerId, affiliations[]}
                current = set()
                for peer_data in data.get('affiliations', []):
                    peer_id = peer_data.get('peerId')
                    for aff in peer_data.get('affiliations', []):
                        src_id = aff.get('srcId')
                        dst_id = aff.get('dstId')
                        if dst_id == watch_tgid:
                            current.add((src_id, peer_id))
                
                # Detect new affiliations
                new_affs = current - known_affiliations
                for src_id, peer_id in new_affs:
                    print(f"NEW: Radio {src_id} affiliated to TG {watch_tgid} on peer {peer_id}")
                
                # Detect removed affiliations
                removed = known_affiliations - current
                for src_id, peer_id in removed:
                    print(f"REMOVED: Radio {src_id} de-affiliated from TG {watch_tgid}")
                
                known_affiliations = current
                
        except Exception as e:
            print(f"Error monitoring affiliations: {e}")
        
        time.sleep(5)

# Example usage
monitor_affiliations("fne.example.com", 9990, "your_token", 1)
```

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | Dec 3, 2025 | Initial documentation based on source code analysis |
| 1.1 | Dec 6, 2025 | Added missing endpoints: `/reload-peers`, `/reload-crypto`, `/stats` |

---

**End of Document**
