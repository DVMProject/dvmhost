# SPDX-License-Identifier: GPL-2.0-only
#/*
# * Digital Voice Modem - CMake Build System
# * GPLv2 Open Source. Use is subject to license terms.
# * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
# *
# *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
# *
# */
file(GLOB tsbkDecode_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)
