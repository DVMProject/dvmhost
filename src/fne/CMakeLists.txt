# SPDX-License-Identifier: GPL-2.0-only
#/*
# * Digital Voice Modem - Converged FNE Software
# * GPLv2 Open Source. Use is subject to license terms.
# * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
# *
# *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
# *
# */
file(GLOB dvmfne_SRC
    "src/fne/network/callhandler/*.h"
    "src/fne/network/callhandler/*.cpp"
    "src/fne/network/callhandler/packetdata/*.h"
    "src/fne/network/callhandler/packetdata/*.cpp"
    "src/fne/network/influxdb/*.h"
    "src/fne/network/*.h"
    "src/fne/network/*.cpp"
    "src/fne/xml/*.h"
    "src/fne/*.h"
    "src/fne/*.cpp"
)
