# SPDX-License-Identifier: GPL-2.0-only
#/**
#* Digital Voice Modem - Converged FNE Software
#* GPLv2 Open Source. Use is subject to license terms.
#* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#*
#* @package DVM / Converged FNE Software
#* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
#*
#*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
#*
#*/
file(GLOB dvmfne_SRC
    "src/host/network/Network.h"
    "src/host/network/Network.cpp"
    
    "src/fne/network/fne/*.h"
    "src/fne/network/fne/*.cpp"
    "src/fne/network/influxdb/*.h"
    "src/fne/network/*.h"
    "src/fne/network/*.cpp"
    "src/fne/*.h"
    "src/fne/*.cpp"
)
