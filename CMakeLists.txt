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
cmake_minimum_required(VERSION 3.16.0)

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
    "src/lookups/*.h"
    "src/lookups/*.cpp"
    "src/modem/*.h"
    "src/modem/*.cpp"
    "src/modem/port/*.h"
    "src/modem/port/*.cpp"
    "src/network/*.h"
    "src/network/*.cpp"
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

#
## dvmtest source/header files
#
file(GLOB dvmtests_SRC
    "tests/nulltest.cpp"
    "tests/edac/*.cpp"
    "tests/p25/*.cpp"
)

# Digital mode options and other compilation features
option(ENABLE_DMR "Enable DMR Digtial Mode" on)
option(ENABLE_P25 "Enable P25 Digital Mode" on)
option(ENABLE_NXDN "Enable NXDN Digital Mode" on)
option(ENABLE_DFSI_SUPPORT "Enable P25 DFSI Transport Support" off)
option(ENABLE_TESTS "Enable compilation of test suite" off)

message(CHECK_START "DMR Digital Mode")
if (ENABLE_DMR)
    add_definitions(-DENABLE_DMR)
    message(CHECK_PASS "enabled")
else ()
    message(CHECK_PASS "disabled")
endif (ENABLE_DMR)
message(CHECK_START "P25 Digital Mode")
if (ENABLE_P25)
    add_definitions(-DENABLE_P25)
    message(CHECK_PASS "enabled")
else ()
    message(CHECK_PASS "disabled")
endif (ENABLE_P25)
message(CHECK_START "NXDN Digital Mode")
if (ENABLE_NXDN)
    add_definitions(-DENABLE_NXDN)
    message(CHECK_PASS "enabled")
else ()
    message(CHECK_PASS "disabled")
endif (ENABLE_NXDN)
message(CHECK_START "P25 DFSI Support")
if (ENABLE_DFSI_SUPPORT)
    add_definitions(-DENABLE_DFSI_SUPPORT)
    message(CHECK_PASS "enabled")
else ()
    message(CHECK_PASS "disabled")
endif (ENABLE_DFSI_SUPPORT)
message(CHECK_START "Enable test suite compilation")
if (ENABLE_TESTS)
    message(CHECK_PASS "enabled")
else ()
    message(CHECK_PASS "disabled")
endif (ENABLE_TESTS)

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

# Cross-compile options
option(CROSS_COMPILE_ARM "Cross-compile for 32-bit ARM" off)
option(CROSS_COMPILE_AARCH64 "Cross-compile for 64-bit ARM" off)
option(CROSS_COMPILE_RPI_ARM "Cross-compile for (old RPi) 32-bit ARM" off)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
set(ARCH amd64)
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)

message(CHECK_START "Cross compiling for 32-bit ARM")
if (CROSS_COMPILE_ARM)
    set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
    set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
    set(ARCH arm)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm)
    message(CHECK_PASS "yes")
else ()
    message(CHECK_PASS "no")
endif (CROSS_COMPILE_ARM)
message(CHECK_START "Cross compiling for 64-bit ARM")
if (CROSS_COMPILE_AARCH64)
    set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
    set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
    set(ARCH arm64)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)
    message(CHECK_PASS "yes")
else ()
    message(CHECK_PASS "no")
endif (CROSS_COMPILE_AARCH64)

option(WITH_RPI_ARM_TOOLS "Specifies the location for the RPI ARM tools" off)
if (WITH_RPI_ARM_TOOLS)
    set(RPI_ARM_TOOLS ${WITH_RPI_ARM_TOOLS})
    message(CHECK_START "With RPi 1 Tools: ${RPI_ARM_TOOLS}")
endif (WITH_RPI_ARM_TOOLS)

message(CHECK_START "Cross compiling for (old RPi) 32-bit ARM")
if (CROSS_COMPILE_RPI_ARM)
    if (NOT WITH_RPI_ARM_TOOLS)
        message("-- Cloning legacy Raspberry Pi compilation toolchain")
        Include(FetchContent)
        FetchContent_Declare(
            RPiTools
            GIT_REPOSITORY https://github.com/raspberrypi/tools.git
        )
        FetchContent_MakeAvailable(RPiTools)
        set(CMAKE_C_COMPILER ${CMAKE_CURRENT_BINARY_DIR}/_deps/rpitools-src/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc)
        set(CMAKE_CXX_COMPILER ${CMAKE_CURRENT_BINARY_DIR}/_deps/rpitools-src/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++)
    else()
        set(CMAKE_C_COMPILER ${RPI_ARM_TOOLS}/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc)
        set(CMAKE_CXX_COMPILER ${RPI_ARM_TOOLS}/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++)
    endif ()

    set(ARCH armhf)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE armhf)
    message(CHECK_PASS "yes")
else ()
    message(CHECK_PASS "no")
endif (CROSS_COMPILE_RPI_ARM)

option(WITH_ASIO "Manually specify the location for the ASIO library" off)
if (WITH_ASIO)
    set(ASIO_INCLUDE_DIR ${WITH_ASIO}/include)
    message(CHECK_START "With ASIO: ${ASIO_INCLUDE_DIR}")
else()
    message("-- Cloning ASIO")
    Include(FetchContent)
    FetchContent_Declare(
        ASIO
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    )
    FetchContent_MakeAvailable(ASIO)
    set(ASIO_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps/asio-src/asio/include)
endif (WITH_ASIO)

# Standard CMake options
set(THREADS_PREFER_PTHREAD_FLAG ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY .)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3 -Wall -std=c++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -Wall -std=c++11")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g -O3 -Wall -std=c++11 -s")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O3 -Wall -std=c++11 -s")

set(CMAKE_BUILD_TYPE "RelWithDebInfo")

set(CMAKE_INSTALL_PREFIX "/usr/local")

set(GIT_VER "")
set(GIT_VER_HASH "")
execute_process(COMMAND git describe --abbrev=8 --dirty --always --tags WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_VER OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND git describe --abbrev=8 --always --tags WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_VER_HASH OUTPUT_STRIP_TRAILING_WHITESPACE)

add_definitions(-D__GIT_VER__="${GIT_VER}")
add_definitions(-D__GIT_VER_HASH__="${GIT_VER_HASH}")

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

add_executable(dvmhost ${dvmhost_SRC})
target_include_directories(dvmhost PRIVATE src)
target_link_libraries(dvmhost PRIVATE asio::asio Threads::Threads util)

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

if (ENABLE_TESTS)
#
## dvmtest project
#
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

add_executable(dvmtests ${dvmhost_SRC} ${dvmtests_SRC})
target_compile_definitions(dvmtests PUBLIC -DCATCH2_TEST_COMPILATION)
target_link_libraries(dvmtests PRIVATE Catch2::Catch2WithMain asio::asio Threads::Threads util)
target_include_directories(dvmtests PRIVATE .)
endif (ENABLE_TESTS)

#
# Standard dvmhost/dvmcmd install
#
install(TARGETS dvmhost DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS dvmcmd DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(FILES configs/config.example.yml configs/iden_table.dat configs/RSSI.dat configs/rid_acl.example.dat configs/tg_acl.example.dat DESTINATION ${CMAKE_INSTALL_PREFIX}/etc)
install(PROGRAMS tools/start-dvm.sh tools/stop-dvm.sh tools/dvm-watchdog.sh tools/stop-watchdog.sh DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(CODE "execute_process(COMMAND bash \"-c\" \"sed -i 's/filePath: ./filePath: \\\\/var\\\\/log\\\\//' /usr/local/etc/config.example.yml\")")
install(CODE "execute_process(COMMAND bash \"-c\" \"sed -i 's/activityFilePath: ./activityFilePath: \\\\/var\\\\/log\\\\//' /usr/local/etc/config.example.yml\")")
install(CODE "execute_process(COMMAND bash \"-c\" \"sed -i 's/file: iden_table.dat/file: \\\\/usr\\\\/local\\\\/etc\\\\/iden_table.dat/' /usr/local/etc/config.example.yml\")")
install(CODE "execute_process(COMMAND bash \"-c\" \"sed -i 's/file: rid_acl.dat/file: \\\\/usr\\\\/local\\\\/etc\\\\/rid_acl.dat/' /usr/local/etc/config.example.yml\")")
install(CODE "execute_process(COMMAND bash \"-c\" \"sed -i 's/file: tg_acl.dat/file: \\\\/usr\\\\/local\\\\/etc\\\\/tg_acl.dat/' /usr/local/etc/config.example.yml\")")

#
# Helper target to force strip binaries.
#
add_custom_target(strip
    COMMAND strip -s dvmhost
    COMMAND strip -s dvmcmd)

#/*
#** bryanb: Please do not change the following section unless adding or removing paths that need to be part of a build
#** these sections are maintained for internal use.
#*/

#
# Custom make target to perform a tarball packaging. This will ultimately contain the same type of pathing
# the non-standard legacy install to "/opt/dvm" does.
# 
set(CMAKE_INSTALL_PREFIX_TARBALL "tar_build")
add_custom_target(tarball
    COMMAND rm -rf ${CMAKE_INSTALL_PREFIX_TARBALL}
    COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
    COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/log
    COMMAND touch ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/log/INCLUDE_DIRECTORY
    COMMAND cp -v dvmhost ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
    COMMAND cp -v dvmcmd ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
    COMMAND cp ../tools/*.sh ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm
    COMMAND chmod +x ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/*.sh
    COMMAND cp -v ../configs/config*.yml ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm
    COMMAND cp -v ../configs/*.dat ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm
    COMMAND cd ${CMAKE_INSTALL_PREFIX_TARBALL} && tar czvf ../dvmhost_${CPACK_DEBIAN_PACKAGE_VERSION}_${ARCH}.tar.gz *
    COMMAND rm -rf ${CMAKE_INSTALL_PREFIX_TARBALL})

#
# Custom make target to perform non-standard legacy install to "/opt/dvm". This is meant
# to retain backward compatibility with deployment scripts and other tools that work with "/opt/dvm"
# instead of system paths.
#
# This is inherited logic from the old DVMHost 2.0 Makefile.
#
set(CMAKE_LEGACY_INSTALL_PREFIX "/opt/dvm")
add_custom_target(old_install
    COMMAND mkdir -p ${CMAKE_LEGACY_INSTALL_PREFIX}
    COMMAND mkdir -p ${CMAKE_LEGACY_INSTALL_PREFIX}/bin
    COMMAND mkdir -p ${CMAKE_LEGACY_INSTALL_PREFIX}/log
    COMMAND install -m 755 dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/bin
    COMMAND install -m 755 dvmcmd ${CMAKE_LEGACY_INSTALL_PREFIX}/bin
    COMMAND install -m 644 ../configs/config.example.yml ${CMAKE_LEGACY_INSTALL_PREFIX}/config.example.yml
    COMMAND install -m 644 ../configs/iden_table.dat ${CMAKE_LEGACY_INSTALL_PREFIX}/iden_table.dat
    COMMAND install -m 644 ../configs/RSSI.dat ${CMAKE_LEGACY_INSTALL_PREFIX}/RSSI.dat
    COMMAND install -m 644 ../configs/rid_acl.example.dat ${CMAKE_LEGACY_INSTALL_PREFIX}/rid_acl.dat
    COMMAND install -m 644 ../configs/tg_acl.example.dat ${CMAKE_LEGACY_INSTALL_PREFIX}/tg_acl.dat
    COMMAND install -m 755 ../tools/start-dvm.sh ${CMAKE_LEGACY_INSTALL_PREFIX}
    COMMAND install -m 755 ../tools/stop-dvm.sh ${CMAKE_LEGACY_INSTALL_PREFIX}
    COMMAND install -m 755 ../tools/dvm-watchdog.sh ${CMAKE_LEGACY_INSTALL_PREFIX}
    COMMAND install -m 755 ../tools/stop-watchdog.sh ${CMAKE_LEGACY_INSTALL_PREFIX})

#
# Custom make target to perform non-standard legacy service install. This is meant
# to retain backward compatibility with deployment scripts and other tools that work with "/opt/dvm"
# instead of system paths.
#
# This is inherited logic from the old DVMHost 2.0 Makefile.
#
add_custom_target(old_install-service
    COMMAND useradd --user-group -M --system dvmhost --shell /bin/false || true
    COMMAND usermod --groups dialout --append dvmhost || true
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/config.example.yml
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/iden_table.dat
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/RSSI.dat
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/rid_acl.dat
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/tg_acl.dat
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/log
    COMMAND cp ../linux/dvmhost.service /lib/systemd/system/
    COMMAND bash \"-c\" \"sed -i 's/\\\\/usr\\\\/local\\\\/bin/\\\\/opt\\\\/dvm\\\\/bin/' /lib/systemd/system/dvmhost.service\"
    COMMAND bash \"-c\" \"sed -i 's/\\\\/usr\\\\/local\\\\/etc/\\\\/opt\\\\/dvm/' /lib/systemd/system/dvmhost.service\")
