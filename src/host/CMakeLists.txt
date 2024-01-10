#/**
#* Digital Voice Modem - Host Software
#* GPLv2 Open Source. Use is subject to license terms.
#* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#*
#* @package DVM / Host Software
#*
#*/
#/*
#*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
#*   Copyright (C) 2022 by Natalie Moore <https://github.com/jelimoore>
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
file(GLOB dvmhost_SRC
    # DMR module
    "src/host/dmr/*.h"
    "src/host/dmr/*.cpp"
    "src/host/dmr/lc/csbk/*.h"
    "src/host/dmr/lc/csbk/*.cpp"
    "src/host/dmr/lookups/*.h"
    "src/host/dmr/lookups/*.cpp"
    "src/host/dmr/packet*.h"
    "src/host/dmr/packet/*.cpp"

    # P25 module
    "src/host/p25/*.h"
    "src/host/p25/*.cpp"
    "src/host/p25/lc/tsbk/*.h"
    "src/host/p25/lc/tsbk/*.cpp"
    "src/host/p25/lookups/*.h"
    "src/host/p25/lookups/*.cpp"
    "src/host/p25/packet/*.h"
    "src/host/p25/packet/*.cpp"

    # NXDN module
    "src/host/nxdn/*.h"
    "src/host/nxdn/*.cpp"
    "src/host/nxdn/packet/*.h"
    "src/host/nxdn/packet/*.cpp"

    # Core
    "src/host/*.h"
    "src/host/*.cpp"
    "src/host/calibrate/*.h"
    "src/host/calibrate/*.cpp"
    "src/host/setup/*.h"
    "src/host/setup/*.cpp"
    "src/host/modem/*.h"
    "src/host/modem/*.cpp"
    "src/host/modem/port/*.h"
    "src/host/modem/port/*.cpp"
    "src/host/network/*.h"
    "src/host/network/*.cpp"
    "src/remote/RESTClient.cpp"
    "src/remote/RESTClient.h"
)
