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
    "src/host/network/Network.h"
    "src/host/network/Network.cpp"
    "src/dfsi/network/*.h"
    "src/dfsi/network/*.cpp"
    "src/dfsi/*.h"
    "src/dfsi/*.cpp"
)