# SPDX-License-Identifier: GPL-2.0-only
#/*
# * Digital Voice Modem - Test Suite
# * GPLv2 Open Source. Use is subject to license terms.
# * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
# *
# *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
# *  Copyright (C) 2022 Natalie Moore
# *
# */
file(GLOB dvmtests_SRC
    "tests/*.h"
    "tests/*.cpp"
    "tests/crypto/*.cpp"
    "tests/edac/*.cpp"
    "tests/p25/*.cpp"
    "tests/nxdn/*.cpp"
)
