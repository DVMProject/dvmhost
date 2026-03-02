#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
#/**
#* Digital Voice Modem - Host Configuration Generator
#* GPLv2 Open Source. Use is subject to license terms.
#* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#*
#*/
#/*
#*   Copyright (C) 2026 by Bryan Biedenkapp N2PLL
#*
#*   This program is free software; you can redistribute it and/or modify
#*   it under the terms of the GNU General Public License as published by
#*   the Free Software Foundation; either version 2 of the License, or
#*   (at your option) any later version.
#*
#*   This program is distributed in the hope that it will be useful,
#*   but WITHOUT ANY WARRANTY; without even the implied warranty of
#*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#*   GNU General Public License for more details.
#*
#*   You should have received a copy of the GNU General Public License
#*   along with this program; if not, write to the Free Software
#*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#*/
"""
dvmcfggen - DVMHost Configuration Generator

Pre-configured templates loaded from dvmhost config.example.yml
Ensures all configuration parameters are present
"""

from typing import Dict, Any, Optional
import secrets
import yaml
from pathlib import Path


# Possible locations for config.example.yml
CONFIG_EXAMPLE_PATHS = [
    Path(__file__).parent.parent / 'configs' / 'config.example.yml',
    Path('/opt/dvm/config.example.yml'),
    Path('./config.example.yml'),
]


def find_config_example() -> Optional[Path]:
    """Find the config.example.yml file from dvmhost"""
    for path in CONFIG_EXAMPLE_PATHS:
        if path.exists():
            return path
    return None


def load_base_config() -> Dict[str, Any]:
    """
    Load the base configuration from config.example.yml
    Falls back to minimal config if not found
    """
    example_path = find_config_example()
    
    if example_path:
        try:
            with open(example_path, 'r') as f:
                config = yaml.safe_load(f)
                if config:
                    return config
        except Exception as e:
            print(f"Warning: Could not load {example_path}: {e}")
    
    # Fallback to minimal config if example not found
    print("Warning: config.example.yml not found, using minimal fallback config")
    return get_minimal_fallback_config()


def get_minimal_fallback_config() -> Dict[str, Any]:
    """Minimal fallback configuration if config.example.yml is not available"""
    return {
        'daemon': True,
        'log': {
            'displayLevel': 1,
            'fileLevel': 1,
            'filePath': '.',
            'activityFilePath': '.',
            'fileRoot': 'DVM'
        },
        'network': {
            'enable': True,
            'id': 100000,
            'address': '127.0.0.1',
            'port': 62031,
            'password': 'PASSWORD',
            'rpcAddress': '127.0.0.1',
            'rpcPort': 9890,
            'rpcPassword': 'ULTRA-VERY-SECURE-DEFAULT',
        },
        'system': {
            'identity': 'DVM',
            'timeout': 180,
            'duplex': True,
            'modeHang': 10,
            'rfTalkgroupHang': 10,
            'activeTickDelay': 5,
            'idleTickDelay': 5,
            'info': {
                'latitude': 0.0,
                'longitude': 0.0,
                'height': 1,
                'power': 0,
                'location': 'Anywhere'
            },
            'config': {
                'authoritative': True,
                'supervisor': False,
                'channelId': 1,
                'channelNo': 1,
                'serviceClass': 1,
                'siteId': 1,
                'dmrNetId': 1,
                'dmrColorCode': 1,
                'p25NAC': 0x293,
                'p25NetId': 0xBB800,
                'nxdnRAN': 1
            },
            'cwId': {
                'enable': True,
                'time': 10,
                'callsign': 'MYCALL'
            },
            'modem': {
                'protocol': {
                    'type': 'uart',
                    'uart': {
                        'port': '/dev/ttyUSB0',
                        'speed': 115200
                    }
                },
                'rxLevel': 50,
                'txLevel': 50,
                'rxDCOffset': 0,
                'txDCOffset': 0
            }
        },
        'protocols': {
            'dmr': {
                'enable': True,
                'control': {
                    'enable': True,
                    'dedicated': False
                }
            },
            'p25': {
                'enable': True,
                'control': {
                    'enable': True,
                    'dedicated': False
                }
            },
            'nxdn': {
                'enable': False,
                'control': {
                    'enable': False,
                    'dedicated': False
                }
            }
        }
    }


def generate_secure_key(length: int = 64) -> str:
    """Generate a secure random hex key"""
    return secrets.token_hex(length // 2).upper()


def apply_template_customizations(config: Dict[str, Any], template_type: str) -> Dict[str, Any]:
    """
    Apply template-specific customizations to the base config
    
    Args:
        config: Base configuration dict
        template_type: Template name (repeater, etc.)
    
    Returns:
        Customized configuration dict
    """
    # Make a deep copy to avoid modifying the original
    import copy
    config = copy.deepcopy(config)
    
    if template_type == 'control-channel-p25':
        # P25 dedicated control channel
        config['system']['duplex'] = True
        config['system']['identity'] = 'CC-P25'
        config['system']['config']['authoritative'] = True
        config['system']['config']['supervisor'] = True
        config['protocols']['dmr']['enable'] = False
        config['protocols']['p25']['enable'] = True
        config['protocols']['nxdn']['enable'] = False
        config['protocols']['p25']['control']['enable'] = True
        config['protocols']['p25']['control']['dedicated'] = True
        
    elif template_type == 'control-channel-dmr':
        # DMR dedicated control channel
        config['system']['duplex'] = True
        config['system']['identity'] = 'CC-DMR'
        config['system']['config']['authoritative'] = True
        config['system']['config']['supervisor'] = True
        config['protocols']['dmr']['enable'] = True
        config['protocols']['p25']['enable'] = False
        config['protocols']['nxdn']['enable'] = False
        config['protocols']['dmr']['control']['enable'] = True
        config['protocols']['dmr']['control']['dedicated'] = True
        
    elif template_type == 'voice-channel':
        # Voice channel for trunking
        config['system']['duplex'] = True
        config['system']['identity'] = 'VC'
        config['system']['config']['authoritative'] = False
        config['system']['config']['supervisor'] = False
        config['protocols']['dmr']['enable'] = True
        config['protocols']['p25']['enable'] = True
        config['protocols']['nxdn']['enable'] = False
        config['protocols']['dmr']['control']['enable'] = False
        config['protocols']['dmr']['control']['dedicated'] = False
        config['protocols']['p25']['control']['enable'] = False
        config['protocols']['p25']['control']['dedicated'] = False
        
    elif template_type == 'enhanced':
        # Enhanced conventional repeater with grants
        config['system']['duplex'] = True
        config['system']['identity'] = 'CONV'
        config['protocols']['dmr']['enable'] = True
        config['protocols']['p25']['enable'] = True
        config['protocols']['nxdn']['enable'] = False
        config['protocols']['dmr']['control']['enable'] = True
        config['protocols']['dmr']['control']['dedicated'] = False
        config['protocols']['p25']['control']['enable'] = True
        config['protocols']['p25']['control']['dedicated'] = False
    
    elif template_type == 'conventional':
        # Conventional repeater
        config['system']['duplex'] = True
        config['system']['identity'] = 'RPT'
        config['protocols']['dmr']['enable'] = False
        config['protocols']['p25']['enable'] = True
        config['protocols']['nxdn']['enable'] = False
        config['protocols']['dmr']['control']['enable'] = False
        config['protocols']['dmr']['control']['dedicated'] = False
        config['protocols']['p25']['control']['enable'] = True
        config['protocols']['p25']['control']['dedicated'] = False
    
    return config


def get_template(template_name: str) -> Dict[str, Any]:
    """
    Get a configuration template by name
    
    Args:
        template_name: Name of the template
    
    Returns:
        Complete configuration dictionary with all parameters
    """
    if template_name not in TEMPLATES:
        raise ValueError(f"Unknown template: {template_name}")
    
    # Load base config from config.example.yml
    base_config = load_base_config()
    
    # Apply template-specific customizations
    customized_config = apply_template_customizations(base_config, template_name)
    
    return customized_config


# Available templates
TEMPLATES = {
    'conventional': 'Conventional repeater/hotspot',
    'enhanced': 'Enhanced conventional repeater/hotspot with grants',
    'control-channel-p25': 'P25 dedicated control channel for trunking',
    'control-channel-dmr': 'DMR dedicated control channel for trunking',
    'voice-channel': 'Voice channel for trunking system',
}
