# SPDX-License-Identifier: GPL-2.0-only
#/*
# * Digital Voice Modem - IPSC Gateway
# * GPLv2 Open Source. Use is subject to license terms.
# * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
# *
# *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
# *
# */
file(GLOB ipsc_SRC
    "src/ipsc/dmr/*.h"
    "src/ipsc/dmr/*.cpp"

    "src/ipsc/protocol/*.h"
    "src/ipsc/protocol/*.cpp"

    "src/ipsc/*.h"
    "src/ipsc/*.cpp"
)

#
# Windows Resource Scripts
#
file(GLOB ipsc_RC
    "src/ipsc/win32/*.rc"
)
