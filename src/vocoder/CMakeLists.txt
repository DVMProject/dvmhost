# SPDX-License-Identifier: GPL-2.0-only
#/*
# * Digital Voice Modem - Remote Command Client
# * GPLv2 Open Source. Use is subject to license terms.
# * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
# *
# *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
# *
# */
file(GLOB vocoder_SRC
    "src/vocoder/imbe/*.cpp"
    "src/vocoder/*.cpp"
    "src/vocoder/*.c"
)

file(GLOB vocoder_INCLUDE
    "src/vocoder/imbe/*.h"
    "src/vocoder/*.h"
)
