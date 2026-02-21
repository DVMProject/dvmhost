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

Multi-Instance Trunking Manager
Manages complete trunked systems with control and voice channels
"""

from pathlib import Path
from typing import List, Dict, Any, Optional
import yaml
from config_manager import DVMConfig
from templates import get_template


class TrunkingSystem:
    """Manages a complete trunked system configuration"""
    
    def __init__(self, base_dir: Path, system_name: str = "trunked"):
        """
        Initialize trunking system manager
        
        Args:
            base_dir: Base directory for configuration files
            system_name: Name of the trunking system
        """
        self.base_dir = Path(base_dir)
        self.system_name = system_name
        self.cc_config: Optional[DVMConfig] = None
        self.vc_configs: List[DVMConfig] = []
        
    def create_system(self,
                     protocol: str = 'p25',
                     vc_count: int = 2,
                     fne_address: str = '127.0.0.1',
                     fne_port: int = 62031,
                     fne_password: str = 'PASSWORD',
                     base_peer_id: int = 100000,
                     base_rpc_port: int = 9890,
                     rpc_password: str = 'PASSWORD',
                     base_rest_port: int = 8080,
                     rest_password: str = None,
                     update_lookups: bool = True,
                     save_lookups: bool = True,
                     allow_activity_transfer: bool = True,
                     allow_diagnostic_transfer: bool = False,
                     allow_status_transfer: bool = True,
                     radio_id_file: str = None,
                     radio_id_time: int = 0,
                     radio_id_acl: bool = False,
                     talkgroup_id_file: str = None,
                     talkgroup_id_time: int = 0,
                     talkgroup_id_acl: bool = False,
                     system_identity: str = 'SKYNET',
                     nac: int = 0x293,
                     color_code: int = 1,
                     site_id: int = 1,
                     modem_type: str = 'uart',
                     cc_dfsi_rtrt: int = None,
                     cc_dfsi_jitter: int = None,
                     cc_dfsi_call_timeout: int = None,
                     cc_dfsi_full_duplex: bool = None,
                     cc_channel_id: int = 0,
                     cc_channel_no: int = 0,
                     dmr_net_id: int = None,
                     net_id: int = None,
                     sys_id: int = None,
                     rfss_id: int = None,
                     ran: int = None,
                     vc_channels: List[Dict[str, int]] = None,
                     log_path: str = None,
                     activity_log_path: str = None,
                     log_root: str = None,
                     use_syslog: bool = False,
                     disable_non_auth_logging: bool = False) -> None:
        """
        Create a complete trunked system
        
        Args:
            protocol: Protocol type ('p25' or 'dmr')
            vc_count: Number of voice channels
            fne_address: FNE address
            fne_port: FNE port
            fne_password: FNE password
            base_peer_id: Base peer ID (CC gets this, VCs increment)
            base_rpc_port: Base RPC port (CC gets this, VCs increment)
            rpc_password: RPC password
            base_rest_port: Base REST API port (CC gets this, VCs increment)
            rest_password: REST API password
            update_lookups: Update lookups from network
            save_lookups: Save lookups to network
            allow_activity_transfer: Allow activity transfer to FNE
            allow_diagnostic_transfer: Allow diagnostic transfer to FNE
            allow_status_transfer: Allow status transfer to FNE
            radio_id_file: Radio ID ACL file path
            radio_id_time: Radio ID update time (seconds)
            radio_id_acl: Enforce Radio ID ACLs
            talkgroup_id_file: Talkgroup ID ACL file path
            talkgroup_id_time: Talkgroup ID update time (seconds)
            talkgroup_id_acl: Enforce Talkgroup ID ACLs
            system_identity: System identity prefix
            nac: P25 NAC (for P25 systems)
            color_code: DMR color code (for DMR systems)
            site_id: Site ID
            modem_type: Modem type ('uart' or 'null')
            cc_dfsi_rtrt: Control channel DFSI RTRT (Round Trip Response Time, ms)
            cc_dfsi_jitter: Control channel DFSI Jitter (ms)
            cc_dfsi_call_timeout: Control channel DFSI Call Timeout (seconds)
            cc_dfsi_full_duplex: Control channel DFSI Full Duplex enabled
            cc_channel_id: Control channel ID
            cc_channel_no: Control channel number
            dmr_net_id: DMR Network ID (for DMR systems)
            net_id: P25 Network ID (for P25 systems)
            sys_id: P25 System ID (for P25 systems)
            rfss_id: P25 RFSS ID (for P25 systems)
            ran: NXDN RAN (for NXDN systems)
            vc_channels: List of voice channel configs with optional DFSI settings [{'channel_id': int, 'channel_no': int, 'dfsi_rtrt': int, ...}, ...]
            log_path: Log file directory path
            activity_log_path: Activity log directory path
            log_root: Log filename prefix and syslog prefix
            use_syslog: Enable syslog output
            disable_non_auth_logging: Disable non-authoritative logging
        """
        
        # If no VC channel info provided, use defaults (same ID as CC, sequential numbers)
        if vc_channels is None:
            vc_channels = []
            for i in range(1, vc_count + 1):
                vc_channels.append({
                    'channel_id': cc_channel_id,
                    'channel_no': cc_channel_no + i
                })
        # Create base directory
        self.base_dir.mkdir(parents=True, exist_ok=True)
        
        # Create control channel
        print(f"Creating control channel configuration...")
        if protocol.lower() == 'p25':
            cc_template = get_template('control-channel-p25')
        else:
            cc_template = get_template('control-channel-dmr')
        
        self.cc_config = DVMConfig()
        self.cc_config.config = cc_template
        
        # Configure control channel
        self.cc_config.set('system.identity', f'{system_identity}-CC')
        self.cc_config.set('network.id', base_peer_id)
        self.cc_config.set('network.address', fne_address)
        self.cc_config.set('network.port', fne_port)
        self.cc_config.set('network.password', fne_password)
        self.cc_config.set('network.rpcPort', base_rpc_port)
        self.cc_config.set('network.rpcPassword', rpc_password)
        if rest_password is not None:
            self.cc_config.set('network.restEnable', True)
            self.cc_config.set('network.restPort', base_rest_port)
            self.cc_config.set('network.restPassword', rest_password)
        else:
            self.cc_config.set('network.restEnable', False)
        
        # Network lookup and transfer settings
        self.cc_config.set('network.updateLookups', update_lookups)
        self.cc_config.set('network.saveLookups', save_lookups)
        self.cc_config.set('network.allowActivityTransfer', allow_activity_transfer)
        self.cc_config.set('network.allowDiagnosticTransfer', allow_diagnostic_transfer)
        self.cc_config.set('network.allowStatusTransfer', allow_status_transfer)
        
        # If updating lookups, set time values to 0
        if update_lookups:
            self.cc_config.set('system.radio_id.time', 0)
            self.cc_config.set('system.talkgroup_id.time', 0)
        else:
            # If not updating lookups, use provided values
            if radio_id_time > 0:
                self.cc_config.set('system.radio_id.time', radio_id_time)
            if talkgroup_id_time > 0:
                self.cc_config.set('system.talkgroup_id.time', talkgroup_id_time)
        
        # Radio ID and Talkgroup ID ACL settings
        if radio_id_file:
            self.cc_config.set('system.radio_id.file', radio_id_file)
        self.cc_config.set('system.radio_id.acl', radio_id_acl)
        
        if talkgroup_id_file:
            self.cc_config.set('system.talkgroup_id.file', talkgroup_id_file)
        self.cc_config.set('system.talkgroup_id.acl', talkgroup_id_acl)
        
        # Logging settings
        if log_path:
            self.cc_config.set('log.filePath', log_path)
        if activity_log_path:
            self.cc_config.set('log.activityFilePath', activity_log_path)
        if log_root:
            self.cc_config.set('log.fileRoot', log_root)
        if use_syslog:
            self.cc_config.set('log.useSysLog', use_syslog)
        if disable_non_auth_logging:
            self.cc_config.set('log.disableNonAuthoritiveLogging', disable_non_auth_logging)
        
        self.cc_config.set('system.modem.protocol.type', modem_type)
        
        # DFSI Configuration for control channel (if provided)
        if cc_dfsi_rtrt is not None:
            self.cc_config.set('system.modem.dfsiRtrt', cc_dfsi_rtrt)
        if cc_dfsi_jitter is not None:
            self.cc_config.set('system.modem.dfsiJitter', cc_dfsi_jitter)
        if cc_dfsi_call_timeout is not None:
            self.cc_config.set('system.modem.dfsiCallTimeout', cc_dfsi_call_timeout)
        if cc_dfsi_full_duplex is not None:
            self.cc_config.set('system.modem.dfsiFullDuplex', cc_dfsi_full_duplex)
        
        self.cc_config.set('system.config.channelId', cc_channel_id)
        self.cc_config.set('system.config.channelNo', cc_channel_no)
        
        # Set network/system IDs consistently
        if site_id is not None:
            self.cc_config.set('system.config.siteId', site_id)
        if nac is not None:
            self.cc_config.set('system.config.nac', nac)
        if color_code is not None:
            self.cc_config.set('system.config.colorCode', color_code)
        if dmr_net_id is not None:
            self.cc_config.set('system.config.dmrNetId', dmr_net_id)
        if net_id is not None:
            self.cc_config.set('system.config.netId', net_id)
        if sys_id is not None:
            self.cc_config.set('system.config.sysId', sys_id)
        if rfss_id is not None:
            self.cc_config.set('system.config.rfssId', rfss_id)
        if ran is not None:
            self.cc_config.set('system.config.ran', ran)
        
        # Build voice channel list for CC
        voice_channels = []
        for i in range(vc_count):
            vc_rpc_port = base_rpc_port + i + 1
            vc_info = vc_channels[i]
            voice_channels.append({
                'channelId': vc_info['channel_id'],
                'channelNo': vc_info['channel_no'],
                'rpcAddress': '127.0.0.1',
                'rpcPort': vc_rpc_port,
                'rpcPassword': fne_password
            })
        
        self.cc_config.set('system.config.voiceChNo', voice_channels)
        
        # Save control channel config
        cc_path = self.base_dir / f'{self.system_name}-cc.yml'
        self.cc_config.save(cc_path)
        print(f"  Saved: {cc_path}")
        
        # Create voice channels
        print(f"\nCreating {vc_count} voice channel configurations...")
        for i in range(vc_count):
            vc_index = i + 1
            vc_peer_id = base_peer_id + vc_index
            vc_rpc_port = base_rpc_port + vc_index
            vc_rest_port = base_rest_port + vc_index if rest_password else None
            vc_info = vc_channels[i]
            channel_id = vc_info['channel_id']
            channel_no = vc_info['channel_no']
            
            vc_template = get_template('voice-channel')
            
            vc_config = DVMConfig()
            vc_config.config = vc_template
            
            # Configure voice channel
            vc_config.set('system.identity', f'{system_identity}-VC{vc_index:02d}')
            vc_config.set('network.id', vc_peer_id)
            vc_config.set('network.address', fne_address)
            vc_config.set('network.port', fne_port)
            vc_config.set('network.password', fne_password)
            vc_config.set('network.rpcPort', vc_rpc_port)
            vc_config.set('network.rpcPassword', rpc_password)
            if rest_password is not None:
                vc_config.set('network.restEnable', True)
                vc_config.set('network.restPort', vc_rest_port)
                vc_config.set('network.restPassword', rest_password)
            else:
                vc_config.set('network.restEnable', False)
            
            # Network lookup and transfer settings
            vc_config.set('network.updateLookups', update_lookups)
            vc_config.set('network.saveLookups', save_lookups)
            vc_config.set('network.allowActivityTransfer', allow_activity_transfer)
            vc_config.set('network.allowDiagnosticTransfer', allow_diagnostic_transfer)
            vc_config.set('network.allowStatusTransfer', allow_status_transfer)
            
            # If updating lookups, set time values to 0
            if update_lookups:
                vc_config.set('system.radio_id.time', 0)
                vc_config.set('system.talkgroup_id.time', 0)
            else:
                # If not updating lookups, use provided values
                if radio_id_time > 0:
                    vc_config.set('system.radio_id.time', radio_id_time)
                if talkgroup_id_time > 0:
                    vc_config.set('system.talkgroup_id.time', talkgroup_id_time)
            
            # Radio ID and Talkgroup ID ACL settings
            if radio_id_file:
                vc_config.set('system.radio_id.file', radio_id_file)
            vc_config.set('system.radio_id.acl', radio_id_acl)
            
            if talkgroup_id_file:
                vc_config.set('system.talkgroup_id.file', talkgroup_id_file)
            vc_config.set('system.talkgroup_id.acl', talkgroup_id_acl)
            
            # Logging settings
            if log_path:
                vc_config.set('log.filePath', log_path)
            if activity_log_path:
                vc_config.set('log.activityFilePath', activity_log_path)
            if log_root:
                vc_config.set('log.fileRoot', log_root)
            if use_syslog:
                vc_config.set('log.useSysLog', use_syslog)
            if disable_non_auth_logging:
                vc_config.set('log.disableNonAuthoritiveLogging', disable_non_auth_logging)
            
            vc_config.set('system.config.channelId', channel_id)
            vc_config.set('system.config.channelNo', channel_no)
            vc_config.set('system.modem.protocol.type', modem_type)
            
            # DFSI Configuration for voice channel (if provided)
            if 'dfsi_rtrt' in vc_info and vc_info['dfsi_rtrt'] is not None:
                vc_config.set('system.modem.dfsiRtrt', vc_info['dfsi_rtrt'])
            if 'dfsi_jitter' in vc_info and vc_info['dfsi_jitter'] is not None:
                vc_config.set('system.modem.dfsiJitter', vc_info['dfsi_jitter'])
            if 'dfsi_call_timeout' in vc_info and vc_info['dfsi_call_timeout'] is not None:
                vc_config.set('system.modem.dfsiCallTimeout', vc_info['dfsi_call_timeout'])
            if 'dfsi_full_duplex' in vc_info and vc_info['dfsi_full_duplex'] is not None:
                vc_config.set('system.modem.dfsiFullDuplex', vc_info['dfsi_full_duplex'])
            
            # Set network/system IDs consistently (same as CC)
            if site_id is not None:
                vc_config.set('system.config.siteId', site_id)
            if nac is not None:
                vc_config.set('system.config.nac', nac)
            if color_code is not None:
                vc_config.set('system.config.colorCode', color_code)
            if dmr_net_id is not None:
                vc_config.set('system.config.dmrNetId', dmr_net_id)
            if net_id is not None:
                vc_config.set('system.config.netId', net_id)
            if sys_id is not None:
                vc_config.set('system.config.sysId', sys_id)
            if rfss_id is not None:
                vc_config.set('system.config.rfssId', rfss_id)
            if ran is not None:
                vc_config.set('system.config.ran', ran)
            
            # Ensure protocol consistency
            if protocol.lower() == 'p25':
                vc_config.set('protocols.p25.enable', True)
                vc_config.set('protocols.dmr.enable', False)
            else:
                vc_config.set('protocols.dmr.enable', True)
                vc_config.set('protocols.p25.enable', False)
            
            # Configure control channel reference for voice channel
            vc_config.set('system.config.controlCh.rpcAddress', '127.0.0.1')
            vc_config.set('system.config.controlCh.rpcPort', base_rpc_port)
            vc_config.set('system.config.controlCh.rpcPassword', fne_password)
            vc_config.set('system.config.controlCh.notifyEnable', True)
            
            self.vc_configs.append(vc_config)
            
            # Save voice channel config
            vc_path = self.base_dir / f'{self.system_name}-vc{vc_index:02d}.yml'
            vc_config.save(vc_path)
            print(f"  Saved: {vc_path}")
        
        print(f"\nTrunked system '{self.system_name}' created successfully!")
        print(f"  Control Channel: {cc_path}")
        print(f"  Voice Channels: {vc_count}")
        print(f"  Base Directory: {self.base_dir}")

    def load_system(self) -> None:
        """Load existing trunked system"""
        cc_path = self.base_dir / f'{self.system_name}-cc.yml'
        if not cc_path.exists():
            raise FileNotFoundError(f"Control channel config not found: {cc_path}")
        
        self.cc_config = DVMConfig(cc_path)
        
        # Load voice channels
        self.vc_configs = []
        i = 1
        while True:
            vc_path = self.base_dir / f'{self.system_name}-vc{i:02d}.yml'
            if not vc_path.exists():
                break
            
            vc_config = DVMConfig(vc_path)
            self.vc_configs.append(vc_config)
            i += 1
    
    def validate_system(self) -> Dict[str, List[str]]:
        """
        Validate entire trunked system
        
        Returns:
            Dictionary mapping config files to error lists
        """
        errors = {}
        
        # Validate control channel
        cc_errors = self.cc_config.validate()
        if cc_errors:
            errors['control_channel'] = cc_errors
        
        # Validate voice channels
        for i, vc in enumerate(self.vc_configs, 1):
            vc_errors = vc.validate()
            if vc_errors:
                errors[f'voice_channel_{i}'] = vc_errors
        
        # Check consistency
        consistency_errors = self._check_consistency()
        if consistency_errors:
            errors['consistency'] = consistency_errors
        
        return errors
    
    def _check_consistency(self) -> List[str]:
        """Check consistency across all configs"""
        errors = []
        
        if not self.cc_config or not self.vc_configs:
            return errors
        
        # Get reference values from CC
        cc_nac = self.cc_config.get('system.config.nac')
        cc_color_code = self.cc_config.get('system.config.colorCode')
        cc_site_id = self.cc_config.get('system.config.siteId')
        cc_net_id = self.cc_config.get('system.config.netId')
        cc_dmr_net_id = self.cc_config.get('system.config.dmrNetId')
        cc_sys_id = self.cc_config.get('system.config.sysId')
        cc_rfss_id = self.cc_config.get('system.config.rfssId')
        cc_ran = self.cc_config.get('system.config.ran')
        cc_fne = self.cc_config.get('network.address')
        cc_fne_port = self.cc_config.get('network.port')
        
        # Check each voice channel
        for i, vc in enumerate(self.vc_configs, 1):
            if cc_nac is not None and vc.get('system.config.nac') != cc_nac:
                errors.append(f"VC{i:02d}: NAC mismatch (expected {cc_nac})")
            
            if cc_color_code is not None and vc.get('system.config.colorCode') != cc_color_code:
                errors.append(f"VC{i:02d}: Color code mismatch (expected {cc_color_code})")
            
            if cc_site_id is not None and vc.get('system.config.siteId') != cc_site_id:
                errors.append(f"VC{i:02d}: Site ID mismatch (expected {cc_site_id})")
            
            if cc_net_id is not None and vc.get('system.config.netId') != cc_net_id:
                errors.append(f"VC{i:02d}: Network ID mismatch (expected {cc_net_id})")
            
            if cc_dmr_net_id is not None and vc.get('system.config.dmrNetId') != cc_dmr_net_id:
                errors.append(f"VC{i:02d}: DMR Network ID mismatch (expected {cc_dmr_net_id})")
            
            if cc_sys_id is not None and vc.get('system.config.sysId') != cc_sys_id:
                errors.append(f"VC{i:02d}: System ID mismatch (expected {cc_sys_id})")
            
            if cc_rfss_id is not None and vc.get('system.config.rfssId') != cc_rfss_id:
                errors.append(f"VC{i:02d}: RFSS ID mismatch (expected {cc_rfss_id})")
            
            if cc_ran is not None and vc.get('system.config.ran') != cc_ran:
                errors.append(f"VC{i:02d}: RAN mismatch (expected {cc_ran})")
            
            if vc.get('network.address') != cc_fne:
                errors.append(f"VC{i:02d}: FNE address mismatch (expected {cc_fne})")
            
            if vc.get('network.port') != cc_fne_port:
                errors.append(f"VC{i:02d}: FNE port mismatch (expected {cc_fne_port})")
        
        return errors
    
    def update_all(self, key_path: str, value: Any) -> None:
        """Update a setting across all configs"""
        self.cc_config.set(key_path, value)
        for vc in self.vc_configs:
            vc.set(key_path, value)
    
    def save_all(self) -> None:
        """Save all configurations"""
        cc_path = self.base_dir / f'{self.system_name}-cc.yml'
        self.cc_config.save(cc_path)
        
        for i, vc in enumerate(self.vc_configs, 1):
            vc_path = self.base_dir / f'{self.system_name}-vc{i:02d}.yml'
            vc.save(vc_path)
        
        self._create_summary()


if __name__ == '__main__':
    # Test
    system = TrunkingSystem(Path('./test_trunk'), 'skynet')
    system.create_system(
        protocol='p25',
        vc_count=2,
        system_identity='SKYNET',
        base_peer_id=100000
    )
