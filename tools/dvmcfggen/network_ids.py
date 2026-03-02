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

Network and System ID validation for DVMHost protocols.

This module provides validation functions for DMR, P25, and NXDN network/system
identification parameters based on limits defined in DVMHost C++ code.
"""

from typing import Tuple, Dict, Any
from enum import Enum


class DMRSiteModel(Enum):
    """DMR site model types (from dmr::defines::SiteModel)"""
    TINY = 0    # netId: 0-0x1FF (511), siteId: 0-0x07 (7)
    SMALL = 1   # netId: 0-0x7F (127), siteId: 0-0x1F (31)
    LARGE = 2   # netId: 0-0x1F (31), siteId: 0-0x7F (127)
    HUGE = 3    # netId: 0-0x03 (3), siteId: 0-0x3FF (1023)


# DMR Limits (from dvmhost/src/common/dmr/DMRUtils.h)
DMR_COLOR_CODE_MIN = 0
DMR_COLOR_CODE_MAX = 15

DMR_SITE_MODEL_LIMITS = {
    DMRSiteModel.TINY: {
        'netId': (1, 0x1FF),    # 1-511
        'siteId': (1, 0x07),    # 1-7
    },
    DMRSiteModel.SMALL: {
        'netId': (1, 0x7F),     # 1-127
        'siteId': (1, 0x1F),    # 1-31
    },
    DMRSiteModel.LARGE: {
        'netId': (1, 0x1F),     # 1-31
        'siteId': (1, 0x7F),    # 1-127
    },
    DMRSiteModel.HUGE: {
        'netId': (1, 0x03),     # 1-3
        'siteId': (1, 0x3FF),   # 1-1023
    },
}

# P25 Limits (from dvmhost/src/common/p25/P25Utils.h)
P25_NAC_MIN = 0x000
P25_NAC_MAX = 0xF7F      # 3967

P25_NET_ID_MIN = 1
P25_NET_ID_MAX = 0xFFFFE  # 1048574

P25_SYS_ID_MIN = 1
P25_SYS_ID_MAX = 0xFFE    # 4094

P25_RFSS_ID_MIN = 1
P25_RFSS_ID_MAX = 0xFE    # 254

P25_SITE_ID_MIN = 1
P25_SITE_ID_MAX = 0xFE    # 254

# NXDN Limits (from dvmhost/src/common/nxdn/SiteData.h and NXDNDefines.h)
NXDN_RAN_MIN = 0
NXDN_RAN_MAX = 63         # 6-bit field

NXDN_LOC_ID_MIN = 1
NXDN_LOC_ID_MAX = 0xFFFFFF  # 16777215 (24-bit field)

NXDN_SITE_ID_MIN = 1
NXDN_SITE_ID_MAX = 0xFFFFFF  # Same as LOC_ID


def validate_dmr_color_code(color_code: int) -> Tuple[bool, str]:
    """
    Validate DMR color code.
    
    Args:
        color_code: Color code value (0-15)
    
    Returns:
        Tuple of (is_valid, error_message)
    """
    if not isinstance(color_code, int):
        return False, "Color code must be an integer"
    
    if color_code < DMR_COLOR_CODE_MIN or color_code > DMR_COLOR_CODE_MAX:
        return False, f"Color code must be between {DMR_COLOR_CODE_MIN} and {DMR_COLOR_CODE_MAX}"
    
    return True, ""


def validate_dmr_network_id(net_id: int, site_model: DMRSiteModel) -> Tuple[bool, str]:
    """
    Validate DMR network ID based on site model.
    
    Args:
        net_id: Network ID value
        site_model: DMR site model
    
    Returns:
        Tuple of (is_valid, error_message)
    """
    if not isinstance(net_id, int):
        return False, "Network ID must be an integer"
    
    limits = DMR_SITE_MODEL_LIMITS[site_model]
    min_val, max_val = limits['netId']
    
    if net_id < min_val or net_id > max_val:
        return False, f"Network ID for {site_model.name} site model must be between {min_val} and {max_val} (0x{max_val:X})"
    
    return True, ""


def validate_dmr_site_id(site_id: int, site_model: DMRSiteModel) -> Tuple[bool, str]:
    """
    Validate DMR site ID based on site model.
    
    Args:
        site_id: Site ID value
        site_model: DMR site model
    
    Returns:
        Tuple of (is_valid, error_message)
    """
    if not isinstance(site_id, int):
        return False, "Site ID must be an integer"
    
    limits = DMR_SITE_MODEL_LIMITS[site_model]
    min_val, max_val = limits['siteId']
    
    if site_id < min_val or site_id > max_val:
        return False, f"Site ID for {site_model.name} site model must be between {min_val} and {max_val} (0x{max_val:X})"
    
    return True, ""


def validate_p25_nac(nac: int) -> Tuple[bool, str]:
    """
    Validate P25 Network Access Code.
    
    Args:
        nac: NAC value (0x000-0xF7F / 0-3967)
    
    Returns:
        Tuple of (is_valid, error_message)
    """
    if not isinstance(nac, int):
        return False, "NAC must be an integer"
    
    if nac < P25_NAC_MIN or nac > P25_NAC_MAX:
        return False, f"NAC must be between {P25_NAC_MIN} (0x{P25_NAC_MIN:03X}) and {P25_NAC_MAX} (0x{P25_NAC_MAX:03X})"
    
    return True, ""


def validate_p25_network_id(net_id: int) -> Tuple[bool, str]:
    """
    Validate P25 Network ID (WACN).
    
    Args:
        net_id: Network ID value (1-0xFFFFE / 1-1048574)
    
    Returns:
        Tuple of (is_valid, error_message)
    """
    if not isinstance(net_id, int):
        return False, "Network ID must be an integer"
    
    if net_id < P25_NET_ID_MIN or net_id > P25_NET_ID_MAX:
        return False, f"Network ID must be between {P25_NET_ID_MIN} and {P25_NET_ID_MAX} (0x{P25_NET_ID_MAX:X})"
    
    return True, ""


def validate_p25_system_id(sys_id: int) -> Tuple[bool, str]:
    """
    Validate P25 System ID.
    
    Args:
        sys_id: System ID value (1-0xFFE / 1-4094)
    
    Returns:
        Tuple of (is_valid, error_message)
    """
    if not isinstance(sys_id, int):
        return False, "System ID must be an integer"
    
    if sys_id < P25_SYS_ID_MIN or sys_id > P25_SYS_ID_MAX:
        return False, f"System ID must be between {P25_SYS_ID_MIN} and {P25_SYS_ID_MAX} (0x{P25_SYS_ID_MAX:X})"
    
    return True, ""


def validate_p25_rfss_id(rfss_id: int) -> Tuple[bool, str]:
    """
    Validate P25 RFSS ID.
    
    Args:
        rfss_id: RFSS ID value (1-0xFE / 1-254)
    
    Returns:
        Tuple of (is_valid, error_message)
    """
    if not isinstance(rfss_id, int):
        return False, "RFSS ID must be an integer"
    
    if rfss_id < P25_RFSS_ID_MIN or rfss_id > P25_RFSS_ID_MAX:
        return False, f"RFSS ID must be between {P25_RFSS_ID_MIN} and {P25_RFSS_ID_MAX} (0x{P25_RFSS_ID_MAX:X})"
    
    return True, ""


def validate_p25_site_id(site_id: int) -> Tuple[bool, str]:
    """
    Validate P25 Site ID.
    
    Args:
        site_id: Site ID value (1-0xFE / 1-254)
    
    Returns:
        Tuple of (is_valid, error_message)
    """
    if not isinstance(site_id, int):
        return False, "Site ID must be an integer"
    
    if site_id < P25_SITE_ID_MIN or site_id > P25_SITE_ID_MAX:
        return False, f"Site ID must be between {P25_SITE_ID_MIN} and {P25_SITE_ID_MAX} (0x{P25_SITE_ID_MAX:X})"
    
    return True, ""


def validate_nxdn_ran(ran: int) -> Tuple[bool, str]:
    """
    Validate NXDN Random Access Number.
    
    Args:
        ran: RAN value (0-63)
    
    Returns:
        Tuple of (is_valid, error_message)
    """
    if not isinstance(ran, int):
        return False, "RAN must be an integer"
    
    if ran < NXDN_RAN_MIN or ran > NXDN_RAN_MAX:
        return False, f"RAN must be between {NXDN_RAN_MIN} and {NXDN_RAN_MAX}"
    
    return True, ""


def validate_nxdn_location_id(loc_id: int) -> Tuple[bool, str]:
    """
    Validate NXDN Location ID (used as System ID).
    
    Args:
        loc_id: Location ID value (1-0xFFFFFF / 1-16777215)
    
    Returns:
        Tuple of (is_valid, error_message)
    """
    if not isinstance(loc_id, int):
        return False, "Location ID must be an integer"
    
    if loc_id < NXDN_LOC_ID_MIN or loc_id > NXDN_LOC_ID_MAX:
        return False, f"Location ID must be between {NXDN_LOC_ID_MIN} and {NXDN_LOC_ID_MAX} (0x{NXDN_LOC_ID_MAX:X})"
    
    return True, ""


def validate_nxdn_site_id(site_id: int) -> Tuple[bool, str]:
    """
    Validate NXDN Site ID.
    
    Args:
        site_id: Site ID value (1-0xFFFFFF / 1-16777215)
    
    Returns:
        Tuple of (is_valid, error_message)
    """
    return validate_nxdn_location_id(site_id)  # Same limits


def get_dmr_site_model_from_string(model_str: str) -> DMRSiteModel:
    """
    Convert site model string to enum.
    
    Args:
        model_str: Site model string ('tiny', 'small', 'large', 'huge')
    
    Returns:
        DMRSiteModel enum value
    """
    model_map = {
        'tiny': DMRSiteModel.TINY,
        'small': DMRSiteModel.SMALL,
        'large': DMRSiteModel.LARGE,
        'huge': DMRSiteModel.HUGE,
    }
    return model_map.get(model_str.lower(), DMRSiteModel.SMALL)


def get_dmr_site_model_name(model: DMRSiteModel) -> str:
    """Get friendly name for DMR site model."""
    return model.name.capitalize()


def get_network_id_info(protocol: str, site_model: str = 'small') -> Dict[str, Any]:
    """
    Get network ID parameter information for a protocol.
    
    Args:
        protocol: Protocol name ('dmr', 'p25', 'nxdn')
        site_model: DMR site model for DMR protocol
    
    Returns:
        Dictionary with parameter info
    """
    protocol = protocol.lower()
    
    if protocol == 'dmr':
        model = get_dmr_site_model_from_string(site_model)
        limits = DMR_SITE_MODEL_LIMITS[model]
        return {
            'colorCode': {
                'min': DMR_COLOR_CODE_MIN,
                'max': DMR_COLOR_CODE_MAX,
                'default': 1,
                'description': 'DMR Color Code',
            },
            'dmrNetId': {
                'min': limits['netId'][0],
                'max': limits['netId'][1],
                'default': 1,
                'description': f'DMR Network ID ({model.name} site model)',
            },
            'siteId': {
                'min': limits['siteId'][0],
                'max': limits['siteId'][1],
                'default': 1,
                'description': f'DMR Site ID ({model.name} site model)',
            },
        }
    
    elif protocol == 'p25':
        return {
            'nac': {
                'min': P25_NAC_MIN,
                'max': P25_NAC_MAX,
                'default': 0x293,  # 659 decimal (common default)
                'description': 'P25 Network Access Code (NAC)',
            },
            'netId': {
                'min': P25_NET_ID_MIN,
                'max': P25_NET_ID_MAX,
                'default': 0xBB800,  # 768000 decimal
                'description': 'P25 Network ID (WACN)',
            },
            'sysId': {
                'min': P25_SYS_ID_MIN,
                'max': P25_SYS_ID_MAX,
                'default': 0x001,
                'description': 'P25 System ID',
            },
            'rfssId': {
                'min': P25_RFSS_ID_MIN,
                'max': P25_RFSS_ID_MAX,
                'default': 1,
                'description': 'P25 RFSS (RF Sub-System) ID',
            },
            'siteId': {
                'min': P25_SITE_ID_MIN,
                'max': P25_SITE_ID_MAX,
                'default': 1,
                'description': 'P25 Site ID',
            },
        }
    
    elif protocol == 'nxdn':
        return {
            'ran': {
                'min': NXDN_RAN_MIN,
                'max': NXDN_RAN_MAX,
                'default': 1,
                'description': 'NXDN Random Access Number (RAN)',
            },
            'sysId': {
                'min': NXDN_LOC_ID_MIN,
                'max': NXDN_LOC_ID_MAX,
                'default': 0x001,
                'description': 'NXDN System ID (Location ID)',
            },
            'siteId': {
                'min': NXDN_SITE_ID_MIN,
                'max': NXDN_SITE_ID_MAX,
                'default': 1,
                'description': 'NXDN Site ID',
            },
        }
    
    return {}


def format_hex_or_decimal(value: int, max_value: int) -> str:
    """
    Format value as hex if it makes sense, otherwise decimal.
    
    Args:
        value: Value to format
        max_value: Maximum value for the field
    
    Returns:
        Formatted string
    """
    if max_value > 999:
        return f"{value} (0x{value:X})"
    return str(value)
