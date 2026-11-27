# Talkgroup Rules Configuration

Talkgroup rules define group-call admission and routing across DMR, P25, and NXDN peers. Use [`configs/talkgroup_rules.example.yml`](../../configs/talkgroup_rules.example.yml) and validate it against [`configs/schema/talkgroup_rules.yaml_schema.json`](../../configs/schema/talkgroup_rules.yaml_schema.json).

The root key is `groupVoice`, a nonempty array. Each entry must contain `name`, `config`, and `source`; `source.tgid`, `source.slot`, and `config.active` are required by the schema. The runtime loader is less strict and supplies defaults for omitted fields, but relying on those defaults makes mistakes difficult to see. Treat schema validation as part of deployment.

## Schema validation

The schema uses JSON Schema draft 4 and deliberately rejects unknown keys at the root, rule, `config`, `source`, and rewrite-entry levels. This catches common misspellings such as `peerId` in place of the supported rewrite key `peerid`. It also enforces:

- A nonempty `groupVoice` array.
- A nonempty string for each rule `name`.
- Boolean values for `active`, `affiliated`, and `parrot`.
- `strapping` values of exactly `0`, `1`, or `2`.
- Positive integer TGIDs and RIDs within the common unsigned 32-bit representation.
- Peer IDs from `1` through `999999999`.
- Slot values of exactly `1` or `2`.
- Unique scalar entries within all peer-ID and RID lists.
- Complete rewrite objects containing `peerid`, `tgid`, and `slot`.

Schema validation cannot express all topology and protocol semantics. It does not prove that `(source.tgid, source.slot)` pairs are unique across rules, that two rewrites do not claim the same external identity, that inclusion and exclusion are not contradictory, or that a common 32-bit TGID fits the selected air protocol. Those checks remain part of deployment review.

```yaml
groupVoice:
  - name: Operations
    alias: Ops
    config:
      active: true
      affiliated: true
      strapping: 0
      parrot: false
      inclusion: []
      exclusion: []
      rewrite: []
      always: []
      preferred: []
      rid_permitted: []
    source:
      tgid: 1001
      slot: 1
```

## Identity fields

| Key | Required | Meaning |
| --- | --- | --- |
| `name` | Yes | Human-readable rule name used in logs, REST output, and operator tools. It is not a routing key. |
| `alias` | No | Secondary display label/grouping. It does not change routing or create another TGID. |
| `source.tgid` | Yes | Canonical internal talkgroup ID. Traffic is normalized to this value before normal rule processing; zero makes the runtime entry invalid. |
| `source.slot` | Yes | Canonical DMR slot (`1` or `2`). For P25, NXDN, and analog paths, lookups frequently use slot zero as a wildcard and the configured value is normally `1`. |

The `(tgid, slot)` pair identifies a DMR rule. A lookup with a nonzero slot requires an exact pair; a lookup with slot zero returns the first matching TGID regardless of configured slot. Because the table is a vector searched in file order, duplicate TGIDs or duplicate pairs are ambiguous rather than merged. Keep each canonical pair unique and do not rely on ordering to select a duplicate.

## Where each field is used

The FNE loads the authoritative table with talkgroup ACL enforcement enabled. A host independently loads the file named by `system.talkgroup_id.file`; its local enforcement is controlled by `system.talkgroup_id.acl`. The same YAML model is used by both components, but several fields are meaningful only at the FNE.

| Field | FNE use | Host use |
| --- | --- | --- |
| `name`, `alias` | Operator-facing rule metadata, logging, REST output, and full-table replication. | Local metadata when loaded from disk; not needed for over-the-wire active-TG validation. |
| `source.tgid` | Canonical group identity used for admission, routing, and rewrites. | Group identity used by RF voice, data, control-signaling, grant, and affiliation validation. |
| `source.slot` | DMR identity and rewrite slot; normally ignored by FDMA protocols through slot-zero lookup. | Exact DMR slot validation and a preferred slot for group-channel selection. P25 and NXDN lookups are TGID based. |
| `active` | Rejects inactive group traffic and controls whether an active or deactivation record is sent to a host. | Required by local ACL validation. A deactivation received from the FNE removes the matching local runtime rule. |
| `affiliated` | Gates delivery to sites with a current group affiliation, subject to peer-class and topology exceptions. | Requires a current affiliation before trunking grant processing, unless that host operation explicitly ignores the affiliation check. |
| `strapping` | Enforces observable clear/encrypted state and sends the policy to hosts. Protocol limitations are described below. | Enforces supported RF-side encrypted-only checks. Host enforcement is not symmetrical for every protocol and value; see [Encryption strapping](#encryption-strapping). |
| `parrot` | Selects the FNE record-and-replay path. | No local host parrot service behavior. |
| `inclusion`, `exclusion` | Filters live recipients and the active/deactive/RID-admission datasets sent to each peer. | Not evaluated as a local RF routing list. A host normally receives only the subset selected for its peer ID. |
| `rewrite` | Bidirectionally maps a peer's external TGID/slot to the canonical identity. | Not applied by the host talkgroup lookup. |
| `always` | Bypasses affiliation gating for listed recipients and some originating-call unknown-RID/per-TG RID rejections. | Not evaluated by host access control. |
| `preferred` | Causes otherwise eligible, unlisted peers to receive the rule with the `nonPreferred` flag. | Uses the received `nonPreferred` state; it does not evaluate the complete `preferred` peer list locally. |
| `rid_permitted` | Restricts originating group traffic and grant/admission checks, and is distributed to eligible hosts. | Restricts the source RID wherever the protocol path invokes talkgroup-access validation, including group voice, data, control signaling, and grants. P25 also applies it to its validated affiliation paths. |

### FNE processing path

For an ordinary group call, the FNE applies the table in these stages:

1. It resolves the sender's external TGID/slot through any matching per-peer rewrite and obtains the canonical rule. DMR and P25 Phase 2 include the slot in this translation; P25 Phase 1, NXDN, and analog translate the TGID.
2. It validates the canonical talkgroup, active state, originating RID admission, and any encryption state that is available at that point in the protocol. Promiscuous-hub mode intentionally relaxes normal TG/RID ACL validation.
3. It evaluates every candidate recipient. Replica and SysView peers are trusted recipient classes and bypass ordinary recipient filtering. For other peers, inclusion/exclusion is evaluated first, followed by `always` and affiliation policy.
4. It rewrites the canonical TGID/slot to the recipient's external identity before transmitting the protocol frames and applicable control messages.

An `always` entry is broader than an affiliation exception. On the ingress/source side, an originating peer in `always` can also bypass some unknown-RID and `rid_permitted` rejection paths. It still does not make an inactive talkgroup active and does not override inclusion/exclusion filtering on delivery.

Affiliation routing has deliberate topology exceptions. Traffic received from an upstream/neighbor FNE is repeated without requiring that link itself to hold the downstream site affiliation. Console peers bypass affiliation gating, and conventional peers can do so when the conventional-site affiliation override is enabled. For voice-channel peers, the FNE checks affiliation against the associated control-channel peer ID rather than assuming the voice-channel peer owns the affiliation database entry.

### Host processing path

With `system.talkgroup_id.acl: true`, the host rejects an unknown, inactive, or invalid talkgroup before accepting applicable RF voice, packet data, control-signaling, affiliation, or grant activity. DMR additionally requires the configured slot to match. P25 has a narrowly scoped `forceAllowTG0` compatibility path; it should not be treated as general permission for an undefined group. A nonempty `rid_permitted` list then requires an exact source-RID match. With the local talkgroup ACL disabled, unknown nonzero groups generally pass these table checks and `rid_permitted` is not enforced.

For trunking operations, `affiliated: true` requires the source radio/site state to show an affiliation with that group before a grant is issued, except in code paths configured to ignore affiliation. The host also consumes `nonPreferred` state derived by the FNE from `preferred`; this can discourage use of a non-preferred site and influence subscriber roaming behavior.

When both `network.updateLookups` and FNE `system.sendTalkgroups` are enabled, the host receives a reduced operational view rather than the complete YAML rule:

- Active records carry TGID, slot, `affiliated`, derived `nonPreferred`, and `strapping` state.
- Deactivation records remove the matching runtime rule.
- A TG-admission update first clears the existing runtime `rid_permitted` lists, then installs the transmitted nonempty lists on matching rules.
- FNE peer routing lists determine which records are sent; fields such as `rewrite`, `always`, `parrot`, and the peer lists themselves are not sent as host policy.

An active record creates a missing host rule. If a matching local rule already exists, the current receiver normally preserves that local entry rather than merging all transmitted attributes; a received non-preferred record is the exception and replaces the matching local entry with the FNE-derived state. Administrators should therefore avoid assuming that `updateLookups` overwrites every field of a pre-existing local rule. `network.saveLookups` can persist network-delivered lookup changes.

A replica FNE is different: it receives the complete YAML file in fragments and reloads that file, rather than receiving only the reduced host dataset.

## Core policy

| Key | Meaning/default behavior |
| --- | --- |
| `config.active` | Enables admission and distribution of the rule. The FNE sends active rules to eligible peers and explicitly sends inactive rules as deactivations. This is preferable to deleting a temporarily disabled rule because identity and policy remain visible. Default false. |
| `config.affiliated` | Send traffic only to sites that currently have a group affiliation for the TGID. This reduces needless wide-area traffic. Neighbor FNE links repeat regardless; `always` peers bypass this check; consoles and optionally conventional peers also bypass it. Default false. |
| `config.strapping` | Encryption-state policy: `0` selectable, `1` encrypted-only, `2` clear-only. Enforcement depends on component, protocol, and when encryption state becomes observable; see [Encryption strapping](#encryption-strapping). Default 0. |
| `config.parrot` | Treat the TGID as record-and-replay. Eligible inbound call frames are retained and replayed after the FNE parrot delay instead of being handled as an ordinary live distribution path. Default false. |

`strapping` and `parrot` are declared by the schema and validated according to their runtime types and accepted values.

`active` determines whether the talkgroup itself is valid; it does not mean that every peer receives it. Peer visibility is filtered by `inclusion`/`exclusion`, and actual call delivery can additionally depend on affiliation, peer class, protocol enablement, slot policy, RID ACLs, call collisions, and topology.

## Peer routing lists

> **Important:** Use either a nonempty `inclusion` list or a nonempty `exclusion` list for a rule—never both. Keeping both keys with empty arrays is valid and is how the example expresses unrestricted peer routing. If both lists contain peer IDs, the FNE tests `inclusion` first and does not evaluate `exclusion`; the entire exclusion list is ignored. The verbose rule loader also logs a warning for this condition.

Choose the list based on the intended policy:

- Use `inclusion` when the talkgroup should reach only a known subset of peers. Every unlisted normal peer is denied.
- Use `exclusion` when the talkgroup should reach most peers except a small denied subset. Every unlisted normal peer remains eligible.
- Leave both lists empty when the talkgroup should have no peer-list restriction. Other rules such as affiliation and RID admission still apply.

| Key | Meaning |
| --- | --- |
| `inclusion` | Allow-list for peer visibility and normal call delivery. When nonempty, an unlisted peer is filtered even if it is absent from `exclusion`. |
| `exclusion` | Deny-list used only when `inclusion` is empty. It is convenient when nearly every peer should carry the TGID. |
| `always` | Peers that bypass subsequent affiliation and some unknown-RID rejection decisions after they have passed inclusion/exclusion filtering. It does not override inclusion/exclusion. |
| `preferred` | Preferred site peer IDs. For each visible peer not in a nonempty list, the FNE marks the distributed rule non-preferred; host access-control logic can use that state to discourage or deny affiliation and drive subscriber roaming. Empty means no peer is marked non-preferred. |

The effective recipient decision is approximately:

```text
replica or SysView peer                 -> permitted by recipient filtering
inclusion nonempty and peer not listed -> deny
otherwise peer listed in exclusion     -> deny
peer listed in always                  -> permit without affiliation check
affiliated=true and no site affiliation-> deny
otherwise                              -> permit
```

This precedence is used by ordinary live routing in the DMR, P25, P25 Phase 2, NXDN, and analog call handlers. The same inclusion-first branch is used when the FNE constructs peer-specific active/deactive talkgroup lists and distributed RID-admission datasets. Consequently, using both lists does not produce an allow-list with exceptions in those paths: it produces only an allow-list.

Parrot playback is a notable implementation exception. Its protocol-specific playback loops test a nonempty inclusion list and then independently test exclusion, rather than using the normal `else if` precedence. If both lists are populated, parrot playback therefore behaves like `inclusion` minus `exclusion`, while live traffic and lookup distribution ignore exclusion. This inconsistency is another reason that a rule must use only one routing-list mode.

Replica/SysView exceptions are high-trust peer-class behavior in the FNE call handlers. Inclusion/exclusion govern routing eligibility; they do not authenticate a connection. If `always` and `affiliated` are both set, the loader warns because listed peers deliberately override affiliation gating.

## Per-peer rewrite

`config.rewrite` maps the canonical TGID/slot to the identifiers used by one peer:

```yaml
rewrite:
  - peerid: 9000990
    tgid: 9999
    slot: 2
```

| Key | Meaning |
| --- | --- |
| `peerid` | Peer for which this external representation applies. The spelling is lowercase `peerid`. Zero is syntactically loadable but should not be used. |
| `tgid` | TGID received from and transmitted to that peer. It is translated to/from the rule's canonical `source.tgid`. |
| `slot` | DMR slot received from and transmitted to that peer. Slot-zero lookups can act as a wildcard on FDMA paths. |

On ingress, the FNE first tries the canonical source TGID and then searches rewrites for the sending peer; a matching rewritten identity resolves to the canonical rule. On egress to that peer, the canonical identity is replaced with its configured TGID/slot. This makes a single rewrite logically bidirectional for that FNE/peer boundary. It does not automatically create equivalent mappings on another FNE, host, or DVMPatch instance.

Only one rewrite per peer and external `(tgid,slot)` should match. The lookup returns the first matching rule, so duplicate external identities are ambiguous. Avoid cycles where separate rules, FNEs, or patches translate a call back and forth. Rewriting changes group addressing only; it does not rewrite the source RID, authorize the peer, bypass RID admission, or transform encryption.

## RID admission

`config.rid_permitted` is a talkgroup-specific allow-list of subscriber RIDs permitted to originate or request applicable group service on the canonical `(tgid,slot)`. It is enforced only in protocol paths that call talkgroup-access validation; P25 affiliation processing does so, while affiliation handling is not identical across all protocols. With talkgroup ACL enforcement active:

- An invalid/missing talkgroup denies the RID.
- An empty list admits every RID that passes the other ACL layers; it does not deny all.
- A nonempty list requires an exact RID match.
- When the FNE distributes admissions, zero entries are removed and the list is sorted and deduplicated.

This is an additional layer, not a replacement for `rid_acl.dat`. A caller may need to pass global RID enablement, unknown-RID policy, peer permissions, and this talkgroup-specific list. The list controls source admission in the paths that consult it, not which sites receive an otherwise valid call.

Private/unit-to-unit and telephone-interconnect calls are not group talkgroup traffic and use their dedicated routing policy.

## Parrot rules

A parrot rule normally has `parrot: true`, a unique TGID/slot, and carefully scoped peer routing. The FNE buffers the completed eligible call and starts playback after `master.parrotDelay`. `parrotGrantDemand` optionally generates a resource request, `parrotOnlyToOrginiatingPeer` limits replay scope, and `parrotOverrideSrcId` can replace the replay source RID. When replay is not restricted to the originating peer, current peer-list and affiliation eligibility is checked again. The routing-list exception described above applies to this playback loop.

Source-RID override changes the replay envelope/source value. In the DMR implementation, embedded link-control portions of already recorded frames are not comprehensively regenerated, so consumers that inspect embedded LC can still observe the original source. Avoid enabling parrot on routine operational TGIDs: replay consumes airtime and can be mistaken for a new live transmission.

## Encryption strapping

The values are policy declarations, not key-management controls:

- `0` (`selectable`) permits either clear or encrypted traffic.
- `1` (`strapped`) requires encrypted traffic where the implementation can determine encryption state.
- `2` (`clear-only`) rejects encrypted traffic where that state is checked.

At the FNE, P25 and NXDN handlers enforce both encrypted-only and clear-only conflicts once the relevant header/link-control state is observable. DMR privacy indication can arrive too late for equivalent, reliable call-start enforcement, so DMR FNE handling is not symmetrical. Analog handling also has only the checks supported by its transported metadata.

At the host RF boundary, DMR, P25, and NXDN voice paths contain encrypted-only (`strapping: 1`) checks. The host code does not provide an equivalent clear-only check in every corresponding path. NXDN grant requests cannot be rejected by strapping at request time because the VCALL request does not carry encryption state; enforcement occurs when voice information is available. Treat strapping as an admission guard, not proof of end-to-end encryption, and test the exact protocol, call type, and control-channel mode used by the deployment.

## Protocol considerations

- DMR rules distinguish Slot 1 and Slot 2. The same TGID can have separate rules per slot, but network and host slot enablement must also permit the selected slot.
- P25, NXDN, and analog normally use slot `1` in the file; their runtime lookups commonly pass slot zero and match by TGID.
- Protocol-wide IDs such as P25 `65535` or DMR `16777215` must be intentional; they can produce very broad behavior.
- Affiliation and preference are trunking concepts. Conventional and console peer exceptions exist in FNE routing, so conventional interoperability needs an explicit policy rather than blindly enabling `affiliated`.
- Encryption strapping cannot create or distribute keys. Configure key services and endpoint crypto separately.

## Deployment checklist

1. Ensure every `(tgid, slot)` pair is unique and active only where intended.
2. Resolve inclusion/exclusion/always contradictions.
3. Verify every peer ID against the peer list and actual topology.
4. Trace each rewrite in both directions and check for loops with DVMPatch mappings.
5. Confirm affiliation, preferred-site, and RID admission behavior in a lab topology.
6. Verify parrot and system-wide IDs cannot leak traffic to unintended peers.
7. Reload the FNE table and inspect loader warnings before live use.

## Reload, replication, and editing behavior

The loader parses a complete YAML document, then replaces the in-memory vector. A missing or empty `groupVoice` list fails the load. Periodic reload time is configured by the owning host/FNE configuration; a zero interval disables the reload thread. FNE REST operations can also modify and commit the in-memory table.

When `system.sendTalkgroups` is enabled, the FNE sends each peer only rules visible through that rule's inclusion/exclusion policy. Active and inactive states are sent separately, and only nonempty RID-admission lists are encoded after zero removal, sorting, and deduplication. On receipt of an admission update, the host clears all current per-rule admission lists before applying the transmitted records, so an omitted rule becomes unrestricted rather than retaining a stale list. Replica peers can receive the full replicated table. Host-side network updates should therefore be understood as policy synchronization, not merely alias synchronization.

Use atomic file replacement when editing a live deployment. A schema-valid but semantically conflicting file may still load successfully; verbose mode is valuable because it reports rule counts and warns about inclusion/exclusion and affiliation/always contradictions.

See the [FNE example configuration](../../configs/fne-config.example.yml), [host example configuration](../../configs/config.example.yml), and [lookup/data-file reference](lookup-files.md).
