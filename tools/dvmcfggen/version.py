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

Version and banner constants
"""

# Version information
VERSION_MAJOR = "05"
VERSION_MINOR = "04"
VERSION_REV = "A"
__VER__ = f"{VERSION_MAJOR}.{VERSION_MINOR}{VERSION_REV} (R{VERSION_MAJOR}{VERSION_REV}{VERSION_MINOR})"

# ASCII Banner
__BANNER__ = """
                                        .         .           
8 888888888o.   `8.`888b           ,8' ,8.       ,8.          
8 8888    `^888. `8.`888b         ,8' ,888.     ,888.         
8 8888        `88.`8.`888b       ,8' .`8888.   .`8888.        
8 8888         `88 `8.`888b     ,8' ,8.`8888. ,8.`8888.       
8 8888          88  `8.`888b   ,8' ,8'8.`8888,8^8.`8888.      
8 8888          88   `8.`888b ,8' ,8' `8.`8888' `8.`8888.     
8 8888         ,88    `8.`888b8' ,8'   `8.`88'   `8.`8888.    
8 8888        ,88'     `8.`888' ,8'     `8.`'     `8.`8888.   
8 8888    ,o88P'        `8.`8' ,8'       `8        `8.`8888.  
8 888888888P'            `8.` ,8'         `         `8.`8888. 
"""
