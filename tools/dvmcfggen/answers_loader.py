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
answers_loader.py - Load and validate wizard answers from YAML files

This module handles loading answers files that can be used to pre-populate
wizard defaults, enabling batch configuration generation and automation.
"""

from pathlib import Path
from typing import Dict, Any, Optional
import yaml
from rich.console import Console

console = Console()


class AnswersLoader:
    """Load and validate wizard answers from YAML files"""
    
    # All supported answer keys for ConfigWizard
    CONFIG_WIZARD_KEYS = {
        'template', 'config_dir', 'system_identity', 'network_peer_id',
        'configure_logging', 'log_path', 'activity_log_path', 'log_root',
        'use_syslog', 'disable_non_auth_logging', 'modem_type', 'modem_mode',
        'serial_port', 'rx_level', 'tx_level', 'rpc_config', 'generate_rpc_password',
        'rest_enable', 'generate_rest_password', 'update_lookups', 'save_lookups',
        'allow_activity_transfer', 'allow_diagnostic_transfer', 'allow_status_transfer',
        'radio_id_file', 'radio_id_time', 'radio_id_acl', 'talkgroup_id_file',
        'talkgroup_id_time', 'talkgroup_id_acl', 'dmr_color_code', 'dmr_network_id',
        'dmr_site_id', 'dmr_site_model', 'p25_nac', 'p25_network_id', 'p25_system_id',
        'p25_rfss_id', 'p25_site_id', 'nxdn_ran', 'nxdn_location_id', 'site_id',
        'latitude', 'longitude', 'location', 'tx_power', 'tx_freq', 'band'
    }
    
    # All supported answer keys for TrunkingWizard
    TRUNKING_WIZARD_KEYS = {
        'system_name', 'base_dir', 'identity', 'protocol', 'vc_count',
        'configure_logging', 'log_path', 'activity_log_path', 'log_root',
        'use_syslog', 'disable_non_auth_logging', 'fne_address', 'fne_port',
        'fne_password', 'base_peer_id', 'base_rpc_port', 'modem_type',
        'rpc_password', 'generate_rpc_password', 'base_rest_port', 'rest_enable',
        'rest_password', 'generate_rest_password', 'update_lookups', 'save_lookups',
        'allow_activity_transfer', 'allow_diagnostic_transfer', 'allow_status_transfer',
        'radio_id_file', 'radio_id_time', 'radio_id_acl', 'talkgroup_id_file',
        'talkgroup_id_time', 'talkgroup_id_acl', 'dmr_color_code', 'dmr_network_id',
        'dmr_site_id', 'p25_nac', 'p25_network_id', 'p25_system_id', 'p25_rfss_id',
        'p25_site_id', 'site_id', 'cc_tx_freq', 'cc_band', 'vc_tx_freqs', 'vc_bands'
    }
    
    ALL_KEYS = CONFIG_WIZARD_KEYS | TRUNKING_WIZARD_KEYS
    
    @staticmethod
    def load_answers(filepath: Path) -> Dict[str, Any]:
        """
        Load answers from YAML file
        
        Args:
            filepath: Path to YAML answers file
            
        Returns:
            Dictionary of answers
            
        Raises:
            FileNotFoundError: If file doesn't exist
            yaml.YAMLError: If file is invalid YAML
        """
        try:
            with open(filepath, 'r') as f:
                answers = yaml.safe_load(f)
            
            if answers is None:
                return {}
            
            if not isinstance(answers, dict):
                raise ValueError("Answers file must contain a YAML dictionary")
            
            return answers
        except FileNotFoundError:
            console.print(f"[red]Error: Answers file not found: {filepath}[/red]")
            raise
        except yaml.YAMLError as e:
            console.print(f"[red]Error: Invalid YAML in answers file: {e}[/red]")
            raise
    
    @staticmethod
    def validate_answers(answers: Dict[str, Any], strict: bool = False) -> bool:
        """
        Validate answers file has recognized keys
        
        Args:
            answers: Dictionary of answers to validate
            strict: If True, fail on unrecognized keys; if False, warn only
            
        Returns:
            True if valid, False if invalid (in strict mode)
        """
        invalid_keys = set(answers.keys()) - AnswersLoader.ALL_KEYS
        
        if invalid_keys:
            msg = f"Unrecognized keys in answers file: {', '.join(sorted(invalid_keys))}"
            if strict:
                console.print(f"[red]{msg}[/red]")
                return False
            else:
                console.print(f"[yellow]Warning: {msg}[/yellow]")
        
        return True
    
    @staticmethod
    def save_answers(answers: Dict[str, Any], filepath: Path) -> None:
        """
        Save answers to YAML file
        
        Args:
            answers: Dictionary of answers to save
            filepath: Path to save answers to
        """
        filepath.parent.mkdir(parents=True, exist_ok=True)
        
        with open(filepath, 'w') as f:
            yaml.dump(answers, f, default_flow_style=False, sort_keys=False)
    
    @staticmethod
    def merge_answers(base: Dict[str, Any], override: Dict[str, Any]) -> Dict[str, Any]:
        """
        Merge two answers dictionaries (override takes precedence)
        
        Args:
            base: Base answers dictionary
            override: Override answers dictionary
            
        Returns:
            Merged dictionary
        """
        merged = base.copy()
        merged.update(override)
        return merged
    
    @staticmethod
    def create_template() -> Dict[str, Any]:
        """
        Create a template answers dictionary with common fields
        
        Returns:
            Template dictionary with helpful comments
        """
        return {
            # Wizard type
            'template': 'enhanced',
            
            # Basic configuration
            'config_dir': '.',
            'system_identity': 'SITE001',
            'network_peer_id': 100000,
            
            # Logging configuration
            'configure_logging': True,
            'log_path': '/var/log/dvm',
            'activity_log_path': '/var/log/dvm/activity',
            'log_root': 'DVM',
            'use_syslog': False,
            'disable_non_auth_logging': False,
            
            # Modem configuration
            'modem_type': 'uart',
            'modem_mode': 'air',
            'serial_port': '/dev/ttyUSB0',
            'rx_level': 50,
            'tx_level': 50,
        }
