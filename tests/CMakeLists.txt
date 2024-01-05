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
file(GLOB dvmtests_SRC
    "tests/*.h"
    "tests/*.cpp"
    "tests/crypto/*.cpp"
    "tests/edac/*.cpp"
    "tests/p25/*.cpp"
    "tests/nxdn/*.cpp"
)

project(dvmtest)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.0.1 # or a later release
)

FetchContent_MakeAvailable(Catch2)
find_package(Threads REQUIRED)

# add ASIO
target_include_directories(asio::asio INTERFACE ${ASIO_INCLUDE_DIR})
target_compile_definitions(asio::asio INTERFACE "ASIO_STANDALONE")
target_link_libraries(asio::asio INTERFACE Threads::Threads)

add_executable(dvmtests ${common_INCLUDE} ${dvmhost_SRC} ${dvmtests_SRC})
target_compile_definitions(dvmtests PUBLIC -DCATCH2_TEST_COMPILATION)
target_link_libraries(dvmtests PRIVATE Catch2::Catch2WithMain common asio::asio Threads::Threads util)
target_include_directories(dvmtests PRIVATE src src/host tests)
