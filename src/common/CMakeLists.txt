#/**
#* Digital Voice Modem - Common Library
#* GPLv2 Open Source. Use is subject to license terms.
#* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#*
#* @package DVM / Common Library
#*
#*/
#/*
#*   Copyright (C) 2024 by Bryan Biedenkapp N2PLL
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
file(GLOB common_SRC
    # DMR module
    "src/common/dmr/*.cpp"
    "src/common/dmr/acl/*.cpp"
    "src/common/dmr/data/*.cpp"
    "src/common/dmr/lc/*.cpp"
    "src/common/dmr/lc/csbk/*.cpp"
    "src/common/dmr/lookups/*.cpp"

    # P25 module
    "src/common/p25/*.cpp"
    "src/common/p25/acl/*.cpp"
    "src/common/p25/data/*.cpp"
    "src/common/p25/dfsi/*.cpp"
    "src/common/p25/lc/*.cpp"
    "src/common/p25/lc/tdulc/*.cpp"
    "src/common/p25/lc/tsbk/*.cpp"
    "src/common/p25/lc/tsbk/mbt/*.cpp"
    "src/common/p25/lookups/*.cpp"

    # NXDN module
    "src/common/nxdn/*.cpp"
    "src/common/nxdn/acl/*.cpp"
    "src/common/nxdn/channel/*.cpp"
    "src/common/nxdn/edac/*.cpp"
    "src/common/nxdn/lc/*.cpp"
    "src/common/nxdn/lc/rcch/*.cpp"

    # Core
    "src/common/edac/*.cpp"
    "src/common/lookups/*.cpp"
    "src/common/network/*.cpp"
    "src/common/network/rest/*.cpp"
    "src/common/network/rest/http/*.cpp"
    "src/common/yaml/*.cpp"
    "src/common/*.cpp"
)

file(GLOB common_INCLUDE
    # DMR module
    "src/common/dmr/*.h"
    "src/common/dmr/acl/*.h"
    "src/common/dmr/data/*.h"
    "src/common/dmr/lc/*.h"
    "src/common/dmr/lc/csbk/*.h"
    "src/common/dmr/lookups/*.h"

    # P25 module
    "src/common/p25/*.h"
    "src/common/p25/acl/*.h"
    "src/common/p25/data/*.h"
    "src/common/p25/dfsi/*.h"
    "src/common/p25/lc/*.h"
    "src/common/p25/lc/tdulc/*.h"
    "src/common/p25/lc/tsbk/*.h"
    "src/common/p25/lc/tsbk/mbt/*.h"
    "src/common/p25/lookups/*.h"

    # NXDN module
    "src/common/nxdn/*.h"
    "src/common/nxdn/acl/*.h"
    "src/common/nxdn/channel/*.h"
    "src/common/nxdn/edac/*.h"
    "src/common/nxdn/lc/*.h"
    "src/common/nxdn/lc/rcch/*.h"

    # Core
    "src/common/edac/*.h"
    "src/common/edac/rs/*.h"
    "src/common/lookups/*.h"
    "src/common/network/*.h"
    "src/common/network/json/*.h"
    "src/common/network/rest/*.h"
    "src/common/network/rest/http/*.h"
    "src/common/yaml/*.h"
    "src/common/*.h"
)
