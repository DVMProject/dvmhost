# SPDX-License-Identifier: GPL-2.0-only
#/*
# * Digital Voice Modem - Bridge
# * GPLv2 Open Source. Use is subject to license terms.
# * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
# *
# *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
# *
# */
file(GLOB bridge_SRC
    "src/bridge/audio/*.h"
    "src/bridge/audio/*.c"

    "src/bridge/mdc/*.h"
    "src/bridge/mdc/*.c"

    "src/bridge/network/*.h"
    "src/bridge/network/*.cpp"
    "src/bridge/*.h"
    "src/bridge/*.cpp"
)
