# callsim

`callsim` is a command-line FNE peer that generates deterministic group voice
calls for exercising DMR, P25, and NXDN call handling. It authenticates to an
FNE traffic port as one peer and can emit calls sequentially or multiplex
simultaneous calls to several talkgroups over that single connection.

The generated audio is a fixed calibration-tone test pattern. P25 uses the
project's 1011 Hz IMBE vector, while DMR uses its 1031 Hz AMBE vector. NXDN
builds its voice payload from the fixed AMBE test bits and applies the NXDN
voice encoding and interleaving used by the common library. This is synthetic
protocol traffic; the tool does not read audio from a file or sound device.

## Build

Build the target from a configured repository build tree:

```sh
cmake --build build --target callsim -j6
```

Depending on whether `dvmhost` is built directly or as part of the
superproject, the executable will normally be `build/callsim` or
`build/dvmhost/callsim`.

## Basic usage

The protocol mode and at least one destination talkgroup are required:

```sh
build/callsim \
    --address 127.0.0.1 \
    --port 62031 \
    --peer-id 9000998 \
    --password passw0rd \
    --mode p25 \
    --talkgroups 1001 \
    --calls 3 \
    --duration 5
```

This example logs in as peer `9000998` and sends three five-second P25 Phase 1
group calls to talkgroup `1001`. Source radio IDs begin at `100001` and increase
by one for every generated call.

## Options

| Option | Description |
| --- | --- |
| `--address <host>` | FNE address. The default is `127.0.0.1`. |
| `--port <port>` | FNE UDP traffic port. The default is `62031`. |
| `--peer-id <id>` | Peer ID used for FNE authentication. The default is `9000998`. |
| `--password <password>` | FNE peer password. The default is `passw0rd`. |
| `--identity <name>` | Peer identity reported to the FNE. The default is `CallSim`. |
| `--mode <mode>` | Required digital mode: `p25`, `dmr`, or `nxdn`. |
| `--talkgroups <list>` | Required comma-separated destination talkgroup IDs. |
| `--calls <count>` | Number of calls, or simultaneous-call batches. The default is `1`. |
| `--duration <seconds>` | Duration of each call in seconds. The default is `5`. |
| `--gap <ms>` | Delay between calls or batches in milliseconds. The default is `500`. |
| `--source <rid>` | Source radio ID for the first call. The default is `100001`. |
| `--slots <list>` | Comma-separated DMR slots (`1` or `2`), cycled as needed. The default is slot `1`. |
| `--grant-demand` | Sends an FNE group-call grant request before every call. |
| `--simultaneous` | Starts one call per configured talkgroup in each batch. |
| `--debug` | Enables verbose network logging. |
| `--help`, `-h` | Displays command-line help. |

P25 and DMR source and destination IDs may be up to 24 bits. NXDN source and
destination IDs must not exceed `65535`. The `--slots` setting is used only for
DMR; P25 and NXDN are FDMA modes.

## Multiple talkgroups

Without `--simultaneous`, calls are sent one at a time and the talkgroup and
DMR slot lists are selected round-robin:

```sh
build/callsim \
    --mode dmr \
    --talkgroups 1001,1002 \
    --slots 1,2 \
    --calls 6 \
    --duration 10
```

This produces six sequential calls, alternating talkgroups `1001` and `1002`
and slots 1 and 2.

With `--simultaneous`, each repetition starts one call for every talkgroup:

```sh
build/callsim \
    --mode dmr \
    --talkgroups 1001,1002 \
    --slots 1,2 \
    --calls 3 \
    --duration 10 \
    --grant-demand \
    --simultaneous
```

This produces three batches of two concurrent calls. Each call has independent
RTP stream and sequence state, but all traffic uses one authenticated FNE peer
connection. The gap is applied between completed batches, not between members
of a batch. If fewer slots than talkgroups are supplied, the slot list cycles;
concurrent calls assigned to the same DMR slot may be rejected or contend under
the connected FNE's policy.

## Grant requests and call termination

By default, voice traffic begins without a preceding grant request. Add
`--grant-demand` when the target system should receive an FNE group-call grant
request before each voice stream. The request uses the call's source ID,
talkgroup, and DMR slot where applicable.

The simulator sends normal protocol-specific call termination signalling after
the requested duration: a TDU for P25, a terminator-with-link-control burst for
DMR, or an RTCH transmission release for NXDN. `SIGINT` and `SIGTERM` request an
orderly shutdown.

The peer ID and credentials must be accepted by the target FNE, and the selected
source IDs, talkgroups, modes, and slots remain subject to its access and grant
policy. Use the simulator only against a test system where generated call
traffic is expected.
