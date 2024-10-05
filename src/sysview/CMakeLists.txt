# SPDX-License-Identifier: GPL-2.0-only
#/*
# * Digital Voice Modem - FNE System View
# * GPLv2 Open Source. Use is subject to license terms.
# * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
# *
# *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
# *
# */
file(GLOB sysView_SRC
    "src/host/modem/Modem.h"
    "src/host/network/Network.h"
    "src/host/network/Network.cpp"

    "src/remote/RESTClient.cpp"
    "src/remote/RESTClient.h"
    
    "src/sysview/network/*.h"
    "src/sysview/network/*.cpp"
    "src/sysview/*.h"
    "src/sysview/*.cpp"
)
