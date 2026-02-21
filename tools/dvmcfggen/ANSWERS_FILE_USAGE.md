# DVMCfgGen Answers File Feature

## Overview

The answers file feature allows you to pre-populate wizard prompts using a YAML file. This enables:

- **Batch Configuration** - Generate multiple configurations with shared defaults
- **Automation** - Integrate with deployment scripts and CI/CD pipelines
- **Reproducibility** - Save and reuse configuration templates
- **Learning** - Understand what each prompt expects

## Quick Start

### 1. Use Example Files

Example answers files are provided in the `examples/` directory:

```bash
# For conventional systems
dvmcfg wizard -a examples/conventional-answers.yml

# For trunked systems
dvmcfg wizard --type trunk -a examples/trunked-answers.yml
```

### 2. Create Your Own

Copy an example file and customize it:

```bash
cp examples/conventional-answers.yml my-config-answers.yml
vim my-config-answers.yml
dvmcfg wizard -a my-config-answers.yml
```

### 3. Run Without Answers File (Default Behavior)

The answers file is completely optional. The wizard works normally without it:

```bash
dvmcfg wizard
```

## Answers File Format

Answers files are YAML format with one key-value pair per configuration option. All fields are optional.

### Conventional System Template

```yaml
# Step 2: Basic Configuration
template: enhanced
config_dir: .
system_identity: MYSITE001
network_peer_id: 100001

# Step 3: Logging Configuration
configure_logging: true
log_path: /var/log/dvm
activity_log_path: /var/log/dvm/activity
log_root: DVM
use_syslog: false
disable_non_auth_logging: false

# Step 4: Modem Configuration
modem_type: uart
modem_mode: air
serial_port: /dev/ttyUSB0
rx_level: 50
tx_level: 50
```

### Trunked System Template

```yaml
# Step 1: System Configuration
system_name: trunked_test
base_dir: .
identity: TRUNKED001
protocol: p25
vc_count: 2

# Step 2: Logging Configuration
configure_logging: true
log_path: /var/log/dvm
activity_log_path: /var/log/dvm/activity
log_root: TRUNKED
use_syslog: false
disable_non_auth_logging: false
```

## Supported Answer Keys

### ConfigWizard (Conventional Systems)

**Basic Configuration:**
- `template` - Template name (conventional, enhanced)
- `config_dir` - Configuration directory
- `system_identity` - System identity/callsign
- `network_peer_id` - Network peer ID (integer)

**Logging Configuration:**
- `configure_logging` - Enable logging (true/false)
- `log_path` - Log file directory
- `activity_log_path` - Activity log directory
- `log_root` - Log filename prefix
- `use_syslog` - Enable syslog (true/false)
- `disable_non_auth_logging` - Disable non-authoritative logging (true/false)

**Modem Configuration:**
- `modem_type` - Modem type (uart, null)
- `modem_mode` - Modem mode (air, dfsi)
- `serial_port` - Serial port path
- `rx_level` - RX level (0-100)
- `tx_level` - TX level (0-100)

**Optional Settings:**
- `rpc_config` - Configure RPC (true/false)
- `generate_rpc_password` - Generate RPC password (true/false)
- `rest_enable` - Enable REST API (true/false)
- `generate_rest_password` - Generate REST password (true/false)
- `update_lookups` - Update lookups (true/false)
- `save_lookups` - Save lookups (true/false)
- `allow_activity_transfer` - Allow activity transfer (true/false)
- `allow_diagnostic_transfer` - Allow diagnostic transfer (true/false)
- `allow_status_transfer` - Allow status transfer (true/false)

**Protocol Configuration (for Step 8/9):**
- `dmr_color_code` - DMR color code (integer)
- `dmr_network_id` - DMR network ID (integer)
- `dmr_site_id` - DMR site ID (integer)
- `dmr_site_model` - DMR site model (small, tiny, large, huge)
- `p25_nac` - P25 NAC code (hex string)
- `p25_network_id` - P25 network ID (integer)
- `p25_system_id` - P25 system ID (integer)
- `p25_rfss_id` - P25 RFSS ID (integer)
- `p25_site_id` - P25 site ID (integer)

### TrunkingWizard (Trunked Systems)

**System Configuration:**
- `system_name` - System name
- `base_dir` - Base directory
- `identity` - System identity
- `protocol` - Protocol (p25, dmr)
- `vc_count` - Number of voice channels (integer)

**Logging Configuration:**
- `configure_logging` - Enable logging (true/false)
- `log_path` - Log file directory
- `activity_log_path` - Activity log directory
- `log_root` - Log filename prefix
- `use_syslog` - Enable syslog (true/false)
- `disable_non_auth_logging` - Disable non-authoritative logging (true/false)

**Network Settings:**
- `fne_address` - FNE address
- `fne_port` - FNE port (integer)
- `fne_password` - FNE password

**Optional Settings:**
- `base_peer_id` - Base peer ID (integer)
- `base_rpc_port` - Base RPC port (integer)
- `modem_type` - Modem type (uart, null)
- `generate_rpc_password` - Generate RPC password (true/false)
- `base_rest_port` - Base REST port (integer)
- `rest_enable` - Enable REST API (true/false)
- `generate_rest_password` - Generate REST password (true/false)

## Usage Examples

### Example 1: Single Configuration with All Defaults

```yaml
# hotspot-answers.yml
template: enhanced
config_dir: /etc/dvm/hotspot
system_identity: MY_HOTSPOT
network_peer_id: 100001
configure_logging: true
log_path: /var/log/dvm
log_root: HOTSPOT
modem_type: uart
modem_mode: air
serial_port: /dev/ttyUSB0
```

Usage:
```bash
dvmcfg wizard -a hotspot-answers.yml
```

### Example 2: Trunked System

```yaml
# trunk-p25-answers.yml
system_name: trunk_p25
base_dir: /etc/dvm/trunk
identity: TRUNK_P25
protocol: p25
vc_count: 4
configure_logging: true
log_path: /var/log/dvm
log_root: TRUNK_P25
p25_nac: 0x293
p25_network_id: 1
p25_system_id: 1
```

Usage:
```bash
dvmcfg wizard --type trunk -a trunk-p25-answers.yml
```

### Example 3: Batch Generation

Create multiple configs programmatically:

```bash
#!/bin/bash

for SITE_NUM in {1..5}; do
  SITE_ID=$((100000 + SITE_NUM))
  SITE_NAME="SITE$(printf '%03d' $SITE_NUM)"
  
  cat > /tmp/site-${SITE_NUM}-answers.yml << EOF
template: enhanced
config_dir: ./site-${SITE_NUM}
system_identity: ${SITE_NAME}
network_peer_id: ${SITE_ID}
configure_logging: true
log_root: ${SITE_NAME}
modem_type: uart
modem_mode: air
EOF
  
  dvmcfg wizard -a /tmp/site-${SITE_NUM}-answers.yml
  echo "Generated config for ${SITE_NAME}"
done
```

### Example 4: CI/CD Pipeline

```yaml
# .github/workflows/generate-test-configs.yml
name: Generate Test Configs

on: [push]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Setup Python
        uses: actions/setup-python@v2
        with:
          python-version: '3.9'
      
      - name: Generate conventional config
        run: |
          python3 dvmhost/tools/dvmcfggen/dvmcfg.py wizard \
            -a examples/conventional-answers.yml
      
      - name: Generate trunked config
        run: |
          python3 dvmhost/tools/dvmcfggen/dvmcfg.py wizard \
            --type trunk \
            -a examples/trunked-answers.yml
      
      - name: Validate configs
        run: |
          python3 dvmhost/tools/dvmcfggen/dvmcfg.py validate -c config.yml
```

## Advanced Usage

### Partial Answers Files

You only need to specify the fields you want to pre-populate. Missing fields will be prompted for interactively:

```yaml
# minimal-answers.yml
system_identity: MYSITE
network_peer_id: 100001
# User will be prompted for other values
```

### Saving Generated Answers

After creating a configuration interactively, you can extract the answers:

```python
from answers_loader import AnswersLoader
from pathlib import Path

# After wizard completes
answers = {
    'template': 'enhanced',
    'system_identity': 'MYSITE001',
    'network_peer_id': 100001,
    # ... etc
}

AnswersLoader.save_answers(answers, Path('my-site-answers.yml'))
```

### Validation

Answers files are validated automatically, but you can check them manually:

```python
from answers_loader import AnswersLoader
from pathlib import Path

answers = AnswersLoader.load_answers(Path('my-answers.yml'))

# Non-strict validation (warns about unknown keys)
is_valid = AnswersLoader.validate_answers(answers, strict=False)

# Strict validation (fails on unknown keys)
is_valid = AnswersLoader.validate_answers(answers, strict=True)
```

## Key Features

### ✅ Backward Compatible
- Existing wizard usage works unchanged
- Answers file is completely optional
- No breaking changes

### ✅ Flexible
- Answer any or all questions
- Interactive prompts for missing answers
- Easy to create custom templates

### ✅ Easy to Use
- Simple YAML format
- Clear comments in example files
- Error messages for invalid files

### ✅ Reproducible
- Save and version control answers files
- Generate identical configs across systems
- Document configuration decisions

## Troubleshooting

### "Unrecognized keys in answers file"

This is a warning (not an error) if your answers file contains unknown keys. It won't stop the wizard from running. To suppress this, ensure you're only using valid keys from the lists above.

### "Error loading answers file"

Check that:
1. The file path is correct
2. The file is valid YAML (use `python3 -c "import yaml; yaml.safe_load(open('file.yml'))"`
3. The file has proper indentation (2 spaces per level)

### Answers not being used

Verify that:
1. The key name matches exactly (case-sensitive)
2. The file is being passed with `-a` or `--answers-file`
3. The value type is correct (string, integer, boolean)

## File Reference

### Core Files
- `answers_loader.py` - Utility for loading/validating answers
- `wizard.py` - ConfigWizard and TrunkingWizard classes (refactored)
- `dvmcfg.py` - CLI interface (updated with `--answers-file` support)

### Example Files
- `examples/conventional-answers.yml` - Example for conventional systems
- `examples/trunked-answers.yml` - Example for trunked systems

