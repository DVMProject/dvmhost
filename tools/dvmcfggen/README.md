# dvmcfggen - DVMHost Configuration Generator

A comprehensive Python-based configuration utility for creating/managing DVMHost configurations, including support for multi-instance trunking configurations and automatic frequency planning.

## Features

- **Complete Configuration Files**: Generated configs include ALL 265+ parameters from DVMHost's config.example.yml
- **Interactive Wizard**: Step-by-step guided configuration creation
- **Frequency Configuration**: Automatic channel assignment and identity table generation
  - 6 preset frequency bands (800/700/900 MHz, UHF, VHF)
  - Automatic channel ID and channel number calculation
  - RX frequency calculation with offset support
  - Identity table (`iden_table.dat`) generation
- **Template System**: Pre-configured templates for common deployments
  - Repeater/Hotspot (duplex)
  - Control Channels (P25/DMR)
  - Voice Channels
  - Conventional Repeater
- **Configuration Validation**: Comprehensive validation of all parameters
- **Multi-Instance Trunking**: Manage complete trunked systems
  - Automatic CC + VC configuration
  - Peer ID and port management
  - System-wide updates
  - Consistent frequency planning across all instances
- **CLI Interface**: Command-line tools for automation
- **Rich Output**: Beautiful terminal formatting and tables

## Installation

```bash
cd dvmcfg
pip install -r requirements.txt
```

## Quick Start

### Interactive Wizard (Easiest!)
```bash
./dvmcfg wizard
```
The wizard guides you step-by-step through configuration creation with prompts and validation.

### Create New Configuration (CLI)
```bash
./dvmcfg create --template hotspot --output /etc/dvm/config.yml
```

### Create Trunked System
```bash
./dvmcfg trunk create --cc-port 62031 --vc-count 2 --base-dir /etc/dvm/trunked
```

### Validate Configuration
```bash
./dvmcfg validate /etc/dvm/config.yml
```

## Templates

- **repeater**: Standalone repeater
- **control-channel**: Dedicated control channel for trunking
- **voice-channel**: Voice channel for trunking
- **conventional**: Conventional repeater with channel grants

## License

This project is licensed under the GPLv2 License - see the [LICENSE](LICENSE) file for details. Use of this project is intended, for amateur and/or educational use ONLY. Any other use is at the risk of user and all commercial purposes is strictly discouraged.
