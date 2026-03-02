# Adaptive Jitter Buffer Configuration Guide

## Overview

The FNE (Fixed Network Equipment) includes an adaptive jitter buffer system that can automatically reorder out-of-sequence RTP packets from peers experiencing network issues such as:

- **Satellite links** with high latency and variable jitter
- **Cellular connections** with packet reordering
- **Congested network paths** causing sporadic delays
- **Multi-path routing** leading to out-of-order delivery

The jitter buffer operates with **zero latency for perfect networks** - if packets arrive in order, they pass through immediately without buffering. Only out-of-order packets trigger the adaptive buffering mechanism.

## How It Works

### Zero-Latency Fast Path
When packets arrive in perfect sequence order, they are processed immediately with **no additional latency**. The jitter buffer is effectively transparent.

### Adaptive Reordering
When an out-of-order packet is detected:
1. The jitter buffer holds the packet temporarily
2. Waits for missing packets to arrive
3. Delivers frames in correct sequence order
4. Times out after a configurable period if gaps persist

### Per-Peer, Per-Stream Isolation
- Each peer connection can have independent jitter buffer settings
- Within each peer, each call/stream has its own isolated buffer
- This prevents one problematic stream from affecting others

## Configuration

### Location

Jitter buffer configuration is defined in the FNE configuration file (typically `fne-config.yml`) under the `master` section:

```yaml
master:
    # ... other master configuration ...
    
    jitterBuffer:
        enabled: false
        defaultMaxSize: 4
        defaultMaxWait: 40000
```

### Parameters

#### Global Settings

- **enabled** (boolean, default: `false`)
  - Master enable/disable switch for jitter buffering
  - When `false`, all peers operate with zero-latency pass-through
  - When `true`, peers use jitter buffering with default parameters

- **defaultMaxSize** (integer, range: 2-8, default: `4`)
  - Maximum number of frames to buffer per stream
  - Larger values provide more reordering capability but add latency
  - **Recommended values:**
    - `4` - Standard networks (LAN, stable WAN)
    - `6` - High-jitter networks (cellular, congested paths)
    - `8` - Extreme conditions (satellite, very poor links)

- **defaultMaxWait** (integer, range: 10000-200000 microseconds, default: `40000`)
  - Maximum time to wait for missing packets
  - Frames older than this are delivered even with gaps
  - **Recommended values:**
    - `40000` (40ms) - Terrestrial networks
    - `60000` (60ms) - Cellular networks
    - `80000` (80ms) - Satellite links

Per-Peer overrides occur with the jitter buffer parameters within the peer ACL file. The same global parameters, apply
there but on a per-peer basis. Global jitter buffer parameters take precedence over per-peer.

## Configuration Examples

### Example 1: Disabled (Default)

For networks with reliable connectivity:

```yaml
master:
    jitterBuffer:
        enabled: false
        defaultMaxSize: 4
        defaultMaxWait: 40000
```

All peers operate with zero-latency pass-through. Best for:
- Local area networks
- Stable dedicated connections
- Networks with minimal packet loss/reordering

### Example 2: Global Enable with Defaults

Enable jitter buffering for all peers with conservative settings:

```yaml
master:
    jitterBuffer:
        enabled: true
        defaultMaxSize: 4
        defaultMaxWait: 40000
```

Good starting point for:
- Mixed network environments
- Networks with occasional jitter
- General purpose deployments

## Performance Characteristics

### CPU Impact

- **Zero-latency path:** Negligible overhead (~1 comparison per packet)
- **Buffering path:** Minimal overhead (~map lookup + timestamp check)
- **Memory:** ~500 bytes per active stream buffer

### Latency Impact

- **In-order packets:** 0ms additional latency
- **Out-of-order packets:** Buffered until:
  - Missing packets arrive, OR
  - `maxWait` timeout expires
- **Typical latency:** 10-40ms for reordered packets on terrestrial networks

### Effectiveness

Based on the adaptive jitter buffer design:
- **100% pass-through** for perfect networks (zero latency)
- **~95-99% recovery** of out-of-order packets within timeout window
- **Automatic timeout delivery** prevents indefinite stalling

## Troubleshooting

### Symptom: Audio/Data Gaps Despite Jitter Buffer

**Possible Causes:**
1. `maxWait` timeout too short for network conditions
2. `maxSize` buffer too small for reordering depth
3. Actual packet loss (not just reordering)

**Solutions:**
- Increase `maxWait` by 20-40ms increments
- Increase `maxSize` by 1-2 frames
- Verify network packet loss with diagnostics

### Symptom: Excessive Latency

**Possible Causes:**
1. Jitter buffer enabled on stable connections
2. `maxWait` set too high
3. `maxSize` set too large

**Solutions:**
- Disable jitter buffer for known-good peers using overrides
- Reduce `maxWait` in 10-20ms decrements
- Reduce `maxSize` to minimum (2-4 frames)

### Symptom: No Improvement

**Possible Causes:**
1. Jitter buffer not actually enabled for the problematic peer
2. Issues beyond reordering (e.g., corruption, auth failures)
3. Problems at application layer, not transport layer

**Solutions:**
- Verify peer override configuration is correct
- Check FNE logs for peer-specific configuration messages
- Enable verbose and debug logging to trace packet flow

## Best Practices

1. **Start Disabled**: Begin with jitter buffering disabled and enable only as needed
2. **Target Specific Peers**: Use per-peer overrides rather than global enable when possible
3. **Conservative Tuning**: Start with default parameters and adjust incrementally
4. **Monitor Performance**: Watch for signs of latency or audio quality issues
5. **Document Changes**: Keep records of which peers need special configuration
6. **Test Thoroughly**: Validate changes don't introduce unintended latency

## Reference

### Configuration Schema

```yaml
jitterBuffer:
    enabled: <boolean>          # false
    defaultMaxSize: <2-8>       # 4
    defaultMaxWait: <10000-200000>  # 40000
```
