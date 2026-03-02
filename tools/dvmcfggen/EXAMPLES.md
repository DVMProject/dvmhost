# dvmcfggen - Examples

This file contains complete, copy-paste ready examples for common scenarios.

## Example 0: Using the Interactive Wizard (Easiest!)

The simplest way to create any configuration:

```bash
# Start the wizard
./dvmcfg wizard

# Follow the prompts:
# 1. Choose single instance or trunked system
# 2. Select template (hotspot, repeater, etc.)
# 3. Enter system details (identity, peer ID, etc.)
# 4. Configure network settings (FNE address, port, password)
# 5. Enable/disable protocols (DMR, P25, NXDN)
# 6. Set radio parameters (color code, NAC, site ID)
# 7. Configure modem (type, serial port, levels)
# 8. Optionally add location info
# 9. Review summary and save
```

The wizard validates all inputs and provides helpful defaults. Perfect for first-time users!

---

## Example 1: Basic Hotspot Setup (CLI)

```bash
# Create hotspot configuration
./dvmcfg create \
    --template hotspot \
    --output /etc/dvm/hotspot.yml \
    --identity "HOTSPOT-HOME" \
    --peer-id 100001 \
    --fne-address "fne.example.com" \
    --fne-port 62031 \
    --callsign "KC1ABC" \
    --validate

# Customize serial port
./dvmcfg edit /etc/dvm/hotspot.yml \
    system.modem.protocol.uart.port "/dev/ttyACM0"

# Adjust levels
./dvmcfg edit /etc/dvm/hotspot.yml system.modem.rxLevel 60
./dvmcfg edit /etc/dvm/hotspot.yml system.modem.txLevel 60

# Final validation
./dvmcfg validate /etc/dvm/hotspot.yml --summary
```

## Example 2: Standalone Repeater

```bash
# Create repeater configuration
./dvmcfg create \
    --template repeater \
    --output /etc/dvm/repeater.yml \
    --identity "REPEATER-W1ABC" \
    --peer-id 100002 \
    --fne-address "10.0.0.100" \
    --fne-port 62031 \
    --callsign "W1ABC"

# Set location
./dvmcfg edit /etc/dvm/repeater.yml system.info.latitude 42.3601
./dvmcfg edit /etc/dvm/repeater.yml system.info.longitude -71.0589
./dvmcfg edit /etc/dvm/repeater.yml system.info.location "Boston, MA"
./dvmcfg edit /etc/dvm/repeater.yml system.info.power 50

# Configure modem
./dvmcfg edit /etc/dvm/repeater.yml \
    system.modem.protocol.uart.port "/dev/ttyUSB0"

# Validate
./dvmcfg validate /etc/dvm/repeater.yml
```

## Example 3: Small P25 Trunked System (2 VCs)

```bash
# Create trunked system
./dvmcfg trunk create \
    --base-dir /etc/dvm/site1 \
    --name site1 \
    --protocol p25 \
    --vc-count 2 \
    --identity "SITE001" \
    --base-peer-id 100000 \
    --fne-address "10.0.0.1" \
    --fne-port 62031 \
    --nac 0x001 \
    --site-id 1 \
    --color-code 1

# Configure modem ports
./dvmcfg edit /etc/dvm/site1/site1-cc.yml \
    system.modem.protocol.uart.port "/dev/ttyUSB0"
./dvmcfg edit /etc/dvm/site1/site1-vc01.yml \
    system.modem.protocol.uart.port "/dev/ttyUSB1"
./dvmcfg edit /etc/dvm/site1/site1-vc02.yml \
    system.modem.protocol.uart.port "/dev/ttyUSB2"

# Update location across all configs
./dvmcfg trunk update --base-dir /etc/dvm/site1 --name site1 \
    system.info.latitude 42.3601
./dvmcfg trunk update --base-dir /etc/dvm/site1 --name site1 \
    system.info.longitude -71.0589
./dvmcfg trunk update --base-dir /etc/dvm/site1 --name site1 \
    system.info.location "Site 1, Boston"

# Validate system
./dvmcfg trunk validate --base-dir /etc/dvm/site1 --name site1
```

## Example 4: Large P25 Trunked System (6 VCs)

```bash
# Create large trunked system
./dvmcfg trunk create \
    --base-dir /etc/dvm/hub \
    --name hub \
    --protocol p25 \
    --vc-count 6 \
    --identity "HUB" \
    --base-peer-id 100000 \
    --base-rpc-port 9890 \
    --fne-address "172.16.0.1" \
    --fne-port 62031 \
    --fne-password "SecurePassword123" \
    --nac 0x100 \
    --site-id 10

# Enable verbose logging across all
./dvmcfg trunk update --base-dir /etc/dvm/hub --name hub \
    protocols.p25.verbose true

# Configure individual modem ports
for i in {0..6}; do
    if [ $i -eq 0 ]; then
        file="hub-cc.yml"
    else
        file="hub-vc0${i}.yml"
    fi
    ./dvmcfg edit /etc/dvm/hub/$file \
        system.modem.protocol.uart.port "/dev/ttyUSB${i}"
done

# Validate
./dvmcfg trunk validate --base-dir /etc/dvm/hub --name hub
```

## Example 5: DMR Trunked System

```bash
# Create DMR trunked system
./dvmcfg trunk create \
    --base-dir /etc/dvm/dmr-site \
    --name dmrsite \
    --protocol dmr \
    --vc-count 3 \
    --identity "DMR001" \
    --base-peer-id 200000 \
    --color-code 2 \
    --site-id 1

# Configure to use specific slots
./dvmcfg edit /etc/dvm/dmr-site/dmrsite-cc.yml \
    protocols.dmr.control.slot 1

# Enable both slots for voice channels
./dvmcfg trunk update --base-dir /etc/dvm/dmr-site --name dmrsite \
    network.slot1 true
./dvmcfg trunk update --base-dir /etc/dvm/dmr-site --name dmrsite \
    network.slot2 true

# Validate
./dvmcfg trunk validate --base-dir /etc/dvm/dmr-site --name dmrsite
```

## Example 6: Conventional P25 System with Grants

```bash
# Create conventional system
./dvmcfg create \
    --template conventional \
    --output /etc/dvm/conventional.yml \
    --identity "CONV001" \
    --peer-id 100010 \
    --fne-address "10.0.0.1"

# Configure control channel settings
./dvmcfg edit /etc/dvm/conventional.yml \
    protocols.p25.control.interval 300
./dvmcfg edit /etc/dvm/conventional.yml \
    protocols.p25.control.duration 3

# Set as authoritative
./dvmcfg edit /etc/dvm/conventional.yml \
    system.config.authoritative true

# Validate
./dvmcfg validate /etc/dvm/conventional.yml
```

## Example 7: Multi-Site System Migration

```bash
# Create Site 1
./dvmcfg trunk create \
    --base-dir /etc/dvm/sites/site1 \
    --name site1 \
    --vc-count 2 \
    --identity "SITE001" \
    --base-peer-id 100000 \
    --site-id 1 \
    --nac 0x001

# Create Site 2
./dvmcfg trunk create \
    --base-dir /etc/dvm/sites/site2 \
    --name site2 \
    --vc-count 2 \
    --identity "SITE002" \
    --base-peer-id 100010 \
    --site-id 2 \
    --nac 0x001

# Update both sites to new FNE
for site in site1 site2; do
    ./dvmcfg trunk update \
        --base-dir /etc/dvm/sites/$site \
        --name $site \
        network.address "newfne.example.com"
done

# Validate both
./dvmcfg trunk validate --base-dir /etc/dvm/sites/site1 --name site1
./dvmcfg trunk validate --base-dir /etc/dvm/sites/site2 --name site2
```

## Example 8: Testing with Null Modem

```bash
# Create test configuration with null modem
./dvmcfg create \
    --template repeater \
    --output /tmp/test-config.yml \
    --identity "TEST" \
    --peer-id 999999

# Set to null modem
./dvmcfg edit /tmp/test-config.yml system.modem.protocol.type "null"

# Enable all debug logging
./dvmcfg edit /tmp/test-config.yml protocols.dmr.debug true
./dvmcfg edit /tmp/test-config.yml protocols.p25.debug true
./dvmcfg edit /tmp/test-config.yml system.modem.debug true

# Validate
./dvmcfg validate /tmp/test-config.yml --summary
```

## Example 9: Enabling Encryption

```bash
# Create secure configuration
./dvmcfg create \
    --template repeater \
    --output /etc/dvm/secure.yml \
    --identity "SECURE001" \
    --peer-id 100020

# Enable network encryption
./dvmcfg edit /etc/dvm/secure.yml network.encrypted true

# The preshared key is automatically generated
# You can update it if needed (must be 64 hex chars):
# ./dvmcfg edit /etc/dvm/secure.yml network.presharedKey "YOUR64HEXCHARACTERS..."

# Enable REST API with SSL
./dvmcfg edit /etc/dvm/secure.yml network.restEnable true
./dvmcfg edit /etc/dvm/secure.yml network.restSsl true

# Validate
./dvmcfg validate /etc/dvm/secure.yml
```

## Example 10: Batch Configuration Update

```bash
#!/bin/bash
# Update multiple configurations at once

CONFIGS=(
    "/etc/dvm/repeater1.yml"
    "/etc/dvm/repeater2.yml"
    "/etc/dvm/repeater3.yml"
)

NEW_FNE="10.0.0.100"
NEW_PORT=62031

for config in "${CONFIGS[@]}"; do
    echo "Updating $config..."
    ./dvmcfg edit "$config" network.address "$NEW_FNE"
    ./dvmcfg edit "$config" network.port "$NEW_PORT"
    ./dvmcfg validate "$config"
done

echo "All configs updated and validated!"
```

## Pro Tips

1. **Always validate after edits**: Add `&& ./dvmcfg validate $CONFIG` to your commands
2. **Use variables in scripts**: Define common values as variables for consistency
3. **Test with null modem first**: Verify config before connecting hardware
4. **Sequential RPC ports**: Keep RPC ports in sequence for easier troubleshooting
5. **Document peer IDs**: Keep a spreadsheet of all peer IDs in your network
6. **Backup before bulk updates**: `cp config.yml config.yml.bak`
7. **Use trunk update for consistency**: Ensures all configs in a system match
8. **Validate entire trunk systems**: Use `trunk validate` to catch cross-config issues
