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
file(GLOB dvmhost_SRC
    # DMR module
    "src/host/dmr/*.h"
    "src/host/dmr/*.cpp"
    "src/host/dmr/lc/csbk/*.h"
    "src/host/dmr/lc/csbk/*.cpp"
    "src/host/dmr/lookups/*.h"
    "src/host/dmr/lookups/*.cpp"
    "src/host/dmr/packet*.h"
    "src/host/dmr/packet/*.cpp"

    # P25 module
    "src/host/p25/*.h"
    "src/host/p25/*.cpp"
    "src/host/p25/lc/tsbk/*.h"
    "src/host/p25/lc/tsbk/*.cpp"
    "src/host/p25/lookups/*.h"
    "src/host/p25/lookups/*.cpp"
    "src/host/p25/packet/*.h"
    "src/host/p25/packet/*.cpp"

    # NXDN module
    "src/host/nxdn/*.h"
    "src/host/nxdn/*.cpp"
    "src/host/nxdn/packet/*.h"
    "src/host/nxdn/packet/*.cpp"

    # Core
    "src/host/*.h"
    "src/host/*.cpp"
    "src/host/calibrate/*.h"
    "src/host/calibrate/*.cpp"
    "src/host/setup/*.h"
    "src/host/setup/*.cpp"
    "src/host/modem/*.h"
    "src/host/modem/*.cpp"
    "src/host/modem/port/*.h"
    "src/host/modem/port/*.cpp"
    "src/host/network/*.h"
    "src/host/network/*.cpp"
    "src/remote/RESTClient.cpp"
    "src/remote/RESTClient.h"
)

project(dvmhost)
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
set(CPACK_DEBIAN_PACKAGE_VERSION "3.5.5")
set(CPACK_DEBIAN_PACKAGE_RELEASE "0")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/dvmproject")

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/debian/postrm")

set(CPACK_DEBIAN_FILE_NAME ${CPACK_DEBIAN_PACKAGE_NAME}_${CPACK_DEBIAN_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_RELEASE}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb)

include(CPack)
