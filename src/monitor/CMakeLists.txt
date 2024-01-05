#/**
#* Digital Voice Modem - Host Software
#* GPLv2 Open Source. Use is subject to license terms.
#* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#*
#* @package DVM / Host Software
#*
#*/
#/*
#*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
#*   Copyright (C) 2022 by Natalie Moore <https://github.com/jelimoore>
#*
#*   This program is free software; you can redistribute it and/or modify
#*   it under the terms of the GNU General Public License as published by
#*   the Free Software Foundation; either version 2 of the License, or
#*   (at your option) any later version.
#*
#*   This program is distributed in the hope that it will be useful,
#*   but WITHOUT ANY WARRANTY; without even the implied warranty of
#*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#*   GNU General Public License for more details.
#*
#*   You should have received a copy of the GNU General Public License
#*   along with this program; if not, write to the Free Software
#*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#*/
file(GLOB dvmmon_SRC
    "src/host/modem/Modem.h"
    "src/remote/RESTClient.cpp"
    "src/remote/RESTClient.h"
    "src/monitor/*.h"
    "src/monitor/*.cpp"
)

if (ENABLE_TUI_SUPPORT AND (NOT DISABLE_MONITOR))
project(dvmmon)
add_executable(dvmmon ${common_INCLUDE} ${dvmmon_SRC})
target_link_libraries(dvmmon PRIVATE common asio::asio finalcut Threads::Threads)
target_include_directories(dvmmon PRIVATE src src/host src/monitor)
endif (ENABLE_TUI_SUPPORT AND (NOT DISABLE_MONITOR))
