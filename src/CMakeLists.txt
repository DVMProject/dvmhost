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
#
## dvmhost source/header files
#
file(GLOB dvmhost_SRC
    # DMR module
    "src/dmr/*.h"
    "src/dmr/*.cpp"
    "src/dmr/acl/*.h"
    "src/dmr/acl/*.cpp"
    "src/dmr/data/*.h"
    "src/dmr/data/*.cpp"
    "src/dmr/edac/*.h"
    "src/dmr/edac/*.cpp"
    "src/dmr/lc/*.h"
    "src/dmr/lc/*.cpp"
    "src/dmr/lc/csbk/*.h"
    "src/dmr/lc/csbk/*.cpp"
    "src/dmr/lookups/*.h"
    "src/dmr/lookups/*.cpp"
    "src/dmr/packet*.h"
    "src/dmr/packet/*.cpp"

    # P25 module
    "src/p25/*.h"
    "src/p25/*.cpp"
    "src/p25/acl/*.h"
    "src/p25/acl/*.cpp"
    "src/p25/data/*.h"
    "src/p25/data/*.cpp"
    "src/p25/dfsi/*.h"
    "src/p25/dfsi/*.cpp"
    "src/p25/dfsi/packet/*.h"
    "src/p25/dfsi/packet/*.cpp"
    "src/p25/edac/*.h"
    "src/p25/edac/*.cpp"
    "src/p25/lc/*.h"
    "src/p25/lc/*.cpp"
    "src/p25/lc/tdulc/*.h"
    "src/p25/lc/tdulc/*.cpp"
    "src/p25/lc/tsbk/*.h"
    "src/p25/lc/tsbk/*.cpp"
    "src/p25/lookups/*.h"
    "src/p25/lookups/*.cpp"
    "src/p25/packet/*.h"
    "src/p25/packet/*.cpp"

    # NXDN module
    "src/nxdn/*.h"
    "src/nxdn/*.cpp"
    "src/nxdn/acl/*.h"
    "src/nxdn/acl/*.cpp"
    "src/nxdn/channel/*.h"
    "src/nxdn/channel/*.cpp"
    "src/nxdn/edac/*.h"
    "src/nxdn/edac/*.cpp"
    "src/nxdn/lc/*.h"
    "src/nxdn/lc/*.cpp"
    "src/nxdn/lc/rcch/*.h"
    "src/nxdn/lc/rcch/*.cpp"
    "src/nxdn/packet/*.h"
    "src/nxdn/packet/*.cpp"

    # Core
    "src/edac/*.h"
    "src/edac/*.cpp"
    "src/edac/rs/*.h"
    "src/host/*.h"
    "src/host/*.cpp"
    "src/host/calibrate/*.h"
    "src/host/calibrate/*.cpp"
    "src/host/setup/*.h"
    "src/host/setup/*.cpp"
    "src/host/fne/*.h"
    "src/host/fne/*.cpp"
    "src/lookups/*.h"
    "src/lookups/*.cpp"
    "src/modem/*.h"
    "src/modem/*.cpp"
    "src/modem/port/*.h"
    "src/modem/port/*.cpp"
    "src/network/*.h"
    "src/network/*.cpp"
    "src/network/fne/*.h"
    "src/network/fne/*.cpp"
    "src/network/json/*.h"
    "src/network/rest/*.h"
    "src/network/rest/*.cpp"
    "src/network/rest/http/*.h"
    "src/network/rest/http/*.cpp"
    "src/remote/RESTClient.cpp"
    "src/remote/RESTClient.h"
    "src/yaml/*.h"
    "src/yaml/*.cpp"
    "src/*.h"
    "src/*.cpp"
)

#
## dvmcmd source/header files
#
file(GLOB dvmcmd_SRC
    "src/network/UDPSocket.h"
    "src/network/UDPSocket.cpp"
    "src/network/RESTDefines.h"
    "src/network/json/*.h"
    "src/network/rest/*.h"
    "src/network/rest/*.cpp"
    "src/network/rest/http/*.h"
    "src/network/rest/http/*.cpp"
    "src/remote/*.h"
    "src/remote/*.cpp"
    "src/edac/SHA256.h"
    "src/edac/SHA256.cpp"
    "src/Defines.h"
    "src/Thread.h"
    "src/Thread.cpp"
    "src/Log.h"
    "src/Log.cpp"
    "src/Utils.h"
    "src/Utils.cpp"
)

# Digital mode options and other compilation features
option(ENABLE_DMR "Enable DMR Digtial Mode" on)
if (ENABLE_DMR)
    add_definitions(-DENABLE_DMR)
    message(CHECK_START "DMR Digital Mode - enabled")
else ()
    message(CHECK_START "DMR Digital Mode - disabled")
endif (ENABLE_DMR)

option(ENABLE_P25 "Enable P25 Digital Mode" on)
if (ENABLE_P25)
    add_definitions(-DENABLE_P25)
    message(CHECK_START "P25 Digital Mode - enabled")
else ()
    message(CHECK_START "P25 Digital Mode - disabled")
endif (ENABLE_P25)

option(ENABLE_NXDN "Enable NXDN Digital Mode" on)
if (ENABLE_NXDN)
    add_definitions(-DENABLE_NXDN)
    message(CHECK_START "NXDN Digital Mode - enabled")
else ()
    message(CHECK_START "NXDN Digital Mode - disabled")
endif (ENABLE_NXDN)

option(ENABLE_DFSI_SUPPORT "Enable P25 DFSI Transport Support" off)
if (ENABLE_DFSI_SUPPORT)
    add_definitions(-DENABLE_DFSI_SUPPORT)
    message(CHECK_START "P25 DFSI Support - enabled")
endif (ENABLE_DFSI_SUPPORT)

if (ENABLE_TUI_SUPPORT)
    option(ENABLE_SETUP_TUI "Enable interactive setup TUI" on)
    if (ENABLE_SETUP_TUI)
        add_definitions(-DENABLE_SETUP_TUI)
        message(CHECK_START "Interactive Setup TUI - enabled")
    endif (ENABLE_SETUP_TUI)
else()
    set(ENABLE_SETUP_TUI off)
endif (ENABLE_TUI_SUPPORT)

# Debug compilation features/options (these should not be enabled for production!)
option(DEBUG_DMR_PDU_DATA "" off)
option(DEBUG_CRC "" off)
option(DEBUG_RS "" off)
option(DEBUG_MODEM_CAL "" off)
option(DEBUG_MODEM "" off)
option(DEBUG_NXDN_FACCH1 "" off)
option(DEBUG_NXDN_SACCH "" off)
option(DEBUG_NXDN_UDCH "" off)
option(DEBUG_NXDN_LICH "" off)
option(DEBUG_NXDN_CAC "" off)
option(DEBUG_P25_PDU_DATA "" off)
option(DEBUG_P25_HDU "" off)
option(DEBUG_P25_LDU1 "" off)
option(DEBUG_P25_LDU2 "" off)
option(DEBUG_P25_TDULC "" off)
option(DEBUG_P25_TSBK "" off)
option(FORCE_TSBK_CRC_WARN "" off)
option(DEBUG_P25_DFSI "" off)
option(DEBUG_RINGBUFFER "" off)
option(DEBUG_HTTP_PAYLOAD "" off)
option(DEBUG_TRELLIS "" off)

if (DEBUG_DMR_PDU_DATA)
    add_definitions(-DDEBUG_DMR_PDU_DATA)
endif (DEBUG_DMR_PDU_DATA)
if (DEBUG_CRC_ADD)
    add_definitions(-DDEBUG_CRC_ADD)
endif (DEBUG_CRC_ADD)
if (DEBUG_CRC_CHECK)
    add_definitions(-DDEBUG_CRC_CHECK)
endif (DEBUG_CRC_CHECK)
if (DEBUG_RS)
    add_definitions(-DDEBUG_RS)
endif (DEBUG_RS)
if (DEBUG_MODEM_CAL)
    add_definitions(-DDEBUG_MODEM_CAL)
endif (DEBUG_MODEM_CAL)
if (DEBUG_MODEM)
    add_definitions(-DDEBUG_MODEM)
endif (DEBUG_MODEM)
if (DEBUG_NXDN_FACCH1)
    add_definitions(-DDEBUG_NXDN_FACCH1)
endif (DEBUG_NXDN_FACCH1)
if (DEBUG_NXDN_SACCH)
    add_definitions(-DDEBUG_NXDN_SACCH)
endif (DEBUG_NXDN_SACCH)
if (DEBUG_NXDN_UDCH)
    add_definitions(-DDEBUG_NXDN_UDCH)
endif (DEBUG_NXDN_UDCH)
if (DEBUG_NXDN_LICH)
    add_definitions(-DDEBUG_NXDN_LICH)
endif (DEBUG_NXDN_LICH)
if (DEBUG_NXDN_CAC)
    add_definitions(-DDEBUG_NXDN_CAC)
endif (DEBUG_NXDN_CAC)
if (DEBUG_P25_PDU_DATA)
    add_definitions(-DDEBUG_P25_PDU_DATA)
endif (DEBUG_P25_PDU_DATA)
if (DEBUG_P25_HDU)
    add_definitions(-DDEBUG_P25_HDU)
endif (DEBUG_P25_HDU)
if (DEBUG_P25_LDU1)
    add_definitions(-DDEBUG_P25_LDU1)
endif (DEBUG_P25_LDU1)
if (DEBUG_P25_LDU2)
    add_definitions(-DDEBUG_P25_LDU2)
endif (DEBUG_P25_LDU2)
if (DEBUG_P25_TDULC)
    add_definitions(-DDEBUG_P25_TDULC)
endif (DEBUG_P25_TDULC)
if (DEBUG_P25_TSBK)
    add_definitions(-DDEBUG_P25_TSBK)
endif (DEBUG_P25_TSBK)
if (FORCE_TSBK_CRC_WARN)
    add_definitions(-DFORCE_TSBK_CRC_WARN)
endif (FORCE_TSBK_CRC_WARN)
if (DEBUG_P25_DFSI)
    add_definitions(-DDEBUG_P25_DFSI)
endif (DEBUG_P25_DFSI)
if (DEBUG_RINGBUFFER)
    add_definitions(-DDEBUG_RINGBUFFER)
endif (DEBUG_RINGBUFFER)
if (DEBUG_HTTP_PAYLOAD)
    add_definitions(-DDEBUG_HTTP_PAYLOAD)
endif (DEBUG_HTTP_PAYLOAD)
if (DEBUG_TRELLIS)
    add_definitions(-DDEBUG_TRELLIS)
endif (DEBUG_TRELLIS)

#
## dvmhost project
#
project(dvmhost)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
find_package(Threads REQUIRED)

# add ASIO
add_library(asio::asio INTERFACE IMPORTED)
target_include_directories(asio::asio INTERFACE ${ASIO_INCLUDE_DIR})
target_compile_definitions(asio::asio INTERFACE "ASIO_STANDALONE")
target_link_libraries(asio::asio INTERFACE Threads::Threads)

if (ENABLE_SETUP_TUI)
    # add finalcut
    target_include_directories(finalcut INTERFACE ${FINALCUT_INCLUDE_DIR})
endif (ENABLE_SETUP_TUI)

# Check if platform-specific functions exist
include(CheckCXXSymbolExists)
check_cxx_symbol_exists(sendmsg sys/socket.h HAVE_SENDMSG)
check_cxx_symbol_exists(sendmmsg sys/socket.h HAVE_SENDMMSG)

if (HAVE_SENDMSG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_SENDMSG=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_SENDMSG=1")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DHAVE_SENDMSG=1")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DHAVE_SENDMSG=1")
endif (HAVE_SENDMSG)
if (HAVE_SENDMMSG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_SENDMMSG=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_SENDMMSG=1")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DHAVE_SENDMMSG=1")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DHAVE_SENDMMSG=1")
endif (HAVE_SENDMMSG)

add_executable(dvmhost ${dvmhost_SRC})
target_include_directories(dvmhost PRIVATE src)
if (ENABLE_SETUP_TUI)
    target_link_libraries(dvmhost PRIVATE asio::asio finalcut Threads::Threads util)
else()
    target_link_libraries(dvmhost PRIVATE asio::asio Threads::Threads util)
endif (ENABLE_SETUP_TUI)

set(CPACK_SET_DESTDIR true)
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "dvmhost")
set(CPACK_DEBIAN_PACKAGE_NAME "dvmhost")

set(CPACK_PACKAGE_VENDOR "DVMProject")

set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "The DVM Host software provides the host computer implementation of a mixed-mode DMR, P25 and/or NXDN or dedicated-mode DMR, P25 or NXDN repeater system that talks to the actual modem hardware. The host software; is the portion of a complete Over-The-Air modem implementation that performs the data processing, decision making and FEC correction for a digital repeater.")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "DVMProject Authors")
set(CPACK_DEBIAN_PACKAGE_VERSION "3.0.0")
set(CPACK_DEBIAN_PACKAGE_RELEASE "0")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/dvmproject")

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/debian/postrm")

set(CPACK_DEBIAN_FILE_NAME ${CPACK_DEBIAN_PACKAGE_NAME}_${CPACK_DEBIAN_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_RELEASE}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb)

include(CPack)

#
## dvmcmd project
#
project(dvmcmd)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
find_package(Threads REQUIRED)

# add ASIO
target_include_directories(asio::asio INTERFACE ${ASIO_INCLUDE_DIR})
target_compile_definitions(asio::asio INTERFACE "ASIO_STANDALONE")
target_link_libraries(asio::asio INTERFACE Threads::Threads)

add_executable(dvmcmd ${dvmcmd_SRC})
target_link_libraries(dvmcmd PRIVATE asio::asio Threads::Threads)
target_include_directories(dvmcmd PRIVATE src)
