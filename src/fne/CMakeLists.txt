# SPDX-License-Identifier: GPL-2.0-only
#/*
# * Digital Voice Modem - Converged FNE Software
# * GPLv2 Open Source. Use is subject to license terms.
# * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
# *
# *  Copyright (C) 2024,2025 Bryan Biedenkapp, N2PLL
# *
# */
file(GLOB dvmfne_SRC
    "src/fne/lookups/*.h"
    "src/fne/lookups/*.cpp"
    "src/fne/network/callhandler/*.h"
    "src/fne/network/callhandler/*.cpp"
    "src/fne/network/callhandler/packetdata/*.h"
    "src/fne/network/callhandler/packetdata/*.cpp"
    "src/fne/network/influxdb/*.h"
    "src/fne/network/influxdb/*.cpp"
    "src/fne/network/*.h"
    "src/fne/network/*.cpp"
    "src/fne/xml/*.h"
    "src/fne/win32/*.h"
    "src/fne/*.h"
    "src/fne/*.cpp"
)

#
# Windows Resource Scripts
#
file(GLOB dvmfne_RC
    "src/fne/win32/*.rc"
)
