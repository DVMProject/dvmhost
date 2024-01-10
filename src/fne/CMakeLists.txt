#/**
#* Digital Voice Modem - Converged FNE Software
#* GPLv2 Open Source. Use is subject to license terms.
#* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#*
#* @package DVM / Converged FNE Software
#*
#*/
#/*
#*   Copyright (C) 2024 by Bryan Biedenkapp N2PLL
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
file(GLOB dvmfne_SRC
    "src/host/network/Network.h"
    "src/host/network/Network.cpp"
    "src/fne/network/fne/*.h"
    "src/fne/network/fne/*.cpp"
    "src/fne/network/*.h"
    "src/fne/network/*.cpp"
    "src/fne/*.h"
    "src/fne/*.cpp"
)

project(dvmfne)
add_executable(dvmfne ${common_INCLUDE} ${dvmfne_SRC})
target_link_libraries(dvmfne PRIVATE common asio::asio Threads::Threads)
target_include_directories(dvmfne PRIVATE src src/host src/fne)
