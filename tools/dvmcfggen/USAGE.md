# dvmcfggen - Usage Guide
# DVMCfg Usage Guide

## Getting Started

### Interactive Wizard (Recommended)

The easiest way to create configurations:

```bash
./dvmcfg wizard
```

The wizard provides:
- Step-by-step guided configuration
- Input validation in real-time
- Template selection help
- Configuration summary before saving
- Support for both single and trunked systems

**Wizard Types:**
```bash
# Auto-detect (asks user what to create)
./dvmcfg wizard

# Single instance wizard
./dvmcfg wizard --type single

# Trunking system wizard
./dvmcfg wizard --type trunk
```

---

## Command Reference

### Create Configuration

Create a new DVMHost configuration from a template:

```bash
./dvmcfg create --template <template_name> --output <file_path> [options]
```

**Options:**
- `--template` - Template to use (required)
- `--output, -o` - Output file path (required)
- `--identity` - System identity (e.g., "REPEATER01")
- `--peer-id` - Network peer ID
- `--fne-address` - FNE server address
- `--fne-port` - FNE server port
- `--callsign` - CWID callsign
- `--validate` - Validate configuration after creation

**Examples:**

```bash
# Create a basic hotspot configuration
./dvmcfg create --template hotspot --output /etc/dvm/hotspot.yml \
    --identity "HOTSPOT01" --peer-id 100001 --callsign "KC1ABC"

# Create a repeater configuration
./dvmcfg create --template repeater --output /etc/dvm/repeater.yml \
    --identity "REPEATER01" --peer-id 100002 \
    --fne-address "192.168.1.100" --fne-port 62031

# Create P25 control channel
./dvmcfg create --template control-channel-p25 --output /etc/dvm/cc.yml \
    --identity "CC001" --peer-id 100000 --validate
```

### Validate Configuration

Validate an existing configuration file:

```bash
./dvmcfg validate <config_file> [--summary]
```

**Options:**
- `--summary, -s` - Display configuration summary

**Examples:**

```bash
# Basic validation
./dvmcfg validate /etc/dvm/config.yml

# Validation with summary
./dvmcfg validate /etc/dvm/config.yml --summary
```

### Edit Configuration

Edit a specific configuration value:

```bash
./dvmcfg edit <config_file> <key> <value>
```

**Examples:**

```bash
# Change system identity
./dvmcfg edit /etc/dvm/config.yml system.identity "NEWID"

# Update FNE address
./dvmcfg edit /etc/dvm/config.yml network.address "192.168.1.200"

# Enable DMR protocol
./dvmcfg edit /etc/dvm/config.yml protocols.dmr.enable true

# Change color code
./dvmcfg edit /etc/dvm/config.yml system.config.colorCode 2
```

### Trunked System Management

#### Create Trunked System

Create a complete trunked system with control and voice channels:

```bash
./dvmcfg trunk create --base-dir <directory> [options]
```

**Options:**
- `--base-dir` - Base directory for configs (required)
- `--name` - System name (default: "trunked")
- `--protocol` - Protocol type: p25 or dmr (default: p25)
- `--vc-count` - Number of voice channels (default: 2)
- `--fne-address` - FNE address (default: 127.0.0.1)
- `--fne-port` - FNE port (default: 62031)
- `--fne-password` - FNE password
- `--base-peer-id` - Base peer ID (default: 100000)
- `--base-rpc-port` - Base RPC port (default: 9890)
- `--identity` - System identity prefix (default: SITE001)
- `--nac` - P25 NAC in hex (default: 0x293)
- `--color-code` - DMR color code (default: 1)
- `--site-id` - Site ID (default: 1)
- `--modem-type` - Modem type: uart or null (default: uart)

**Examples:**

```bash
# Create basic P25 trunked system with 2 voice channels
./dvmcfg trunk create --base-dir /etc/dvm/trunk \
    --name test --vc-count 2

# Create P25 trunked system with 4 voice channels
./dvmcfg trunk create --base-dir /etc/dvm/site1 \
    --name site1 --protocol p25 --vc-count 4 \
    --identity "SITE001" --base-peer-id 100000 \
    --fne-address "10.0.0.1" --nac 0x001

# Create DMR trunked system
./dvmcfg trunk create --base-dir /etc/dvm/dmr_trunk \
    --protocol dmr --vc-count 3 --color-code 2
```

**Generated Files:**

For a system named "test" with 2 voice channels:
- `test-cc.yml` - Control channel configuration
- `test-vc01.yml` - Voice channel 1 configuration
- `test-vc02.yml` - Voice channel 2 configuration

#### Validate Trunked System

Validate all configurations in a trunked system:

```bash
./dvmcfg trunk validate --base-dir <directory> [--name <system_name>]
```

**Examples:**

```bash
# Validate trunked system
./dvmcfg trunk validate --base-dir /etc/dvm/trunk --name test
```

#### Update Trunked System

Update a setting across all configurations in a system:

```bash
./dvmcfg trunk update --base-dir <directory> --name <system_name> <key> <value>
```

**Examples:**

```bash
# Update FNE address across all configs
./dvmcfg trunk update --base-dir /etc/dvm/trunk \
    --name test network.address "10.0.0.100"

# Update NAC across all configs
./dvmcfg trunk update --base-dir /etc/dvm/trunk \
    --name test system.config.nac 0x001

# Enable verbose logging on all instances
./dvmcfg trunk update --base-dir /etc/dvm/trunk \
    --name test protocols.p25.verbose true
```

### List Templates

Display all available configuration templates:

```bash
./dvmcfg templates
```

## Configuration Key Paths

Common configuration key paths for use with `edit` and `trunk update` commands:

### Network Settings
- `network.id` - Peer ID
- `network.address` - FNE address
- `network.port` - FNE port
- `network.password` - FNE password
- `network.encrypted` - Enable encryption (true/false)
- `network.rpcPort` - RPC port
- `network.rpcPassword` - RPC password
- `network.restEnable` - Enable REST API (true/false)
- `network.restPort` - REST API port

### System Settings
- `system.identity` - System identity
- `system.duplex` - Duplex mode (true/false)
- `system.timeout` - Call timeout (seconds)
- `system.config.colorCode` - DMR color code (0-15)
- `system.config.nac` - P25 NAC (0-4095)
- `system.config.ran` - NXDN RAN (0-63)
- `system.config.siteId` - Site ID
- `system.config.channelNo` - Channel number

### Protocol Settings
- `protocols.dmr.enable` - Enable DMR (true/false)
- `protocols.dmr.control.enable` - Enable DMR control (true/false)
- `protocols.dmr.verbose` - DMR verbose logging (true/false)
- `protocols.p25.enable` - Enable P25 (true/false)
- `protocols.p25.control.enable` - Enable P25 control (true/false)
- `protocols.p25.control.dedicated` - Dedicated control channel (true/false)
- `protocols.p25.verbose` - P25 verbose logging (true/false)
- `protocols.nxdn.enable` - Enable NXDN (true/false)

### Modem Settings
- `system.modem.protocol.type` - Modem type (uart/null)
- `system.modem.protocol.uart.port` - Serial port
- `system.modem.protocol.uart.speed` - Serial speed
- `system.modem.rxLevel` - RX level (0-100)
- `system.modem.txLevel` - TX level (0-100)

### CW ID Settings
- `system.cwId.enable` - Enable CWID (true/false)
- `system.cwId.time` - CWID interval (minutes)
- `system.cwId.callsign` - Callsign

## Workflow Examples

### Setting Up a Standalone Hotspot

```bash
# Create configuration
./dvmcfg create --template hotspot --output /etc/dvm/hotspot.yml \
    --identity "HOTSPOT01" --peer-id 100001 \
    --fne-address "fne.example.com" --fne-port 62031 \
    --callsign "KC1ABC"

# Validate
./dvmcfg validate /etc/dvm/hotspot.yml --summary

# Customize modem settings
./dvmcfg edit /etc/dvm/hotspot.yml system.modem.protocol.uart.port "/dev/ttyACM0"
./dvmcfg edit /etc/dvm/hotspot.yml system.modem.rxLevel 60
./dvmcfg edit /etc/dvm/hotspot.yml system.modem.txLevel 60
```

### Setting Up a 4-Channel P25 Trunked System

```bash
# Create trunked system
./dvmcfg trunk create --base-dir /etc/dvm/site1 \
    --name site1 --protocol p25 --vc-count 4 \
    --identity "SITE001" --base-peer-id 100000 \
    --fne-address "10.0.0.1" --fne-port 62031 \
    --nac 0x001 --site-id 1

# Validate system
./dvmcfg trunk validate --base-dir /etc/dvm/site1 --name site1

# Update modem type on all instances
./dvmcfg trunk update --base-dir /etc/dvm/site1 \
    --name site1 system.modem.protocol.type uart

# Customize individual configs as needed
./dvmcfg edit /etc/dvm/site1/site1-cc.yml \
    system.modem.protocol.uart.port "/dev/ttyUSB0"
./dvmcfg edit /etc/dvm/site1/site1-vc01.yml \
    system.modem.protocol.uart.port "/dev/ttyUSB1"
```

### Migrating Between FNE Servers

```bash
# Update single config
./dvmcfg edit /etc/dvm/config.yml network.address "newfne.example.com"
./dvmcfg edit /etc/dvm/config.yml network.port 62031

# Update entire trunked system
./dvmcfg trunk update --base-dir /etc/dvm/trunk \
    --name test network.address "newfne.example.com"
./dvmcfg trunk update --base-dir /etc/dvm/trunk \
    --name test network.port 62031

# Validate changes
./dvmcfg trunk validate --base-dir /etc/dvm/trunk --name test
```

## Tips

1. **Always validate** after creating or editing configurations
2. **Backup configurations** before making bulk updates
3. **Use consistent naming** for trunked system components
4. **Test with null modem** before connecting real hardware
5. **Keep RPC ports sequential** for easier management
6. **Document peer IDs** for your network topology
