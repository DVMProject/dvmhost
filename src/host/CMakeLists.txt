# SPDX-License-Identifier: GPL-2.0-only
#/*
# * Digital Voice Modem - Modem Host Software
# * GPLv2 Open Source. Use is subject to license terms.
# * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
# *
# *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
# *  Copyright (C) 2022 Natalie Moore
# *
# */
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
    "src/host/modem/port/specialized/*.h"
    "src/host/modem/port/specialized/*.cpp"
    "src/host/network/*.h"
    "src/host/network/*.cpp"
    "src/remote/RESTClient.cpp"
    "src/remote/RESTClient.h"
)
