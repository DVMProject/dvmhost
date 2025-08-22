# SPDX-License-Identifier: GPL-2.0-only
#/*
# * Digital Voice Modem - Remote Command Client
# * GPLv2 Open Source. Use is subject to license terms.
# * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
# *
# *  Copyright (C) 2024,2025 Bryan Biedenkapp, N2PLL
# *
# */
file(GLOB dvmcmd_SRC
    "src/remote/*.h"
    "src/remote/*.cpp"
)

#
# Windows Resource Scripts
#
file(GLOB dvmcmd_RC
    "src/remote/win32/*.rc"
)
