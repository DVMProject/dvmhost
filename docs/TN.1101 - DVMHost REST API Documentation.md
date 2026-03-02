# DVM Host REST API Technical Documentation

**Version:** 1.0  
**Date:** December 3, 2025  
**Author:** AI Assistant (based on source code analysis)

AI WARNING: This document was mainly generated using AI assistance. As such, there is the possibility of some error or inconsistency. Examples in Section 13 are *strictly* examples only for how the API *could* be used.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Authentication](#2-authentication)
3. [System Endpoints](#3-system-endpoints)
4. [Modem Control](#4-modem-control)
5. [Trunking and Supervisory Control](#5-trunking-and-supervisory-control)
6. [Radio ID Lookup](#6-radio-id-lookup)
7. [DMR Protocol Endpoints](#7-dmr-protocol-endpoints)
8. [P25 Protocol Endpoints](#8-p25-protocol-endpoints)
9. [NXDN Protocol Endpoints](#9-nxdn-protocol-endpoints)
10. [Response Formats](#10-response-formats)
11. [Error Handling](#11-error-handling)
12. [Security Considerations](#12-security-considerations)
13. [Examples](#13-examples)

---

## 1. Overview

The DVM (Digital Voice Modem) Host REST API provides a comprehensive interface for managing and controlling dvmhost instances. The dvmhost software interfaces directly with radio modems to provide DMR, P25, and NXDN digital voice repeater/hotspot functionality. The REST API allows remote configuration, mode control, protocol-specific operations, and real-time monitoring.

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
- **Protocol Handlers:** Interfaces with DMR, P25, and NXDN control classes
- **Lookup Tables:** Radio ID and Talkgroup Rules

### 1.3 Use Cases

- **Remote Mode Control:** Switch between DMR, P25, NXDN, or idle modes
- **Diagnostics:** Enable/disable debug logging for protocols
- **Trunking Operations:** Grant channels, permit talkgroups, manage affiliations
- **Radio Management:** Send radio checks, inhibits, pages, and other RID commands
- **Control Channel Management:** Enable/disable control channels and broadcast modes
- **System Monitoring:** Query status, voice channels, and affiliations

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

**Description:** Authenticate with the dvmhost REST API and obtain an authentication token.

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
- Auth string must be exactly 64 hexadecimal characters
- Valid characters: `0-9`, `a-f`, `A-F`

**Example (bash with curl):**
```bash
# Generate SHA-256 hash of password
PASSWORD="your_password_here"
HASH=$(echo -n "$PASSWORD" | sha256sum | cut -d' ' -f1)

# Authenticate
TOKEN=$(curl -X PUT http://dvmhost.example.com:9990/auth \
  -H "Content-Type: application/json" \
  -d "{\"auth\":\"$HASH\"}" | jq -r '.token')

echo "Token: $TOKEN"
```

---

## 3. System Endpoints

### 3.1 Endpoint: GET /version

**Method:** `GET`

**Description:** Retrieve dvmhost software version information.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200,
  "version": "dvmhost 4.0.0 (built Dec 03 2025 12:00:00)"
}
```

**Notes:**
- Returns program name, version, and build timestamp
- Useful for compatibility checks and diagnostics

---

### 3.2 Endpoint: GET /status

**Method:** `GET`

**Description:** Retrieve current dvmhost system status and configuration.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200,
  "dmrEnabled": true,
  "p25Enabled": true,
  "nxdnEnabled": false,
  "state": 4,
  "isTxCW": false,
  "fixedMode": false,
  "dmrTSCCEnable": false,
  "dmrCC": false,
  "p25CtrlEnable": true,
  "p25CC": true,
  "nxdnCtrlEnable": false,
  "nxdnCC": false,
  "tx": false,
  "channelId": 1,
  "channelNo": 1,
  "lastDstId": 9001,
  "lastSrcId": 123456,
  "peerId": 10001,
  "sysId": 1,
  "siteId": 1,
  "p25RfssId": 1,
  "p25NetId": 48896,
  "p25NAC": 293,
  "vcChannels": [
    {
      "channelNo": 1,
      "channelId": 1,
      "tx": false,
      "lastDstId": 9001,
      "lastSrcId": 123456
    }
  ],
  "modem": {
    "portType": "uart",
    "modemPort": "/dev/ttyUSB0",
    "portSpeed": 115200,
    "pttInvert": false,
    "rxInvert": false,
    "txInvert": false,
    "dcBlocker": true,
    "rxLevel": 50.0,
    "cwTxLevel": 50.0,
    "dmrTxLevel": 50.0,
    "p25TxLevel": 50.0,
    "nxdnTxLevel": 50.0,
    "rxDCOffset": 0,
    "txDCOffset": 0,
    "dmrSymLevel3Adj": 0,
    "dmrSymLevel1Adj": 0,
    "p25SymLevel3Adj": 0,
    "p25SymLevel1Adj": 0,
    "nxdnSymLevel3Adj": 0,
    "nxdnSymLevel1Adj": 0,
    "fdmaPreambles": 80,
    "dmrRxDelay": 7,
    "p25CorrCount": 4,
    "rxFrequency": 449000000,
    "txFrequency": 444000000,
    "rxTuning": 0,
    "txTuning": 0,
    "rxFrequencyEffective": 449000000,
    "txFrequencyEffective": 444000000,
    "v24Connected": false,
    "protoVer": 3
  }
}
```

**Response Fields (Top Level):**
- `status`: HTTP status code (always 200 for success)
- `dmrEnabled`: DMR protocol enabled
- `p25Enabled`: P25 protocol enabled
- `nxdnEnabled`: NXDN protocol enabled
- `state`: Current host state (0=IDLE, 1=LOCKOUT, 2=ERROR, 4=DMR, 5=P25, 6=NXDN)
- `isTxCW`: Currently transmitting CW ID
- `fixedMode`: Host is in fixed mode (true) or dynamic mode (false)
- `dmrTSCCEnable`: DMR TSCC (Tier III Control Channel) data enabled
- `dmrCC`: DMR control channel mode active
- `p25CtrlEnable`: P25 control channel data enabled
- `p25CC`: P25 control channel mode active
- `nxdnCtrlEnable`: NXDN control channel data enabled
- `nxdnCC`: NXDN control channel mode active
- `tx`: Modem currently transmitting
- `channelId`: Current RF channel ID
- `channelNo`: Current RF channel number
- `lastDstId`: Last destination ID (talkgroup)
- `lastSrcId`: Last source ID (radio ID)
- `peerId`: Peer ID from network configuration
- `sysId`: System ID
- `siteId`: Site ID
- `p25RfssId`: P25 RFSS ID
- `p25NetId`: P25 Network ID (WACN)
- `p25NAC`: P25 Network Access Code

**Voice Channels Array (`vcChannels[]`):**
- `channelNo`: Voice channel number
- `channelId`: Voice channel ID
- `tx`: Channel currently transmitting
- `lastDstId`: Last destination ID on this channel
- `lastSrcId`: Last source ID on this channel

**Modem Object (`modem`):**
- `portType`: Port type (uart, tcp, udp, null)
- `modemPort`: Serial port path
- `portSpeed`: Serial port speed (baud rate)
- `pttInvert`: PTT signal inverted (repeater only)
- `rxInvert`: RX signal inverted (repeater only)
- `txInvert`: TX signal inverted (repeater only)
- `dcBlocker`: DC blocker enabled (repeater only)
- `rxLevel`: Receive audio level (0.0-100.0)
- `cwTxLevel`: CW ID transmit level (0.0-100.0)
- `dmrTxLevel`: DMR transmit level (0.0-100.0)
- `p25TxLevel`: P25 transmit level (0.0-100.0)
- `nxdnTxLevel`: NXDN transmit level (0.0-100.0)
- `rxDCOffset`: Receive DC offset adjustment
- `txDCOffset`: Transmit DC offset adjustment
- `dmrSymLevel3Adj`: DMR symbol level 3 adjustment (repeater only)
- `dmrSymLevel1Adj`: DMR symbol level 1 adjustment (repeater only)
- `p25SymLevel3Adj`: P25 symbol level 3 adjustment (repeater only)
- `p25SymLevel1Adj`: P25 symbol level 1 adjustment (repeater only)
- `nxdnSymLevel3Adj`: NXDN symbol level 3 adjustment (repeater only, protocol v3+)
- `nxdnSymLevel1Adj`: NXDN symbol level 1 adjustment (repeater only, protocol v3+)
- `dmrDiscBW`: DMR discriminator bandwidth adjustment (hotspot only)
- `dmrPostBW`: DMR post-demod bandwidth adjustment (hotspot only)
- `p25DiscBW`: P25 discriminator bandwidth adjustment (hotspot only)
- `p25PostBW`: P25 post-demod bandwidth adjustment (hotspot only)
- `nxdnDiscBW`: NXDN discriminator bandwidth adjustment (hotspot only, protocol v3+)
- `nxdnPostBW`: NXDN post-demod bandwidth adjustment (hotspot only, protocol v3+)
- `afcEnabled`: Automatic Frequency Control enabled (hotspot only, protocol v3+)
- `afcKI`: AFC integral gain (hotspot only, protocol v3+)
- `afcKP`: AFC proportional gain (hotspot only, protocol v3+)
- `afcRange`: AFC range (hotspot only, protocol v3+)
- `gainMode`: ADF7021 gain mode string (hotspot only)
- `fdmaPreambles`: FDMA preamble count
- `dmrRxDelay`: DMR receive delay
- `p25CorrCount`: P25 correlation count
- `rxFrequency`: Receive frequency (Hz)
- `txFrequency`: Transmit frequency (Hz)
- `rxTuning`: Receive tuning offset (Hz)
- `txTuning`: Transmit tuning offset (Hz)
- `rxFrequencyEffective`: Effective RX frequency (rxFrequency + rxTuning)
- `txFrequencyEffective`: Effective TX frequency (txFrequency + txTuning)
- `v24Connected`: V.24/RS-232 connected
- `protoVer`: Modem protocol version

**Notes:**
- This endpoint provides comprehensive system status including modem parameters
- Modem fields vary based on hardware type (hotspot vs repeater) and protocol version
- Hotspot-specific fields only appear for hotspot hardware
- Repeater-specific fields only appear for repeater hardware
- Protocol v3+ fields only appear if modem firmware is version 3 or higher
- Voice channels array populated when operating as control channel with voice channels configured

---

### 3.3 Endpoint: GET /voice-ch

**Method:** `GET`

**Description:** Retrieve configured voice channel information.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response:**
```json
{
  "status": 200,
  "channels": [
    {
      "chNo": 1,
      "address": "192.168.1.100",
      "port": 54321
    },
    {
      "chNo": 2,
      "address": "192.168.1.101",
      "port": 54322
    }
  ]
}
```

**Response Fields:**
- `chNo`: Channel number
- `address`: Network address for voice channel
- `port`: Network port for voice channel

**Notes:**
- Used in multi-site trunking configurations
- Returns empty array if no voice channels configured
- Voice channels are typically FNE peer connections

---

## 4. Modem Control

### 4.1 Endpoint: PUT /mdm/mode

**Method:** `PUT`

**Description:** Set the dvmhost operational mode.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "mode": "idle"
}
```

**Request Fields:**
- `mode` (required, string): Operating mode to set

**Supported Modes:**
- `"idle"`: Dynamic mode (automatic protocol switching)
- `"lockout"`: Lockout mode (no transmissions allowed)
- `"dmr"`: Fixed DMR mode
- `"p25"`: Fixed P25 mode
- `"nxdn"`: Fixed NXDN mode

**Response (Success):**
```json
{
  "status": 200,
  "message": "Dynamic mode",
  "mode": 0
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Mode description
  - `"Dynamic mode"` for `idle` and `lockout` modes
  - `"Fixed mode"` for `dmr`, `p25`, and `nxdn` modes
- `mode`: Numeric mode value (0=IDLE, 1=LOCKOUT, 4=DMR, 5=P25, 6=NXDN)

**Error Responses:**

**Missing or Invalid Mode:**
```json
{
  "status": 400,
  "message": "password was not a valid string"
}
```
*Note: Implementation bug - error message incorrectly says "password" instead of "mode"*

**Invalid Mode Value:**
```json
{
  "status": 400,
  "message": "invalid mode"
}
```

**Protocol Not Enabled:**
```json
{
  "status": 503,
  "message": "DMR mode is not enabled"
}
```
*Similar messages for P25 and NXDN when attempting to set disabled protocols*

**Notes:**
- `"idle"` mode enables dynamic protocol switching based on received data
- Fixed modes (`dmr`, `p25`, `nxdn`) lock the modem to a single protocol
- `"lockout"` mode prevents all RF transmissions
- Attempting to set a fixed mode for a disabled protocol returns 503 Service Unavailable
- Mode strings are case-insensitive
- `idle` and `lockout` set `fixedMode` to false; protocol modes set it to true

---

### 4.2 Endpoint: PUT /mdm/kill

**Method:** `PUT`

**Description:** Request graceful shutdown of dvmhost.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "force": false
}
```

**Request Fields:**
- `force` (required, boolean): Shutdown mode
  - `false`: Graceful shutdown (allows cleanup, wait for transmissions to complete)
  - `true`: Forced shutdown (immediate termination)

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**Missing or Invalid Force Parameter:**
```json
{
  "status": 400,
  "message": "force was not a valid boolean"
}
```

**Implementation Behavior:**
- Sets global `g_killed` flag to `true` for graceful shutdown
- If `force=true`, also sets `HOST_STATE_QUIT` for immediate termination
- Both shutdown methods prevent new transmissions and stop RF processing
- Graceful shutdown allows completion of in-progress operations
- Forced shutdown terminates immediately without cleanup

**Notes:**
- *Implementation detail:* The function sets the success response before validating the `force` parameter, but validation still occurs and will return an error for invalid input
- Graceful shutdown (`force=false`) is recommended for normal operations
- Forced shutdown (`force=true`) should only be used when immediate termination is required
- After successful shutdown request, the process will terminate and no further API calls will be possible
- If the force parameter is not a valid boolean, a 400 error is returned despite the early success response initialization
- Use with caution in production environments

---

## 5. Trunking and Supervisory Control

### 5.1 Endpoint: PUT /set-supervisor

**Method:** `PUT`

**Description:** Enable or disable supervisory (trunking) mode for the host.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "state": 4,
  "enable": true
}
```

**Request Fields:**
- `state` (required, integer): Protocol state value
  - `4` = DMR
  - `5` = P25
  - `6` = NXDN
- `enable` (required, boolean): Enable (`true`) or disable (`false`) supervisory mode

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**Host Not Authoritative:**
```json
{
  "status": 400,
  "message": "Host is not authoritative, cannot set supervisory state"
}
```

**Invalid State Parameter:**
```json
{
  "status": 400,
  "message": "state was not a valid integer"
}
```

**Invalid Enable Parameter:**
```json
{
  "status": 400,
  "message": "enable was not a boolean"
}
```

**Invalid State Value:**
```json
{
  "status": 400,
  "message": "invalid mode"
}
```

**Protocol Not Enabled:**
```json
{
  "status": 503,
  "message": "DMR mode is not enabled"
}
```
*Similar messages for P25 and NXDN protocols*

**Notes:**
- Only available when host is configured as authoritative (`authoritative: true` in config)
- Supervisory mode enables trunking control features for the specified protocol
- The host must have the requested protocol enabled in its configuration
- Each protocol (DMR, P25, NXDN) has independent supervisory mode settings

---

### 5.2 Endpoint: PUT /permit-tg

**Method:** `PUT`

**Description:** Permit traffic on a specific talkgroup. Used by non-authoritative hosts to allow group calls on specified talkgroups.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body (DMR):**
```json
{
  "state": 4,
  "dstId": 1,
  "slot": 1
}
```

**Request Body (P25):**
```json
{
  "state": 5,
  "dstId": 1,
  "dataPermit": false
}
```

**Request Body (NXDN):**
```json
{
  "state": 6,
  "dstId": 1
}
```

**Request Fields:**
- `state` (required, integer): Protocol state value
  - `4` = DMR
  - `5` = P25
  - `6` = NXDN
- `dstId` (required, integer): Destination talkgroup ID to permit
- `slot` (required for DMR, integer): TDMA slot number
  - `1` = Slot 1
  - `2` = Slot 2
- `dataPermit` (optional for P25, boolean): Enable data permissions (default: `false`)

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**Host Is Authoritative:**
```json
{
  "status": 400,
  "message": "Host is authoritative, cannot permit TG"
}
```

**Invalid State Parameter:**
```json
{
  "status": 400,
  "message": "state was not a valid integer"
}
```

**Invalid Destination ID:**
```json
{
  "status": 400,
  "message": "destination ID was not a valid integer"
}
```

**Invalid Slot (DMR only):**
```json
{
  "status": 400,
  "message": "slot was not a valid integer"
}
```

**Illegal DMR Slot Value:**
```json
{
  "status": 400,
  "message": "illegal DMR slot"
}
```
*Returned when slot is 0 or greater than 2*

**Invalid State Value:**
```json
{
  "status": 400,
  "message": "invalid mode"
}
```

**Protocol Not Enabled:**
```json
{
  "status": 503,
  "message": "DMR mode is not enabled"
}
```
*Similar messages for P25 and NXDN protocols*

**Notes:**
- Only available when host is configured as non-authoritative (`authoritative: false` in config)
- Temporarily permits traffic on a talkgroup for the specified protocol
- Used in trunking systems to allow group calls
- DMR requires valid slot specification (1 or 2)
- P25 supports optional `dataPermit` flag for data call permissions
- NXDN only requires state and destination ID

---

### 5.3 Endpoint: PUT /grant-tg

**Method:** `PUT`

**Description:** Grant a voice channel for a specific talkgroup and source radio. Used by non-authoritative hosts or non-control-channel hosts to manually grant channel access.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body (DMR):**
```json
{
  "state": 4,
  "dstId": 1,
  "srcId": 123456,
  "slot": 1,
  "unitToUnit": false
}
```

**Request Body (P25):**
```json
{
  "state": 5,
  "dstId": 1,
  "srcId": 123456,
  "unitToUnit": false
}
```

**Request Body (NXDN):**
```json
{
  "state": 6,
  "dstId": 1,
  "srcId": 123456,
  "unitToUnit": false
}
```

**Request Fields:**
- `state` (required, integer): Protocol state value
  - `4` = DMR
  - `5` = P25
  - `6` = NXDN
- `dstId` (required, integer): Destination talkgroup ID (must not be 0)
- `srcId` (required, integer): Source radio ID requesting grant (must not be 0)
- `slot` (required for DMR, integer): TDMA slot number (1 or 2)
- `unitToUnit` (optional, boolean): Unit-to-unit call flag (default: `false`)
  - `false` = Group call (passed as `true` to grant function - inverted logic)
  - `true` = Unit-to-unit call (passed as `false` to grant function)

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**Host Is Authoritative Control Channel:**
```json
{
  "status": 400,
  "message": "Host is authoritative, cannot grant TG"
}
```
*Only returned when host is both authoritative AND configured as a control channel*

**Invalid State Parameter:**
```json
{
  "status": 400,
  "message": "state was not a valid integer"
}
```

**Invalid Destination ID:**
```json
{
  "status": 400,
  "message": "destination ID was not a valid integer"
}
```

**Illegal Destination TGID:**
```json
{
  "status": 400,
  "message": "destination ID is an illegal TGID"
}
```
*Returned when `dstId` is 0*

**Invalid Source ID:**
```json
{
  "status": 400,
  "message": "source ID was not a valid integer"
}
```

**Illegal Source ID:**
```json
{
  "status": 400,
  "message": "soruce ID is an illegal TGID"
}
```
*Note: Implementation typo - says "soruce" instead of "source". Returned when `srcId` is 0*

**Invalid Slot (DMR only):**
```json
{
  "status": 400,
  "message": "slot was not a valid integer"
}
```

**Illegal DMR Slot Value:**
```json
{
  "status": 400,
  "message": "illegal DMR slot"
}
```
*Returned when slot is 0 or greater than 2*

**Invalid State Value:**
```json
{
  "status": 400,
  "message": "invalid mode"
}
```

**Protocol Not Enabled:**
```json
{
  "status": 503,
  "message": "DMR mode is not enabled"
}
```
*Similar messages for P25 and NXDN protocols*

**Notes:**
- Available when host is non-authoritative OR when authoritative but not configured as a control channel
- Used in trunked radio systems to manually assign voice channel grants
- The `unitToUnit` parameter has inverted logic: the value is negated before being passed to the grant function
  - `unitToUnit: false` results in group call (`true` passed to grant function)
  - `unitToUnit: true` results in unit-to-unit call (`false` passed to grant function)
- DMR requires valid slot specification (1 or 2)
- Both `srcId` and `dstId` must be non-zero values
- **Implementation Bug**: Error message for invalid source ID contains typo "soruce ID" instead of "source ID"

---

### 5.4 Endpoint: GET /release-grants

**Method:** `GET`

**Description:** Release all active voice channel grants across all enabled protocols.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Implementation Behavior:**
- Calls `releaseGrant(0, true)` on affiliations for all enabled protocols (DMR, P25, NXDN)
- Releases all grants by passing talkgroup ID `0` with `true` flag
- Processes each protocol independently if enabled

**Notes:**
- Clears all active voice channel grants across the system
- Forces radios to re-request channel grants if they wish to transmit
- Useful for emergency channel clearing or system maintenance
- Only affects protocols that are enabled in the host configuration
- No error is returned if a protocol is not enabled; it is simply skipped

---

### 5.5 Endpoint: GET /release-affs

**Method:** `GET`

**Description:** Release all radio affiliations across all enabled protocols.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Implementation Behavior:**
- Calls `clearGroupAff(0, true)` on affiliations for all enabled protocols (DMR, P25, NXDN)
- Clears all group affiliations by passing talkgroup ID `0` with `true` flag
- Processes each protocol independently if enabled

**Notes:**
- Clears all radio-to-talkgroup affiliations across the system
- Forces radios to re-affiliate with their desired talkgroups
- Used for system maintenance, troubleshooting, or forcing re-registration
- Only affects protocols that are enabled in the host configuration
- No error is returned if a protocol is not enabled; it is simply skipped
- Different from `/release-grants` which releases active transmissions, this releases standing affiliations

---

## 6. Radio ID Lookup

### 6.1 Endpoint: GET /rid-whitelist/{rid}

**Method:** `GET`

**Description:** Toggle the whitelist status for a radio ID. This endpoint enables/whitelists the specified radio ID in the system.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**URL Parameters:**
- `rid` (required, numeric): Radio ID to whitelist/toggle

**Example URL:**
```
GET /rid-whitelist/123456
```

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**Invalid Arguments:**
```json
{
  "status": 400,
  "message": "invalid API call arguments"
}
```

**RID Zero Not Allowed:**
```json
{
  "status": 400,
  "message": "tried to whitelist RID 0"
}
```

**Implementation Behavior:**
- Calls `m_ridLookup->toggleEntry(srcId, true)` to enable/whitelist the radio ID
- RID value is extracted from URL path parameter
- RID 0 is explicitly rejected as invalid

**Notes:**
- This is a **toggle/enable** operation, not a query operation
- The endpoint name suggests "GET" but it actually modifies state by whitelisting the RID
- Use this to authorize a specific radio ID to access the system
- Does not return the current whitelist status; only confirms the operation succeeded
- RID must be non-zero (RID 0 is reserved and cannot be whitelisted)

---

### 6.2 Endpoint: GET /rid-blacklist/{rid}

**Method:** `GET`

**Description:** Toggle the blacklist status for a radio ID. This endpoint disables/blacklists the specified radio ID in the system.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**URL Parameters:**
- `rid` (required, numeric): Radio ID to blacklist/toggle

**Example URL:**
```
GET /rid-blacklist/123456
```

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**Invalid Arguments:**
```json
{
  "status": 400,
  "message": "invalid API call arguments"
}
```

**RID Zero Not Allowed:**
```json
{
  "status": 400,
  "message": "tried to blacklist RID 0"
}
```

**Implementation Behavior:**
- Calls `m_ridLookup->toggleEntry(srcId, false)` to disable/blacklist the radio ID
- RID value is extracted from URL path parameter
- RID 0 is explicitly rejected as invalid

**Notes:**
- This is a **toggle/disable** operation, not a query operation
- The endpoint name suggests "GET" but it actually modifies state by blacklisting the RID
- Use this to deny a specific radio ID access to the system
- Does not return the current blacklist status; only confirms the operation succeeded
- RID must be non-zero (RID 0 is reserved and cannot be blacklisted)
- Blacklisted radios are denied access to the system

---

## 7. DMR Protocol Endpoints

### 7.1 Endpoint: GET /dmr/beacon

**Method:** `GET`

**Description:** Fire a DMR beacon transmission.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**DMR Not Enabled:**
```json
{
  "status": 503,
  "message": "DMR mode is not enabled"
}
```

**Beacons Not Enabled:**
```json
{
  "status": 503,
  "message": "DMR beacons are not enabled"
}
```

**Implementation Behavior:**
- Sets global flag `g_fireDMRBeacon = true` to trigger beacon transmission
- Requires DMR mode enabled in configuration
- Requires DMR beacons enabled (`dmr.beacons.enable: true` in config)

**Notes:**
- Triggers immediate DMR beacon transmission on next opportunity
- Beacons must be enabled in host configuration
- Used for system identification, timing synchronization, and testing
- Returns success immediately; beacon fires asynchronously

---

### 7.2 Endpoint: GET /dmr/debug/{debug}/{verbose}

**Method:** `GET`

**Description:** Get or set DMR debug logging state. Functions as both query and setter based on URL parameters.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Query Mode (No URL Parameters):**

**Example URL:**
```
GET /dmr/debug
```

**Response (Query):**
```json
{
  "status": 200,
  "debug": true,
  "verbose": false
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `debug` (boolean): Current debug logging state
- `verbose` (boolean): Current verbose logging state

**Set Mode (With URL Parameters):**

**URL Parameters:**
- `debug` (required, numeric): Enable debug logging (`0` = disabled, `1` = enabled)
- `verbose` (required, numeric): Enable verbose logging (`0` = disabled, `1` = enabled)

**Example URL:**
```
GET /dmr/debug/1/1
```

**Response (Set):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**DMR Not Enabled:**
```json
{
  "status": 503,
  "message": "DMR mode is not enabled"
}
```

**Implementation Behavior:**
- If `match.size() <= 1`: Query mode - returns current debug/verbose states
- If `match.size() == 3`: Set mode - updates debug and verbose flags
- Parameters extracted from URL path using regex: `/dmr/debug/(\\d+)/(\\d+)`
- Values converted: `1` → `true`, anything else → `false`
- Calls `m_dmr->setDebugVerbose(debug, verbose)` to apply changes

**Notes:**
- Dual-purpose endpoint: query without parameters, set with parameters
- `debug` enables standard debug logging for DMR operations
- `verbose` enables very detailed logging (can be overwhelming)
- Changes apply immediately without restart
- Both parameters must be provided together in set mode

---

### 7.3 Endpoint: GET /dmr/dump-csbk/{enable}

**Method:** `GET`

**Description:** Get or set DMR CSBK (Control Signaling Block) packet dumping. Functions as both query and setter based on URL parameters.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Query Mode (No URL Parameter):**

**Example URL:**
```
GET /dmr/dump-csbk
```

**Response (Query):**
```json
{
  "status": 200,
  "verbose": true
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `verbose` (boolean): Current CSBK dump state

**Set Mode (With URL Parameter):**

**URL Parameters:**
- `enable` (required, numeric): Enable CSBK dumping (`0` = disabled, `1` = enabled)

**Example URL:**
```
GET /dmr/dump-csbk/1
```

**Response (Set):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**DMR Not Enabled:**
```json
{
  "status": 503,
  "message": "DMR mode is not enabled"
}
```

**Implementation Behavior:**
- If `match.size() <= 1`: Query mode - returns current CSBK verbose state
- If `match.size() == 2`: Set mode - updates CSBK verbose flag
- Parameter extracted from URL path using regex: `/dmr/dump-csbk/(\\d+)`
- Value converted: `1` → `true`, anything else → `false`
- Calls `m_dmr->setCSBKVerbose(enable)` to apply changes

**Notes:**
- Dual-purpose endpoint: query without parameter, set with parameter
- Query mode returns current CSBK dump state
- Set mode enables/disables CSBK packet logging to console
- CSBK packets contain control channel signaling information
- Useful for troubleshooting trunking and control channel issues
- Changes apply immediately without restart

---

### 7.4 Endpoint: PUT /dmr/rid

**Method:** `PUT`

**Description:** Execute DMR-specific radio ID operations (page, check, inhibit, uninhibit).

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "command": "check",
  "dstId": 123456,
  "slot": 1
}
```

**Request Fields:**
- `command` (required, string): Command to execute
  - `"page"`: Radio Page (Call Alert)
  - `"check"`: Radio Check
  - `"inhibit"`: Radio Inhibit (disable radio)
  - `"uninhibit"`: Radio Un-inhibit (enable radio)
- `dstId` (required, integer): Target radio ID (must not be 0)
- `slot` (required, integer): TDMA slot number (1 or 2)

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**DMR Not Enabled:**
```json
{
  "status": 503,
  "message": "DMR mode is not enabled"
}
```

**Invalid Command:**
```json
{
  "status": 400,
  "message": "command was not valid"
}
```

**Invalid Destination ID Type:**
```json
{
  "status": 400,
  "message": "destination ID was not valid"
}
```

**Zero Destination ID:**
```json
{
  "status": 400,
  "message": "destination ID was not valid"
}
```

**Invalid Slot:**
```json
{
  "status": 400,
  "message": "slot was not valid"
}
```

**Invalid Slot Number:**
```json
{
  "status": 400,
  "message": "invalid DMR slot number (slot == 0 or slot > 3)"
}
```

**Unknown Command:**
```json
{
  "status": 400,
  "message": "invalid command"
}
```

**Implementation Behavior:**
- Commands are case-insensitive (converted to lowercase)
- Validates slot is not 0 and is less than 3 (must be 1 or 2)
- `"page"`: Calls `writeRF_Call_Alrt(slot, WUID_ALL, dstId)` - sends call alert
- `"check"`: Calls `writeRF_Ext_Func(slot, CHECK, WUID_ALL, dstId)` - sends radio check request
- `"inhibit"`: Calls `writeRF_Ext_Func(slot, INHIBIT, WUID_STUNI, dstId)` - uses STUN Individual addressing
- `"uninhibit"`: Calls `writeRF_Ext_Func(slot, UNINHIBIT, WUID_STUNI, dstId)` - removes inhibit state

**Notes:**
- Commands sent over DMR control channel to target radio
- Slot must be 1 or 2 (DMR TDMA slots)
- Slot validation correctly rejects 0 and values >= 3
- Target radio must be registered on the system
- `inhibit`/`uninhibit` use STUNI (stun individual) addressing mode
- `page` and `check` use WUID_ALL (all call) addressing mode
- Commands execute immediately and return success before RF transmission completes

---

### 7.5 Endpoint: GET /dmr/cc-enable

**Method:** `GET`

**Description:** Toggle DMR control channel (CC) dedicated mode enable state.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Response (Success):**
```json
{
  "status": 200,
  "message": "DMR CC is enabled"
}
```
*or*
```json
{
  "status": 200,
  "message": "DMR CC is disabled"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Current state after toggle

**Error Responses:**

**DMR Not Enabled:**
```json
{
  "status": 503,
  "message": "DMR mode is not enabled"
}
```

**TSCC Data Not Enabled:**
```json
{
  "status": 400,
  "message": "DMR control data is not enabled!"
}
```

**P25 Enabled (Conflict):**
```json
{
  "status": 400,
  "message": "Can't enable DMR control channel while P25 is enabled!"
}
```

**NXDN Enabled (Conflict):**
```json
{
  "status": 400,
  "message": "Can't enable DMR control channel while NXDN is enabled!"
}
```

**Implementation Behavior:**
- Toggles `m_host->m_dmrCtrlChannel` flag (true ↔ false)
- Requires `m_host->m_dmrTSCCData` to be enabled (TSCC control data)
- Prevents enabling if P25 or NXDN protocols are active
- Returns current state after toggle in message
- Response message correctly reflects DMR CC state

**Notes:**
- This is a **toggle** operation, not a query (repeatedly calling switches state)
- Toggles DMR dedicated control channel on/off
- Cannot enable DMR CC while P25 or NXDN is enabled (protocols are mutually exclusive for CC)
- Requires TSCC (Two-Slot Control Channel) data configuration in host config
- Control channel handles trunking signaling and system management

---

### 7.6 Endpoint: GET /dmr/cc-broadcast

**Method:** `GET`

**Description:** Toggle DMR control channel broadcast mode (TSCC data transmission).

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Response (Success):**
```json
{
  "status": 200,
  "message": "DMR CC broadcast is enabled"
}
```
*or*
```json
{
  "status": 200,
  "message": "DMR CC broadcast is disabled"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Current state after toggle

**Error Responses:**

**DMR Not Enabled:**
```json
{
  "status": 503,
  "message": "DMR mode is not enabled"
}
```

**Implementation Behavior:**
- Toggles `m_host->m_dmrTSCCData` flag (true ↔ false)
- Controls whether TSCC (Two-Slot Control Channel) data is broadcast
- Returns current state after toggle in message

**Notes:**
- This is a **toggle** operation, not a query (repeatedly calling switches state)
- Toggles broadcast mode for DMR control channel data
- Affects how TSCC (Two-Slot Control Channel) data beacons are transmitted
- TSCC data includes system information, adjacent sites, and trunking parameters
- Can be toggled independently of the dedicated control channel enable state

---

### 7.7 Endpoint: GET /dmr/report-affiliations

**Method:** `GET`

**Description:** Get current DMR radio group affiliations.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Response (Success):**
```json
{
  "status": 200,
  "affiliations": [
    {
      "srcId": 123456,
      "grpId": 1
    },
    {
      "srcId": 234567,
      "grpId": 2
    }
  ]
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `affiliations` (array): List of current affiliations
  - `srcId` (integer): Radio ID (subscriber unit)
  - `grpId` (integer): Talkgroup ID the radio is affiliated to

**Implementation Behavior:**
- Retrieves affiliation table from `m_dmr->affiliations()->grpAffTable()`
- Returns `std::unordered_map<uint32_t, uint32_t>` as JSON array
- Map key is `srcId` (radio ID), value is `grpId` (talkgroup ID)
- Returns empty array if no affiliations exist

**Notes:**
- Returns all current DMR group affiliations in the system
- Useful for monitoring which radios are affiliated to which talkgroups
- Does not include slot information (unlike what previous documentation suggested)
- Affiliations persist until radio de-affiliates or system timeout
- Empty affiliations array returned if no radios are currently affiliated

---

## 8. P25 Protocol Endpoints

### 8.1 Endpoint: GET /p25/cc

**Method:** `GET`

**Description:** Fire a P25 control channel transmission.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**P25 Not Enabled:**
```json
{
  "status": 503,
  "message": "P25 mode is not enabled"
}
```

**P25 Control Data Not Enabled:**
```json
{
  "status": 503,
  "message": "P25 control data is not enabled"
}
```

**Implementation Behavior:**
- Sets global flag `g_fireP25Control = true` to trigger control channel transmission
- Requires P25 mode enabled in configuration
- Requires P25 control data enabled (`p25.control.enable: true` in config)

**Notes:**
- Triggers immediate P25 control channel burst on next opportunity
- Requires P25 control channel configuration
- Used for testing, system identification, and control channel synchronization
- Returns success immediately; control burst fires asynchronously

---

### 8.2 Endpoint: GET /p25/debug/{debug}/{verbose}

**Method:** `GET`

**Description:** Get or set P25 debug logging state. Functions as both query and setter based on URL parameters.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Query Mode (No URL Parameters):**

**Example URL:**
```
GET /p25/debug
```

**Response (Query):**
```json
{
  "status": 200,
  "debug": true,
  "verbose": false
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `debug` (boolean): Current debug logging state
- `verbose` (boolean): Current verbose logging state

**Set Mode (With URL Parameters):**

**URL Parameters:**
- `debug` (required, numeric): Enable debug logging (`0` = disabled, `1` = enabled)
- `verbose` (required, numeric): Enable verbose logging (`0` = disabled, `1` = enabled)

**Example URL:**
```
GET /p25/debug/1/0
```

**Response (Set):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**P25 Not Enabled:**
```json
{
  "status": 503,
  "message": "P25 mode is not enabled"
}
```

**Implementation Behavior:**
- If `match.size() <= 1`: Query mode - returns current debug/verbose states
- If `match.size() == 3`: Set mode - updates debug and verbose flags
- Parameters extracted from URL path using regex: `/p25/debug/(\\d+)/(\\d+)`
- Values converted: `1` → `true`, anything else → `false`
- Calls `m_p25->setDebugVerbose(debug, verbose)` to apply changes
- Correctly checks `if (m_p25 != nullptr)` before accessing P25 object

**Notes:**
- Dual-purpose endpoint: query without parameters, set with parameters
- Same behavior pattern as DMR debug endpoint
- `debug` enables standard debug logging for P25 operations
- `verbose` enables very detailed logging (can be overwhelming)
- Changes apply immediately without restart
- Both parameters must be provided together in set mode

---

### 8.3 Endpoint: GET /p25/dump-tsbk/{enable}

**Method:** `GET`

**Description:** Get or set P25 TSBK (Trunking Signaling Block) packet dumping. Functions as both query and setter based on URL parameters.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Query Mode (No URL Parameter):**

**Example URL:**
```
GET /p25/dump-tsbk
```

**Response (Query):**
```json
{
  "status": 200,
  "verbose": true
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `verbose` (boolean): Current TSBK dump state

**Set Mode (With URL Parameter):**

**URL Parameters:**
- `enable` (required, numeric): Enable TSBK dumping (`0` = disabled, `1` = enabled)

**Example URL:**
```
GET /p25/dump-tsbk/1
```

**Response (Set):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**P25 Not Enabled:**
```json
{
  "status": 503,
  "message": "P25 mode is not enabled"
}
```

**Implementation Behavior:**
- If `match.size() <= 1`: Query mode - returns current TSBK verbose state
- If `match.size() == 2`: Set mode - updates TSBK verbose flag
- Parameter extracted from URL path using regex: `/p25/dump-tsbk/(\\d+)`
- Value converted: `1` → `true`, anything else → `false`
- Calls `m_p25->control()->setTSBKVerbose(enable)` to apply changes

**Notes:**
- Dual-purpose endpoint: query without parameter, set with parameter
- Query mode returns current TSBK dump state
- Set mode enables/disables TSBK packet logging to console
- TSBK packets contain P25 trunking signaling information
- Useful for troubleshooting P25 trunking and control channel issues
- Changes apply immediately without restart

---

### 8.4 Endpoint: PUT /p25/rid

**Method:** `PUT`

**Description:** Execute P25-specific radio ID operations including paging, radio checks, inhibit/uninhibit, dynamic regrouping, and emergency alarms.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body (Basic Commands):**
```json
{
  "command": "check",
  "dstId": 123456
}
```

**Request Body (Set MFID):**
```json
{
  "command": "p25-setmfid",
  "mfId": 144
}
```

**Request Body (Dynamic Regroup):**
```json
{
  "command": "dyn-regrp",
  "dstId": 123456,
  "tgId": 5000
}
```

**Request Body (Emergency Alarm):**
```json
{
  "command": "emerg",
  "dstId": 5000,
  "srcId": 123456
}
```

**Supported Commands:**
- `"p25-setmfid"`: Set manufacturer ID (no dstId required)
- `"page"`: Send radio page (call alert)
- `"check"`: Radio check request
- `"inhibit"`: Radio inhibit (disable radio)
- `"uninhibit"`: Radio un-inhibit (enable radio)
- `"dyn-regrp"`: Dynamic regroup request
- `"dyn-regrp-cancel"`: Cancel dynamic regroup
- `"dyn-regrp-lock"`: Lock dynamic regroup
- `"dyn-regrp-unlock"`: Unlock dynamic regroup
- `"group-aff-req"`: Group affiliation query (GAQ)
- `"unit-reg"`: Unit registration command (U_REG)
- `"emerg"`: Emergency alarm

**Request Fields:**
- `command` (required, string): Command to execute (see above)
- `dstId` (required for most commands, integer): Target radio ID (must not be 0)
  - **Not required for**: `"p25-setmfid"`
- `mfId` (required for `p25-setmfid`, integer): Manufacturer ID (uint8_t)
- `tgId` (required for `dyn-regrp`, integer): Target talkgroup ID
- `srcId` (required for `emerg`, integer): Source radio ID (must not be 0)

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**P25 Not Enabled:**
```json
{
  "status": 503,
  "message": "P25 mode is not enabled"
}
```

**Invalid Command:**
```json
{
  "status": 400,
  "message": "command was not valid"
}
```

**Invalid Destination ID:**
```json
{
  "status": 400,
  "message": "destination ID was not valid"
}
```

**Invalid MFID:**
```json
{
  "status": 400,
  "message": "MFID was not valid"
}
```

**Invalid Talkgroup ID:**
```json
{
  "status": 400,
  "message": "talkgroup ID was not valid"
}
```

**Invalid Source ID:**
```json
{
  "status": 400,
  "message": "source ID was not valid"
}
```

**Unknown Command:**
```json
{
  "status": 400,
  "message": "invalid command"
}
```

**Implementation Behavior:**
- Commands are case-insensitive (converted to lowercase)
- Most commands use WUID_FNE (FNE unit ID) addressing
- Command implementations:
  * `"p25-setmfid"`: Calls `control()->setLastMFId(mfId)` - no RF transmission
  * `"page"`: Calls `writeRF_TSDU_Call_Alrt(WUID_FNE, dstId)`
  * `"check"`: Calls `writeRF_TSDU_Ext_Func(CHECK, WUID_FNE, dstId)`
  * `"inhibit"`: Calls `writeRF_TSDU_Ext_Func(INHIBIT, WUID_FNE, dstId)`
  * `"uninhibit"`: Calls `writeRF_TSDU_Ext_Func(UNINHIBIT, WUID_FNE, dstId)`
  * `"dyn-regrp"`: Calls `writeRF_TSDU_Ext_Func(DYN_REGRP_REQ, tgId, dstId)`
  * `"dyn-regrp-cancel"`: Calls `writeRF_TSDU_Ext_Func(DYN_REGRP_CANCEL, 0, dstId)`
  * `"dyn-regrp-lock"`: Calls `writeRF_TSDU_Ext_Func(DYN_REGRP_LOCK, 0, dstId)`
  * `"dyn-regrp-unlock"`: Calls `writeRF_TSDU_Ext_Func(DYN_REGRP_UNLOCK, 0, dstId)`
  * `"group-aff-req"`: Calls `writeRF_TSDU_Grp_Aff_Q(dstId)` - GAQ message
  * `"unit-reg"`: Calls `writeRF_TSDU_U_Reg_Cmd(dstId)` - U_REG message
  * `"emerg"`: Calls `writeRF_TSDU_Emerg_Alrm(srcId, dstId)`

**Notes:**
- Commands sent via P25 TSBK (Trunking Signaling Block) messages
- Target radio must be registered on the system
- Dynamic regroup allows temporary talkgroup assignments for incident response
- Manufacturer ID (MFID) affects radio behavior and feature availability
- Emergency alarm sends alarm from srcId to dstId (dstId is typically a talkgroup)
- Commands execute immediately and return success before RF transmission completes
- WUID_FNE is the Fixed Network Equipment unit ID used for system commands

---

### 8.5 Endpoint: GET /p25/cc-enable

**Method:** `GET`

**Description:** Toggle P25 control channel (CC) dedicated mode enable state.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Response (Success):**
```json
{
  "status": 200,
  "message": "P25 CC is enabled"
}
```
*or*
```json
{
  "status": 200,
  "message": "P25 CC is disabled"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Current state after toggle

**Error Responses:**

**P25 Not Enabled:**
```json
{
  "status": 503,
  "message": "P25 mode is not enabled"
}
```

**P25 Control Data Not Enabled:**
```json
{
  "status": 400,
  "message": "P25 control data is not enabled!"
}
```

**DMR Enabled (Conflict):**
```json
{
  "status": 400,
  "message": "Can't enable P25 control channel while DMR is enabled!"
}
```

**NXDN Enabled (Conflict):**
```json
{
  "status": 400,
  "message": "Can't enable P25 control channel while NXDN is enabled!"
}
```

**Implementation Behavior:**
- Toggles `m_host->m_p25CtrlChannel` flag (true ↔ false)
- Sets `m_host->m_p25CtrlBroadcast = true` when enabling
- Sets `g_fireP25Control = true` to trigger control burst
- Calls `m_p25->setCCHalted(false)` to resume control channel
- Requires `m_host->m_p25CCData` to be enabled
- Prevents enabling if DMR or NXDN protocols are active
- Returns current state after toggle in message

**Notes:**
- This is a **toggle** operation, not a query (repeatedly calling switches state)
- Toggles P25 dedicated control channel on/off
- Cannot enable P25 CC while DMR or NXDN is enabled (protocols are mutually exclusive for CC)
- Requires P25 control channel data configuration in host config
- Control channel handles trunking signaling and system management
- Automatically enables broadcast mode when enabling control channel

---

### 8.6 Endpoint: GET /p25/cc-broadcast

**Method:** `GET`

**Description:** Toggle P25 control channel broadcast mode.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Response (Success):**
```json
{
  "status": 200,
  "message": "P25 CC broadcast is enabled"
}
```
*or*
```json
{
  "status": 200,
  "message": "P25 CC broadcast is disabled"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Current state after toggle

**Error Responses:**

**P25 Not Enabled:**
```json
{
  "status": 503,
  "message": "P25 mode is not enabled"
}
```

**P25 Control Data Not Enabled:**
```json
{
  "status": 400,
  "message": "P25 control data is not enabled!"
}
```

**Implementation Behavior:**
- Toggles `m_host->m_p25CtrlBroadcast` flag (true ↔ false)
- If disabling broadcast:
  * Sets `g_fireP25Control = false` to stop control bursts
  * Calls `m_p25->setCCHalted(true)` to halt control channel
- If enabling broadcast:
  * Sets `g_fireP25Control = true` to start control bursts
  * Calls `m_p25->setCCHalted(false)` to resume control channel
- Returns current state after toggle in message

**Notes:**
- This is a **toggle** operation, not a query (repeatedly calling switches state)
- Toggles broadcast mode for P25 control channel
- Affects P25 control channel beacon transmission
- Requires P25 control data enabled in configuration
- Can be toggled independently but requires CC data configuration
- Halting broadcast stops control channel transmissions while keeping CC enabled

---

### 8.7 Endpoint: PUT /p25/raw-tsbk

**Method:** `PUT`

**Description:** Transmit a raw P25 TSBK (Trunking Signaling Block) packet. Allows sending custom TSBK messages for advanced control.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
Content-Type: application/json
```

**Request Body:**
```json
{
  "tsbk": "00112233445566778899AABB"
}
```

**Request Fields:**
- `tsbk` (required, string): Raw TSBK data as hexadecimal string (must be exactly 24 characters / 12 bytes)

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**P25 Not Enabled:**
```json
{
  "status": 503,
  "message": "P25 mode is not enabled"
}
```

**Invalid TSBK Field:**
```json
{
  "status": 400,
  "message": "tsbk was not valid"
}
```

**Invalid TSBK Length:**
```json
{
  "status": 400,
  "message": "TSBK must be 24 characters in length"
}
```

**Invalid TSBK Characters:**
```json
{
  "status": 400,
  "message": "TSBK contains invalid characters"
}
```

**Implementation Behavior:**
- Validates TSBK string is exactly 24 hex characters (12 bytes)
- Validates all characters are hexadecimal (0-9, a-f, A-F)
- Converts hex string to byte array (P25_TSBK_LENGTH_BYTES = 12)
- Calls `m_p25->control()->writeRF_TSDU_Raw(tsbk)` to transmit
- If debug enabled, dumps raw TSBK bytes to log

**Notes:**
- **Advanced feature**: Requires knowledge of P25 TSBK packet structure
- TSBK must be properly formatted according to P25 specification
- No validation of TSBK content (opcode, manufacturer ID, etc.)
- Used for testing, custom signaling, or implementing unsupported TSBK types
- Transmitted directly without additional processing
- Incorrect TSBK data may cause radio misbehavior or system issues
- Advanced feature for custom P25 control messaging
- TSBK data must be valid hexadecimal
- Use with caution - invalid TSBK can disrupt system

---

### 8.8 Endpoint: GET /p25/report-affiliations

**Method:** `GET`

**Description:** Retrieve current P25 radio affiliations (which radios are affiliated to which talkgroups).

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK",
  "affiliations": [
    {
      "srcId": 123456,
      "grpId": 1
    },
    {
      "srcId": 234567,
      "grpId": 2
    }
  ]
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`
- `affiliations`: Array of affiliation objects
  - `srcId` (uint32): Radio ID (subscriber unit)
  - `grpId` (uint32): Talkgroup ID the radio is affiliated to

**Empty Affiliations Response:**
```json
{
  "status": 200,
  "message": "OK",
  "affiliations": []
}
```

**Implementation Behavior:**
- Retrieves affiliation table from `m_p25->affiliations()->grpAffTable()`
- Returns map of srcId → grpId associations
- Empty array if no affiliations exist
- No pagination (returns all affiliations)

**Notes:**
- Shows current state of P25 group affiliations
- P25 does not use TDMA slots (unlike DMR)
- Radios must affiliate before they can participate in talkgroups
- Affiliations can change dynamically as radios join/leave talkgroups
- Used for monitoring system state and troubleshooting
- Similar to DMR affiliations but without slot information

---

## 9. NXDN Protocol Endpoints

### 9.1 Endpoint: GET /nxdn/cc

**Method:** `GET`

**Description:** Fire an NXDN control channel transmission. Triggers an immediate control channel burst on the configured control channel.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**NXDN Not Enabled:**
```json
{
  "status": 503,
  "message": "NXDN mode is not enabled"
}
```

**NXDN Control Data Not Configured:**
```json
{
  "status": 503,
  "message": "NXDN control data is not enabled"
}
```

**Implementation Behavior:**
- Checks if `m_nxdn != nullptr` (NXDN mode enabled)
- Checks if `m_host->m_nxdnCCData` (control data configured)
- Sets `g_fireNXDNControl = true` to trigger control channel transmission
- Control channel burst fires on next opportunity

**Notes:**
- Triggers immediate NXDN control channel burst
- Requires NXDN mode enabled in configuration
- Requires NXDN control channel data configured
- Used for manual control channel testing or forcing system announcements
- Control burst contains site parameters, adjacent site information, etc.

---

### 9.2 Endpoint: GET /nxdn/debug/{debug}/{verbose}

**Method:** `GET`

**Description:** Get or set NXDN debug logging state. Functions as both query and setter based on URL parameters.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Query Mode (No URL Parameters):**

**Example URL:**
```
GET /nxdn/debug
```

**Response (Query):**
```json
{
  "status": 200,
  "debug": true,
  "verbose": false
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `debug` (boolean): Current debug logging state
- `verbose` (boolean): Current verbose logging state

**Set Mode (With URL Parameters):**

**URL Parameters:**
- `debug` (required, numeric): Enable debug logging (`0` = disabled, `1` = enabled)
- `verbose` (required, numeric): Enable verbose logging (`0` = disabled, `1` = enabled)

**Example URL:**
```
GET /nxdn/debug/1/0
```

**Response (Set):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**NXDN Not Enabled:**
```json
{
  "status": 503,
  "message": "NXDN mode is not enabled"
}
```

**Implementation Behavior:**
- If `match.size() <= 1`: Query mode - returns current debug/verbose states
- If `match.size() == 3`: Set mode - updates debug and verbose flags
- Parameters extracted from URL path using regex: `/nxdn/debug/(\\d+)/(\\d+)`
- Values converted: `1` → `true`, anything else → `false`
- Calls `m_nxdn->setDebugVerbose(debug, verbose)` to apply changes
- Correctly checks `if (m_nxdn != nullptr)` before accessing NXDN object

**Notes:**
- Dual-purpose endpoint: query without parameters, set with parameters
- Same behavior pattern as DMR/P25 debug endpoints
- `debug` enables standard debug logging for NXDN operations
- `verbose` enables very detailed logging (can be overwhelming)
- Changes apply immediately without restart
- Both parameters must be provided together in set mode

---

### 9.3 Endpoint: GET /nxdn/dump-rcch/{enable}

**Method:** `GET`

**Description:** Get or set NXDN RCCH (Radio Control Channel) packet dumping. Functions as both query and setter based on URL parameters.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Query Mode (No URL Parameters):**

**Example URL:**
```
GET /nxdn/dump-rcch
```

**Response (Query):**
```json
{
  "status": 200,
  "verbose": true
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `verbose` (boolean): Current RCCH verbose dump state

**Set Mode (With URL Parameter):**

**URL Parameter:**
- `enable` (required, numeric): Enable RCCH dumping (`0` = disabled, `1` = enabled)

**Example URL:**
```
GET /nxdn/dump-rcch/1
```

**Response (Set):**
```json
{
  "status": 200,
  "message": "OK"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`

**Error Responses:**

**NXDN Not Enabled:**
```json
{
  "status": 503,
  "message": "NXDN mode is not enabled"
}
```

**Implementation Behavior:**
- If `match.size() <= 1`: Query mode - returns current RCCH verbose state
- If `match.size() == 2`: Set mode - updates RCCH verbose flag
- Parameter extracted from URL path using regex: `/nxdn/dump-rcch/(\\d+)`
- Value converted: `1` → `true`, anything else → `false`
- Calls `m_nxdn->setRCCHVerbose(enable)` to apply change
- Correctly checks `if (m_nxdn != nullptr)` before accessing NXDN object

**Notes:**
- Dual-purpose endpoint: query without parameter, set with parameter
- Similar pattern to DMR/P25 CSBK/TSBK dump endpoints
- RCCH = Radio Control Channel (NXDN's control signaling)
- Verbose mode dumps RCCH packets to log for debugging
- Changes apply immediately without restart
- Can generate significant log output when enabled

---

### 9.4 Endpoint: GET /nxdn/cc-enable

**Method:** `GET`

**Description:** Toggle NXDN control channel (CC) enable state. Switches between dedicated control channel enabled and disabled.

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Response (Success):**
```json
{
  "status": 200,
  "message": "NXDN CC is enabled"
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Dynamic message indicating new CC state (`"NXDN CC is enabled"` or `"NXDN CC is disabled"`)

**Error Responses:**

**NXDN Not Enabled:**
```json
{
  "status": 503,
  "message": "NXDN mode is not enabled"
}
```

**NXDN Control Data Not Configured:**
```json
{
  "status": 400,
  "message": "NXDN control data is not enabled!"
}
```

**DMR Protocol Conflict:**
```json
{
  "status": 400,
  "message": "Can't enable NXDN control channel while DMR is enabled!"
}
```

**P25 Protocol Conflict:**
```json
{
  "status": 400,
  "message": "Can't enable NXDN control channel while P25 is enabled!"
}
```

**Implementation Behavior:**
- Checks if `m_nxdn != nullptr` (NXDN mode enabled)
- Checks if `m_host->m_nxdnCCData` (control data configured)
- Checks for protocol conflicts with DMR (`m_dmr != nullptr`)
- Checks for protocol conflicts with P25 (`m_p25 != nullptr`)
- Toggles `m_host->m_nxdnCtrlChannel` flag (current state → opposite state)
- Sets `m_host->m_nxdnCtrlBroadcast = true` (enables broadcast mode)
- Sets `g_fireNXDNControl = true` (triggers control channel transmission)
- Calls `m_nxdn->setCCHalted(false)` (ensures CC is not halted)

**Notes:**
- Toggles NXDN dedicated control channel on/off
- Cannot enable NXDN CC while DMR or P25 is enabled (protocol conflict)
- When enabling: Sets broadcast mode and fires control channel
- When disabling: Turns off control channel operation
- Automatically un-halts control channel when toggling
- Used for switching between traffic and control channel modes
- Similar behavior to P25 cc-enable but without broadcast toggle option

---

### 9.5 Endpoint: GET /nxdn/report-affiliations

**Method:** `GET`

**Description:** Retrieve current NXDN radio affiliations (which radios are affiliated to which talkgroups).

**Request Headers:**
```
X-DVM-Auth-Token: {token}
```

**Request Body:** None

**Response (Success):**
```json
{
  "status": 200,
  "message": "OK",
  "affiliations": [
    {
      "srcId": 123456,
      "grpId": 1
    },
    {
      "srcId": 234567,
      "grpId": 2
    }
  ]
}
```

**Response Fields:**
- `status`: HTTP status code (200)
- `message`: Always `"OK"`
- `affiliations`: Array of affiliation objects
  - `srcId` (uint32): Radio ID (subscriber unit)
  - `grpId` (uint32): Talkgroup ID the radio is affiliated to

**Empty Affiliations Response:**
```json
{
  "status": 200,
  "message": "OK",
  "affiliations": []
}
```

**Implementation Behavior:**
- Retrieves affiliation table from `m_nxdn->affiliations()->grpAffTable()`
- Returns map of srcId → grpId associations
- Empty array if no affiliations exist
- No pagination (returns all affiliations)

**Notes:**
- Shows current state of NXDN group affiliations
- NXDN does not use TDMA slots (like P25, unlike DMR)
- Radios must affiliate before they can participate in talkgroups
- Affiliations can change dynamically as radios join/leave talkgroups
- Used for monitoring system state and troubleshooting
- Similar to DMR/P25 affiliations but without slot information
- NXDN uses FDMA (Frequency Division Multiple Access)

---

## 10. Response Formats

### 10.1 Standard Success Response

All successful API calls return HTTP 200 with a JSON object containing at minimum:

```json
{
  "status": 200,
  "message": "OK"
}
```

Many endpoints omit the `message` field and only include `status`. Additional fields are added based on the endpoint's specific functionality.

### 10.2 Query Response Formats

**Single Value Response:**
```json
{
  "status": 200,
  "message": "OK",
  "value": true
}
```

**Multiple Values Response:**
```json
{
  "status": 200,
  "message": "OK",
  "debug": true,
  "verbose": false
}
```

**Array Response:**
```json
{
  "status": 200,
  "message": "OK",
  "affiliations": [
    {"srcId": 123456, "grpId": 1, "slot": 1}
  ]
}
```

**Complex Object Response:**
```json
{
  "status": 200,
  "message": "OK",
  "state": 5,
  "dmrEnabled": true,
  "p25Enabled": true,
  "nxdnEnabled": false,
  "fixedMode": true
}
```

### 10.3 Standard Error Response

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
- `404 Not Found`: Endpoint does not exist
- `405 Method Not Allowed`: Wrong HTTP method for endpoint
- `500 Internal Server Error`: Server-side error
- `503 Service Unavailable`: Requested service/protocol not enabled

### 10.4 Protocol-Specific Responses

**DMR Responses:**
- Include `slot` field (1 or 2) where applicable
- TDMA slot-based operations
- CSBK verbose state for dump endpoints

**P25 Responses:**
- No slot field (FDMA only)
- TSBK verbose state for dump endpoints
- Emergency flag for certain operations
- Dynamic regrouping status

**NXDN Responses:**
- No slot field (FDMA only)
- RCCH verbose state for dump endpoints
- Simplified control channel management

### 10.5 Toggle Endpoint Response Pattern

Toggle endpoints (cc-enable, cc-broadcast) return dynamic messages:

```json
{
  "status": 200,
  "message": "DMR CC is enabled"
}
```

or

```json
{
  "status": 200,
  "message": "DMR CC is disabled"
}
```

The message reflects the **new state** after toggling, not the previous state.

---

## 11. Error Handling

### 11.1 Authentication Errors

**Missing Token:**
```json
{
  "status": 401,
  "message": "no authentication token"
}
```

**Invalid Token:**
```json
{
  "status": 401,
  "message": "invalid authentication token"
}
```

**Illegal Token:**
```json
{
  "status": 401,
  "message": "illegal authentication token"
}
```

**Authentication Failed (Wrong Password):**
```json
{
  "status": 401,
  "message": "authentication failed"
}
```

### 11.2 Validation Errors

**Invalid JSON:**
```json
{
  "status": 400,
  "message": "JSON parse error: unexpected character"
}
```

**Invalid Content-Type:**
```json
{
  "status": 400,
  "message": "invalid content-type (must be application/json)"
}
```

**Missing Required Field:**
```json
{
  "status": 400,
  "message": "field 'dstId' is required"
}
```

**Invalid Field Value:**
```json
{
  "status": 400,
  "message": "dstId was not valid"
}
```

**Invalid Command:**
```json
{
  "status": 400,
  "message": "unknown command specified"
}
```

**Invalid Hex String (P25 raw-tsbk):**
```json
{
  "status": 400,
  "message": "TSBK must be 24 characters in length"
}
```

or

```json
{
  "status": 400,
  "message": "TSBK contains invalid characters"
}
```

### 11.3 Service Errors

**Protocol Not Enabled:**
```json
{
  "status": 503,
  "message": "DMR mode is not enabled"
}
```

```json
{
  "status": 503,
  "message": "P25 mode is not enabled"
}
```

```json
{
  "status": 503,
  "message": "NXDN mode is not enabled"
}
```

**Feature Not Configured:**
```json
{
  "status": 503,
  "message": "DMR beacons are not enabled"
}
```

```json
{
  "status": 503,
  "message": "DMR control data is not enabled"
}
```

```json
{
  "status": 503,
  "message": "P25 control data is not enabled"
}
```

```json
{
  "status": 503,
  "message": "NXDN control data is not enabled"
}
```

**Unauthorized Operation:**
```json
{
  "status": 400,
  "message": "Host is not authoritative, cannot set supervisory state"
}
```

**Protocol Conflicts:**
```json
{
  "status": 400,
  "message": "Can't enable DMR control channel while P25 is enabled!"
}
```

```json
{
  "status": 400,
  "message": "Can't enable P25 control channel while DMR is enabled!"
}
```

```json
{
  "status": 400,
  "message": "Can't enable NXDN control channel while DMR is enabled!"
}
```

### 11.4 Parameter-Specific Errors

**Invalid Slot (DMR):**
```json
{
  "status": 400,
  "message": "slot is invalid, must be 1 or 2"
}
```

**Invalid Source ID:**
```json
{
  "status": 400,
  "message": "srcId was not valid"
}
```

**Invalid Talkgroup ID:**
```json
{
  "status": 400,
  "message": "tgId was not valid"
}
```

**Invalid Voice Channel:**
```json
{
  "status": 400,
  "message": "voiceChNo was not valid"
}
```

**Invalid Mode:**
```json
{
  "status": 400,
  "message": "mode is invalid"
}
```

### 11.5 Error Handling Best Practices

1. **Always check HTTP status code first** - 200 means success, anything else is an error
2. **Parse error messages** - The `message` field contains human-readable error details
3. **Handle 503 errors gracefully** - Service unavailable often means protocol not enabled in config
4. **Retry on 401** - May need to re-authenticate if token expired
5. **Log errors** - Keep error responses for debugging and audit trails
6. **Validate input before sending** - Many errors can be prevented with client-side validation
7. **Check protocol conflicts** - Only one protocol's control channel can be active at a time

---

## 12. Security Considerations

### 12.1 Password Security

- **Never send plaintext passwords:** Always hash with SHA-256 before transmission
- **Use HTTPS in production:** Prevents token interception
- **Rotate passwords regularly:** Change dvmhost password periodically
- **Strong passwords:** Use complex passwords (minimum 16 characters recommended)

### 12.2 Token Management

- **Tokens are session-based:** Bound to client IP/hostname
- **Token invalidation:** Tokens are invalidated on:
  - Re-authentication
  - Explicit invalidation
  - Server restart
- **Token format:** 64-bit unsigned integer (not cryptographically secure by itself)

### 12.3 Network Security

- **Use HTTPS:** Enable SSL/TLS for production deployments
- **Firewall rules:** Restrict REST API access to trusted networks
- **Rate limiting:** Consider implementing rate limiting for brute-force protection
- **Audit logging:** Enable debug logging to track API access

### 12.4 Operational Security

- **Mode changes:** Use caution when changing modes during active traffic
- **Kill command:** Restricted to authorized administrators
- **Supervisory mode:** Only enable on authoritative/master hosts
- **Raw TSBK:** Advanced feature requiring protocol knowledge

---

## 13. Examples

### 13.1 Complete Authentication and Status Check

```bash
#!/bin/bash

# Configuration
DVMHOST="dvmhost.example.com"
PORT="9990"
PASSWORD="your_password_here"

# Step 1: Generate password hash
echo "Generating password hash..."
HASH=$(echo -n "$PASSWORD" | sha256sum | cut -d' ' -f1)

# Step 2: Authenticate
echo "Authenticating..."
AUTH_RESPONSE=$(curl -s -X PUT "http://${DVMHOST}:${PORT}/auth" \
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
curl -s -X GET "http://${DVMHOST}:${PORT}/version" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

# Step 4: Get status
echo -e "\nGetting status..."
curl -s -X GET "http://${DVMHOST}:${PORT}/status" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

# Step 5: Get voice channels
echo -e "\nGetting voice channels..."
curl -s -X GET "http://${DVMHOST}:${PORT}/voice-ch" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq
```

### 13.2 Switch to Fixed P25 Mode

```bash
#!/bin/bash

TOKEN="your_token_here"
DVMHOST="dvmhost.example.com"
PORT="9990"

# Set fixed P25 mode
curl -X PUT "http://${DVMHOST}:${PORT}/mdm/mode" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "mode": "p25"
  }' | jq

# Verify mode change
curl -X GET "http://${DVMHOST}:${PORT}/status" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq '.state, .fixedMode'
```

### 13.3 Enable DMR Debug Logging

```bash
#!/bin/bash

TOKEN="your_token_here"
DVMHOST="dvmhost.example.com"
PORT="9990"

# Enable DMR debug and verbose logging
curl -X GET "http://${DVMHOST}:${PORT}/dmr/debug/1/1" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

# Query current debug state
curl -X GET "http://${DVMHOST}:${PORT}/dmr/debug" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq
```

### 13.4 Send DMR Radio Check

```bash
#!/bin/bash

TOKEN="your_token_here"
DVMHOST="dvmhost.example.com"
PORT="9990"

# Send radio check to radio 123456 on slot 1
curl -X PUT "http://${DVMHOST}:${PORT}/dmr/rid" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "command": "check",
    "dstId": 123456,
    "slot": 1
  }' | jq
```

### 13.5 Grant P25 Voice Channel

```bash
#!/bin/bash

TOKEN="your_token_here"
DVMHOST="dvmhost.example.com"
PORT="9990"

# Grant voice channel 1 for TG 100 to radio 123456
curl -X PUT "http://${DVMHOST}:${PORT}/grant-tg" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "state": 5,
    "dstId": 100,
    "srcId": 123456,
    "grp": true,
    "voiceChNo": 1,
    "emergency": false
  }' | jq
```

### 13.6 P25 Dynamic Regroup

```bash
#!/bin/bash

TOKEN="your_token_here"
DVMHOST="dvmhost.example.com"
PORT="9990"

# Regroup radio 123456 to talkgroup 5000
curl -X PUT "http://${DVMHOST}:${PORT}/p25/rid" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "command": "dyn-regrp",
    "dstId": 123456,
    "tgId": 5000
  }' | jq

# Cancel the regroup
curl -X PUT "http://${DVMHOST}:${PORT}/p25/rid" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "command": "dyn-regrp-cancel",
    "dstId": 123456
  }' | jq
```

### 13.7 Check Radio ID Authorization

```bash
#!/bin/bash

TOKEN="your_token_here"
DVMHOST="dvmhost.example.com"
PORT="9990"
RADIO_ID="123456"

# Check if radio is whitelisted
WHITELIST_RESPONSE=$(curl -s -X GET "http://${DVMHOST}:${PORT}/rid-whitelist/${RADIO_ID}" \
  -H "X-DVM-Auth-Token: $TOKEN")

IS_WHITELISTED=$(echo "$WHITELIST_RESPONSE" | jq -r '.whitelisted')

# Check if radio is blacklisted
BLACKLIST_RESPONSE=$(curl -s -X GET "http://${DVMHOST}:${PORT}/rid-blacklist/${RADIO_ID}" \
  -H "X-DVM-Auth-Token: $TOKEN")

IS_BLACKLISTED=$(echo "$BLACKLIST_RESPONSE" | jq -r '.blacklisted')

echo "Radio $RADIO_ID:"
echo "  Whitelisted: $IS_WHITELISTED"
echo "  Blacklisted: $IS_BLACKLISTED"

if [ "$IS_WHITELISTED" == "true" ] && [ "$IS_BLACKLISTED" == "false" ]; then
  echo "  Status: AUTHORIZED"
else
  echo "  Status: NOT AUTHORIZED"
fi
```

### 13.8 Monitor Affiliations Across All Protocols

```bash
#!/bin/bash

TOKEN="your_token_here"
DVMHOST="dvmhost.example.com"
PORT="9990"

echo "Monitoring affiliations (Ctrl+C to stop)..."
echo "================================================"

while true; do
  clear
  echo "=== DMR Affiliations ==="
  curl -s -X GET "http://${DVMHOST}:${PORT}/dmr/report-affiliations" \
    -H "X-DVM-Auth-Token: $TOKEN" | jq -r '.affiliations[] | "Radio \(.srcId) -> TG \(.grpId) (Slot \(.slot))"'
  
  echo -e "\n=== P25 Affiliations ==="
  curl -s -X GET "http://${DVMHOST}:${PORT}/p25/report-affiliations" \
    -H "X-DVM-Auth-Token: $TOKEN" | jq -r '.affiliations[] | "Radio \(.srcId) -> TG \(.grpId)"'
  
  echo -e "\n=== NXDN Affiliations ==="
  curl -s -X GET "http://${DVMHOST}:${PORT}/nxdn/report-affiliations" \
    -H "X-DVM-Auth-Token: $TOKEN" | jq -r '.affiliations[] | "Radio \(.srcId) -> TG \(.grpId)"'
  
  echo -e "\n[Updated: $(date '+%Y-%m-%d %H:%M:%S')]"
  sleep 5
done
```

### 13.9 Toggle Control Channels

```bash
#!/bin/bash

TOKEN="your_token_here"
DVMHOST="dvmhost.example.com"
PORT="9990"

# Toggle DMR control channel
echo "Toggling DMR CC..."
curl -X GET "http://${DVMHOST}:${PORT}/dmr/cc-enable" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

# Toggle DMR broadcast mode
echo -e "\nToggling DMR CC broadcast mode..."
curl -X GET "http://${DVMHOST}:${PORT}/dmr/cc-broadcast" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

# Fire DMR control channel
echo -e "\nFiring DMR control channel..."
curl -X GET "http://${DVMHOST}:${PORT}/dmr/beacon" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq
```

### 13.10 P25 Radio Operations

```bash
#!/bin/bash

TOKEN="your_token_here"
DVMHOST="dvmhost.example.com"
PORT="9990"
RADIO_ID="123456"

# Page radio
echo "Paging radio ${RADIO_ID}..."
curl -X PUT "http://${DVMHOST}:${PORT}/p25/rid" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d "{
    \"command\": \"page\",
    \"dstId\": ${RADIO_ID}
  }" | jq

# Radio check
echo -e "\nSending radio check to ${RADIO_ID}..."
curl -X PUT "http://${DVMHOST}:${PORT}/p25/rid" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d "{
    \"command\": \"check\",
    \"dstId\": ${RADIO_ID}
  }" | jq

# Radio inhibit
echo -e "\nInhibiting radio ${RADIO_ID}..."
curl -X PUT "http://${DVMHOST}:${PORT}/p25/rid" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d "{
    \"command\": \"inhibit\",
    \"dstId\": ${RADIO_ID}
  }" | jq

# Wait 5 seconds
sleep 5

# Radio uninhibit
echo -e "\nUninhibiting radio ${RADIO_ID}..."
curl -X PUT "http://${DVMHOST}:${PORT}/p25/rid" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d "{
    \"command\": \"uninhibit\",
    \"dstId\": ${RADIO_ID}
  }" | jq
```

### 13.11 DMR Radio Operations

```bash
#!/bin/bash

TOKEN="your_token_here"
DVMHOST="dvmhost.example.com"
PORT="9990"
RADIO_ID="123456"
SLOT="1"

# Radio check
echo "Sending DMR radio check to ${RADIO_ID} on slot ${SLOT}..."
curl -X PUT "http://${DVMHOST}:${PORT}/dmr/rid" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d "{
    \"command\": \"check\",
    \"dstId\": ${RADIO_ID},
    \"slot\": ${SLOT}
  }" | jq

# Radio inhibit
echo -e "\nInhibiting DMR radio ${RADIO_ID} on slot ${SLOT}..."
curl -X PUT "http://${DVMHOST}:${PORT}/dmr/rid" \
  -H "X-DVM-Auth-Token: $TOKEN" \
  -H "Content-Type: application/json" \
  -d "{
    \"command\": \"inhibit\",
    \"dstId\": ${RADIO_ID},
    \"slot\": ${SLOT}
  }" | jq
```

### 13.12 Protocol Debug Control

```bash
#!/bin/bash

TOKEN="your_token_here"
DVMHOST="dvmhost.example.com"
PORT="9990"

# Enable all protocol debugging
echo "Enabling debug for all protocols..."

curl -X GET "http://${DVMHOST}:${PORT}/dmr/debug/1/1" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

curl -X GET "http://${DVMHOST}:${PORT}/p25/debug/1/1" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

curl -X GET "http://${DVMHOST}:${PORT}/nxdn/debug/1/1" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

# Enable packet dumping
echo -e "\nEnabling packet dumping..."

curl -X GET "http://${DVMHOST}:${PORT}/dmr/dump-csbk/1" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

curl -X GET "http://${DVMHOST}:${PORT}/p25/dump-tsbk/1" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

curl -X GET "http://${DVMHOST}:${PORT}/nxdn/dump-rcch/1" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

echo -e "\nDebug and packet dumping enabled for all protocols."
echo "Check dvmhost logs for detailed output."
```

### 13.13 Release All Grants and Affiliations

```bash
#!/bin/bash

TOKEN="your_token_here"
DVMHOST="dvmhost.example.com"
PORT="9990"

# Release all voice channel grants
echo "Releasing all voice channel grants..."
curl -X GET "http://${DVMHOST}:${PORT}/release-grants" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

# Release all affiliations
echo -e "\nReleasing all affiliations..."
curl -X GET "http://${DVMHOST}:${PORT}/release-affs" \
  -H "X-DVM-Auth-Token: $TOKEN" | jq

echo -e "\nAll grants and affiliations released."
```

### 13.14 Python Client Library

```python
#!/usr/bin/env python3

import requests
import hashlib
import json
from typing import Optional, Dict, Any

class DVMHostClient:
    def __init__(self, host: str, port: int, password: str, use_https: bool = False):
        self.base_url = f"{'https' if use_https else 'http'}://{host}:{port}"
        self.password = password
        self.token: Optional[str] = None
        
    def authenticate(self) -> bool:
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
    
    def _headers(self) -> Dict[str, str]:
        """Get headers with auth token"""
        return {
            "X-DVM-Auth-Token": self.token,
            "Content-Type": "application/json"
        }
    
    def get_version(self) -> Dict[str, Any]:
        """Get dvmhost version"""
        response = requests.get(
            f"{self.base_url}/version",
            headers=self._headers()
        )
        return response.json()
    
    def get_status(self) -> Dict[str, Any]:
        """Get dvmhost status"""
        response = requests.get(
            f"{self.base_url}/status",
            headers=self._headers()
        )
        return response.json()
    
    def set_mode(self, mode: str) -> Dict[str, Any]:
        """Set modem mode"""
        response = requests.put(
            f"{self.base_url}/mdm/mode",
            headers=self._headers(),
            json={"mode": mode}
        )
        return response.json()
    
    def dmr_radio_check(self, dst_id: int, slot: int) -> Dict[str, Any]:
        """Send DMR radio check"""
        response = requests.put(
            f"{self.base_url}/dmr/rid",
            headers=self._headers(),
            json={
                "command": "check",
                "dstId": dst_id,
                "slot": slot
            }
        )
        return response.json()
    
    def p25_radio_check(self, dst_id: int) -> Dict[str, Any]:
        """Send P25 radio check"""
        response = requests.put(
            f"{self.base_url}/p25/rid",
            headers=self._headers(),
            json={
                "command": "check",
                "dstId": dst_id
            }
        )
        return response.json()
    
    def get_dmr_affiliations(self) -> Dict[str, Any]:
        """Get DMR affiliations"""
        response = requests.get(
            f"{self.base_url}/dmr/report-affiliations",
            headers=self._headers()
        )
        return response.json()
    
    def get_p25_affiliations(self) -> Dict[str, Any]:
        """Get P25 affiliations"""
        response = requests.get(
            f"{self.base_url}/p25/report-affiliations",
            headers=self._headers()
        )
        return response.json()
    
    def get_nxdn_affiliations(self) -> Dict[str, Any]:
        """Get NXDN affiliations"""
        response = requests.get(
            f"{self.base_url}/nxdn/report-affiliations",
            headers=self._headers()
        )
        return response.json()
    
    def set_dmr_debug(self, debug: bool, verbose: bool) -> Dict[str, Any]:
        """Set DMR debug state"""
        debug_val = 1 if debug else 0
        verbose_val = 1 if verbose else 0
        response = requests.get(
            f"{self.base_url}/dmr/debug/{debug_val}/{verbose_val}",
            headers=self._headers()
        )
        return response.json()
    
    def set_p25_debug(self, debug: bool, verbose: bool) -> Dict[str, Any]:
        """Set P25 debug state"""
        debug_val = 1 if debug else 0
        verbose_val = 1 if verbose else 0
        response = requests.get(
            f"{self.base_url}/p25/debug/{debug_val}/{verbose_val}",
            headers=self._headers()
        )
        return response.json()
    
    def set_nxdn_debug(self, debug: bool, verbose: bool) -> Dict[str, Any]:
        """Set NXDN debug state"""
        debug_val = 1 if debug else 0
        verbose_val = 1 if verbose else 0
        response = requests.get(
            f"{self.base_url}/nxdn/debug/{debug_val}/{verbose_val}",
            headers=self._headers()
        )
        return response.json()
    
    def p25_dynamic_regroup(self, dst_id: int, tg_id: int) -> Dict[str, Any]:
        """P25 dynamic regroup radio to talkgroup"""
        response = requests.put(
            f"{self.base_url}/p25/rid",
            headers=self._headers(),
            json={
                "command": "dyn-regrp",
                "dstId": dst_id,
                "tgId": tg_id
            }
        )
        return response.json()
    
    def p25_dynamic_regroup_cancel(self, dst_id: int) -> Dict[str, Any]:
        """Cancel P25 dynamic regroup"""
        response = requests.put(
            f"{self.base_url}/p25/rid",
            headers=self._headers(),
            json={
                "command": "dyn-regrp-cancel",
                "dstId": dst_id
            }
        )
        return response.json()
    
    def dmr_radio_inhibit(self, dst_id: int, slot: int) -> Dict[str, Any]:
        """Inhibit DMR radio"""
        response = requests.put(
            f"{self.base_url}/dmr/rid",
            headers=self._headers(),
            json={
                "command": "inhibit",
                "dstId": dst_id,
                "slot": slot
            }
        )
        return response.json()
    
    def p25_radio_inhibit(self, dst_id: int) -> Dict[str, Any]:
        """Inhibit P25 radio"""
        response = requests.put(
            f"{self.base_url}/p25/rid",
            headers=self._headers(),
            json={
                "command": "inhibit",
                "dstId": dst_id
            }
        )
        return response.json()
    
    def release_all_grants(self) -> Dict[str, Any]:
        """Release all voice channel grants"""
        response = requests.get(
            f"{self.base_url}/release-grants",
            headers=self._headers()
        )
        return response.json()
    
    def release_all_affiliations(self) -> Dict[str, Any]:
        """Release all affiliations"""
        response = requests.get(
            f"{self.base_url}/release-affs",
            headers=self._headers()
        )
        return response.json()
    
    def check_rid_whitelist(self, rid: int) -> bool:
        """Check if RID is whitelisted"""
        response = requests.get(
            f"{self.base_url}/rid-whitelist/{rid}",
            headers=self._headers()
        )
        return response.json().get('whitelisted', False)
    
    def check_rid_blacklist(self, rid: int) -> bool:
        """Check if RID is blacklisted"""
        response = requests.get(
            f"{self.base_url}/rid-blacklist/{rid}",
            headers=self._headers()
        )
        return response.json().get('blacklisted', False)
    
    def grant_channel(self, state: int, dst_id: int, src_id: int, 
                     voice_ch_no: int, slot: int = 1, grp: bool = True,
                     emergency: bool = False) -> Dict[str, Any]:
        """Grant voice channel"""
        data = {
            "state": state,
            "dstId": dst_id,
            "srcId": src_id,
            "grp": grp,
            "voiceChNo": voice_ch_no
        }
        
        if state == 4:  # DMR
            data["slot"] = slot
        elif state == 5:  # P25
            data["emergency"] = emergency
        
        response = requests.put(
            f"{self.base_url}/grant-tg",
            headers=self._headers(),
            json=data
        )
        return response.json()

# Example usage
if __name__ == "__main__":
    # Create client
    client = DVMHostClient("dvmhost.example.com", 9990, "your_password_here")
    
    # Authenticate
    if client.authenticate():
        print("Authenticated successfully!")
        
        # Get version
        version = client.get_version()
        print(f"Version: {version['version']}")
        
        # Get status
        status = client.get_status()
        print(f"Current state: {status['state']}")
        print(f"DMR enabled: {status['dmrEnabled']}")
        print(f"P25 enabled: {status['p25Enabled']}")
        
        # Set fixed P25 mode
        result = client.set_mode("p25")
        print(f"Mode change: {result}")
        
        # Send P25 radio check
        result = client.p25_radio_check(123456)
        print(f"Radio check result: {result}")
        
        # Get P25 affiliations
        affs = client.get_p25_affiliations()
        print(f"P25 Affiliations: {json.dumps(affs, indent=2)}")
    else:
        print("Authentication failed!")
```

---

## Appendix A: Endpoint Summary Table

| Method | Endpoint | Description | Auth Required | Protocol |
|--------|----------|-------------|---------------|----------|
| PUT | /auth | Authenticate and get token | No | N/A |
| GET | /version | Get version information | Yes | N/A |
| GET | /status | Get host status | Yes | N/A |
| GET | /voice-ch | Get voice channel states | Yes | N/A |
| PUT | /mdm/mode | Set modem mode | Yes | N/A |
| PUT | /mdm/kill | Shutdown host | Yes | N/A |
| PUT | /set-supervisor | Set supervisory mode | Yes | N/A |
| PUT | /permit-tg | Permit talkgroup | Yes | All |
| PUT | /grant-tg | Grant voice channel | Yes | All |
| GET | /release-grants | Release all grants | Yes | All |
| GET | /release-affs | Release all affiliations | Yes | All |
| GET | /rid-whitelist/{rid} | Check RID whitelist | Yes | N/A |
| GET | /rid-blacklist/{rid} | Check RID blacklist | Yes | N/A |
| GET | /dmr/beacon | Fire DMR beacon | Yes | DMR |
| GET | /dmr/debug[/{debug}/{verbose}] | Get/Set DMR debug | Yes | DMR |
| GET | /dmr/dump-csbk[/{enable}] | Get/Set CSBK dump | Yes | DMR |
| PUT | /dmr/rid | DMR RID operations (7 commands) | Yes | DMR |
| GET | /dmr/cc-enable | Toggle DMR CC | Yes | DMR |
| GET | /dmr/cc-broadcast | Toggle DMR CC broadcast | Yes | DMR |
| GET | /dmr/report-affiliations | Get DMR affiliations | Yes | DMR |
| GET | /p25/cc | Fire P25 CC | Yes | P25 |
| GET | /p25/debug[/{debug}/{verbose}] | Get/Set P25 debug | Yes | P25 |
| GET | /p25/dump-tsbk[/{enable}] | Get/Set TSBK dump | Yes | P25 |
| PUT | /p25/rid | P25 RID operations (12 commands) | Yes | P25 |
| GET | /p25/cc-enable | Toggle P25 CC | Yes | P25 |
| GET | /p25/cc-broadcast | Toggle P25 CC broadcast | Yes | P25 |
| PUT | /p25/raw-tsbk | Send raw TSBK packet | Yes | P25 |
| GET | /p25/report-affiliations | Get P25 affiliations | Yes | P25 |
| GET | /nxdn/cc | Fire NXDN CC | Yes | NXDN |
| GET | /nxdn/debug[/{debug}/{verbose}] | Get/Set NXDN debug | Yes | NXDN |
| GET | /nxdn/dump-rcch[/{enable}] | Get/Set RCCH dump | Yes | NXDN |
| GET | /nxdn/cc-enable | Toggle NXDN CC | Yes | NXDN |
| GET | /nxdn/report-affiliations | Get NXDN affiliations | Yes | NXDN |

### Command Summaries

**DMR /rid Commands (7 total):**
- `page` - Page radio
- `check` - Radio check
- `inhibit` - Inhibit radio
- `uninhibit` - Uninhibit radio
- `dmr-setmfid` - Set manufacturer ID

**P25 /rid Commands (12 total):**
- `p25-setmfid` - Set manufacturer ID
- `page` - Page radio
- `check` - Radio check
- `inhibit` - Inhibit radio
- `uninhibit` - Uninhibit radio
- `dyn-regrp` - Dynamic regroup
- `dyn-regrp-cancel` - Cancel dynamic regroup
- `dyn-regrp-lock` - Lock dynamic regroup
- `dyn-regrp-unlock` - Unlock dynamic regroup
- `group-aff-req` - Group affiliation request
- `unit-reg` - Unit registration
- `emerg` - Emergency acknowledgment

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
  
  # SHA-256 hashed password (plaintext - hashed internally)
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

## Appendix C: Host State Codes

| Code | State | Description |
|------|-------|-------------|
| 0 | IDLE | Dynamic mode, no active protocol |
| 1 | LOCKOUT | Lockout mode, no transmissions |
| 2 | ERROR | Error state |
| 3 | QUIT | Shutting down |
| 4 | DMR | DMR mode active |
| 5 | P25 | P25 mode active |
| 6 | NXDN | NXDN mode active |

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | Dec 3, 2025 | Initial documentation based on source code analysis |

---

**End of Document**
