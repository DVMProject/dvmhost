# SPDX-License-Identifier: GPL-2.0-only
#/**
#* Digital Voice Modem - Host Monitor Software
#* GPLv2 Open Source. Use is subject to license terms.
#* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#*
#* @package DVM / Host Monitor Software
#* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
#*
#*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
#*
#*/
file(GLOB dvmmon_SRC
    "src/host/modem/Modem.h"

    "src/remote/RESTClient.cpp"
    "src/remote/RESTClient.h"
    
    "src/monitor/*.h"
    "src/monitor/*.cpp"
)
