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

Identity Table (IDEN) Management
Handles frequency mapping and channel ID/number calculation
Based on iden_channel_calc.py from skynet_tools_host
"""

from typing import Dict, List, Tuple, Optional
from pathlib import Path


# Constants
MAX_FREQ_GAP = 30000000  # 30 MHz maximum offset from base frequency
HZ_MHZ = 1000000.0
KHZ_HZ = 1000.0


class IdenEntry:
    """Represents a single identity table entry"""
    
    def __init__(self, channel_id: int, base_freq: int, spacing: float, 
                 input_offset: float, bandwidth: float):
        """
        Initialize IDEN entry
        
        Args:
            channel_id: Channel identity (0-15)
            base_freq: Base frequency in Hz
            spacing: Channel spacing in kHz
            input_offset: Input offset (RX-TX) in MHz
            bandwidth: Channel bandwidth in kHz
        """
        self.channel_id = channel_id
        self.base_freq = base_freq
        self.spacing = spacing
        self.input_offset = input_offset
        self.bandwidth = bandwidth
    
    def to_line(self) -> str:
        """Convert to iden_table.dat format line"""
        return f"{self.channel_id},{self.base_freq},{self.spacing:.2f},{self.input_offset:.5f},{self.bandwidth:.1f},"
    
    def calculate_channel_number(self, tx_freq: int) -> int:
        """
        Calculate channel number for a given transmit frequency
        
        Args:
            tx_freq: Transmit frequency in Hz
            
        Returns:
            Channel number in hex
        """
        if tx_freq < self.base_freq:
            raise ValueError(f"TX frequency ({tx_freq/HZ_MHZ:.5f} MHz) is below base frequency "
                           f"({self.base_freq/HZ_MHZ:.5f} MHz)")
        
        if tx_freq > (self.base_freq + MAX_FREQ_GAP):
            raise ValueError(f"TX frequency ({tx_freq/HZ_MHZ:.5f} MHz) is too far above base frequency "
                           f"({self.base_freq/HZ_MHZ:.5f} MHz). Maximum gap is 25.5 MHz")
        
        space_hz = int(self.spacing * KHZ_HZ)
        root_freq = tx_freq - self.base_freq
        ch_no = int(root_freq / space_hz)
        
        return ch_no
    
    def calculate_rx_frequency(self, tx_freq: int) -> int:
        """
        Calculate receive frequency for a given transmit frequency
        
        Args:
            tx_freq: Transmit frequency in Hz
            
        Returns:
            Receive frequency in Hz
        """
        offset_hz = int(self.input_offset * HZ_MHZ)
        rx_freq = tx_freq + offset_hz
        
        # Note: For negative offsets (like -45 MHz on 800MHz band),
        # RX will be lower than TX and may be below base freq - this is normal
        # Only validate that the frequency is reasonable (above 0)
        if rx_freq < 0:
            raise ValueError(f"RX frequency ({rx_freq/HZ_MHZ:.5f} MHz) is invalid (negative)")
        
        return rx_freq
    
    def __repr__(self) -> str:
        return (f"IdenEntry(id={self.channel_id}, base={self.base_freq/HZ_MHZ:.3f}MHz, "
                f"spacing={self.spacing}kHz, offset={self.input_offset}MHz, bw={self.bandwidth}kHz)")


class IdenTable:
    """Manages identity table (iden_table.dat)"""
    
    def __init__(self):
        self.entries: Dict[int, IdenEntry] = {}
    
    def add_entry(self, entry: IdenEntry):
        """Add an identity table entry"""
        if entry.channel_id < 0 or entry.channel_id > 15:
            raise ValueError(f"Channel ID must be 0-15, got {entry.channel_id}")
        self.entries[entry.channel_id] = entry
    
    def get_entry(self, channel_id: int) -> Optional[IdenEntry]:
        """Get entry by channel ID"""
        return self.entries.get(channel_id)
    
    def find_entry_for_frequency(self, tx_freq: int) -> Optional[IdenEntry]:
        """
        Find an identity entry that can accommodate the given TX frequency
        
        Args:
            tx_freq: Transmit frequency in Hz
            
        Returns:
            IdenEntry if found, None otherwise
        """
        for entry in self.entries.values():
            try:
                # Check if this entry can accommodate the frequency
                if entry.base_freq <= tx_freq <= (entry.base_freq + MAX_FREQ_GAP):
                    entry.calculate_channel_number(tx_freq)  # Validate
                    return entry
            except ValueError:
                continue
        return None
    
    def save(self, filepath: Path):
        """Save identity table to file"""
        with open(filepath, 'w') as f:
            f.write("#\n")
            f.write("# Identity Table - Frequency Bandplan\n")
            f.write("# Generated by DVMCfg\n")
            f.write("#\n")
            f.write("# ChId,Base Freq (Hz),Spacing (kHz),Input Offset (MHz),Bandwidth (kHz),\n")
            f.write("#\n")
            
            for ch_id in sorted(self.entries.keys()):
                f.write(self.entries[ch_id].to_line() + "\n")
    
    def load(self, filepath: Path):
        """Load identity table from file"""
        self.entries.clear()
        
        with open(filepath, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                
                parts = [p.strip() for p in line.split(',') if p.strip()]
                if len(parts) >= 5:
                    try:
                        entry = IdenEntry(
                            channel_id=int(parts[0]),
                            base_freq=int(parts[1]),
                            spacing=float(parts[2]),
                            input_offset=float(parts[3]),
                            bandwidth=float(parts[4])
                        )
                        self.add_entry(entry)
                    except (ValueError, IndexError) as e:
                        print(f"Warning: Skipping invalid line: {line} ({e})")
    
    def __len__(self) -> int:
        return len(self.entries)
    
    def __repr__(self) -> str:
        return f"IdenTable({len(self)} entries)"


# Common band presets
BAND_PRESETS = {
    '800mhz': {
        'name': '800 MHz Trunked (800T)',
        'base_freq': 851006250,
        'spacing': 6.25,
        'input_offset': -45.0,
        'bandwidth': 12.5,
        'tx_range': (851.0, 870.0),
        'rx_range': (806.0, 825.0),
    },
    '900mhz': {
        'name': '900 MHz ISM/Business',
        'base_freq': 935001250,
        'spacing': 6.25,
        'input_offset': -39.0,
        'bandwidth': 12.5,
        'tx_range': (935.0, 960.0),
        'rx_range': (896.0, 901.0),
    },
    'uhf': {
        'name': 'UHF 450-470 MHz',
        'base_freq': 450000000,
        'spacing': 6.25,
        'input_offset': 5.0,
        'bandwidth': 12.5,
        'tx_range': (450.0, 470.0),
        'rx_range': (455.0, 475.0),
    },
    'vhf': {
        'name': 'VHF 146-148 MHz (2m Ham)',
        'base_freq': 146000000,
        'spacing': 6.25,
        'input_offset': 0.6,
        'bandwidth': 12.5,
        'tx_range': (146.0, 148.0),
        'rx_range': (146.6, 148.6),
    },
    'vhf-hi': {
        'name': 'VHF-Hi 150-174 MHz',
        'base_freq': 150000000,
        'spacing': 6.25,
        'input_offset': 0.6,
        'bandwidth': 12.5,
        'tx_range': (150.0, 174.0),
        'rx_range': (155.0, 179.0),
    },
    'uhf-ham': {
        'name': 'UHF 430-450 MHz (70cm Ham)',
        'base_freq': 430000000,
        'spacing': 6.25,
        'input_offset': 5.0,
        'bandwidth': 12.5,
        'tx_range': (430.0, 450.0),
        'rx_range': (435.0, 455.0),
    },
}


def create_iden_entry_from_preset(channel_id: int, preset: str) -> IdenEntry:
    """
    Create an IdenEntry from a band preset
    
    Args:
        channel_id: Channel ID (0-15)
        preset: Preset name (e.g., '800mhz', 'uhf', 'vhf')
        
    Returns:
        IdenEntry configured for the band
    """
    if preset not in BAND_PRESETS:
        raise ValueError(f"Unknown preset: {preset}. Available: {list(BAND_PRESETS.keys())}")
    
    band = BAND_PRESETS[preset]
    return IdenEntry(
        channel_id=channel_id,
        base_freq=band['base_freq'],
        spacing=band['spacing'],
        input_offset=band['input_offset'],
        bandwidth=band['bandwidth']
    )


def calculate_channel_assignment(tx_freq_mhz: float, preset: str = None, 
                                 channel_id: int = None) -> Tuple[int, int, int, int]:
    """
    Calculate channel ID and channel number for a given TX frequency
    
    Args:
        tx_freq_mhz: Transmit frequency in MHz
        preset: Band preset to use (optional)
        channel_id: Specific channel ID to use (optional, 0-15)
        
    Returns:
        Tuple of (channel_id, channel_number, tx_freq_hz, rx_freq_hz)
    """
    tx_freq_hz = int(tx_freq_mhz * HZ_MHZ)
    
    # If preset specified, use it
    if preset:
        entry = create_iden_entry_from_preset(channel_id or 0, preset)
        ch_no = entry.calculate_channel_number(tx_freq_hz)
        rx_freq = entry.calculate_rx_frequency(tx_freq_hz)
        return (entry.channel_id, ch_no, tx_freq_hz, rx_freq)
    
    # Otherwise, try to find a matching preset
    for preset_name, band in BAND_PRESETS.items():
        if band['tx_range'][0] <= tx_freq_mhz <= band['tx_range'][1]:
            entry = create_iden_entry_from_preset(channel_id or 0, preset_name)
            try:
                ch_no = entry.calculate_channel_number(tx_freq_hz)
                rx_freq = entry.calculate_rx_frequency(tx_freq_hz)
                return (entry.channel_id, ch_no, tx_freq_hz, rx_freq)
            except ValueError:
                continue
    
    raise ValueError(f"No suitable band preset found for {tx_freq_mhz} MHz. "
                    f"Please specify a preset or configure manually.")


def create_default_iden_table() -> IdenTable:
    """Create a default identity table with common bands"""
    table = IdenTable()
    
    # Add common band presets with different channel IDs
    table.add_entry(create_iden_entry_from_preset(0, '800mhz'))
    table.add_entry(create_iden_entry_from_preset(2, 'uhf'))
    table.add_entry(create_iden_entry_from_preset(3, 'vhf'))
    table.add_entry(create_iden_entry_from_preset(4, 'vhf-hi'))
    table.add_entry(create_iden_entry_from_preset(5, 'uhf-ham'))
    table.add_entry(create_iden_entry_from_preset(15, '900mhz'))
    
    return table
