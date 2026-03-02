# dvmcfggen - Quick Reference

## Installation
```bash
cd dvmcfg
pip install -r requirements.txt
# or just run ./dvmcfg (auto-creates venv)
```

## Common Commands

### Interactive Wizard
```bash
# Guided configuration (recommended for beginners)
./dvmcfg wizard
./dvmcfg wizard --type single    # Single instance
./dvmcfg wizard --type trunk     # Trunked system
```

### Single Configuration
```bash
# Create from template
./dvmcfg create --template <name> --output <file> [options]

# Validate
./dvmcfg validate <file> [--summary]

# Edit value
./dvmcfg edit <file> <key> <value>

# List templates
./dvmcfg templates
```

### Trunked Systems
```bash
# Create system
./dvmcfg trunk create --base-dir <dir> --vc-count <n> [options]

# Validate system
./dvmcfg trunk validate --base-dir <dir> --name <name>

# Update all configs
./dvmcfg trunk update --base-dir <dir> --name <name> <key> <value>
```

## Quick Examples

### Hotspot
```bash
./dvmcfg create --template hotspot --output hotspot.yml \
    --identity "HS001" --peer-id 100001 --callsign "KC1ABC"
```

### P25 Trunk (4 VCs)
```bash
./dvmcfg trunk create --base-dir /etc/dvm/trunk \
    --protocol p25 --vc-count 4 --identity "SITE001" \
    --base-peer-id 100000 --nac 0x001
```

### DMR Trunk (2 VCs)
```bash
./dvmcfg trunk create --base-dir /etc/dvm/dmr \
    --protocol dmr --vc-count 2 --color-code 2
```

## Common Config Keys

```
network.id              - Peer ID
network.address         - FNE address  
network.port            - FNE port
system.identity         - System ID
system.config.nac       - P25 NAC
system.config.colorCode - DMR color code
system.config.siteId    - Site ID
system.modem.rxLevel    - RX level (0-100)
system.modem.txLevel    - TX level (0-100)
protocols.p25.enable    - Enable P25
protocols.dmr.enable    - Enable DMR
```

## Templates

- `hotspot` - Simplex hotspot
- `repeater` - Duplex repeater
- `control-channel-p25` - P25 CC for trunking
- `control-channel-dmr` - DMR CC for trunking
- `voice-channel` - VC for trunking
- `conventional` - Conventional with grants

## File Structure

Single config:
```
config.yml
```

Trunked system (name: "test", 2 VCs):
```
test-cc.yml       # Control channel
test-vc01.yml     # Voice channel 1
test-vc02.yml     # Voice channel 2
```

## Default Values

| Parameter | Default | Notes |
|-----------|---------|-------|
| FNE Address | 127.0.0.1 | Localhost |
| FNE Port | 62031 | Standard FNE port |
| Base Peer ID | 100000 | CC gets this, VCs increment |
| Base RPC Port | 9890 | CC gets this, VCs increment |
| P25 NAC | 0x293 | Standard NAC |
| DMR Color Code | 1 | Standard CC |
| Site ID | 1 | First site |
| Modem Type | uart | Serial modem |

## Validation Checks

- IP addresses (format and range)
- Port numbers (1-65535)
- Hex keys (length and format)
- NAC (0-4095), Color Code (0-15), RAN (0-63)
- Identity/callsign (length limits)
- RX/TX levels (0-100)
- Trunk consistency (NAC, CC, Site ID match)

## Tips

✓ Always validate after creation/editing
✓ Use `--summary` to see config overview
✓ Test with `--modem-type null` first
✓ Keep peer IDs sequential
✓ Use trunk update for system-wide changes
✓ Backup configs before bulk updates

## Help

```bash
./dvmcfg --help
./dvmcfg create --help
./dvmcfg trunk create --help
```

See USAGE.md and EXAMPLES.md for detailed documentation.
