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
dvmcfggen Package
Main entry point for the DVMHost Configuration Generator
"""

__version__ = "1.0.0"
__author__ = "Bryan Biedenkapp N2PLL"
__license__ = "GPL-2.0-only"

from config_manager import DVMConfig, ConfigValidator
from templates import get_template, TEMPLATES
from trunking_manager import TrunkingSystem

__all__ = [
    'DVMConfig',
    'ConfigValidator',
    'get_template',
    'TEMPLATES',
    'TrunkingSystem',
]
