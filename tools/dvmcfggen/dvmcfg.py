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

Command-line interface for creating/managing DVMHost configurations
"""

import argparse
import sys
from pathlib import Path
from rich.console import Console
from rich.table import Table
from rich import print as rprint

from config_manager import DVMConfig
from templates import TEMPLATES, get_template
from trunking_manager import TrunkingSystem
from answers_loader import AnswersLoader
from wizard import run_wizard, generate_random_password
from version import __BANNER__, __VER__
from iden_table import IdenTable, calculate_channel_assignment, create_iden_entry_from_preset, BAND_PRESETS
from network_ids import (
    get_network_id_info, validate_dmr_color_code, validate_dmr_network_id, validate_dmr_site_id,
    validate_p25_nac, validate_p25_network_id, validate_p25_system_id, validate_p25_rfss_id,
    validate_p25_site_id, validate_nxdn_ran, validate_nxdn_location_id,
    get_dmr_site_model_from_string
)

console = Console()

def cmd_create(args):
    """Create new configuration from template"""
    try:
        config = DVMConfig()
        config.config = get_template(args.template)
        
        # Apply custom settings if provided
        if args.identity:
            config.set('system.identity', args.identity)
        if args.peer_id:
            config.set('network.id', args.peer_id)
        if args.fne_address:
            config.set('network.address', args.fne_address)
        if args.fne_port:
            config.set('network.port', args.fne_port)
        if args.fne_password:
            config.set('network.password', args.fne_password)
        if args.callsign:
            config.set('system.cwId.callsign', args.callsign)
        
        # Handle modem configuration
        if args.modem_type:
            config.set('system.modem.protocol.type', args.modem_type)
        if args.modem_mode:
            config.set('system.modem.protocol.mode', args.modem_mode)
        if args.modem_port:
            config.set('system.modem.protocol.uart.port', args.modem_port)
        if args.rx_level is not None:
            config.set('system.modem.rxLevel', args.rx_level)
        if args.tx_level is not None:
            config.set('system.modem.txLevel', args.tx_level)
        
        # Handle DFSI settings
        if args.dfsi_rtrt is not None:
            config.set('system.modem.dfsiRtrt', args.dfsi_rtrt)
        if args.dfsi_jitter is not None:
            config.set('system.modem.dfsiJitter', args.dfsi_jitter)
        if args.dfsi_call_timeout is not None:
            config.set('system.modem.dfsiCallTimeout', args.dfsi_call_timeout)
        if args.dfsi_full_duplex is not None:
            config.set('system.modem.dfsiFullDuplex', args.dfsi_full_duplex)
        
        # Handle RPC/REST settings
        if args.rpc_password:
            config.set('network.rpcPassword', args.rpc_password)
        elif args.generate_rpc_password:
            rpc_pwd = generate_random_password()
            config.set('network.rpcPassword', rpc_pwd)
            console.print(f"[cyan]Generated RPC password:[/cyan] {rpc_pwd}")
        
        if args.rest_enable:
            config.set('network.restEnable', True)
            if args.rest_password:
                config.set('network.restPassword', args.rest_password)
            elif args.generate_rest_password:
                rest_pwd = generate_random_password()
                config.set('network.restPassword', rest_pwd)
                console.print(f"[cyan]Generated REST API password:[/cyan] {rest_pwd}")
        
        # Handle network lookup/transfer settings
        if args.update_lookups is not None:
            config.set('network.updateLookups', args.update_lookups)
        if args.save_lookups is not None:
            config.set('network.saveLookups', args.save_lookups)
        if args.allow_activity_transfer is not None:
            config.set('network.allowActivityTransfer', args.allow_activity_transfer)
        if args.allow_diagnostic_transfer is not None:
            config.set('network.allowDiagnosticTransfer', args.allow_diagnostic_transfer)
        if args.allow_status_transfer is not None:
            config.set('network.allowStatusTransfer', args.allow_status_transfer)
        
        # Handle Radio ID and Talkgroup ID settings
        if args.radio_id_file:
            config.set('system.radio_id.file', args.radio_id_file)
        if args.radio_id_time is not None:
            config.set('system.radio_id.time', args.radio_id_time)
        if args.radio_id_acl is not None:
            config.set('system.radio_id.acl', args.radio_id_acl)
        if args.talkgroup_id_file:
            config.set('system.talkgroup_id.file', args.talkgroup_id_file)
        if args.talkgroup_id_time is not None:
            config.set('system.talkgroup_id.time', args.talkgroup_id_time)
        if args.talkgroup_id_acl is not None:
            config.set('system.talkgroup_id.acl', args.talkgroup_id_acl)
        
        # Handle protocol settings
        if args.enable_p25 is not None:
            config.set('protocols.p25.enable', args.enable_p25)
        if args.enable_dmr is not None:
            config.set('protocols.dmr.enable', args.enable_dmr)
        if args.enable_nxdn is not None:
            config.set('protocols.nxdn.enable', args.enable_nxdn)
        
        # Handle DMR configuration
        if args.color_code is not None:
            valid, error = validate_dmr_color_code(args.color_code)
            if not valid:
                console.print(f"[red]Error:[/red] {error}")
                sys.exit(1)
            config.set('system.config.colorCode', args.color_code)
        
        if args.dmr_net_id is not None:
            site_model = get_dmr_site_model_from_string(args.dmr_site_model) if args.dmr_site_model else get_dmr_site_model_from_string('small')
            valid, error = validate_dmr_network_id(args.dmr_net_id, site_model)
            if not valid:
                console.print(f"[red]Error:[/red] {error}")
                sys.exit(1)
            config.set('system.config.dmrNetId', args.dmr_net_id)
        
        if args.dmr_site_id is not None:
            site_model = get_dmr_site_model_from_string(args.dmr_site_model) if args.dmr_site_model else get_dmr_site_model_from_string('small')
            valid, error = validate_dmr_site_id(args.dmr_site_id, site_model)
            if not valid:
                console.print(f"[red]Error:[/red] {error}")
                sys.exit(1)
            config.set('system.config.siteId', args.dmr_site_id)
        
        # Handle P25 configuration
        if args.nac is not None:
            valid, error = validate_p25_nac(args.nac)
            if not valid:
                console.print(f"[red]Error:[/red] {error}")
                sys.exit(1)
            config.set('system.config.nac', args.nac)
        
        if args.p25_net_id is not None:
            valid, error = validate_p25_network_id(args.p25_net_id)
            if not valid:
                console.print(f"[red]Error:[/red] {error}")
                sys.exit(1)
            config.set('system.config.netId', args.p25_net_id)
        
        if args.p25_sys_id is not None:
            valid, error = validate_p25_system_id(args.p25_sys_id)
            if not valid:
                console.print(f"[red]Error:[/red] {error}")
                sys.exit(1)
            config.set('system.config.sysId', args.p25_sys_id)
        
        if args.p25_rfss_id is not None:
            valid, error = validate_p25_rfss_id(args.p25_rfss_id)
            if not valid:
                console.print(f"[red]Error:[/red] {error}")
                sys.exit(1)
            config.set('system.config.rfssId', args.p25_rfss_id)
        
        if args.p25_site_id is not None:
            valid, error = validate_p25_site_id(args.p25_site_id)
            if not valid:
                console.print(f"[red]Error:[/red] {error}")
                sys.exit(1)
            config.set('system.config.siteId', args.p25_site_id)
        
        # Handle NXDN configuration
        if args.nxdn_ran is not None:
            valid, error = validate_nxdn_ran(args.nxdn_ran)
            if not valid:
                console.print(f"[red]Error:[/red] {error}")
                sys.exit(1)
            config.set('system.config.ran', args.nxdn_ran)
        
        if args.site_id is not None:
            config.set('system.config.siteId', args.site_id)
        
        # Handle location settings
        if args.latitude is not None:
            config.set('system.info.latitude', args.latitude)
        if args.longitude is not None:
            config.set('system.info.longitude', args.longitude)
        if args.location:
            config.set('system.info.location', args.location)
        if args.tx_power is not None:
            config.set('system.info.power', args.tx_power)
        
        # Handle logging configuration
        if args.log_path:
            config.set('log.filePath', args.log_path)
        if args.activity_log_path:
            config.set('log.activityFilePath', args.activity_log_path)
        if args.log_root:
            config.set('log.fileRoot', args.log_root)
        if args.use_syslog is not None:
            config.set('log.useSysLog', args.use_syslog)
        if args.disable_non_auth_logging is not None:
            config.set('log.disableNonAuthoritiveLogging', args.disable_non_auth_logging)
        
        # Handle frequency configuration
        iden_table = None
        if args.tx_freq and args.band:
            if args.band not in BAND_PRESETS:
                console.print(f"[red]Error:[/red] Invalid band '{args.band}'. Available: {', '.join(BAND_PRESETS.keys())}")
                sys.exit(1)
            
            preset = BAND_PRESETS[args.band]
            tx_freq_mhz = args.tx_freq
            
            # Validate frequency is in band
            if not (preset['tx_range'][0] <= tx_freq_mhz <= preset['tx_range'][1]):
                console.print(f"[red]Error:[/red] TX frequency {tx_freq_mhz} MHz is outside {args.band} range "
                            f"({preset['tx_range'][0]}-{preset['tx_range'][1]} MHz)")
                sys.exit(1)
            
            # Calculate channel assignment
            # Use band index as channel ID, except 900MHz is always 15
            band_list = list(BAND_PRESETS.keys())
            band_index = band_list.index(args.band)
            channel_id = 15 if args.band == '900mhz' else band_index
            
            channel_id_result, channel_no, tx_hz, rx_hz = calculate_channel_assignment(
                tx_freq_mhz, args.band, channel_id
            )
            
            # Set channel configuration
            config.set('system.config.channelId', channel_id_result)
            config.set('system.config.channelNo', channel_no)
            config.set('system.config.txFrequency', tx_hz)
            config.set('system.config.rxFrequency', rx_hz)
            
            console.print(f"[green]✓[/green] Frequency configured: TX {tx_freq_mhz:.6f} MHz, "
                        f"RX {rx_hz/1000000:.6f} MHz")
            console.print(f"[green]✓[/green] Channel ID {channel_id_result}, Channel# {channel_no} (0x{channel_no:03X})")
            
            # Create IDEN table entry
            iden_table = IdenTable()
            entry = create_iden_entry_from_preset(channel_id_result, args.band)
            iden_table.add_entry(entry)
        
        # Save configuration
        output_path = Path(args.output)
        config.save(output_path)
        
        console.print(f"[green]✓[/green] Configuration created: {output_path}")
        
        # Save IDEN table if frequency was configured
        if iden_table and len(iden_table) > 0:
            iden_path = output_path.parent / "iden_table.dat"
            iden_table.save(iden_path)
            console.print(f"[green]✓[/green] Identity table created: {iden_path}")
        
        # Validate if requested
        if args.validate:
            errors = config.validate()
            if errors:
                console.print("\n[yellow]Validation warnings:[/yellow]")
                for error in errors:
                    console.print(f"  • {error}")
            else:
                console.print("[green]✓[/green] Configuration is valid")
        
    except Exception as e:
        console.print(f"[red]Error:[/red] {e}", style="bold red")
        sys.exit(1)


def cmd_validate(args):
    """Validate configuration file"""
    try:
        config = DVMConfig(Path(args.config))
        errors = config.validate()
        
        if errors:
            console.print(f"[red]✗[/red] Configuration has {len(errors)} error(s):\n")
            for error in errors:
                console.print(f"  [red]•[/red] {error}")
            sys.exit(1)
        else:
            console.print(f"[green]✓[/green] Configuration is valid: {args.config}")
            
            # Show summary if requested
            if args.summary:
                summary = config.get_summary()
                table = Table(title="Configuration Summary")
                table.add_column("Parameter", style="cyan")
                table.add_column("Value", style="yellow")
                
                table.add_row("Identity", summary['identity'])
                table.add_row("Peer ID", str(summary['peer_id']))
                table.add_row("FNE Address", summary['fne_address'])
                table.add_row("FNE Port", str(summary['fne_port']))
                table.add_row("DMR Color Code", str(summary['color_code']))
                table.add_row("P25 NAC", f"0x{summary['nac']:03X}")
                table.add_row("Site ID", str(summary['site_id']))
                table.add_row("Modem Type", summary['modem_type'])
                
                protocols = []
                if summary['protocols']['dmr']:
                    protocols.append("DMR")
                if summary['protocols']['p25']:
                    protocols.append("P25")
                if summary['protocols']['nxdn']:
                    protocols.append("NXDN")
                table.add_row("Protocols", ", ".join(protocols))
                table.add_row("Control Channel", "Yes" if summary['is_control'] else "No")
                
                console.print()
                console.print(table)
    
    except FileNotFoundError:
        console.print(f"[red]Error:[/red] File not found: {args.config}")
        sys.exit(1)
    except Exception as e:
        console.print(f"[red]Error:[/red] {e}")
        sys.exit(1)


def cmd_edit(args):
    """Edit configuration value"""
    try:
        config_path = Path(args.config)
        config = DVMConfig(config_path)
        
        # Get current value
        current = config.get(args.key)
        console.print(f"Current value of '{args.key}': {current}")
        
        # Set new value
        # Try to preserve type
        if isinstance(current, bool):
            new_value = args.value.lower() in ('true', '1', 'yes', 'on')
        elif isinstance(current, int):
            new_value = int(args.value)
        elif isinstance(current, float):
            new_value = float(args.value)
        else:
            new_value = args.value
        
        config.set(args.key, new_value)
        config.save(config_path)
        
        console.print(f"[green]✓[/green] Updated '{args.key}' to: {new_value}")
        
        # Validate after edit
        errors = config.validate()
        if errors:
            console.print("\n[yellow]Validation warnings after edit:[/yellow]")
            for error in errors:
                console.print(f"  • {error}")
    
    except FileNotFoundError:
        console.print(f"[red]Error:[/red] File not found: {args.config}")
        sys.exit(1)
    except Exception as e:
        console.print(f"[red]Error:[/red] {e}")
        sys.exit(1)


def cmd_trunk_create(args):
    """Create trunked system"""
    try:
        system = TrunkingSystem(Path(args.base_dir), args.name)
        
        console.print(f"[cyan]Creating trunked system '{args.name}'...[/cyan]\n")
        
        # Handle frequency configuration
        cc_channel_id = 0
        cc_channel_no = 0
        vc_channels = None
        iden_table = None
        
        # Control channel frequency
        if args.cc_tx_freq and args.cc_band:
            if args.cc_band not in BAND_PRESETS:
                console.print(f"[red]Error:[/red] Invalid CC band '{args.cc_band}'. Available: {', '.join(BAND_PRESETS.keys())}")
                sys.exit(1)
            
            preset = BAND_PRESETS[args.cc_band]
            if not (preset['tx_range'][0] <= args.cc_tx_freq <= preset['tx_range'][1]):
                console.print(f"[red]Error:[/red] CC TX frequency {args.cc_tx_freq} MHz is outside {args.cc_band} range")
                sys.exit(1)
            
            band_list = list(BAND_PRESETS.keys())
            band_index = band_list.index(args.cc_band)
            cc_channel_id = 15 if args.cc_band == '900mhz' else band_index
            
            cc_channel_id, cc_channel_no, tx_hz, rx_hz = calculate_channel_assignment(
                args.cc_tx_freq, args.cc_band, cc_channel_id
            )
            
            console.print(f"[green]✓[/green] CC: TX {args.cc_tx_freq:.6f} MHz, RX {rx_hz/1000000:.6f} MHz, "
                        f"Ch ID {cc_channel_id}, Ch# {cc_channel_no} (0x{cc_channel_no:03X})")
            
            iden_table = IdenTable()
            entry = create_iden_entry_from_preset(cc_channel_id, args.cc_band)
            iden_table.add_entry(entry)
        
        # Voice channel frequencies
        if args.vc_tx_freqs and args.vc_bands:
            vc_tx_freqs = [float(f) for f in args.vc_tx_freqs.split(',')]
            vc_bands = args.vc_bands.split(',')
            
            if len(vc_tx_freqs) != args.vc_count:
                console.print(f"[red]Error:[/red] Number of VC frequencies ({len(vc_tx_freqs)}) "
                            f"doesn't match VC count ({args.vc_count})")
                sys.exit(1)
            
            if len(vc_bands) != args.vc_count:
                console.print(f"[red]Error:[/red] Number of VC bands ({len(vc_bands)}) "
                            f"doesn't match VC count ({args.vc_count})")
                sys.exit(1)
            
            vc_channels = []
            if iden_table is None:
                iden_table = IdenTable()
            
            for i, (vc_tx_freq, vc_band) in enumerate(zip(vc_tx_freqs, vc_bands), 1):
                if vc_band not in BAND_PRESETS:
                    console.print(f"[red]Error:[/red] Invalid VC{i} band '{vc_band}'")
                    sys.exit(1)
                
                preset = BAND_PRESETS[vc_band]
                if not (preset['tx_range'][0] <= vc_tx_freq <= preset['tx_range'][1]):
                    console.print(f"[red]Error:[/red] VC{i} TX frequency {vc_tx_freq} MHz is outside {vc_band} range")
                    sys.exit(1)
                
                band_list = list(BAND_PRESETS.keys())
                band_index = band_list.index(vc_band)
                vc_channel_id = 15 if vc_band == '900mhz' else band_index
                
                vc_channel_id, vc_channel_no, tx_hz, rx_hz = calculate_channel_assignment(
                    vc_tx_freq, vc_band, vc_channel_id
                )
                
                vc_channels.append({
                    'channel_id': vc_channel_id,
                    'channel_no': vc_channel_no
                })
                
                console.print(f"[green]✓[/green] VC{i}: TX {vc_tx_freq:.6f} MHz, RX {rx_hz/1000000:.6f} MHz, "
                            f"Ch ID {vc_channel_id}, Ch# {vc_channel_no} (0x{vc_channel_no:03X})")
                
                # Add IDEN entry if not already present
                if vc_channel_id not in iden_table.entries:
                    entry = create_iden_entry_from_preset(vc_channel_id, vc_band)
                    iden_table.add_entry(entry)
        
        # Prepare base system creation kwargs
        create_kwargs = {
            'protocol': args.protocol,
            'vc_count': args.vc_count,
            'fne_address': args.fne_address,
            'fne_port': args.fne_port,
            'fne_password': args.fne_password,
            'base_peer_id': args.base_peer_id,
            'base_rpc_port': args.base_rpc_port,
            'system_identity': args.identity,
            'modem_type': args.modem_type,
            'cc_channel_id': cc_channel_id,
            'cc_channel_no': cc_channel_no,
            'vc_channels': vc_channels
        }
        
        # Add RPC/REST settings
        if args.rpc_password:
            create_kwargs['rpc_password'] = args.rpc_password
        elif args.generate_rpc_password:
            rpc_pwd = generate_random_password()
            create_kwargs['rpc_password'] = rpc_pwd
            console.print(f"[cyan]Generated RPC password:[/cyan] {rpc_pwd}")
        
        if args.base_rest_port:
            create_kwargs['base_rest_port'] = args.base_rest_port
        
        if args.rest_enable:
            if args.rest_password:
                create_kwargs['rest_password'] = args.rest_password
            elif args.generate_rest_password:
                rest_pwd = generate_random_password()
                create_kwargs['rest_password'] = rest_pwd
                console.print(f"[cyan]Generated REST API password:[/cyan] {rest_pwd}")
        
        # Add network lookup/transfer settings
        if args.update_lookups is not None:
            create_kwargs['update_lookups'] = args.update_lookups
        if args.save_lookups is not None:
            create_kwargs['save_lookups'] = args.save_lookups
        if args.allow_activity_transfer is not None:
            create_kwargs['allow_activity_transfer'] = args.allow_activity_transfer
        if args.allow_diagnostic_transfer is not None:
            create_kwargs['allow_diagnostic_transfer'] = args.allow_diagnostic_transfer
        if args.allow_status_transfer is not None:
            create_kwargs['allow_status_transfer'] = args.allow_status_transfer
        
        # Add Radio ID and Talkgroup ID settings
        if args.radio_id_file:
            create_kwargs['radio_id_file'] = args.radio_id_file
        if args.radio_id_time is not None:
            create_kwargs['radio_id_time'] = args.radio_id_time
        if args.radio_id_acl is not None:
            create_kwargs['radio_id_acl'] = args.radio_id_acl
        if args.talkgroup_id_file:
            create_kwargs['talkgroup_id_file'] = args.talkgroup_id_file
        if args.talkgroup_id_time is not None:
            create_kwargs['talkgroup_id_time'] = args.talkgroup_id_time
        if args.talkgroup_id_acl is not None:
            create_kwargs['talkgroup_id_acl'] = args.talkgroup_id_acl
        
        # Add logging settings
        if args.log_path:
            create_kwargs['log_path'] = args.log_path
        if args.activity_log_path:
            create_kwargs['activity_log_path'] = args.activity_log_path
        if args.log_root:
            create_kwargs['log_root'] = args.log_root
        if args.use_syslog is not None:
            create_kwargs['use_syslog'] = args.use_syslog
        if args.disable_non_auth_logging is not None:
            create_kwargs['disable_non_auth_logging'] = args.disable_non_auth_logging
        
        # Add protocol-specific settings
        if args.protocol == 'p25':
            create_kwargs['nac'] = args.nac
            create_kwargs['net_id'] = args.p25_net_id or 0xBB800
            create_kwargs['sys_id'] = args.p25_sys_id or 0x001
            create_kwargs['rfss_id'] = args.p25_rfss_id or 1
            create_kwargs['site_id'] = args.p25_site_id or args.site_id
        elif args.protocol == 'dmr':
            create_kwargs['color_code'] = args.color_code
            create_kwargs['dmr_net_id'] = args.dmr_net_id or 1
            create_kwargs['site_id'] = args.dmr_site_id or args.site_id
        
        system.create_system(**create_kwargs)
        
        # Save IDEN table if frequencies were configured
        if iden_table and len(iden_table) > 0:
            iden_path = Path(args.base_dir) / "iden_table.dat"
            iden_table.save(iden_path)
            console.print(f"[green]✓[/green] Identity table created: {iden_path}")
        
        console.print(f"\n[green]✓[/green] Trunked system created successfully!")
        
    except Exception as e:
        console.print(f"[red]Error:[/red] {e}")
        sys.exit(1)


def cmd_trunk_validate(args):
    """Validate trunked system"""
    try:
        system = TrunkingSystem(Path(args.base_dir), args.name)
        system.load_system()
        
        console.print(f"[cyan]Validating trunked system '{args.name}'...[/cyan]\n")
        
        errors = system.validate_system()
        
        if errors:
            console.print(f"[red]✗[/red] System has errors:\n")
            for component, error_list in errors.items():
                console.print(f"[yellow]{component}:[/yellow]")
                for error in error_list:
                    console.print(f"  [red]•[/red] {error}")
                console.print()
            sys.exit(1)
        else:
            console.print(f"[green]✓[/green] Trunked system is valid")
            
            # Show summary
            table = Table(title=f"Trunked System: {args.name}")
            table.add_column("Component", style="cyan")
            table.add_column("Identity", style="yellow")
            table.add_column("Peer ID", style="magenta")
            table.add_column("RPC Port", style="green")
            
            cc = system.cc_config
            table.add_row(
                "Control Channel",
                cc.get('system.identity'),
                str(cc.get('network.id')),
                str(cc.get('network.rpcPort'))
            )
            
            for i, vc in enumerate(system.vc_configs, 1):
                table.add_row(
                    f"Voice Channel {i}",
                    vc.get('system.identity'),
                    str(vc.get('network.id')),
                    str(vc.get('network.rpcPort'))
                )
            
            console.print()
            console.print(table)
    
    except FileNotFoundError as e:
        console.print(f"[red]Error:[/red] {e}")
        sys.exit(1)
    except Exception as e:
        console.print(f"[red]Error:[/red] {e}")
        sys.exit(1)


def cmd_trunk_update(args):
    """Update trunked system setting"""
    try:
        system = TrunkingSystem(Path(args.base_dir), args.name)
        system.load_system()
        
        console.print(f"[cyan]Updating '{args.key}' across all configs...[/cyan]")
        
        # Determine value type
        value = args.value
        if value.lower() in ('true', 'false'):
            value = value.lower() == 'true'
        elif value.isdigit():
            value = int(value)
        
        system.update_all(args.key, value)
        system.save_all()
        
        console.print(f"[green]✓[/green] Updated '{args.key}' to '{value}' in all configs")
        
    except Exception as e:
        console.print(f"[red]Error:[/red] {e}")
        sys.exit(1)


def cmd_list_templates(args):
    """List available templates"""
    table = Table(title="Available Templates")
    table.add_column("Template Name", style="cyan")
    table.add_column("Description", style="yellow")
    
    descriptions = {
        'conventional': 'Standalone repeater',
        'enhanced': 'Enhanced repeater with channel grants',
        'control-channel-p25': 'P25 dedicated control channel for trunking',
        'control-channel-dmr': 'DMR dedicated control channel for trunking',
        'voice-channel': 'Voice channel for trunking system',
    }
    
    for name in sorted(TEMPLATES.keys()):
        desc = descriptions.get(name, 'N/A')
        table.add_row(name, desc)
    
    console.print(table)


def cmd_wizard(args):
    """Run interactive wizard"""
    answers = {}
    
    # Load answers from file if provided
    if hasattr(args, 'answers_file') and args.answers_file:
        try:
            answers = AnswersLoader.load_answers(args.answers_file)
            # Optionally validate answers
            AnswersLoader.validate_answers(answers, strict=False)
        except Exception as e:
            console.print(f"[red]Error loading answers file:[/red] {e}")
            sys.exit(1)
    
    wizard_type = args.type if hasattr(args, 'type') else 'auto'
    result = run_wizard(wizard_type, answers)
    if not result:
        sys.exit(1)


def main():
    # Prepare description with banner and copyright
    description = f"""
Digital Voice Modem (DVM) Configuration Generator {__VER__}
Copyright (c) 2026 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.

DVMHost Configuration Manager"""
    
    parser = argparse.ArgumentParser(
        description=description,
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    subparsers = parser.add_subparsers(dest='command', help='Commands')
    
    # create command
    create_parser = subparsers.add_parser('create', help='Create new configuration')
    create_parser.add_argument('--template', default='enhanced', choices=list(TEMPLATES.keys()),
                              help='Configuration template (default: enhanced)')
    create_parser.add_argument('--output', '-o', required=True, help='Output file path')
    create_parser.add_argument('--identity', help='System identity')
    create_parser.add_argument('--peer-id', type=int, help='Network peer ID')
    create_parser.add_argument('--fne-address', help='FNE address')
    create_parser.add_argument('--fne-port', type=int, help='FNE port')
    create_parser.add_argument('--fne-password', help='FNE password')
    create_parser.add_argument('--callsign', help='CWID callsign')
    
    # Modem configuration
    create_parser.add_argument('--modem-type', choices=['uart', 'null'],
                              help='Modem type')
    create_parser.add_argument('--modem-mode', choices=['air', 'dfsi'],
                              help='Modem mode')
    create_parser.add_argument('--modem-port', help='Serial port for UART modem')
    create_parser.add_argument('--rx-level', type=int, help='RX level (0-100)')
    create_parser.add_argument('--tx-level', type=int, help='TX level (0-100)')
    
    # DFSI settings
    create_parser.add_argument('--dfsi-rtrt', type=lambda x: x.lower() in ('true', '1', 'yes'),
                              help='Enable DFSI RT/RT (true/false)')
    create_parser.add_argument('--dfsi-jitter', type=int, help='DFSI jitter (ms)')
    create_parser.add_argument('--dfsi-call-timeout', type=int, help='DFSI call timeout (seconds)')
    create_parser.add_argument('--dfsi-full-duplex', type=lambda x: x.lower() in ('true', '1', 'yes'),
                              help='Enable DFSI full duplex (true/false)')
    
    # RPC/REST settings
    create_parser.add_argument('--rpc-password', help='RPC password')
    create_parser.add_argument('--generate-rpc-password', action='store_true',
                              help='Generate random RPC password')
    create_parser.add_argument('--rest-enable', action='store_true', help='Enable REST API')
    create_parser.add_argument('--rest-password', help='REST API password')
    create_parser.add_argument('--generate-rest-password', action='store_true',
                              help='Generate random REST API password')
    
    # Network lookup/transfer settings
    create_parser.add_argument('--update-lookups', type=lambda x: x.lower() in ('true', '1', 'yes'),
                              help='Update lookups from network (true/false)')
    create_parser.add_argument('--save-lookups', type=lambda x: x.lower() in ('true', '1', 'yes'),
                              help='Save lookups from network (true/false)')
    create_parser.add_argument('--allow-activity-transfer', type=lambda x: x.lower() in ('true', '1', 'yes'),
                              help='Allow activity transfer to FNE (true/false)')
    create_parser.add_argument('--allow-diagnostic-transfer', type=lambda x: x.lower() in ('true', '1', 'yes'),
                              help='Allow diagnostic transfer to FNE (true/false)')
    create_parser.add_argument('--allow-status-transfer', type=lambda x: x.lower() in ('true', '1', 'yes'),
                              help='Allow status transfer to FNE (true/false)')
    
    # Radio ID and Talkgroup ID settings
    create_parser.add_argument('--radio-id-file', help='Radio ID ACL file path')
    create_parser.add_argument('--radio-id-time', type=int, help='Radio ID update time (seconds)')
    create_parser.add_argument('--radio-id-acl', type=lambda x: x.lower() in ('true', '1', 'yes'),
                              help='Enforce Radio ID ACLs (true/false)')
    create_parser.add_argument('--talkgroup-id-file', help='Talkgroup ID ACL file path')
    create_parser.add_argument('--talkgroup-id-time', type=int, help='Talkgroup ID update time (seconds)')
    create_parser.add_argument('--talkgroup-id-acl', type=lambda x: x.lower() in ('true', '1', 'yes'),
                              help='Enforce Talkgroup ID ACLs (true/false)')
    
    # Protocol settings
    create_parser.add_argument('--enable-p25', type=lambda x: x.lower() in ('true', '1', 'yes'),
                              help='Enable P25 protocol (true/false)')
    create_parser.add_argument('--enable-dmr', type=lambda x: x.lower() in ('true', '1', 'yes'),
                              help='Enable DMR protocol (true/false)')
    create_parser.add_argument('--enable-nxdn', type=lambda x: x.lower() in ('true', '1', 'yes'),
                              help='Enable NXDN protocol (true/false)')
    
    # DMR settings
    create_parser.add_argument('--color-code', type=int, help='DMR color code')
    create_parser.add_argument('--dmr-net-id', type=int, help='DMR network ID')
    create_parser.add_argument('--dmr-site-id', type=int, help='DMR site ID')
    create_parser.add_argument('--dmr-site-model', choices=['small', 'tiny', 'large', 'huge'],
                              help='DMR site model')
    
    # P25 settings
    create_parser.add_argument('--nac', type=lambda x: int(x, 0), help='P25 NAC (hex or decimal)')
    create_parser.add_argument('--p25-net-id', type=lambda x: int(x, 0), help='P25 network ID (hex or decimal)')
    create_parser.add_argument('--p25-sys-id', type=lambda x: int(x, 0), help='P25 system ID (hex or decimal)')
    create_parser.add_argument('--p25-rfss-id', type=int, help='P25 RFSS ID')
    create_parser.add_argument('--p25-site-id', type=int, help='P25 site ID')
    
    # NXDN settings
    create_parser.add_argument('--nxdn-ran', type=int, help='NXDN RAN')
    
    # Generic site ID
    create_parser.add_argument('--site-id', type=int, help='Generic site ID')
    
    # Location settings
    create_parser.add_argument('--latitude', type=float, help='Location latitude')
    create_parser.add_argument('--longitude', type=float, help='Location longitude')
    create_parser.add_argument('--location', help='Location description')
    create_parser.add_argument('--tx-power', type=int, help='TX power (watts)')
    
    # Frequency configuration
    create_parser.add_argument('--tx-freq', type=float, help='Transmit frequency in MHz')
    create_parser.add_argument('--band', choices=list(BAND_PRESETS.keys()),
                              help='Frequency band (required with --tx-freq)')
    
    # Logging configuration
    create_parser.add_argument('--log-path', help='Log file directory path')
    create_parser.add_argument('--activity-log-path', help='Activity log directory path')
    create_parser.add_argument('--log-root', help='Log filename prefix and syslog prefix')
    create_parser.add_argument('--use-syslog', type=lambda x: x.lower() in ('true', '1', 'yes'),
                              help='Enable syslog output (true/false)')
    create_parser.add_argument('--disable-non-auth-logging', type=lambda x: x.lower() in ('true', '1', 'yes'),
                              help='Disable non-authoritative logging (true/false)')
    
    create_parser.add_argument('--validate', action='store_true', help='Validate after creation')
    create_parser.set_defaults(func=cmd_create)
    
    # validate command
    validate_parser = subparsers.add_parser('validate', help='Validate configuration')
    validate_parser.add_argument('config', help='Configuration file path')
    validate_parser.add_argument('--summary', '-s', action='store_true', help='Show summary')
    validate_parser.set_defaults(func=cmd_validate)
    
    # edit command
    edit_parser = subparsers.add_parser('edit', help='Edit configuration value')
    edit_parser.add_argument('config', help='Configuration file path')
    edit_parser.add_argument('key', help='Configuration key (dot notation)')
    edit_parser.add_argument('value', help='New value')
    edit_parser.set_defaults(func=cmd_edit)
    
    # trunk commands
    trunk_parser = subparsers.add_parser('trunk', help='Trunking system management')
    trunk_sub = trunk_parser.add_subparsers(dest='trunk_command')
    
    # trunk create
    trunk_create_parser = trunk_sub.add_parser('create', help='Create trunked system')
    trunk_create_parser.add_argument('--name', default='trunked', help='System name')
    trunk_create_parser.add_argument('--base-dir', required=True, help='Base directory')
    trunk_create_parser.add_argument('--protocol', choices=['p25', 'dmr'], default='p25',
                                     help='Protocol type')
    trunk_create_parser.add_argument('--vc-count', type=int, default=2,
                                     help='Number of voice channels')
    trunk_create_parser.add_argument('--fne-address', default='127.0.0.1', help='FNE address')
    trunk_create_parser.add_argument('--fne-port', type=int, default=62031, help='FNE port')
    trunk_create_parser.add_argument('--fne-password', default='PASSWORD', help='FNE password')
    trunk_create_parser.add_argument('--base-peer-id', type=int, default=100000,
                                     help='Base peer ID')
    trunk_create_parser.add_argument('--base-rpc-port', type=int, default=9890,
                                     help='Base RPC port')
    trunk_create_parser.add_argument('--identity', default='SKYNET', help='System identity')
    trunk_create_parser.add_argument('--modem-type', choices=['uart', 'null'], default='uart',
                                     help='Modem type')
    
    # RPC/REST settings
    trunk_create_parser.add_argument('--rpc-password', help='RPC password')
    trunk_create_parser.add_argument('--generate-rpc-password', action='store_true',
                                     help='Generate random RPC password')
    trunk_create_parser.add_argument('--base-rest-port', type=int, help='Base REST API port')
    trunk_create_parser.add_argument('--rest-enable', action='store_true', help='Enable REST API')
    trunk_create_parser.add_argument('--rest-password', help='REST API password')
    trunk_create_parser.add_argument('--generate-rest-password', action='store_true',
                                     help='Generate random REST API password')
    
    # Network lookup/transfer settings
    trunk_create_parser.add_argument('--update-lookups', type=lambda x: x.lower() in ('true', '1', 'yes'),
                                     help='Update lookups from network (true/false)')
    trunk_create_parser.add_argument('--save-lookups', type=lambda x: x.lower() in ('true', '1', 'yes'),
                                     help='Save lookups from network (true/false)')
    trunk_create_parser.add_argument('--allow-activity-transfer', type=lambda x: x.lower() in ('true', '1', 'yes'),
                                     help='Allow activity transfer to FNE (true/false)')
    trunk_create_parser.add_argument('--allow-diagnostic-transfer', type=lambda x: x.lower() in ('true', '1', 'yes'),
                                     help='Allow diagnostic transfer to FNE (true/false)')
    trunk_create_parser.add_argument('--allow-status-transfer', type=lambda x: x.lower() in ('true', '1', 'yes'),
                                     help='Allow status transfer to FNE (true/false)')
    
    # Radio ID and Talkgroup ID settings
    trunk_create_parser.add_argument('--radio-id-file', help='Radio ID ACL file path')
    trunk_create_parser.add_argument('--radio-id-time', type=int, help='Radio ID update time (seconds)')
    trunk_create_parser.add_argument('--radio-id-acl', type=lambda x: x.lower() in ('true', '1', 'yes'),
                                     help='Enforce Radio ID ACLs (true/false)')
    trunk_create_parser.add_argument('--talkgroup-id-file', help='Talkgroup ID ACL file path')
    trunk_create_parser.add_argument('--talkgroup-id-time', type=int, help='Talkgroup ID update time (seconds)')
    trunk_create_parser.add_argument('--talkgroup-id-acl', type=lambda x: x.lower() in ('true', '1', 'yes'),
                                     help='Enforce Talkgroup ID ACLs (true/false)')
    
    # Protocol-specific settings
    trunk_create_parser.add_argument('--nac', type=lambda x: int(x, 0), default=0x293,
                                     help='P25 NAC (hex or decimal)')
    trunk_create_parser.add_argument('--p25-net-id', type=lambda x: int(x, 0),
                                     help='P25 network ID (hex or decimal)')
    trunk_create_parser.add_argument('--p25-sys-id', type=lambda x: int(x, 0),
                                     help='P25 system ID (hex or decimal)')
    trunk_create_parser.add_argument('--p25-rfss-id', type=int, help='P25 RFSS ID')
    trunk_create_parser.add_argument('--p25-site-id', type=int, help='P25 site ID')
    
    trunk_create_parser.add_argument('--color-code', type=int, default=1, help='DMR color code')
    trunk_create_parser.add_argument('--dmr-net-id', type=int, help='DMR network ID')
    trunk_create_parser.add_argument('--dmr-site-id', type=int, help='DMR site ID')
    
    # Generic site ID
    trunk_create_parser.add_argument('--site-id', type=int, default=1, help='Site ID')
    
    # Frequency configuration
    trunk_create_parser.add_argument('--cc-tx-freq', type=float,
                                     help='Control channel TX frequency in MHz')
    trunk_create_parser.add_argument('--cc-band', choices=list(BAND_PRESETS.keys()),
                                     help='Control channel frequency band')
    trunk_create_parser.add_argument('--vc-tx-freqs',
                                     help='Voice channel TX frequencies (comma-separated, e.g., 851.0125,851.0250)')
    trunk_create_parser.add_argument('--vc-bands',
                                     help='Voice channel bands (comma-separated, e.g., 800mhz,800mhz)')
    
    # Logging configuration
    trunk_create_parser.add_argument('--log-path', help='Log file directory path')
    trunk_create_parser.add_argument('--activity-log-path', help='Activity log directory path')
    trunk_create_parser.add_argument('--log-root', help='Log filename prefix and syslog prefix')
    trunk_create_parser.add_argument('--use-syslog', type=lambda x: x.lower() in ('true', '1', 'yes'),
                                     help='Enable syslog output (true/false)')
    trunk_create_parser.add_argument('--disable-non-auth-logging', type=lambda x: x.lower() in ('true', '1', 'yes'),
                                     help='Disable non-authoritative logging (true/false)')
    
    trunk_create_parser.set_defaults(func=cmd_trunk_create)
    
    # trunk validate
    trunk_validate_parser = trunk_sub.add_parser('validate', help='Validate trunked system')
    trunk_validate_parser.add_argument('--name', default='trunked', help='System name')
    trunk_validate_parser.add_argument('--base-dir', required=True, help='Base directory')
    trunk_validate_parser.set_defaults(func=cmd_trunk_validate)
    
    # trunk update
    trunk_update_parser = trunk_sub.add_parser('update', help='Update all configs in system')
    trunk_update_parser.add_argument('--name', default='trunked', help='System name')
    trunk_update_parser.add_argument('--base-dir', required=True, help='Base directory')
    trunk_update_parser.add_argument('key', help='Configuration key')
    trunk_update_parser.add_argument('value', help='New value')
    trunk_update_parser.set_defaults(func=cmd_trunk_update)
    
    # templates command
    templates_parser = subparsers.add_parser('templates', help='List available templates')
    templates_parser.set_defaults(func=cmd_list_templates)
    
    # wizard command
    wizard_parser = subparsers.add_parser('wizard', help='Interactive configuration wizard')
    wizard_parser.add_argument('--type', choices=['single', 'trunk', 'auto'], default='auto',
                              help='Wizard type (auto asks user)')
    wizard_parser.add_argument('--answers-file', '-a', type=Path, 
                              help='Optional YAML file with wizard answers (uses as defaults)')
    wizard_parser.set_defaults(func=cmd_wizard)
    
    args = parser.parse_args()
    
    if args.command is None:
        # Display banner when run without command
        console.print(f"[cyan]{__BANNER__}[/cyan]")
        console.print(f"[bold cyan]Digital Voice Modem (DVM) Configuration Generator {__VER__}[/bold cyan]")
        console.print("[dim]Copyright (c) 2026 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.[/dim]")
        console.print()
        parser.print_help()
        sys.exit(1)
    
    if hasattr(args, 'func'):
        args.func(args)
    else:
        parser.print_help()


if __name__ == '__main__':
    main()
