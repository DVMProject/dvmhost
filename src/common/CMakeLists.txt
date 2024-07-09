# SPDX-License-Identifier: GPL-2.0-only
#/**
#* Digital Voice Modem - Common Library
#* GPLv2 Open Source. Use is subject to license terms.
#* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#*
#* @package DVM / Common Library
#* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
#*
#*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
#*
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
    "src/common/p25/sndcp/*.cpp"
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
    "src/common/network/tcp/*.cpp"
    "src/common/network/udp/*.cpp"
    "src/common/network/viface/*.cpp"
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
    "src/common/p25/sndcp/*.h"
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
    "src/common/network/tcp/*.h"
    "src/common/network/udp/*.h"
    "src/common/network/viface/*.h"
    "src/common/yaml/*.h"
    "src/common/*.h"
)
