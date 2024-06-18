# SPDX-License-Identifier: GPL-2.0-only
#/**
#* Digital Voice Modem - Modem Host Software
#* GPLv2 Open Source. Use is subject to license terms.
#* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#*
#* @package DVM / DFSI peer application
#* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
#* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
#*
#*   Copyright (C) 2024 Patrick McDonnell, W3AXL
#*
#*/
file(GLOB dvmdfsi_SRC
    # Modem libs for serial communication
    "src/host/modem/*.h"
    "src/host/modem/*.cpp"
    "src/host/modem/port/*.h"
    "src/host/modem/port/*.cpp"
    # Core network libs
    "src/host/network/Network.h"
    "src/host/network/Network.cpp"
    # DFSI network libs
    "src/dfsi/network/*.h"
    "src/dfsi/network/*.cpp"
    # DFSI rtp libs
    "src/dfsi/rtp/*.h"
    "src/dfsi/rtp/*.cpp"
    # Core DFSI
    "src/dfsi/*.h"
    "src/dfsi/*.cpp"
)