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

Interactive Configuration Wizard
Step-by-step guided configuration creation
"""

from pathlib import Path
from typing import Optional, Any, Dict, Tuple
import secrets
import string
from rich.console import Console
from rich.prompt import Prompt, Confirm, IntPrompt
from rich.panel import Panel
from rich.table import Table
from rich import print as rprint

from config_manager import DVMConfig, ConfigValidator
from templates import get_template, TEMPLATES
from trunking_manager import TrunkingSystem
from answers_loader import AnswersLoader
from iden_table import (IdenTable, create_default_iden_table, calculate_channel_assignment,
                        BAND_PRESETS, create_iden_entry_from_preset)
from network_ids import (
    get_network_id_info, validate_dmr_color_code, validate_dmr_network_id, validate_dmr_site_id,
    validate_p25_nac, validate_p25_network_id, validate_p25_system_id, validate_p25_rfss_id,
    validate_p25_site_id, validate_nxdn_ran, validate_nxdn_location_id,
    DMRSiteModel, get_dmr_site_model_from_string, format_hex_or_decimal
)
from version import __BANNER__, __VER__


console = Console()
validator = ConfigValidator()


def generate_random_password(length: int = 24) -> str:
    """Generate a random password using letters, digits, and special characters"""
    chars = string.ascii_letters + string.digits + "!@#$%^&*-_=+"
    return ''.join(secrets.choice(chars) for _ in range(length))


class ConfigWizard:
    """Interactive configuration wizard"""
    
    def __init__(self, answers: Optional[Dict[str, Any]] = None):
        self.answers = answers or {}
        self.config: Optional[DVMConfig] = None
        self.template_name: Optional[str] = None
        self.iden_table: IdenTable = create_default_iden_table()
        self.iden_file_path: Optional[Path] = None
        self.config_dir: str = "."
        self.rpc_password: Optional[str] = None
        self.rest_password: Optional[str] = None
    
    def _get_answer(self, key: str, prompt_func, *args, **kwargs) -> Any:
        """
        Get answer from answers file or prompt user
        
        Args:
            key: Answer key to look up
            prompt_func: Function to call if answer not found (e.g., Prompt.ask)
            *args: Arguments to pass to prompt_func
            **kwargs: Keyword arguments to pass to prompt_func
            
        Returns:
            Answer value (from file or user prompt)
        """
        if key in self.answers:
            value = self.answers[key]
            # Display the value that was loaded from answers file
            if isinstance(value, bool):
                display_value = "Yes" if value else "No"
            else:
                display_value = str(value)
            console.print(f"[dim]{key}: {display_value}[/dim]")
            return value
        
        # Prompt user for input
        return prompt_func(*args, **kwargs)
        
    def run(self) -> Optional[Path]:
        """Run the interactive wizard"""
        # Don't clear or show banner here - run_wizard handles it
        console.print()
        console.print(Panel.fit(
            "[bold cyan]Conventional System Configuration Wizard[/bold cyan]\n"
            "Create a conventional system configuration",
            border_style="cyan"
        ))
        console.print()
        
        # Step 1: Choose template
        self.template_name = self._choose_template()
        if not self.template_name:
            return None
        
        console.print()
        
        # Step 2: Create base config
        self.config = DVMConfig()
        self.config.config = get_template(self.template_name)
        
        # Step 3: Collect configuration
        if self.template_name in ['conventional', 'enhanced']:
            return self._configure_single_instance()
        elif 'control-channel' in self.template_name or 'voice-channel' in self.template_name:
            console.print("[yellow]For trunked systems, use the trunking wizard instead.[/yellow]")
            if Confirm.ask("Start trunking wizard?"):
                return self._run_trunking_wizard()
            return None
        
        return None
    
    def _choose_template(self) -> Optional[str]:
        """Choose configuration template"""
        console.print("[bold]Step 1: Choose Configuration Template[/bold]\n")
        
        table = Table(show_header=True, header_style="bold cyan")
        table.add_column("#", style="dim", width=3)
        table.add_column("Template", style="cyan")
        table.add_column("Type", style="yellow")
        table.add_column("Description", style="white")
        
        templates = [
            ('conventional', 'Single', 'Conventional repeater/hotspot'),
            ('enhanced', 'Single', 'Enhanced conventional repeater/hotspot with grants'),
        ]
        
        for i, (name, type_, desc) in enumerate(templates, 1):
            table.add_row(str(i), name, type_, desc)
        
        console.print(table)
        console.print()
        
        choice = IntPrompt.ask(
            "Select template",
            choices=[str(i) for i in range(1, len(templates) + 1)],
            default="2"  # Enhanced template
        )
        
        return templates[int(choice) - 1][0]
    
    def _configure_single_instance(self) -> Optional[Path]:
        """Configure a single instance"""
        console.print("\n[bold]Step 2: Basic Configuration[/bold]\n")
        
        # Configuration directory
        self.config_dir = self._get_answer(
            'config_dir',
            Prompt.ask,
            "Configuration directory",
            default="."
        )
        
        # System Identity
        identity = self._get_answer(
            'system_identity',
            Prompt.ask,
            "System identity (callsign or site name)",
            default="SITE001"
        )
        self.config.set('system.identity', identity)
        self.config.set('system.cwId.callsign', identity[:8])  # Truncate for CW
        
        # Peer ID
        peer_id = self._get_answer(
            'network_peer_id',
            IntPrompt.ask,
            "Network peer ID",
            default=100000
        )
        self.config.set('network.id', peer_id)
        
        # Logging Configuration
        console.print("\n[bold]Step 3: Logging Configuration[/bold]\n")
        
        configure_logging = self._get_answer(
            'configure_logging',
            Confirm.ask,
            "Configure logging settings?",
            default=False
        )
        
        if configure_logging:
            # Log file path
            log_path = self._get_answer(
                'log_path',
                Prompt.ask,
                "Log file directory",
                default="."
            )
            self.config.set('log.filePath', log_path)
            
            # Activity log file path
            activity_log_path = self._get_answer(
                'activity_log_path',
                Prompt.ask,
                "Activity log directory",
                default="."
            )
            self.config.set('log.activityFilePath', activity_log_path)
            
            # Log file root/prefix
            log_root = self._get_answer(
                'log_root',
                Prompt.ask,
                "Log filename prefix (used for filenames and syslog)",
                default="DVM"
            )
            self.config.set('log.fileRoot', log_root)
            
            # Syslog usage
            use_syslog = self._get_answer(
                'use_syslog',
                Confirm.ask,
                "Enable syslog output?",
                default=False
            )
            self.config.set('log.useSysLog', use_syslog)
            
            # Disable non-authoritive logging
            disable_non_auth = self._get_answer(
                'disable_non_auth_logging',
                Confirm.ask,
                "Disable non-authoritative logging?",
                default=False
            )
            self.config.set('log.disableNonAuthoritiveLogging', disable_non_auth)
        
        console.print("\n[bold]Step 4: Modem Configuration[/bold]\n")
        
        # Modem Type
        console.print("Modem type:")
        console.print("  uart - Serial UART (most common)")
        console.print("  null - Test mode (no hardware)\n")
        
        modem_type = self._get_answer(
            'modem_type',
            Prompt.ask,
            "Modem type",
            choices=['uart', 'null'],
            default='uart'
        )
        self.config.set('system.modem.protocol.type', modem_type)
        
        # Modem Mode
        console.print("\nModem mode:")
        console.print("  air - Standard air interface (repeater/hotspot)")
        console.print("  dfsi - DFSI mode for interfacing with V.24 repeaters\n")
        
        modem_mode = self._get_answer(
            'modem_mode',
            Prompt.ask,
            "Modem mode",
            choices=['air', 'dfsi'],
            default='air'
        )
        self.config.set('system.modem.protocol.mode', modem_mode)
        
        if modem_type == 'uart':
            serial_port = self._get_answer(
                'serial_port',
                Prompt.ask,
                "Serial port",
                default="/dev/ttyUSB0"
            )
            self.config.set('system.modem.protocol.uart.port', serial_port)
            
            console.print("\n[dim]Leave at 50 unless you know specific values[/dim]")
            rx_level = self._get_answer(
                'rx_level',
                IntPrompt.ask,
                "RX level (0-100)",
                default=50
            )
            self.config.set('system.modem.rxLevel', rx_level)
            
            tx_level = self._get_answer(
                'tx_level',
                IntPrompt.ask,
                "TX level (0-100)",
                default=50
            )
            self.config.set('system.modem.txLevel', tx_level)

        # DFSI Configuration (if dfsi mode selected)
        if modem_mode == 'dfsi':
            console.print("\n[bold]Step 3b: DFSI Settings[/bold]\n")
            
            if self._get_answer('configure_dfsi', Confirm.ask, "Configure DFSI settings?", default=False):
                rtrt = self._get_answer(
                    'dfsi_rtrt',
                    Confirm.ask,
                    "Enable DFSI RT/RT?",
                    default=True
                )
                self.config.set('system.modem.dfsiRtrt', rtrt)
                
                jitter = self._get_answer(
                    'dfsi_jitter',
                    IntPrompt.ask,
                    "DFSI Jitter (ms)",
                    default=200
                )
                self.config.set('system.modem.dfsiJitter', jitter)
                
                call_timeout = self._get_answer(
                    'dfsi_call_timeout',
                    IntPrompt.ask,
                    "DFSI Call Timeout (seconds)",
                    default=200
                )
                self.config.set('system.modem.dfsiCallTimeout', call_timeout)
                
                full_duplex = self._get_answer(
                    'dfsi_full_duplex',
                    Confirm.ask,
                    "Enable DFSI Full Duplex?",
                    default=False
                )
                self.config.set('system.modem.dfsiFullDuplex', full_duplex)
        
        console.print("\n[bold]Step 5: Network Settings[/bold]\n")
        
        # FNE Settings
        fne_address = self._get_answer(
            'fne_address',
            Prompt.ask,
            "FNE address",
            default="127.0.0.1"
        )
        while not validator.validate_ip_address(fne_address):
            console.print("[red]Invalid IP address[/red]")
            fne_address = self._get_answer(
                'fne_address',
                Prompt.ask,
                "FNE address",
                default="127.0.0.1"
            )
        self.config.set('network.address', fne_address)
        
        fne_port = self._get_answer(
            'fne_port',
            IntPrompt.ask,
            "FNE port",
            default=62031
        )
        self.config.set('network.port', fne_port)
        
        fne_password = self._get_answer(
            'fne_password',
            Prompt.ask,
            "FNE password",
            default="PASSWORD",
            password=True
        )
        self.config.set('network.password', fne_password)
        
        # RPC Configuration
        console.print("\n[bold]Step 5a: RPC & REST API Configuration[/bold]\n")
        
        # Generate random RPC password
        rpc_password = generate_random_password()
        console.print(f"[cyan]Generated RPC password:[/cyan] {rpc_password}")
        self.config.set('network.rpcPassword', rpc_password)
        
        # Store for display in summary
        self.rpc_password = rpc_password
        
        # Ask about REST API
        if self._get_answer(
            'rest_enable',
            Confirm.ask,
            "Enable REST API?",
            default=False
        ):
            self.config.set('network.restEnable', True)
            rest_password = generate_random_password()
            console.print(f"[cyan]Generated REST API password:[/cyan] {rest_password}")
            self.config.set('network.restPassword', rest_password)
            self.rest_password = rest_password
        else:
            self.config.set('network.restEnable', False)
            self.rest_password = None
        
        # Network Lookup and Transfer Settings
        console.print("\n[bold]Step 5b: Network Lookup & Transfer Settings[/bold]\n")
        
        update_lookups = self._get_answer(
            'update_lookups',
            Confirm.ask,
            "Update lookups from network?",
            default=True
        )
        self.config.set('network.updateLookups', update_lookups)
        if update_lookups:
            self.config.set('system.radio_id.time', 0)
            self.config.set('system.talkgroup_id.time', 0)
        
        save_lookups = self._get_answer(
            'save_lookups',
            Confirm.ask,
            "Save lookups from network?",
            default=False
        )
        self.config.set('network.saveLookups', save_lookups)
        
        allow_activity = self._get_answer(
            'allow_activity_transfer',
            Confirm.ask,
            "Allow activity transfer to FNE?",
            default=True
        )
        self.config.set('network.allowActivityTransfer', allow_activity)
        
        allow_diagnostic = self._get_answer(
            'allow_diagnostic_transfer',
            Confirm.ask,
            "Allow diagnostic transfer to FNE?",
            default=False
        )
        self.config.set('network.allowDiagnosticTransfer', allow_diagnostic)
        
        allow_status = self._get_answer(
            'allow_status_transfer',
            Confirm.ask,
            "Allow status transfer to FNE?",
            default=True)
        self.config.set('network.allowStatusTransfer', allow_status)
        
        # Radio ID and Talkgroup ID Configuration
        console.print("\n[bold]Step 5c: Radio ID & Talkgroup ID Configuration[/bold]\n")
        
        if self._get_answer(
            'configure_radio_talkgroup_ids',
            Confirm.ask,
            "Configure Radio ID and Talkgroup ID settings?",
            default=False
        ):
            # Radio ID Configuration
            console.print("\n[dim]Radio ID Settings:[/dim]")
            radio_id_file = self._get_answer(
                'radio_id_file',
                Prompt.ask,
                "Radio ID ACL file (path, or leave empty to skip)",
                default=""
            )
            if radio_id_file:
                self.config.set('system.radio_id.file', radio_id_file)
            
            radio_id_time = self._get_answer(
                'radio_id_time',
                IntPrompt.ask,
                "Radio ID update time (seconds, 0 = disabled)",
                default=0
            )
            self.config.set('system.radio_id.time', radio_id_time)
            
            radio_id_acl = self._get_answer(
                'radio_id_acl',
                Confirm.ask,
                "Enforce Radio ID ACLs?",
                default=False
            )
            self.config.set('system.radio_id.acl', radio_id_acl)
            
            # Talkgroup ID Configuration
            console.print("\n[dim]Talkgroup ID Settings:[/dim]")
            talkgroup_id_file = self._get_answer(
                'talkgroup_id_file',
                Prompt.ask,
                "Talkgroup ID ACL file (path, or leave empty to skip)",
                default=""
            )
            if talkgroup_id_file:
                self.config.set('system.talkgroup_id.file', talkgroup_id_file)
            
            talkgroup_id_time = self._get_answer(
                'talkgroup_id_time',
                IntPrompt.ask,
                "Talkgroup ID update time (seconds, 0 = disabled)",
                default=0
            )
            self.config.set('system.talkgroup_id.time', talkgroup_id_time)
            
            talkgroup_id_acl = self._get_answer(
                'talkgroup_id_acl',
                Confirm.ask,
                "Enforce Talkgroup ID ACLs?",
                default=False
            )
            self.config.set('system.talkgroup_id.acl', talkgroup_id_acl)
        
        # Protocol Selection
        console.print("\n[bold]Step 6: Protocol Selection[/bold]\n")
        
        console.print("[yellow]⚠ WARNING:[/yellow] Hotspots only support a [bold]single[/bold] digital mode.")
        console.print("[yellow]           Multi-mode is only supported on repeaters.\n[/yellow]")
        
        console.print("Select primary digital mode:")
        console.print("  1. P25")
        console.print("  2. DMR")
        console.print("  3. NXDN\n")
        
        primary_mode = self._get_answer(
            'primary_mode',
            IntPrompt.ask,
            "Primary mode",
            choices=['1', '2', '3'],
            default='1'
        )
        
        # Set primary mode
        enable_p25 = (int(primary_mode) == 1)
        enable_dmr = (int(primary_mode) == 2)
        enable_nxdn = (int(primary_mode) == 3)
        
        self.config.set('protocols.p25.enable', enable_p25)
        self.config.set('protocols.dmr.enable', enable_dmr)
        self.config.set('protocols.nxdn.enable', enable_nxdn)
        
        # Ask about additional modes (for repeaters)
        console.print("\n[dim]Additional modes (for multi-mode repeaters only):[/dim]")
        
        if not enable_p25:
            if self._get_answer(
                'also_enable_p25',
                Confirm.ask,
                "Also enable P25?",
                default=False
            ):
                enable_p25 = True
                self.config.set('protocols.p25.enable', True)
        
        if not enable_dmr:
            if self._get_answer(
                'also_enable_dmr',
                Confirm.ask,
                "Also enable DMR?",
                default=False
            ):
                enable_dmr = True
                self.config.set('protocols.dmr.enable', True)
        
        if not enable_nxdn:
            if self._get_answer(
                'also_enable_nxdn',
                Confirm.ask,
                "Also enable NXDN?",
                default=False
            ):
                enable_nxdn = True
                self.config.set('protocols.nxdn.enable', True)
        
        # Radio Parameters
        console.print("\n[bold]Step 7: Network/System ID Configuration[/bold]\n")
        
        # DMR Configuration
        if enable_dmr:
            console.print("[cyan]DMR Configuration:[/cyan]")
            
            # DMR Site Model (affects netId and siteId ranges)
            console.print("\nDMR Site Model (affects Network ID and Site ID ranges):")
            console.print("  1. SMALL - Most common (NetID: 1-127, SiteID: 1-31)")
            console.print("  2. TINY - Large NetID range (NetID: 1-511, SiteID: 1-7)")
            console.print("  3. LARGE - Large SiteID range (NetID: 1-31, SiteID: 1-127)")
            console.print("  4. HUGE - Very large SiteID (NetID: 1-3, SiteID: 1-1023)")
            site_model_choice = self._get_answer(
                'dmr_site_model',
                IntPrompt.ask,
                "Select site model",
                default=1
            )
            site_model_map = {1: 'small', 2: 'tiny', 3: 'large', 4: 'huge'}
            site_model_str = site_model_map.get(site_model_choice, 'small')
            site_model = get_dmr_site_model_from_string(site_model_str)
            
            dmr_info = get_network_id_info('dmr', site_model_str)
            
            # Color Code
            color_code = self._get_answer(
                'dmr_color_code',
                IntPrompt.ask,
                f"DMR Color Code ({dmr_info['colorCode']['min']}-{dmr_info['colorCode']['max']})",
                default=dmr_info['colorCode']['default']
            )
            while True:
                valid, error = validate_dmr_color_code(color_code)
                if valid:
                    break
                console.print(f"[red]{error}[/red]")
                color_code = IntPrompt.ask("DMR Color Code", default=1)
            self.config.set('system.config.colorCode', color_code)
            
            # DMR Network ID
            dmr_net_id = self._get_answer(
                'dmr_network_id',
                IntPrompt.ask,
                f"DMR Network ID ({dmr_info['dmrNetId']['min']}-{dmr_info['dmrNetId']['max']})",
                default=dmr_info['dmrNetId']['default']
            )
            while True:
                valid, error = validate_dmr_network_id(dmr_net_id, site_model)
                if valid:
                    break
                console.print(f"[red]{error}[/red]")
                dmr_net_id = IntPrompt.ask("DMR Network ID", default=1)
            self.config.set('system.config.dmrNetId', dmr_net_id)
            
            # DMR Site ID
            dmr_site_id = self._get_answer(
                'dmr_site_id',
                IntPrompt.ask,
                f"DMR Site ID ({dmr_info['siteId']['min']}-{dmr_info['siteId']['max']})",
                default=dmr_info['siteId']['default']
            )
            while True:
                valid, error = validate_dmr_site_id(dmr_site_id, site_model)
                if valid:
                    break
                console.print(f"[red]{error}[/red]")
                dmr_site_id = IntPrompt.ask("DMR Site ID", default=1)
            self.config.set('system.config.siteId', dmr_site_id)
        
        # P25 Configuration
        if enable_p25:
            console.print("\n[cyan]P25 Configuration:[/cyan]")
            p25_info = get_network_id_info('p25')
            
            # NAC
            nac_str = self._get_answer(
                'p25_nac',
                Prompt.ask,
                f"P25 NAC (hex: 0x000-0x{p25_info['nac']['max']:03X}, decimal: {p25_info['nac']['min']}-{p25_info['nac']['max']})",
                default=f"0x{p25_info['nac']['default']:03X}"
            )
            while True:
                try:
                    nac = int(nac_str, 0)
                    valid, error = validate_p25_nac(nac)
                    if valid:
                        break
                    console.print(f"[red]{error}[/red]")
                    nac_str = Prompt.ask("P25 NAC", default="0x293")
                except ValueError:
                    console.print("[red]Invalid format. Use hex (0x293) or decimal (659)[/red]")
                    nac_str = Prompt.ask("P25 NAC", default="0x293")
            self.config.set('system.config.nac', nac)
            
            # Network ID (WACN)
            net_id_str = self._get_answer(
                'p25_network_id',
                Prompt.ask,
                f"P25 Network ID/WACN (hex: 0x1-0x{p25_info['netId']['max']:X}, decimal: {p25_info['netId']['min']}-{p25_info['netId']['max']})",
                default=f"0x{p25_info['netId']['default']:X}"
            )
            while True:
                try:
                    net_id = int(net_id_str, 0)
                    valid, error = validate_p25_network_id(net_id)
                    if valid:
                        break
                    console.print(f"[red]{error}[/red]")
                    net_id_str = Prompt.ask("P25 Network ID", default="0xBB800")
                except ValueError:
                    console.print("[red]Invalid format. Use hex (0xBB800) or decimal (768000)[/red]")
                    net_id_str = Prompt.ask("P25 Network ID", default="0xBB800")
            self.config.set('system.config.netId', net_id)
            
            # System ID
            sys_id_str = self._get_answer(
                'p25_system_id',
                Prompt.ask,
                f"P25 System ID (hex: 0x1-0x{p25_info['sysId']['max']:X}, decimal: {p25_info['sysId']['min']}-{p25_info['sysId']['max']})",
                default=f"0x{p25_info['sysId']['default']:03X}"
            )
            while True:
                try:
                    sys_id = int(sys_id_str, 0)
                    valid, error = validate_p25_system_id(sys_id)
                    if valid:
                        break
                    console.print(f"[red]{error}[/red]")
                    sys_id_str = Prompt.ask("P25 System ID", default="0x001")
                except ValueError:
                    console.print("[red]Invalid format. Use hex (0x001) or decimal (1)[/red]")
                    sys_id_str = Prompt.ask("P25 System ID", default="0x001")
            self.config.set('system.config.sysId', sys_id)
            
            # RFSS ID
            rfss_id = self._get_answer(
                'p25_rfss_id',
                IntPrompt.ask,
                f"P25 RFSS ID ({p25_info['rfssId']['min']}-{p25_info['rfssId']['max']})",
                default=p25_info['rfssId']['default']
            )
            while True:
                valid, error = validate_p25_rfss_id(rfss_id)
                if valid:
                    break
                console.print(f"[red]{error}[/red]")
                rfss_id = IntPrompt.ask("P25 RFSS ID", default=1)
            self.config.set('system.config.rfssId', rfss_id)
            
            # P25 Site ID
            p25_site_id = self._get_answer(
                'p25_site_id',
                IntPrompt.ask,
                f"P25 Site ID ({p25_info['siteId']['min']}-{p25_info['siteId']['max']})",
                default=p25_info['siteId']['default']
            )
            while True:
                valid, error = validate_p25_site_id(p25_site_id)
                if valid:
                    break
                console.print(f"[red]{error}[/red]")
                p25_site_id = IntPrompt.ask("P25 Site ID", default=1)
            # Note: siteId is shared, only set if DMR didn't set it
            if not enable_dmr:
                self.config.set('system.config.siteId', p25_site_id)
        
        # NXDN Configuration
        if enable_nxdn:
            console.print("\n[cyan]NXDN Configuration:[/cyan]")
            nxdn_info = get_network_id_info('nxdn')
            
            # RAN
            ran = self._get_answer(
                'nxdn_ran',
                IntPrompt.ask,
                f"NXDN RAN ({nxdn_info['ran']['min']}-{nxdn_info['ran']['max']})",
                default=nxdn_info['ran']['default']
            )
            while True:
                valid, error = validate_nxdn_ran(ran)
                if valid:
                    break
                console.print(f"[red]{error}[/red]")
                ran = IntPrompt.ask("NXDN RAN", default=1)
            self.config.set('system.config.ran', ran)
            
            # System ID (Location ID)
            nxdn_sys_id_str = self._get_answer(
                'nxdn_system_id',
                Prompt.ask,
                f"NXDN System ID (hex: 0x1-0x{nxdn_info['sysId']['max']:X}, decimal: {nxdn_info['sysId']['min']}-{nxdn_info['sysId']['max']})",
                default=f"0x{nxdn_info['sysId']['default']:03X}"
            )
            while True:
                try:
                    nxdn_sys_id = int(nxdn_sys_id_str, 0)
                    valid, error = validate_nxdn_location_id(nxdn_sys_id)
                    if valid:
                        break
                    console.print(f"[red]{error}[/red]")
                    nxdn_sys_id_str = Prompt.ask("NXDN System ID", default="0x001")
                except ValueError:
                    console.print("[red]Invalid format. Use hex (0x001) or decimal (1)[/red]")
                    nxdn_sys_id_str = Prompt.ask("NXDN System ID", default="0x001")
            # Note: sysId is shared with P25, only set if P25 didn't set it
            if not enable_p25:
                self.config.set('system.config.sysId', nxdn_sys_id)
            
            # NXDN Site ID
            nxdn_site_id_str = self._get_answer(
                'nxdn_site_id',
                Prompt.ask,
                f"NXDN Site ID (hex: 0x1-0x{nxdn_info['siteId']['max']:X}, decimal: {nxdn_info['siteId']['min']}-{nxdn_info['siteId']['max']})",
                default=str(nxdn_info['siteId']['default'])
            )
            while True:
                try:
                    nxdn_site_id = int(nxdn_site_id_str, 0)
                    valid, error = validate_nxdn_location_id(nxdn_site_id)
                    if valid:
                        break
                    console.print(f"[red]{error}[/red]")
                    nxdn_site_id_str = Prompt.ask("NXDN Site ID", default="1")
                except ValueError:
                    console.print("[red]Invalid format. Use hex or decimal[/red]")
                    nxdn_site_id_str = Prompt.ask("NXDN Site ID", default="1")
            # Note: siteId is shared, only set if neither DMR nor P25 set it
            if not enable_dmr and not enable_p25:
                self.config.set('system.config.siteId', nxdn_site_id)
        
        # If no protocols enabled DMR/P25/NXDN individually, still prompt for generic site ID
        if not enable_dmr and not enable_p25 and not enable_nxdn:
            site_id = self._get_answer(
                'site_id',
                IntPrompt.ask,
                "Site ID",
                default=1
            )
            self.config.set('system.config.siteId', site_id)
        
        # Frequency Configuration
        console.print("\n[bold]Step 8: Frequency Configuration[/bold]\n")
        
        # Always configure frequency for conventional systems
        self._configure_frequency()
        
        # Location (optional)
        console.print("\n[bold]Step 9: Location Information (optional)[/bold]\n")
        
        if self._get_answer(
            'configure_location',
            Confirm.ask,
            "Configure location information?",
            default=False
        ):
            try:
                latitude = float(self._get_answer(
                    'latitude',
                    Prompt.ask,
                    "Latitude",
                    default="0.0"
                ))
                self.config.set('system.info.latitude', latitude)
                
                longitude = float(self._get_answer(
                    'longitude',
                    Prompt.ask,
                    "Longitude",
                    default="0.0"
                ))
                self.config.set('system.info.longitude', longitude)
                
                location = self._get_answer(
                    'location_description',
                    Prompt.ask,
                    "Location description",
                    default=""
                )
                if location:
                    self.config.set('system.info.location', location)
                
                power = self._get_answer(
                    'tx_power',
                    IntPrompt.ask,
                    "TX power (watts)",
                    default=10
                )
                self.config.set('system.info.power', power)
            except ValueError:
                console.print("[yellow]Invalid location data, skipping[/yellow]")
        
        # Summary and Save
        return self._show_summary_and_save()
    
    def _configure_frequency(self):
        """Configure frequency and channel assignment"""
        # Show available bands
        console.print("[cyan]Available Frequency Bands:[/cyan]\n")
        
        table = Table(show_header=True, header_style="bold cyan")
        table.add_column("#", style="dim", width=3)
        table.add_column("Band", style="cyan")
        table.add_column("TX Range", style="yellow")
        table.add_column("Offset", style="green")
        
        band_list = list(BAND_PRESETS.items())
        for i, (key, band) in enumerate(band_list, 1):
            tx_min, tx_max = band['tx_range']
            offset = band['input_offset']
            table.add_row(
                str(i),
                band['name'],
                f"{tx_min:.1f}-{tx_max:.1f} MHz",
                f"{offset:+.1f} MHz"
            )
        
        console.print(table)
        console.print()
        
        # Select band
        band_choice = self._get_answer(
            'frequency_band',
            IntPrompt.ask,
            "Select frequency band",
            choices=[str(i) for i in range(1, len(band_list) + 1)],
            default="3"
        )
        
        band_index = int(band_choice) - 1
        preset_key = band_list[band_index][0]
        preset = BAND_PRESETS[preset_key]
        
        # Confirm 800MHz selection
        if preset_key == '800mhz':
            if not self._get_answer(
                'confirm_800mhz',
                Confirm.ask,
                f"\n[yellow]Are you sure {preset['name']} is the frequency band you want?[/yellow]",
                default=False
            ):
                return self._configure_frequency()
        
        # Use band index as channel ID (0-15 range), except 900MHz is always channel ID 15
        channel_id = 15 if preset_key == '900mhz' else band_index
        
        # Get TX frequency
        console.print(f"\n[cyan]Selected: {preset['name']}[/cyan]")
        console.print(f"TX Range: {preset['tx_range'][0]:.1f}-{preset['tx_range'][1]:.1f} MHz")
        console.print(f"Input Offset: {preset['input_offset']:+.1f} MHz\n")
        
        tx_freq_mhz = None
        while tx_freq_mhz is None:
            try:
                tx_input = self._get_answer(
                    'tx_frequency',
                    Prompt.ask,
                    "Transmit frequency (MHz)",
                    default=f"{preset['tx_range'][0]:.4f}"
                )
                tx_freq_mhz = float(tx_input)
                
                # Validate frequency is in range
                if not (preset['tx_range'][0] <= tx_freq_mhz <= preset['tx_range'][1]):
                    console.print(f"[red]Frequency must be between {preset['tx_range'][0]:.1f} and "
                                f"{preset['tx_range'][1]:.1f} MHz[/red]")
                    tx_freq_mhz = None
                    continue
                
                # Calculate channel assignment
                channel_id_result, channel_no, tx_hz, rx_hz = calculate_channel_assignment(
                    tx_freq_mhz, preset_key, channel_id
                )
                
                # Show calculated values
                console.print(f"\n[green]✓ Channel Assignment Calculated:[/green]")
                console.print(f"  Channel ID:     {channel_id_result}")
                console.print(f"  Channel Number: {channel_no} (0x{channel_no:03X})")
                console.print(f"  TX Frequency:   {tx_hz/1000000:.6f} MHz")
                console.print(f"  RX Frequency:   {rx_hz/1000000:.6f} MHz")
                
                # Apply to configuration
                self.config.set('system.config.channelId', channel_id_result)
                self.config.set('system.config.channelNo', channel_no)
                self.config.set('system.config.txFrequency', tx_hz)
                self.config.set('system.config.rxFrequency', rx_hz)
                
                # Ensure the band is in the IDEN table
                if channel_id_result not in self.iden_table.entries:
                    entry = create_iden_entry_from_preset(channel_id_result, preset_key)
                    self.iden_table.add_entry(entry)
                
            except ValueError as e:
                console.print(f"[red]Error: {e}[/red]")
                tx_freq_mhz = None
    
    def _show_summary_and_save(self) -> Optional[Path]:
        """Show configuration summary, allow corrections, and save"""
        while True:
            console.print("\n[bold cyan]Configuration Summary[/bold cyan]\n")
            
            summary = self.config.get_summary()
            
            table = Table(show_header=False)
            table.add_column("Parameter", style="cyan")
            table.add_column("Value", style="yellow")
            
            table.add_row("Template", self.template_name)
            table.add_row("Identity", summary['identity'])
            table.add_row("Peer ID", str(summary['peer_id']))
            table.add_row("FNE", f"{summary['fne_address']}:{summary['fne_port']}")
            
            protocols = []
            if summary['protocols']['dmr']:
                protocols.append(f"DMR (CC:{summary['color_code']})")
            if summary['protocols']['p25']:
                protocols.append(f"P25 (NAC:0x{summary['nac']:03X})")
            if summary['protocols']['nxdn']:
                protocols.append("NXDN")
            table.add_row("Protocols", ", ".join(protocols))
            
            table.add_row("Modem Type", summary['modem_type'])
            table.add_row("Modem Mode", summary['modem_mode'].upper())
            if summary['modem_port'] != 'N/A':
                table.add_row("Modem Port", summary['modem_port'])
            
            table.add_row("RPC Password", summary['rpc_password'])
            if summary['rest_enabled']:
                table.add_row("REST API", "Enabled")
                table.add_row("REST API Password", summary['rest_password'])
            else:
                table.add_row("REST API", "Disabled")
            
            # Add frequency information if configured
            if summary['channel_id'] >= 0:
                table.add_row("Channel ID", str(summary['channel_id']))
                table.add_row("Channel Number", f"0x{summary['channel_no']:03X}")
                if summary['tx_frequency'] > 0:
                    tx_mhz = summary['tx_frequency'] / 1000000
                    rx_mhz = summary['rx_frequency'] / 1000000
                    table.add_row("TX Frequency", f"{tx_mhz:.6f} MHz")
                    table.add_row("RX Frequency", f"{rx_mhz:.6f} MHz")
            
            console.print(table)
            console.print()
            
            # Validate
            errors = self.config.validate()
            if errors:
                console.print("[yellow]Configuration has warnings:[/yellow]")
                for error in errors:
                    console.print(f"  [yellow]•[/yellow] {error}")
                console.print()
                
                if not Confirm.ask("Continue anyway?", default=True):
                    return None
            else:
                console.print("[green]✓ Configuration is valid[/green]\n")
            
            # Review and correction
            console.print("[bold]Review Your Configuration[/bold]\n")
            choice = Prompt.ask(
                "Is this configuration correct?",
                choices=["yes", "no"],
                default="yes"
            )
            
            if choice.lower() == "no":
                console.print("\n[yellow]Configuration returned for editing.[/yellow]")
                console.print("[dim]Please restart the wizard or manually edit the configuration files.[/dim]\n")
                return None
            
            console.print()

            default_filename = f"{summary['identity'].lower()}.yml"
            default_path = str(Path(self.config_dir) / default_filename)
            output_path = Prompt.ask(
                "Output file name",
                default=default_path
            )
            
            output_file = Path(str(Path(self.config_dir) / Path(output_path)))
            
            # Check if file exists
            if output_file.exists():
                if not Confirm.ask(f"[yellow]{output_file} already exists. Overwrite?[/yellow]", default=False):
                    output_path = Prompt.ask("Output file path")
                    output_file = Path(output_path)
            
            try:
                self.config.save(output_file)

                console.print(f"\n[green]✓ Conventional system created successfully![/green]")
                console.print(f"\nConfiguration files saved in: [cyan]{self.config_dir}[/cyan]")
                console.print(f"  • Conventional Channel: [yellow]{output_file}[/yellow]")
                
                # Save IDEN table if configured
                if len(self.iden_table) > 0:
                    iden_file = Path(self.config_dir) / "iden_table.dat"
                    self.iden_table.save(iden_file)
                    console.print(f"  • Identity Table: [yellow]iden_table.dat[/yellow]")
                
                return output_file
            except Exception as e:
                console.print(f"[red]Error saving configuration:[/red] {e}")
                return None
    
    def _run_trunking_wizard(self) -> Optional[Path]:
        """Run trunking system wizard"""
        wizard = TrunkingWizard(self.answers)
        return wizard.run()


class TrunkingWizard:
    """Interactive trunking system wizard"""
    
    def __init__(self, answers: Optional[Dict[str, Any]] = None):
        self.answers = answers or {}
        self.iden_table: IdenTable = create_default_iden_table()
    
    def _get_answer(self, key: str, prompt_func, *args, **kwargs) -> Any:
        """
        Get answer from answers file or prompt user
        
        Args:
            key: Answer key to look up
            prompt_func: Function to call if answer not found (e.g., Prompt.ask)
            *args: Arguments to pass to prompt_func
            **kwargs: Keyword arguments to pass to prompt_func
            
        Returns:
            Answer value (from file or user prompt)
        """
        if key in self.answers:
            value = self.answers[key]
            # Display the value that was loaded from answers file
            if isinstance(value, bool):
                display_value = "Yes" if value else "No"
            else:
                display_value = str(value)
            console.print(f"[dim]{key}: {display_value}[/dim]")
            return value
        
        # Prompt user for input
        return prompt_func(*args, **kwargs)
    
    def run(self) -> Optional[Path]:
        """Run the trunking wizard"""
        # Don't clear or show banner here - run_wizard handles it
        console.print()
        console.print(Panel.fit(
            "[bold cyan]Trunked System Configuration Wizard[/bold cyan]\n"
            "Create a trunked system configuration with control and voice channels",
            border_style="cyan"
        ))
        console.print()
        
        # System basics
        console.print("[bold]Step 1: System Configuration[/bold]\n")
        
        system_name = self._get_answer(
            'system_name',
            Prompt.ask,
            "System name (for filenames)",
            default="trunked"
        )
        
        base_dir = self._get_answer(
            'base_dir',
            Prompt.ask,
            "Configuration directory",
            default="."
        )
        
        identity = self._get_answer(
            'identity',
            Prompt.ask,
            "System identity prefix",
            default="SITE001"
        )
        
        protocol = self._get_answer(
            'protocol',
            Prompt.ask,
            "Protocol",
            choices=['p25', 'dmr'],
            default='p25'
        )
        
        vc_count = self._get_answer(
            'vc_count',
            IntPrompt.ask,
            "Number of voice channels",
            default=2
        )
        
        # Logging Configuration
        console.print("\n[bold]Step 2: Logging Configuration[/bold]\n")
        
        log_path = None
        activity_log_path = None
        log_root = None
        use_syslog = False
        disable_non_auth_logging = False
        
        configure_logging = self._get_answer(
            'configure_logging',
            Confirm.ask,
            "Configure logging settings?",
            default=False
        )
        
        if configure_logging:
            # Log file path
            log_path = self._get_answer(
                'log_path',
                Prompt.ask,
                "Log file directory",
                default="."
            )
            
            # Activity log file path
            activity_log_path = self._get_answer(
                'activity_log_path',
                Prompt.ask,
                "Activity log directory",
                default="."
            )
            
            # Log file root/prefix
            log_root = self._get_answer(
                'log_root',
                Prompt.ask,
                "Log filename prefix (used for filenames and syslog)",
                default="DVM"
            )
            
            # Syslog usage
            use_syslog = self._get_answer(
                'use_syslog',
                Confirm.ask,
                "Enable syslog output?",
                default=False
            )
            
            # Disable non-authoritive logging
            disable_non_auth_logging = self._get_answer(
                'disable_non_auth_logging',
                Confirm.ask,
                "Disable non-authoritative logging?",
                default=False
            )
        
        # Network settings
        console.print("\n[bold]Step 3: Network Settings[/bold]\n")
        
        fne_address = self._get_answer(
            'fne_address',
            Prompt.ask,
            "FNE address",
            default="127.0.0.1"
        )
        
        fne_port = self._get_answer(
            'fne_port',
            IntPrompt.ask,
            "FNE port",
            default=62031
        )
        
        fne_password = self._get_answer(
            'fne_password',
            Prompt.ask,
            "FNE password",
            default="PASSWORD",
            password=True
        )

        base_peer_id = self._get_answer(
            'base_peer_id',
            IntPrompt.ask,
            "Base peer ID (CC will use this, VCs increment)",
            default=100000
        )
        
        base_rpc_port = self._get_answer(
            'base_rpc_port',
            IntPrompt.ask,
            "Base RPC port (CC will use this, VCs increment)",
            default=9890
        )
        
        # Network Lookup and Transfer Settings
        console.print("\n[bold]Step 3a: Network Lookup & Transfer Settings[/bold]\n")
        
        update_lookups = self._get_answer(
            'update_lookups',
            Confirm.ask,
            "Update lookups from network?",
            default=True
        )
        save_lookups = self._get_answer(
            'save_lookups',
            Confirm.ask,
            "Save lookups from network?",
            default=False
        )
        allow_activity = self._get_answer(
            'allow_activity_transfer',
            Confirm.ask,
            "Allow activity transfer to FNE?",
            default=True
        )
        allow_diagnostic = self._get_answer(
            'allow_diagnostic_transfer',
            Confirm.ask,
            "Allow diagnostic transfer to FNE?",
            default=False
        )
        allow_status = self._get_answer(
            'allow_status_transfer',
            Confirm.ask,
            "Allow status transfer to FNE?",
            default=True
        )
        
        # Radio ID and Talkgroup ID Configuration
        console.print("\n[bold]Step 3b: Radio ID & Talkgroup ID Configuration[/bold]\n")
        
        radio_id_file = None
        radio_id_time = 0
        radio_id_acl = False
        talkgroup_id_file = None
        talkgroup_id_time = 0
        talkgroup_id_acl = False
        
        if self._get_answer(
            'configure_radio_talkgroup_ids',
            Confirm.ask,
            "Configure Radio ID and Talkgroup ID settings?",
            default=False
        ):
            # Radio ID Configuration
            console.print("\n[dim]Radio ID Settings:[/dim]")
            radio_id_file = self._get_answer(
                'radio_id_file',
                Prompt.ask,
                "Radio ID ACL file (path, or leave empty to skip)",
                default=""
            )
            if not radio_id_file:
                radio_id_file = None
            
            radio_id_time = self._get_answer(
                'radio_id_time',
                IntPrompt.ask,
                "Radio ID update time (seconds, 0 = disabled)",
                default=0
            )
            
            radio_id_acl = self._get_answer(
                'radio_id_acl',
                Confirm.ask,
                "Enforce Radio ID ACLs?",
                default=False
            )
            
            # Talkgroup ID Configuration
            console.print("\n[dim]Talkgroup ID Settings:[/dim]")
            talkgroup_id_file = self._get_answer(
                'talkgroup_id_file',
                Prompt.ask,
                "Talkgroup ID ACL file (path, or leave empty to skip)",
                default=""
            )
            if not talkgroup_id_file:
                talkgroup_id_file = None
            
            talkgroup_id_time = self._get_answer(
                'talkgroup_id_time',
                IntPrompt.ask,
                "Talkgroup ID update time (seconds, 0 = disabled)",
                default=0
            )
            
            talkgroup_id_acl = self._get_answer(
                'talkgroup_id_acl',
                Confirm.ask,
                "Enforce Talkgroup ID ACLs?",
                default=False
            )
        
        # RPC & REST API Configuration
        console.print("\n[bold]Step 3c: RPC & REST API Configuration[/bold]\n")
        
        # Generate random RPC password
        rpc_password = generate_random_password()
        console.print(f"[cyan]Generated RPC password:[/cyan] {rpc_password}")
        
        base_rest_port = self._get_answer(
            'base_rest_port',
            IntPrompt.ask,
            "Base REST API port (CC will use this, VCs increment)",
            default=8080
        )
        
        # Ask about REST API
        rest_enabled = self._get_answer(
            'rest_enable',
            Confirm.ask,
            "Enable REST API?",
            default=False
        )
        if rest_enabled:
            rest_password = generate_random_password()
            console.print(f"[cyan]Generated REST API password:[/cyan] {rest_password}")
        else:
            rest_password = None
        
        # Network/System ID Configuration
        console.print("\n[bold]Step 4: Network/System ID Configuration[/bold]\n")
        console.print(f"[cyan]Protocol: {protocol.upper()}[/cyan]\n")
        
        # Initialize variables
        color_code = 1
        dmr_net_id = 1
        nac = 0x293
        net_id = 0xBB800
        sys_id = 0x001
        rfss_id = 1
        site_id = 1
        ran = 1
        
        if protocol == 'p25':
            # P25-specific configuration
            p25_info = get_network_id_info('p25')
            
            # NAC
            nac_str = self._get_answer(
                'p25_nac',
                Prompt.ask,
                f"P25 NAC (hex: 0x000-0x{p25_info['nac']['max']:03X}, decimal: {p25_info['nac']['min']}-{p25_info['nac']['max']})",
                default=f"0x{p25_info['nac']['default']:03X}"
            )
            while True:
                try:
                    nac = int(nac_str, 0)
                    valid, error = validate_p25_nac(nac)
                    if valid:
                        break
                    console.print(f"[red]{error}[/red]")
                    nac_str = Prompt.ask("P25 NAC", default="0x293")
                except ValueError:
                    console.print("[red]Invalid format. Use hex (0x293) or decimal (659)[/red]")
                    nac_str = Prompt.ask("P25 NAC", default="0x293")
            
            # Network ID (WACN)
            net_id_str = self._get_answer(
                'p25_network_id',
                Prompt.ask,
                f"P25 Network ID/WACN (hex: 0x1-0x{p25_info['netId']['max']:X}, decimal: {p25_info['netId']['min']}-{p25_info['netId']['max']})",
                default=f"0x{p25_info['netId']['default']:X}"
            )
            while True:
                try:
                    net_id = int(net_id_str, 0)
                    valid, error = validate_p25_network_id(net_id)
                    if valid:
                        break
                    console.print(f"[red]{error}[/red]")
                    net_id_str = Prompt.ask("P25 Network ID", default="0xBB800")
                except ValueError:
                    console.print("[red]Invalid format. Use hex (0xBB800) or decimal (768000)[/red]")
                    net_id_str = Prompt.ask("P25 Network ID", default="0xBB800")
            
            # System ID
            sys_id_str = self._get_answer(
                'p25_system_id',
                Prompt.ask,
                f"P25 System ID (hex: 0x1-0x{p25_info['sysId']['max']:X}, decimal: {p25_info['sysId']['min']}-{p25_info['sysId']['max']})",
                default=f"0x{p25_info['sysId']['default']:03X}"
            )
            while True:
                try:
                    sys_id = int(sys_id_str, 0)
                    valid, error = validate_p25_system_id(sys_id)
                    if valid:
                        break
                    console.print(f"[red]{error}[/red]")
                    sys_id_str = Prompt.ask("P25 System ID", default="0x001")
                except ValueError:
                    console.print("[red]Invalid format. Use hex (0x001) or decimal (1)[/red]")
                    sys_id_str = Prompt.ask("P25 System ID", default="0x001")
            
            # RFSS ID
            rfss_id = self._get_answer(
                'p25_rfss_id',
                IntPrompt.ask,
                f"P25 RFSS ID ({p25_info['rfssId']['min']}-{p25_info['rfssId']['max']})",
                default=p25_info['rfssId']['default']
            )
            while True:
                valid, error = validate_p25_rfss_id(rfss_id)
                if valid:
                    break
                console.print(f"[red]{error}[/red]")
                rfss_id = IntPrompt.ask("P25 RFSS ID", default=1)
            
            # Site ID
            site_id = self._get_answer(
                'p25_site_id',
                IntPrompt.ask,
                f"P25 Site ID ({p25_info['siteId']['min']}-{p25_info['siteId']['max']})",
                default=p25_info['siteId']['default']
            )
            while True:
                valid, error = validate_p25_site_id(site_id)
                if valid:
                    break
                console.print(f"[red]{error}[/red]")
                site_id = IntPrompt.ask("P25 Site ID", default=1)
                
        elif protocol == 'dmr':
            # DMR-specific configuration
            console.print("DMR Site Model (affects Network ID and Site ID ranges):")
            console.print("  1. SMALL - Most common (NetID: 1-127, SiteID: 1-31)")
            console.print("  2. TINY - Large NetID range (NetID: 1-511, SiteID: 1-7)")
            console.print("  3. LARGE - Large SiteID range (NetID: 1-31, SiteID: 1-127)")
            console.print("  4. HUGE - Very large SiteID (NetID: 1-3, SiteID: 1-1023)")
            site_model_choice = self._get_answer(
                'dmr_site_model',
                IntPrompt.ask,
                "Select site model",
                default=1
            )
            site_model_map = {1: 'small', 2: 'tiny', 3: 'large', 4: 'huge'}
            site_model_str = site_model_map.get(site_model_choice, 'small')
            site_model = get_dmr_site_model_from_string(site_model_str)
            
            dmr_info = get_network_id_info('dmr', site_model_str)
            
            # Color Code
            color_code = self._get_answer(
                'dmr_color_code',
                IntPrompt.ask,
                f"DMR Color Code ({dmr_info['colorCode']['min']}-{dmr_info['colorCode']['max']})",
                default=dmr_info['colorCode']['default']
            )
            while True:
                valid, error = validate_dmr_color_code(color_code)
                if valid:
                    break
                console.print(f"[red]{error}[/red]")
                color_code = IntPrompt.ask("DMR Color Code", default=1)
            
            # DMR Network ID
            dmr_net_id = self._get_answer(
                'dmr_network_id',
                IntPrompt.ask,
                f"DMR Network ID ({dmr_info['dmrNetId']['min']}-{dmr_info['dmrNetId']['max']})",
                default=dmr_info['dmrNetId']['default']
            )
            while True:
                valid, error = validate_dmr_network_id(dmr_net_id, site_model)
                if valid:
                    break
                console.print(f"[red]{error}[/red]")
                dmr_net_id = IntPrompt.ask("DMR Network ID", default=1)
            
            # DMR Site ID
            site_id = self._get_answer(
                'dmr_site_id',
                IntPrompt.ask,
                f"DMR Site ID ({dmr_info['siteId']['min']}-{dmr_info['siteId']['max']})",
                default=dmr_info['siteId']['default']
            )
            while True:
                valid, error = validate_dmr_site_id(site_id, site_model)
                if valid:
                    break
                console.print(f"[red]{error}[/red]")
                site_id = IntPrompt.ask("DMR Site ID", default=1)
        
        console.print(f"\n[dim]These values will be applied consistently across all channels (CC and VCs)[/dim]")
        
        # Frequency configuration for control channel
        console.print("\n[bold]Step 5: Control Channel Frequency and Modem[/bold]\n")
        
        cc_channel_id, cc_channel_no, cc_band, cc_tx_hz, cc_rx_hz = self._configure_channel_frequency("Control Channel")
        cc_modem_config = self._configure_channel_modem("Control Channel")
        
        # Voice channel frequency and modem configuration
        console.print("\n[bold]Step 6: Voice Channel Frequencies and Modems[/bold]\n")
        
        vc_channels = []
        for i in range(1, vc_count + 1):
            console.print(f"\n[cyan]Voice Channel {i}:[/cyan]")
            vc_channel_id, vc_channel_no, _, vc_tx_hz, vc_rx_hz = self._configure_channel_frequency(f"VC{i}", cc_band)
            vc_modem_config = self._configure_channel_modem(f"VC{i}")
            vc_channels.append({
                'channel_id': vc_channel_id,
                'channel_no': vc_channel_no,
                'tx_hz': vc_tx_hz,
                'rx_hz': vc_rx_hz,
                'modem_type': vc_modem_config['modem_type'],
                'modem_mode': vc_modem_config['modem_mode'],
                'modem_port': vc_modem_config.get('modem_port', 'N/A'),
                'dfsi_rtrt': vc_modem_config.get('dfsi_rtrt'),
                'dfsi_jitter': vc_modem_config.get('dfsi_jitter'),
                'dfsi_call_timeout': vc_modem_config.get('dfsi_call_timeout'),
                'dfsi_full_duplex': vc_modem_config.get('dfsi_full_duplex')
            })
            console.print()
        
        # Create system
        console.print("\n[bold cyan]Creating trunked system...[/bold cyan]\n")
        
        try:
            system = TrunkingSystem(Path(base_dir), system_name)
            
            # Show configuration summary
            console.print("\n[bold cyan]Trunked System Configuration Summary[/bold cyan]\n")
            
            table = Table(show_header=False)
            table.add_column("Parameter", style="cyan")
            table.add_column("Value", style="yellow")
            
            table.add_row("System Name", system_name)
            table.add_row("Base Directory", base_dir)
            table.add_row("Protocol", protocol.upper())
            table.add_row("Voice Channels", str(vc_count))
            table.add_row("FNE Address", fne_address)
            table.add_row("FNE Port", str(fne_port))
            table.add_row("Base Peer ID", str(base_peer_id))
            table.add_row("Base RPC Port", str(base_rpc_port))
            table.add_row("RPC Password", rpc_password)
            if rest_enabled:
                table.add_row("REST API", "Enabled")
                table.add_row("Base REST Port", str(base_rest_port))
                table.add_row("REST API Password", rest_password)
            else:
                table.add_row("REST API", "Disabled")
            
            if protocol == 'dmr':
                table.add_row("DMR Color Code", str(color_code))
                table.add_row("DMR Network ID", str(dmr_net_id))
            elif protocol == 'p25':
                table.add_row("P25 NAC", f"0x{nac:03X}")
                table.add_row("P25 Network ID", str(net_id))
                table.add_row("P25 System ID", str(sys_id))
            
            console.print(table)
            console.print()
            
            # Show channel frequency details
            console.print("[bold cyan]Channel Configuration[/bold cyan]\n")
            
            # Control Channel
            cc_tx_mhz = cc_tx_hz / 1000000
            cc_rx_mhz = cc_rx_hz / 1000000
            console.print("[bold]Control Channel (CC):[/bold]")
            console.print(f"  Channel ID:     {cc_channel_id}")
            console.print(f"  Channel Number: 0x{cc_channel_no:03X}")
            console.print(f"  TX Frequency:   {cc_tx_mhz:.6f} MHz")
            console.print(f"  RX Frequency:   {cc_rx_mhz:.6f} MHz")
            console.print(f"  Modem Type:     {cc_modem_config['modem_type']}")
            console.print(f"  Modem Mode:     {cc_modem_config['modem_mode'].upper()}")
            if cc_modem_config.get('modem_port'):
                console.print(f"  Modem Port:     {cc_modem_config['modem_port']}")
            console.print()
            
            # Voice Channels
            console.print("[bold]Voice Channels:[/bold]")
            for i, vc in enumerate(vc_channels, 1):
                vc_tx_mhz = vc['tx_hz'] / 1000000
                vc_rx_mhz = vc['rx_hz'] / 1000000
                console.print(f"  VC{i}:")
                console.print(f"    Channel ID:     {vc['channel_id']}")
                console.print(f"    Channel Number: 0x{vc['channel_no']:03X}")
                console.print(f"    TX Frequency:   {vc_tx_mhz:.6f} MHz")
                console.print(f"    RX Frequency:   {vc_rx_mhz:.6f} MHz")
                console.print(f"    Modem Type:     {vc['modem_type']}")
                console.print(f"    Modem Mode:     {vc['modem_mode'].upper()}")
                if vc.get('modem_port'):
                    console.print(f"    Modem Port:     {vc['modem_port']}")
            
            # Review and correction
            console.print("[bold]Review Your Configuration[/bold]\n")
            choice = Prompt.ask(
                "Is this configuration correct?",
                choices=["yes", "no"],
                default="yes"
            )
            
            if choice.lower() == "no":
                console.print("\n[yellow]Configuration creation cancelled.[/yellow]")
                console.print("[dim]Please restart the wizard or manually edit the configuration files: [cyan]" + base_dir + "[/cyan][/dim]\n")
                return Path(base_dir)
            
            console.print()

            # Prepare kwargs based on protocol
            create_kwargs = {
                'protocol': protocol,
                'vc_count': vc_count,
                'fne_address': fne_address,
                'fne_port': fne_port,
                'fne_password': fne_password,
                'base_peer_id': base_peer_id,
                'base_rpc_port': base_rpc_port,
                'rpc_password': rpc_password,
                'base_rest_port': base_rest_port,
                'rest_password': rest_password,
                'update_lookups': update_lookups,
                'save_lookups': save_lookups,
                'allow_activity_transfer': allow_activity,
                'allow_diagnostic_transfer': allow_diagnostic,
                'allow_status_transfer': allow_status,
                'radio_id_file': radio_id_file,
                'radio_id_time': radio_id_time,
                'radio_id_acl': radio_id_acl,
                'talkgroup_id_file': talkgroup_id_file,
                'talkgroup_id_time': talkgroup_id_time,
                'talkgroup_id_acl': talkgroup_id_acl,
                'system_identity': identity,
                'modem_type': cc_modem_config['modem_type'],
                'cc_dfsi_rtrt': cc_modem_config.get('dfsi_rtrt'),
                'cc_dfsi_jitter': cc_modem_config.get('dfsi_jitter'),
                'cc_dfsi_call_timeout': cc_modem_config.get('dfsi_call_timeout'),
                'cc_dfsi_full_duplex': cc_modem_config.get('dfsi_full_duplex'),
                'cc_channel_id': cc_channel_id,
                'cc_channel_no': cc_channel_no,
                'vc_channels': vc_channels,
                'log_path': log_path,
                'activity_log_path': activity_log_path,
                'log_root': log_root,
                'use_syslog': use_syslog,
                'disable_non_auth_logging': disable_non_auth_logging
            }
            
            # Add protocol-specific network/system IDs
            if protocol == 'p25':
                create_kwargs.update({
                    'nac': nac,
                    'net_id': net_id,
                    'sys_id': sys_id,
                    'rfss_id': rfss_id,
                    'site_id': site_id
                })
            elif protocol == 'dmr':
                create_kwargs.update({
                    'color_code': color_code,
                    'dmr_net_id': dmr_net_id,
                    'site_id': site_id
                })
            
            system.create_system(**create_kwargs)
            
            # Save IDEN table
            if len(self.iden_table) > 0:
                iden_file = Path(base_dir) / "iden_table.dat"
                self.iden_table.save(iden_file)
                console.print(f"[green]✓[/green] Identity table saved: {iden_file}")
                console.print(f"[dim]  ({len(self.iden_table)} channel identity entries)[/dim]\n")
            
            console.print(f"\n[green]✓ Trunked system created successfully![/green]")
            console.print(f"\nConfiguration files saved in: [cyan]{base_dir}[/cyan]")
            console.print(f"  • Control Channel: [yellow]{system_name}-cc.yml[/yellow]")
            for i in range(1, vc_count + 1):
                console.print(f"  • Voice Channel {i}: [yellow]{system_name}-vc{i:02d}.yml[/yellow]")
            console.print(f"  • Identity Table: [yellow]iden_table.dat[/yellow]")
            
            return Path(base_dir)
            
        except Exception as e:
            console.print(f"[red]Error creating system:[/red] {e}")
            return None
    
    def _configure_channel_frequency(self, channel_name: str, default_band: Optional[str] = None) -> Tuple[int, int, str, int, int]:
        """
        Configure frequency for a channel
        
        Args:
            channel_name: Name of the channel (for display)
            default_band: Optional band preset key to use as default selection
            
        Returns:
            Tuple of (channel_id, channel_number, band_preset_key, tx_hz, rx_hz)
        """
        # Show available bands
        console.print(f"[cyan]Configure {channel_name} Frequency:[/cyan]\n")
        
        table = Table(show_header=True, header_style="bold cyan")
        table.add_column("#", style="dim", width=3)
        table.add_column("Band", style="cyan")
        table.add_column("TX Range", style="yellow")
        
        band_list = list(BAND_PRESETS.items())
        for i, (key, band) in enumerate(band_list, 1):
            tx_min, tx_max = band['tx_range']
            table.add_row(
                str(i),
                band['name'],
                f"{tx_min:.1f}-{tx_max:.1f} MHz"
            )
        
        console.print(table)
        console.print()
        
        # Determine default band choice
        default_choice = "3"
        if default_band:
            # Find the index of the default band
            for i, (key, _) in enumerate(band_list, 1):
                if key == default_band:
                    default_choice = str(i)
                    break
        
        # Select band
        band_choice = IntPrompt.ask(
            "Select frequency band",
            choices=[str(i) for i in range(1, len(band_list) + 1)],
            default=default_choice
        )
        
        band_index = int(band_choice) - 1
        preset_key = band_list[band_index][0]
        preset = BAND_PRESETS[preset_key]
        
        # Confirm 800MHz selection
        if preset_key == '800mhz':
            if not Confirm.ask(f"\n[yellow]Are you sure {preset['name']} is the frequency band you want?[/yellow]", default=False):
                return self._configure_channel_frequency(channel_name, default_band)
        
        # Use band index as channel ID, except 900MHz is always channel ID 15
        channel_id = 15 if preset_key == '900mhz' else band_index
        
        # Get TX frequency
        console.print(f"\n[cyan]{preset['name']}[/cyan]")
        console.print(f"TX Range: {preset['tx_range'][0]:.1f}-{preset['tx_range'][1]:.1f} MHz\n")
        
        while True:
            try:
                tx_input = Prompt.ask(
                    f"{channel_name} TX frequency (MHz)",
                    default=f"{preset['tx_range'][0]:.4f}"
                )
                tx_freq_mhz = float(tx_input)
                
                # Validate and calculate
                if not (preset['tx_range'][0] <= tx_freq_mhz <= preset['tx_range'][1]):
                    console.print(f"[red]Frequency must be between {preset['tx_range'][0]:.1f} and "
                                f"{preset['tx_range'][1]:.1f} MHz[/red]")
                    continue
                
                channel_id_result, channel_no, tx_hz, rx_hz = calculate_channel_assignment(
                    tx_freq_mhz, preset_key, channel_id
                )
                
                console.print(f"[green]✓ Assigned:[/green] Ch ID {channel_id_result}, Ch# {channel_no} (0x{channel_no:03X}), "
                            f"RX {rx_hz/1000000:.6f} MHz\n")
                
                # Ensure the band is in the IDEN table
                if channel_id_result not in self.iden_table.entries:
                    entry = create_iden_entry_from_preset(channel_id_result, preset_key)
                    self.iden_table.add_entry(entry)
                
                return (channel_id_result, channel_no, preset_key, tx_hz, rx_hz)
                
            except ValueError as e:
                console.print(f"[red]Error: {e}[/red]")
    
    def _configure_channel_modem(self, channel_name: str) -> Dict[str, Any]:
        """
        Configure modem for a channel
        
        Args:
            channel_name: Name of the channel (for display)
            
        Returns:
            Dict with modem configuration (type, mode, port, and DFSI settings if applicable)
        """
        console.print(f"\n[cyan]Configure {channel_name} Modem:[/cyan]\n")
        
        # Modem Type
        modem_type = Prompt.ask(
            f"{channel_name} modem type",
            choices=['uart', 'null'],
            default='uart'
        )
        
        # Modem Mode
        modem_mode = Prompt.ask(
            f"{channel_name} modem mode",
            choices=['air', 'dfsi'],
            default='air'
        )
        
        # Serial port (only if UART)
        modem_port = None
        if modem_type == 'uart':
            modem_port = Prompt.ask(
                f"{channel_name} serial port",
                default="/dev/ttyUSB0"
            )
        
        # DFSI Configuration (if dfsi mode selected)
        dfsi_rtrt = None
        dfsi_jitter = None
        dfsi_call_timeout = None
        dfsi_full_duplex = None
        
        if modem_mode == 'dfsi':
            if Confirm.ask(f"\nConfigure {channel_name} DFSI settings?", default=False):
                dfsi_rtrt = Confirm.ask(
                    "Enable DFSI RT/RT?",
                    default=True
                )
                
                dfsi_jitter = IntPrompt.ask(
                    "DFSI Jitter (ms)",
                    default=200
                )
                
                dfsi_call_timeout = IntPrompt.ask(
                    "DFSI Call Timeout (seconds)",
                    default=200
                )
                
                dfsi_full_duplex = Confirm.ask(
                    "Enable DFSI Full Duplex?",
                    default=False
                )
        
        return {
            'modem_type': modem_type,
            'modem_mode': modem_mode,
            'modem_port': modem_port,
            'dfsi_rtrt': dfsi_rtrt,
            'dfsi_jitter': dfsi_jitter,
            'dfsi_call_timeout': dfsi_call_timeout,
            'dfsi_full_duplex': dfsi_full_duplex
        }


def run_wizard(wizard_type: str = 'auto', answers: Optional[Dict[str, Any]] = None) -> Optional[Path]:
    """
    Run configuration wizard
    
    Args:
        wizard_type: 'single', 'trunk', or 'auto' (asks user)
        answers: Optional dictionary of answers to pre-populate prompts
    
    Returns:
        Path to saved configuration or None
    """
    try:
        if wizard_type == 'auto':
            console.clear()
            console.print()
            
            # Display banner
            console.print(f"[cyan]{__BANNER__}[/cyan]")
            console.print(f"[bold cyan]Digital Voice Modem (DVM) Configuration Generator {__VER__}[/bold cyan]")
            console.print("[dim]Copyright (c) 2026 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.[/dim]")
            console.print()
            
            console.print("[bold]Configuration Wizard[/bold]\n")
            console.print("What would you like to create?\n")
            console.print("1. Single instance (repeater, hotspot, etc.)")
            console.print("2. Trunked system (control + voice channels)")
            console.print()
            
            choice = IntPrompt.ask(
                "Select type",
                choices=['1', '2'],
                default='1'
            )
            
            wizard_type = 'single' if int(choice) == 1 else 'trunk'

        if wizard_type == 'single':
            wizard = ConfigWizard(answers)
            return wizard.run()
        else:
            wizard = TrunkingWizard(answers)
            return wizard.run()
    
    except KeyboardInterrupt:
        console.print("\n\n[yellow]Wizard cancelled by user.[/yellow]")
        return None


if __name__ == '__main__':
    result = run_wizard()
    if result:
        console.print(f"\n[green]Done! Configuration saved to: {result}[/green]")
