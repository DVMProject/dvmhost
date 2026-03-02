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
Core module for managing DVMHost configuration files
"""

import yaml
import copy
from pathlib import Path
from typing import Dict, Any, Optional, List
import re


class ConfigValidator:
    """Validates DVMHost configuration values"""
    
    @staticmethod
    def validate_ip_address(ip: str) -> bool:
        """Validate IP address format"""
        if ip in ['0.0.0.0', 'localhost']:
            return True
        pattern = r'^(\d{1,3}\.){3}\d{1,3}$'
        if not re.match(pattern, ip):
            return False
        parts = ip.split('.')
        return all(0 <= int(part) <= 255 for part in parts)
    
    @staticmethod
    def validate_port(port: int) -> bool:
        """Validate port number"""
        return 1 <= port <= 65535
    
    @staticmethod
    def validate_hex_key(key: str, required_length: int) -> bool:
        """Validate hexadecimal key"""
        if len(key) != required_length:
            return False
        return all(c in '0123456789ABCDEFabcdef' for c in key)
    
    @staticmethod
    def validate_range(value: int, min_val: int, max_val: int) -> bool:
        """Validate value is within range"""
        return min_val <= value <= max_val
    
    @staticmethod
    def validate_identity(identity: str) -> bool:
        """Validate system identity format"""
        return len(identity) > 0 and len(identity) <= 32
    
    @staticmethod
    def validate_callsign(callsign: str) -> bool:
        """Validate callsign format"""
        return len(callsign) > 0 and len(callsign) <= 16


class DVMConfig:
    """DVMHost configuration manager"""
    
    def __init__(self, config_path: Optional[Path] = None):
        """
        Initialize configuration manager
        
        Args:
            config_path: Path to existing config file to load
        """
        self.config_path = config_path
        self.config: Dict[str, Any] = {}
        self.validator = ConfigValidator()
        
        if config_path and config_path.exists():
            self.load(config_path)
    
    def load(self, config_path: Path) -> None:
        """Load configuration from YAML file"""
        with open(config_path, 'r') as f:
            self.config = yaml.safe_load(f)
        self.config_path = config_path
    
    def save(self, output_path: Optional[Path] = None) -> None:
        """Save configuration to YAML file"""
        path = output_path or self.config_path
        if not path:
            raise ValueError("No output path specified")
        
        # Create backup if file exists
        if path.exists():
            backup_path = path.with_suffix(path.suffix + '.bak')
            path.rename(backup_path)
        
        with open(path, 'w') as f:
            yaml.dump(self.config, f, default_flow_style=False, sort_keys=False)
    
    def get(self, key_path: str, default: Any = None) -> Any:
        """
        Get configuration value using dot notation
        
        Args:
            key_path: Path to value (e.g., 'network.id')
            default: Default value if not found
        """
        keys = key_path.split('.')
        value = self.config
        
        for key in keys:
            if isinstance(value, dict) and key in value:
                value = value[key]
            else:
                return default
        
        return value
    
    def set(self, key_path: str, value: Any) -> None:
        """
        Set configuration value using dot notation
        
        Args:
            key_path: Path to value (e.g., 'network.id')
            value: Value to set
        """
        keys = key_path.split('.')
        config = self.config
        
        # Navigate to the parent dict
        for key in keys[:-1]:
            if key not in config:
                config[key] = {}
            config = config[key]
        
        # Set the value
        config[keys[-1]] = value
    
    def validate(self) -> List[str]:
        """
        Validate configuration
        
        Returns:
            List of error messages (empty if valid)
        """
        errors = []
        
        # Network validation
        if self.get('network.enable'):
            if not self.validator.validate_ip_address(self.get('network.address', '')):
                errors.append("Invalid network.address")
            
            if not self.validator.validate_port(self.get('network.port', 0)):
                errors.append("Invalid network.port")
            
            if self.get('network.encrypted'):
                psk = self.get('network.presharedKey', '')
                if not self.validator.validate_hex_key(psk, 64):
                    errors.append("Invalid network.presharedKey (must be 64 hex characters)")
        
        # RPC validation
        rpc_addr = self.get('network.rpcAddress', '')
        if rpc_addr and not self.validator.validate_ip_address(rpc_addr):
            errors.append("Invalid network.rpcAddress")
        
        rpc_port = self.get('network.rpcPort', 0)
        if rpc_port and not self.validator.validate_port(rpc_port):
            errors.append("Invalid network.rpcPort")
        
        # REST API validation
        if self.get('network.restEnable'):
            rest_addr = self.get('network.restAddress', '')
            if not self.validator.validate_ip_address(rest_addr):
                errors.append("Invalid network.restAddress")
            
            rest_port = self.get('network.restPort', 0)
            if not self.validator.validate_port(rest_port):
                errors.append("Invalid network.restPort")
        
        # System validation
        identity = self.get('system.identity', '')
        if not self.validator.validate_identity(identity):
            errors.append("Invalid system.identity")
        
        # Color code / NAC / RAN validation
        color_code = self.get('system.config.colorCode', 1)
        if not self.validator.validate_range(color_code, 0, 15):
            errors.append("Invalid system.config.colorCode (must be 0-15)")
        
        nac = self.get('system.config.nac', 0)
        if not self.validator.validate_range(nac, 0, 0xFFF):
            errors.append("Invalid system.config.nac (must be 0-4095)")
        
        ran = self.get('system.config.ran', 0)
        if not self.validator.validate_range(ran, 0, 63):
            errors.append("Invalid system.config.ran (must be 0-63)")
        
        # CW ID validation
        if self.get('system.cwId.enable'):
            callsign = self.get('system.cwId.callsign', '')
            if not self.validator.validate_callsign(callsign):
                errors.append("Invalid system.cwId.callsign")
        
        # Modem validation
        rx_level = self.get('system.modem.rxLevel', 0)
        if not self.validator.validate_range(rx_level, 0, 100):
            errors.append("Invalid system.modem.rxLevel (must be 0-100)")
        
        tx_level = self.get('system.modem.txLevel', 0)
        if not self.validator.validate_range(tx_level, 0, 100):
            errors.append("Invalid system.modem.txLevel (must be 0-100)")
        
        # LLA key validation
        lla_key = self.get('system.config.secure.key', '')
        if lla_key and not self.validator.validate_hex_key(lla_key, 32):
            errors.append("Invalid system.config.secure.key (must be 32 hex characters)")
        
        return errors
    
    def get_summary(self) -> Dict[str, Any]:
        """Get configuration summary"""
        return {
            'identity': self.get('system.identity', 'UNKNOWN'),
            'peer_id': self.get('network.id', 0),
            'fne_address': self.get('network.address', 'N/A'),
            'fne_port': self.get('network.port', 0),
            'rpc_password': self.get('network.rpcPassword', 'N/A'),
            'rest_enabled': self.get('network.restEnable', False),
            'rest_password': self.get('network.restPassword', 'N/A'),
            'protocols': {
                'dmr': self.get('protocols.dmr.enable', False),
                'p25': self.get('protocols.p25.enable', False),
                'nxdn': self.get('protocols.nxdn.enable', False),
            },
            'color_code': self.get('system.config.colorCode', 1),
            'nac': self.get('system.config.nac', 0x293),
            'site_id': self.get('system.config.siteId', 1),
            'is_control': self.get('protocols.p25.control.enable', False) or 
                         self.get('protocols.dmr.control.enable', False),
            'modem_type': self.get('system.modem.protocol.type', 'null'),
            'modem_mode': self.get('system.modem.protocol.mode', 'air'),
            'modem_port': self.get('system.modem.protocol.uart.port', 'N/A'),
            'mode': 'Duplex' if self.get('system.duplex', True) else 'Simplex',
            'channel_id': self.get('system.config.channelId', 0),
            'channel_no': self.get('system.config.channelNo', 0),
            'tx_frequency': self.get('system.config.txFrequency', 0),
            'rx_frequency': self.get('system.config.rxFrequency', 0),
        }


if __name__ == '__main__':
    # Quick test
    config = DVMConfig()
    config.set('system.identity', 'TEST001')
    config.set('network.id', 100001)
    print("Config manager initialized successfully")
