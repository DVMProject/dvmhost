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
## dvmtest source/header files
#
file(GLOB dvmtests_SRC
    "tests/nulltest.cpp"
    "tests/edac/*.cpp"
    "tests/p25/*.cpp"
)

option(ENABLE_TESTS "Enable compilation of test suite" off)
message(CHECK_START "Enable compilation of test suite")
if (ENABLE_TESTS)
    message(CHECK_PASS "yes")
else ()
    message(CHECK_PASS "no")
endif (ENABLE_TESTS)

option(ENABLE_TUI_SUPPORT "Enable TUI support" on)
message(CHECK_START "Enable TUI support")
if (ENABLE_TUI_SUPPORT)
    message(CHECK_PASS "yes")
else ()
    message(CHECK_PASS "no")
endif (ENABLE_TUI_SUPPORT)

# Cross-compile options
option(CROSS_COMPILE_ARM "Cross-compile for 32-bit ARM" off)
option(CROSS_COMPILE_AARCH64 "Cross-compile for 64-bit ARM" off)
option(CROSS_COMPILE_RPI_ARM "Cross-compile for (old RPi) 32-bit ARM" off)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
set(ARCH amd64)
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)

if (CROSS_COMPILE_ARM)
    set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
    set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
    set(ARCH arm)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm)
    message(CHECK_START "Cross compiling for 32-bit ARM - ${CMAKE_C_COMPILER}")
endif (CROSS_COMPILE_ARM)
if (CROSS_COMPILE_AARCH64)
    set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
    set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
    set(ARCH arm64)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)
    message(CHECK_START "Cross compiling for 64-bit ARM - ${CMAKE_C_COMPILER}")
endif (CROSS_COMPILE_AARCH64)

option(WITH_RPI_ARM_TOOLS "Specifies the location for the RPI ARM tools" off)
if (WITH_RPI_ARM_TOOLS)
    set(RPI_ARM_TOOLS ${WITH_RPI_ARM_TOOLS})
    message(CHECK_START "With RPi 1 Tools: ${RPI_ARM_TOOLS}")
endif (WITH_RPI_ARM_TOOLS)

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
    message(CHECK_START "Cross compiling for (old RPi) 32-bit ARM - ${CMAKE_C_COMPILER}")
endif (CROSS_COMPILE_RPI_ARM)

# Standard CMake options
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY .)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)
message(CHECK_START "Build Type is ${CMAKE_BUILD_TYPE}")
if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -Wall -std=c++14")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -std=c++14")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g -O0 -Wall -std=c++14")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O0 -Wall -std=c++14")
elseif(CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3 -Wall -std=c++14 -s")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -Wall -std=c++14 -s")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g -O3 -Wall -std=c++14 -s")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O3 -Wall -std=c++14 -s")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3 -Wall -std=c++14")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -Wall -std=c++14")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g -O3 -Wall -std=c++14")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O3 -Wall -std=c++14")
endif()
if (CROSS_COMPILE_ARM)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-psabi")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-psabi")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wno-psabi")
endif (CROSS_COMPILE_ARM)

set(CMAKE_INSTALL_PREFIX "/usr/local")

#
# Library Inclusions
#
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

if (ENABLE_TUI_SUPPORT)
message("-- Cloning finalcut")
Include(FetchContent)
FetchContent_Declare(
    FINALCUT
    GIT_REPOSITORY https://github.com/gatekeep/finalcut-cmake.git
)
set(F_COMPILE_STATIC 1)
FetchContent_MakeAvailable(FINALCUT)
set(FINALCUT_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps/finalcut-src/src)
endif (ENABLE_TUI_SUPPORT)

#
# Set GIT_VER compiler directive
#
set(GIT_VER "")
set(GIT_VER_HASH "")
execute_process(COMMAND git describe --abbrev=8 --dirty --always WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_VER OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND git describe --abbrev=8 --always WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_VER_HASH OUTPUT_STRIP_TRAILING_WHITESPACE)

add_definitions(-D__GIT_VER__="${GIT_VER}")
add_definitions(-D__GIT_VER_HASH__="${GIT_VER_HASH}")

project(dvmhost)
include(src/CMakeLists.txt)

if (ENABLE_TESTS)
include(tests/CMakeLists.txt)
endif (ENABLE_TESTS)

#
# Standard dvmhost/dvmcmd install
#
install(TARGETS dvmhost DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS dvmcmd DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(FILES configs/config.example.yml configs/fne-config.example.yml configs/iden_table.dat configs/RSSI.dat configs/rid_acl.example.dat configs/talkgroup_rules.example.yml DESTINATION ${CMAKE_INSTALL_PREFIX}/etc)
install(PROGRAMS tools/start-dvm.sh tools/stop-dvm.sh tools/dvm-watchdog.sh tools/stop-watchdog.sh DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(CODE "execute_process(COMMAND bash \"-c\" \"sed -i 's/filePath: ./filePath: \\\\/var\\\\/log\\\\//' /usr/local/etc/config.example.yml\")")
install(CODE "execute_process(COMMAND bash \"-c\" \"sed -i 's/activityFilePath: ./activityFilePath: \\\\/var\\\\/log\\\\//' /usr/local/etc/config.example.yml\")")
install(CODE "execute_process(COMMAND bash \"-c\" \"sed -i 's/file: iden_table.dat/file: \\\\/usr\\\\/local\\\\/etc\\\\/iden_table.dat/' /usr/local/etc/config.example.yml\")")
install(CODE "execute_process(COMMAND bash \"-c\" \"sed -i 's/file: rid_acl.dat/file: \\\\/usr\\\\/local\\\\/etc\\\\/rid_acl.dat/' /usr/local/etc/config.example.yml\")")
install(CODE "execute_process(COMMAND bash \"-c\" \"sed -i 's/file: talkgroup_rules.yml/file: \\\\/usr\\\\/local\\\\/etc\\\\/talkgroup_rules.yml/' /usr/local/etc/config.example.yml\")")

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
    COMMAND cp -v ../configs/*.yml ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm
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
    COMMAND install -m 644 ../configs/fne-config.example.yml ${CMAKE_LEGACY_INSTALL_PREFIX}/fne-config.example.yml
    COMMAND install -m 644 ../configs/iden_table.dat ${CMAKE_LEGACY_INSTALL_PREFIX}/iden_table.dat
    COMMAND install -m 644 ../configs/RSSI.dat ${CMAKE_LEGACY_INSTALL_PREFIX}/RSSI.dat
    COMMAND install -m 644 ../configs/rid_acl.example.dat ${CMAKE_LEGACY_INSTALL_PREFIX}/rid_acl.dat
    COMMAND install -m 644 ../configs/talkgroup_rules.example.yml ${CMAKE_LEGACY_INSTALL_PREFIX}/talkgroup_rules.example.yml
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
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/fne-config.example.yml
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/iden_table.dat
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/RSSI.dat
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/rid_acl.dat
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/talkgroup_rules.example.yml
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/log
    COMMAND cp ../linux/dvmhost.service /lib/systemd/system/
    COMMAND bash \"-c\" \"sed -i 's/\\\\/usr\\\\/local\\\\/bin/\\\\/opt\\\\/dvm\\\\/bin/' /lib/systemd/system/dvmhost.service\"
    COMMAND bash \"-c\" \"sed -i 's/\\\\/usr\\\\/local\\\\/etc/\\\\/opt\\\\/dvm/' /lib/systemd/system/dvmhost.service\")
