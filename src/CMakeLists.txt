#/**
#* Digital Voice Modem - Host Software
#* GPLv2 Open Source. Use is subject to license terms.
#* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#*
#* @package DVM / Host Software
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
include(src/CompilerOptions.cmake)

#
## common
#
include(src/common/CMakeLists.txt)
add_library(common STATIC ${common_SRC} ${common_INCLUDE})
target_link_libraries(common PRIVATE asio::asio Threads::Threads util)
target_include_directories(common PRIVATE src src/common)

#
## dvmhost
#
include(src/host/CMakeLists.txt)
if (ENABLE_SETUP_TUI)
    # add finalcut
    target_include_directories(finalcut INTERFACE ${FINALCUT_INCLUDE_DIR})
endif (ENABLE_SETUP_TUI)

add_executable(dvmhost ${common_INCLUDE} ${dvmhost_SRC})
if (ENABLE_SETUP_TUI)
    target_link_libraries(dvmhost PRIVATE common asio::asio finalcut Threads::Threads util)
else()
    target_link_libraries(dvmhost PRIVATE common asio::asio Threads::Threads util)
endif (ENABLE_SETUP_TUI)
target_include_directories(dvmhost PRIVATE src src/host)

set(CPACK_SET_DESTDIR true)
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "dvmhost")
set(CPACK_DEBIAN_PACKAGE_NAME "dvmhost")

set(CPACK_PACKAGE_VENDOR "DVMProject")

set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "The DVM Host software provides the host computer implementation of a mixed-mode DMR, P25 and/or NXDN or dedicated-mode DMR, P25 or NXDN repeater system that talks to the actual modem hardware. The host software; is the portion of a complete Over-The-Air modem implementation that performs the data processing, decision making and FEC correction for a digital repeater.")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "DVMProject Authors")
set(CPACK_DEBIAN_PACKAGE_VERSION "3.6.0")
set(CPACK_DEBIAN_PACKAGE_RELEASE "0")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/dvmproject")

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/debian/postrm")

set(CPACK_DEBIAN_FILE_NAME ${CPACK_DEBIAN_PACKAGE_NAME}_${CPACK_DEBIAN_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_RELEASE}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb)

include(CPack)

#
## dvmfne
#
include(src/fne/CMakeLists.txt)
add_executable(dvmfne ${common_INCLUDE} ${dvmfne_SRC})
target_link_libraries(dvmfne PRIVATE common asio::asio Threads::Threads)
target_include_directories(dvmfne PRIVATE src src/host src/fne)

#
## dvmmon
#
if (ENABLE_TUI_SUPPORT AND (NOT DISABLE_MONITOR))
    include(src/monitor/CMakeLists.txt)
    add_executable(dvmmon ${common_INCLUDE} ${dvmmon_SRC})
    target_link_libraries(dvmmon PRIVATE common asio::asio finalcut Threads::Threads)
    target_include_directories(dvmmon PRIVATE src src/host src/monitor)
endif (ENABLE_TUI_SUPPORT AND (NOT DISABLE_MONITOR))

#
## dvmcmd
#
include(src/remote/CMakeLists.txt)
add_executable(dvmcmd ${common_INCLUDE} ${dvmcmd_SRC})
target_link_libraries(dvmcmd PRIVATE common asio::asio Threads::Threads)
target_include_directories(dvmcmd PRIVATE src src/remote)
