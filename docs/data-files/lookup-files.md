# Lookup and Data Files

DVMHost and FNE use several reloadable lookup files in addition to their primary YAML. File paths and reload intervals are selected by the owning configuration. Relative paths are resolved from the process working directory.

Blank lines and lines whose first character is `#` are ignored by the lookup readers. Indented comments are not comments. CSV parsing is intentionally simple: fields are split at every comma, with no CSV quoting, escaping, or automatic whitespace trimming. Consequently, aliases and passwords cannot contain commas, and leading/trailing spaces become part of string fields. Numeric conversion uses C-style decimal conversion unless stated otherwise; malformed numeric text normally becomes zero rather than producing a parse error. CSV fields are position-dependent, so keep empty placeholders when a later optional field is needed. Duplicate numeric IDs overwrite earlier entries. The supplied examples state that files should end with an empty line.

## Ownership and enforcement overview

| File | Primary consumer | Operational purpose |
| --- | --- | --- |
| `rid_acl.dat` | Host and FNE | Authorizes subscriber RIDs and carries packet-data/key-management permissions. |
| `rid_alias.dat` | Host and FNE | Adds display names without granting authorization. |
| `peer_list.dat` | FNE | Authorizes peer connections and assigns high-trust network capabilities. |
| `iden_table.dat` | Host and SysView | Converts compact channel ID/number pairs into RF frequencies and supplies advertised band-plan data. |
| `RSSI.dat` | Host | Converts modem-specific raw RSSI samples into approximate dBm. |
| `adj_site_map.yml` | FNE | Controls forwarding of received P25 adjacent-site status broadcasts by source peer. |

The host may receive RID, alias, and talkgroup data distributed by an FNE when lookup updates are enabled. The FNE remains the normal policy authority in a networked deployment; the host copy is also used for local RF admission and site-trunking behavior.

## Lookup processing model

Most CSV lookup classes load by clearing the current in-memory table and then parsing the complete file. A syntactically readable file with no usable entries fails the load, but the old table has already been cleared. A periodic reload is therefore not transactional: validate a replacement before installing it and use an atomic rename rather than editing the live file in place.

The configured `time` values are minutes. A value of zero performs the initial read but does not start the periodic reload thread. Lookup misses return type-specific defaults rather than throwing an error: an unknown RID is a disabled/default record, an unknown alias is `UNKNOWN`, an unknown peer is a default peer, and an unknown channel identity is an empty/zero identity record. Callers decide whether that default is permitted.

FNE-to-host synchronization does not always transfer the complete source row. In particular, ordinary RID whitelist/blacklist updates carry only the RID and enabled/disabled state. RID aliases use a separate full-file transfer, while a replica FNE can receive complete RID, alias, peer, and talkgroup files. Do not assume a host receiving RID updates also receives static IP or key-management permissions.

## Radio ID ACL (`rid_acl.dat`)

Source example: [`configs/rid_acl.example.dat`](../../configs/rid_acl.example.dat).

```text
RID,Enabled,Alias,IPAddress,CanRequestKeys,CanRekey,AllowedKIDs,
```

| Column | Required | Meaning |
| --- | --- | --- |
| 1 `RID` | Yes | Unsigned decimal radio ID used as the table key. Zero and protocol-reserved values should not be assigned to subscribers even though the text reader does not range-check them. |
| 2 `Enabled` | Yes | Exactly decimal `1` enables the RID; every other value is disabled. With RID ACL enforcement active, a disabled entry is denied. |
| 3 `Alias` | No | Display/logging alias stored with the ACL record. It is separate from `rid_alias.dat` and has no authorization effect. |
| 4 `IPAddress` | No | Textual IP address associated with packet-data registration/routing. The loader stores the text; this column does not allocate an address or validate the subnet. |
| 5 `CanRequestKeys` | No | `1` permits supported key requests. Default is false. |
| 6 `CanRekey` | No | `1` permits OTAR rekey delivery. Default is false. |
| 7 `AllowedKIDs` | No | Pipe-separated 16-bit hexadecimal KIDs such as `0064|0101|0A50`. Empty means no per-KID restriction when key requests are otherwise allowed. |

At least two columns are required. A lookup miss produces a synthetic disabled/default record. When the owning `radio_id` configuration has ACL enforcement disabled, access-control callers allow unknown RIDs; enabling the ACL changes the table into an allow-list. P25 special system IDs `WUID_ALL` and `WUID_FNE` are treated as enabled independently of the file.

The ACL-disabled behavior is permissive only for unknown RIDs. A known row with `Enabled` set to anything other than `1` is still rejected by the host access-control helpers. This permits an open deployment to maintain an explicit deny-list. The FNE constructs its RID lookup with ACL enforcement enabled, so its normal group-call validation treats undefined RIDs as unknown and applies the FNE `rejectUnknownRID` and call-handler policy on top of the table.

`AllowedKIDs` is parsed token by token. Empty tokens and values above `FFFF` are ignored, duplicates are removed, and valid values preserve their input order. An empty resulting list means all KIDs are eligible—but only after `CanRequestKeys`, peer permission, KMF policy, and key availability also permit the request. `CanRekey` separately controls whether OTAR rekey payloads may be delivered to the RID.

### RID ACL use cases and data flow

- **RF and network admission:** DMR, P25, NXDN, and analog call handlers look up source and, for unit-to-unit service, destination RIDs. Disabled entries are rejected. Undefined entries are accepted or rejected according to the component's ACL and unknown-RID policy.
- **Unit registration and control service:** Host protocol access-control helpers use the table before accepting registrations, affiliations, voice/data requests, and other subscriber operations. The talkgroup-specific `rid_permitted` list is an additional check, not a replacement for this table.
- **Packet-data address provisioning:** DMR packet data maps RID to IPv4 address and performs the reverse address-to-RID lookup using `IPAddress`. P25 conventional and SNDCP registration use a nonempty address as static provisioning; an enabled RID with an empty address may be eligible for dynamic address assignment. Use canonical IPv4 text consistently because reverse DMR matching compares stored text.
- **Key service:** A key request must pass both the peer's `CanRequestKeys` capability and the subscriber RID policy. The requesting RID must exist, be enabled, have `CanRequestKeys: 1`, and—when `AllowedKIDs` is nonempty—request a listed KID. `CanRekey` is separately checked before rekey delivery.
- **Host synchronization:** The FNE sends enabled RIDs as whitelist records and disabled RIDs as blacklist records in chunks. A host with `network.updateLookups` adds or updates those states. The ordinary update does not convey alias, IP address, `CanRequestKeys`, `CanRekey`, or `AllowedKIDs`; an existing host row retains those values, while a newly announced RID receives empty/false defaults. `network.saveLookups` may persist the resulting rows.
- **Replica synchronization:** Replica FNE transfer sends the complete ACL file, preserving all columns when the receiving replica reloads it.

Because the table is also security policy, avoid reusing a subscriber RID for infrastructure or testing. An alias or IP address does not enable a row; `Enabled` remains the admission decision.

## Radio aliases (`rid_alias.dat`)

Source example: [`configs/rid_alias.example.dat`](../../configs/rid_alias.example.dat).

```text
RID,Alias,
```

RID is decimal. Although the format describes the alias as required, the implementation accepts an empty second field. Unknown RIDs display as `UNKNOWN`; the P25 system IDs display as `SYSTEM`. Aliases are informational and never authorize a radio, so keep admission policy in the RID ACL. This split lets operators update names without modifying security policy or key permissions.

The host uses this table for P25 over-the-air user-alias conveyance when `protocols.p25.radioAlias.enable` is enabled. Standard/Harris-style aliases are inserted into eligible outbound voice link-control phases; Motorola mode emits talker-alias TDULCs after a completed duplex call. Empty and `UNKNOWN` aliases are not transmitted. This is an RF presentation feature and does not alter the source RID.

The FNE can send the complete alias file in a fragmented radio-alias synchronization response. A host requests it after connection when both `network.updateLookups` and P25 radio aliases are enabled. Replica FNEs use the same full-file style of transfer. Treat synchronization as replacement of the alias dataset, not as an authorization update.

## FNE peer list (`peer_list.dat`)

Source example: [`configs/peer_list.example.dat`](../../configs/peer_list.example.dat).

```text
PeerID,Password,Replication,Alias,CanRequestKeys,CanIssueInhibit,HasCallPriority,JitterEnabled,JitterMaxFrames,JitterMaxWaitUs,TelephoneInterconnect,AllowUnitToUnit,
```

| Column | Meaning/default |
| --- | --- |
| 1 `PeerID` | Required decimal ID, operationally valid from `1–999999999`. It is the authentication/routing identity, not a display number, and must be globally unique in the connected topology. |
| 2 `Password` | Per-peer password. Empty means the FNE master password is used; it does not mean passwordless authentication. |
| 3 `Replication` | `1` marks an FNE peer-link/replica eligible for full configuration and system traffic. Default false. |
| 4 `Alias` | Optional display name. |
| 5 `CanRequestKeys` | `1` permits encryption-key requests. Default false. |
| 6 `CanIssueInhibit` | `1` permits inhibit signaling. Default false. |
| 7 `HasCallPriority` | `1` allows priority over normal collision handling. Default false. |
| 8 `JitterEnabled` | `1` enables adaptive reordering for this peer. It trades added latency for tolerance of late/out-of-order network frames. |
| 9 `JitterMaxFrames` | Maximum buffered depth. Default `4`; use `2–8` frames. The CSV loader assigns this field directly and currently bypasses the clamping helper, so out-of-range file values can reach the live connection. Larger values tolerate more reordering but increase latency and memory use. |
| 10 `JitterMaxWaitUs` | Maximum age before delivery despite a sequence gap. Default `40000`; use `10000–200000` microseconds. As with frame depth, per-peer CSV loading currently bypasses the available clamping helper. |
| 11 `TelephoneInterconnect` | `1` identifies a P25 telephone-interconnect gateway. |
| 12 `AllowUnitToUnit` | `1` allows private-call origination/reception/transit. If omitted or blank, legacy behavior allows it. |

The FNE first applies peer-list admission when peer ACL enforcement is enabled. A missing peer is rejected when the list contains entries; when enforcement is disabled, the list is not an allow-list. A configured-but-empty peer ACL currently fails open and logs a warning, allowing all peers. Treat that state as a deployment error rather than a safe deny-all configuration. Empty optional Boolean fields default false except `AllowUnitToUnit`, which defaults true for compatibility.

Replication is a high-trust role: replica and SysView peers bypass ordinary talkgroup recipient filtering in call handlers, and replicas receive replicated lookup/configuration datasets and system traffic. `HasCallPriority` bypasses normal collision arbitration for that peer; it is not a general QoS weight. `CanIssueInhibit` controls acceptance of inhibit/uninhibit signaling, while `CanRequestKeys` is only one gate in the key-request path. `TelephoneInterconnect` marks the specialized gateway used for interconnect routing. Do not grant any of these capabilities merely to make a connection succeed.

### Peer-list processing and use cases

1. During login, the FNE checks the peer ID against the list when peer ACL enforcement is enabled. Authentication then uses the row's password, falling back to the master password only when the row password is empty.
2. After configuration exchange identifies the peer type, `Replication` can promote a neighbor FNE connection to replica status. The flag does not turn an ordinary host into an FNE; the connection metadata must also identify a neighbor FNE.
3. The FNE copies `HasCallPriority` into live connection state. Protocol call handlers use it during call-collision arbitration, allowing the peer's new source to preempt/bypass the ordinary collision decision.
4. If the global jitter buffer is enabled, its globally clamped settings override the row. Otherwise the row's jitter settings are applied directly to that connection and buffers are created per stream. Waiting for gaps adds latency, so use it for demonstrably reordered or bursty links rather than every low-latency LAN peer.
5. `AllowUnitToUnit` is checked for both ingress and egress unit-to-unit routing. A private call is allowed only when global unit-to-unit policy and the involved peer route permit it.
6. Key requests, inhibit operations, and telephone-interconnect routing consult their corresponding capabilities independently. Granting one does not grant the others.
7. Replica synchronization sends the complete peer-list file. REST additions and deletions change the in-memory table; a commit is required to make those changes durable.

Passwords are written back in plaintext by the peer-list commit operation. Restrict file permissions and avoid exposing the file through diagnostics or version control.

## Channel identity table (`iden_table.dat`)

Source example: [`configs/iden_table.example.dat`](../../configs/iden_table.example.dat). `dvmcfggen` contains band-plan support in [`iden_table.py`](../../tools/dvmcfggen/iden_table.py).

```text
ChannelID,BaseFrequencyHz,SpacingKHz,TransmitOffsetMHz,BandwidthKHz,
```

| Field | Meaning |
| --- | --- |
| `ChannelID` | Band-plan selector carried with a channel number. The host clamps configured channel IDs to `0–15`; use one unique row per ID. |
| `BaseFrequencyHz` | Frequency represented by channel number zero, in integer hertz. P25 identity updates encode the base in 5 Hz units, so choose a value divisible by 5. |
| `SpacingKHz` | Nominal separation between consecutive channel numbers. The loader accepts a decimal, substitutes half the bandwidth when it is zero, and quantizes operational calculations downward to 125 Hz units. |
| `TransmitOffsetMHz` | Signed offset from site transmit/output frequency to site receive/input frequency, in MHz. In host calculations, `RX = TX + offset`; therefore a `-45` MHz value maps an 851 MHz output to an 806 MHz input. Zero is rejected for the primary host channel. |
| `BandwidthKHz` | Occupied/channel bandwidth advertised in applicable control signaling. It describes the emission channel; it is not the same as channel-number spacing. |

All five fields are required. The loader does not validate licensing, band allocation, overlaps, duplicate channel IDs, or whether bandwidth fits within spacing. A later duplicate `ChannelID` replaces the earlier row. Identity-table saving is not implemented, so treat the file as operator-managed input.

### Identity-table processing and use cases

- **Startup validation and modem frequencies:** The host selects `system.config.channelId`, looks up its row, combines it with hexadecimal `channelNo`, and derives the configured site TX/RX frequencies. A missing identity or invalid primary offset prevents a meaningful channel configuration.
- **Control-channel advertisement:** DMR, P25, and NXDN control implementations receive the table. P25 emits identifier-update messages from the configured entries and uses the selected identity in channel grants and adjacent/control-channel signaling. DMR and NXDN use the selected channel identity/number in their applicable site and grant messages.
- **Voice-channel pools:** Control-channel/voice-channel registration and grant logic use channel ID/number pairs to identify RF resources. Every voice channel advertised under an identity must use the same base, spacing, offset, and bandwidth interpretation as subscribers.
- **Network metadata:** The selected row's offset and bandwidth accompany peer metadata. These values describe the site to monitoring/network consumers; they do not retune a remote peer by themselves.
- **Setup and calibration tools:** Host setup and calibration load the same table to display and calculate channel frequencies. Keeping a single operator-managed table prevents the setup UI and runtime from deriving different values.

Identity IDs are protocol signaling values, not arbitrary row numbers. A subscriber must receive the matching identity update before a channel ID/number grant can be interpreted correctly.

### Exact frequency formulas

At load time, a zero spacing is replaced with `BandwidthKHz / 2`. Values below `0.125` kHz are raised to `0.125` kHz. The current code tests an upper bound of `125000.0` in a value whose unit is kHz; this is much larger than a practical channel raster, so do not rely on that check as meaningful RF-plan validation.

The implementation then converts the configured spacing into 125 Hz units:

```text
spacing_units = floor(SpacingKHz / 0.125)
effective_spacing_hz = spacing_units * 125
offset_hz = trunc(TransmitOffsetMHz * 1,000,000)
```

For channel number `N`:

```text
site_tx_hz = BaseFrequencyHz + (effective_spacing_hz * N)
site_rx_hz = site_tx_hz + offset_hz
```

In DVMHost terminology, `site_tx_hz` is the repeater/base-station output (subscriber receive) frequency and `site_rx_hz` is the repeater input (subscriber transmit) frequency. The reverse calculation for a desired site output is:

```text
N = (desired_site_tx_hz - BaseFrequencyHz) / effective_spacing_hz
```

`N` must be an integer. The host reads `channelNo` as hexadecimal and clamps it to `1–4095`; channel zero is changed to one. Thus a calculated decimal channel number of 800 must be written as hexadecimal `320`. Voice-channel entries use the same ID/number representation.

### Constructing an entry manually

1. Choose an unused channel ID from `0–15`.
2. Choose the real channel raster. It must be exactly representable in 125 Hz increments. For 6.25 kHz, `6.25 / 0.125 = 50`, so the effective step is `50 × 125 = 6250 Hz`.
3. Choose a base frequency at or below the lowest desired site output. It should be divisible by 5 Hz for P25 signaling and selected so every required output differs from it by an integer number of channel steps.
4. Calculate `N` for each output. Reject the plan if there is a remainder; rounding `N` moves the advertised frequency off-channel.
5. Calculate the signed input offset as `(site_rx_hz - site_tx_hz) / 1,000,000`. All channels represented by one row share this offset and spacing; use another channel ID when either changes.
6. Select the actual occupied bandwidth, independently of the raster.
7. Convert `N` to hexadecimal for `system.config.channelNo` and verify the forward formulas reproduce both frequencies exactly.

Worked example using the supplied 800 MHz row and a desired site output of `851.012500 MHz`:

```text
entry: 0,851006250,6.25,-45.000,12.5,

effective_spacing_hz = floor(6.25 / 0.125) * 125 = 6250 Hz
N = (851012500 - 851006250) / 6250 = 1
site_tx_hz = 851006250 + (6250 * 1) = 851012500 Hz
site_rx_hz = 851012500 + (-45 * 1,000,000) = 806012500 Hz
channelNo = 1 decimal = 001 hexadecimal
```

For a second example, with base `450000000`, 6.25 kHz spacing, `+5 MHz` input offset, and desired output `450500000`:

```text
N = (450500000 - 450000000) / 6250 = 80 decimal = 050 hexadecimal
site_rx_hz = 450500000 + 5000000 = 455500000 Hz
```

For P25 control signaling, spacing is represented in 125 Hz units and base frequency in 5 Hz units. VHF/UHF offset signaling derives a magnitude in channel-spacing units; 700/800/900 MHz signaling uses 250 kHz offset units. Choose values exactly representable by the applicable protocol encoding, not merely values the text parser accepts. The generator should remain the normal path; the manual process is primarily for review and recovery.

## RSSI map (`RSSI.dat`)

Source example: [`configs/RSSI.dat`](../../configs/RSSI.dat).

Each whitespace-separated line maps a raw modem sample to dBm:

```text
1134    -90
1123    -100
1000    -109
```

The first value is parsed as an unsigned 16-bit raw sample and the second as signed integer dBm. Duplicate raw samples keep the first inserted value. Points are stored in ascending raw-value order regardless of file order. Between two points `(x1,y1)` and `(x2,y2)`, the host performs linear interpolation and truncates the result to an integer:

```text
p = (raw - x1) / (x2 - x1)
dBm = trunc((1 - p) * y1 + p * y2)
```

Values below the lowest calibration point use the lowest point's dBm; values above the highest use the highest point's dBm. Provide multiple measured points across the useful receiver range. Values are hardware-specific; copying another unit's table defeats calibration.

The host loads this file once at startup when `system.modem.rssiMappingFile` is configured; unlike the `LookupTable`-derived files, it has no periodic reload thread or commit operation. DMR, P25, and NXDN RF paths convert raw modem RSSI samples through the same interpolator for logging, activity reporting, and protocol statistics. With no mapping points, interpolation returns `0`, which is a missing-calibration sentinel in practice—not a believable received signal level.

Calibrate using measured pairs from the exact modem/radio chain. More points improve a nonlinear calibration curve, but duplicate raw values do not replace earlier ones. Arrange the file monotonically for human review even though the map sorts by raw value internally.

## Adjacent-site map (`adj_site_map.yml`)

Source example: [`configs/adj_site_map.example.yml`](../../configs/adj_site_map.example.yml).

```yaml
peers:
  - peerId: 1234567
    active: true
    neighbors: [2345678, 3456789]
```

| Key | Meaning |
| --- | --- |
| `peers[].peerId` | FNE peer/site represented by this entry. |
| `peers[].active` | Whether the adjacency is active. Defaults false when omitted. An inactive record remains representable but should not drive active adjacent-site behavior. |
| `peers[].neighbors` | Stored list of peer IDs described as neighbors. The current forwarding path does not consult this list; it is retained for configuration/REST representation and future or external topology use. |

The current runtime use is narrower than the file name suggests. When the FNE receives a P25 `ADJ_STS_BCAST`, it looks up only the sending `peerId`. If an entry exists and `active` is false, the broadcast is dropped. If an entry exists and is active, the FNE explicitly writes the broadcast to every other connected peer and stops the normal repeat path. If no entry exists, normal call-handler processing continues. The `neighbors` array is not used to select those recipients.

This map therefore controls P25 adjacent-status-broadcast forwarding permission for a source peer; it is not the host's learned over-the-air adjacent-site table, does not create sockets, does not authenticate peers, and does not replace the FNE spanning tree. Global `disallowAdjStsBcast` policy is checked before this map and can prohibit forwarding entirely.

Duplicate `peerId` entries can coexist when loaded, and lookup returns the first match. Programmatic updates replace the first matching entry. Avoid duplicates, and ensure every source peer ID matches the authenticated FNE topology. REST can list, add, delete, and commit these entries.

## Encrypted key container

The FNE `crypto_container` is not a CSV lookup. It is an encrypted managed container selected by `master.crypto_container.file` and protected by its configured password. Do not edit it as text, commit it with real keys, or confuse its password with the FNE network preshared key.

## Reload and persistence

- A positive `time` value controls periodic reload according to the owning lookup class.
- FNE REST provides explicit reload/commit endpoints for selected tables; see [TN.1100](../TN.1100%20-%20FNE%20REST%20API%20Documentation.md).
- Host `network.updateLookups` accepts distributed lookup updates; `saveLookups` controls persistence.
- Atomic replacement is safer than editing a live file in place. Preserve ownership and permissions.

## Cross references

- [Host example configuration](../../configs/config.example.yml)
- [FNE example configuration](../../configs/fne-config.example.yml)
- [Talkgroup rules](talkgroup-rules.md)
