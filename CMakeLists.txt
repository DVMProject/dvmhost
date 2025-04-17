# SPDX-License-Identifier: GPL-2.0-only
#/*
# * Digital Voice Modem - CMake Build System
# * GPLv2 Open Source. Use is subject to license terms.
# * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
# *
# *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
# *  Copyright (C) 2022 Natalie Moore
# *
# */
cmake_minimum_required(VERSION 3.16.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS on) 

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

option(DISABLE_WEBSOCKETS "Disable WebSocket support" off)
if (DISABLE_WEBSOCKETS)
    message(CHECK_START "Disable WebSocket support - enabled")
    add_definitions(-DNO_WEBSOCKETS)
endif (DISABLE_WEBSOCKETS)

# Cross-compile options
option(CROSS_COMPILE_ARM "Cross-compile for 32-bit ARM" off)
option(CROSS_COMPILE_AARCH64 "Cross-compile for 64-bit ARM" off)
option(CROSS_COMPILE_RPI_ARM "Cross-compile for (old RPi) 32-bit ARM" off)
option(COMPILE_WIN32 "Compile for Win32" off)

if (COMPILE_WIN32)
    set(ARCH amd64)
    set(CMAKE_SYSTEM_PROCESSOR amd64)

    # No TUI for this
    set(ENABLE_TUI_SUPPORT OFF)
    message(CHECK_START "Enable TUI support - no; for simplicity WIN32 does not support TUI.")
else()
    set(CMAKE_C_COMPILER /usr/bin/gcc CACHE STRING "C compiler")
    set(CMAKE_CXX_COMPILER /usr/bin/g++ CACHE STRING "C++ compiler")
    set(ARCH amd64)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
endif (COMPILE_WIN32)

if (CROSS_COMPILE_ARM)
    set(CMAKE_C_COMPILER /usr/bin/arm-linux-gnueabihf-gcc CACHE STRING "C compiler" FORCE)
    set(CMAKE_CXX_COMPILER /usr/bin/arm-linux-gnueabihf-g++ CACHE STRING "C++ compiler" FORCE)
    set(ARCH armhf)
    set(CMAKE_SYSTEM_PROCESSOR armhf)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE armhf)
    set(OPENSSL_ROOT_DIR /usr/lib/arm-linux-gnueabihf)
    message(CHECK_START "Cross compiling for 32-bit ARM - ${CMAKE_C_COMPILER}")
endif (CROSS_COMPILE_ARM)
if (CROSS_COMPILE_AARCH64)
    set(CMAKE_C_COMPILER /usr/bin/aarch64-linux-gnu-gcc CACHE STRING "C compiler" FORCE)
    set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++ CACHE STRING "C++ compiler" FORCE)
    set(ARCH arm64)
    set(CMAKE_SYSTEM_PROCESSOR arm64)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)
    set(OPENSSL_ROOT_DIR /usr/lib/aarch64-linux-gnu)
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
        include(FetchContent)
        FetchContent_Declare(
            RPiTools
            GIT_REPOSITORY https://github.com/raspberrypi/tools.git
        )
        FetchContent_MakeAvailable(RPiTools)
        set(CMAKE_C_COMPILER ${CMAKE_CURRENT_BINARY_DIR}/_deps/rpitools-src/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc CACHE STRING "C compiler" FORCE)
        set(CMAKE_CXX_COMPILER ${CMAKE_CURRENT_BINARY_DIR}/_deps/rpitools-src/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/bin/arm-linux-gnueabihf-g++ CACHE STRING "C++ compiler" FORCE)

        message(CHECK_START "Apply OpenSSL library binaries for cross compling (old RPi) 32-bit ARM - ${CMAKE_CURRENT_BINARY_DIR}/_deps/rpitools-src")
        execute_process(COMMAND tar xzf contrib/openssl_cross_patch.RPI_ARM.tar.gz -C ${CMAKE_CURRENT_BINARY_DIR}/_deps/rpitools-src WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

        set(OPENSSL_ROOT_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps/rpitools-src/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/arm-linux-gnueabihf/sysroot/usr/lib)
    else()
        set(CMAKE_C_COMPILER ${RPI_ARM_TOOLS}/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc CACHE STRING "C compiler" FORCE)
        set(CMAKE_CXX_COMPILER ${RPI_ARM_TOOLS}/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/bin/arm-linux-gnueabihf-g++ CACHE STRING "C++ compiler" FORCE)

        message(CHECK_START "Apply OpenSSL library binaries for cross compling (old RPi) 32-bit ARM - ${RPI_ARM_TOOLS}")
        execute_process(COMMAND tar xzf contrib/openssl_cross_patch.RPI_ARM.tar.gz -C ${RPI_ARM_TOOLS} WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

        set(OPENSSL_ROOT_DIR ${RPI_ARM_TOOLS}/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/arm-linux-gnueabihf/sysroot/usr/lib)
    endif ()

    set(ARCH armhf)
    set(CMAKE_SYSTEM_PROCESSOR armhf)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE armhf)
    message(CHECK_START "Cross compiling for (old RPi) 32-bit ARM - ${CMAKE_C_COMPILER}")

    # No TUI for this
    set(ENABLE_TUI_SUPPORT OFF)
    message(CHECK_START "Enable TUI support - no; for simplicity RPI_ARM cross-compiling does not support TUI.")
endif (CROSS_COMPILE_RPI_ARM)

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Standard CMake options
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY .)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)
message(CHECK_START "Build Type is ${CMAKE_BUILD_TYPE}")
if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -Wno-overloaded-virtual -std=c++14")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g -O0 -Wall")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O0 -Wall -Wno-overloaded-virtual -std=c++14")
elseif(CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3 -Wall -s")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -Wall -Wno-overloaded-virtual -std=c++14 -s")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g -O3 -Wall -s")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O3 -Wall -Wno-overloaded-virtual -std=c++14 -s")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3 -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -Wall -Wno-overloaded-virtual -std=c++14")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g -O3 -Wall")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O3 -Wall -Wno-overloaded-virtual -std=c++14")
endif()
if (CROSS_COMPILE_ARM)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-psabi")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-psabi")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wno-psabi")
endif (CROSS_COMPILE_ARM)
if (CROSS_COMPILE_RPI_ARM)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -std=c99")
endif (CROSS_COMPILE_RPI_ARM)
if (COMPILE_WIN32)
    set(CMAKE_C_FLAGS "/EHsc /D_WIN32 /D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR")
    set(CMAKE_CXX_FLAGS "/EHsc /D_WIN32 /D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR")
    set(CMAKE_C_FLAGS_RELEASE "/EHsc /D_WIN32 /D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR")
    set(CMAKE_CXX_FLAGS_RELEASE "/EHsc /D_WIN32 /D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR")
endif (COMPILE_WIN32)

set(CMAKE_INSTALL_PREFIX "/usr/local")

#
# Library Inclusions
#
if (NOT ASIO_INCLUDED)
    option(WITH_ASIO "Manually specify the location for the ASIO library" off)
    if (WITH_ASIO)
        set(ASIO_INCLUDE_DIR ${WITH_ASIO}/include)
        message(CHECK_START "With ASIO: ${ASIO_INCLUDE_DIR}")
    else()
        message("-- Cloning ASIO")
        include(FetchContent)
        FetchContent_Declare(
            ASIO
            GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
            GIT_TAG 7609450f71434bdc9fbd9491a9505b423c2a8496 # asio-1-28-2
        )
        FetchContent_MakeAvailable(ASIO)
        set(ASIO_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps/asio-src/asio/include)
        set(ASIO_INCLUDED 1)
    endif (WITH_ASIO)
endif (NOT ASIO_INCLUDED)

if (NOT DISABLE_WEBSOCKETS)
    if (NOT WEBSOCKETPP_INCLUDED)
        message("-- Cloning WebSocket++")
        include(FetchContent)
        FetchContent_Declare(
            WEBSOCKETPP
            GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
            GIT_TAG 56123c87598f8b1dd471be83ca841ceae07f95ba # 0.8.2
        )
        FetchContent_MakeAvailable(WEBSOCKETPP)
        add_subdirectory(${websocketpp_SOURCE_DIR} EXCLUDE_FROM_ALL)
        set(WEBSOCKETPP_INCLUDED 1)
    endif (NOT WEBSOCKETPP_INCLUDED)
endif (NOT DISABLE_WEBSOCKETS)

if (ENABLE_TUI_SUPPORT AND NOT FC_INCLUDED)
    message("-- Cloning finalcut")
    include(FetchContent)
    FetchContent_Declare(
        FINALCUT
        GIT_REPOSITORY https://github.com/gatekeep/finalcut-cmake.git
    )
    set(FC_INCLUDED 1)
    set(F_COMPILE_STATIC 1)
    FetchContent_MakeAvailable(FINALCUT)
    set(FINALCUT_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps/finalcut-src/src)
endif (ENABLE_TUI_SUPPORT AND NOT FC_INCLUDED)

#
# Set GIT_VER compiler directive
#
set(GIT_VER "")
set(GIT_VER_HASH "")
execute_process(COMMAND git describe --abbrev=8 --dirty --always WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_VER OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND git describe --abbrev=8 --always WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_VER_HASH OUTPUT_STRIP_TRAILING_WHITESPACE)

add_definitions(-D__GIT_VER__="${GIT_VER}")
add_definitions(-D__GIT_VER_HASH__="${GIT_VER_HASH}")

project(dvm)
include(src/CMakeLists.txt)

if (ENABLE_TESTS)
    include(tests/CMakeLists.txt)
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
    target_link_libraries(dvmtests PRIVATE Catch2::Catch2WithMain common ${OPENSSL_LIBRARIES} asio::asio Threads::Threads util)
    target_include_directories(dvmtests PRIVATE ${OPENSSL_INCLUDE_DIR} src src/host tests)
endif (ENABLE_TESTS)

#
# Standard dvmhost/dvmcmd install
#
install(TARGETS dvmhost DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS dvmcmd DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS dvmfne DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
if (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))
install(TARGETS dvmmon DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS sysview DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS tged DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))
install(TARGETS dvmbridge DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(FILES configs/config.example.yml configs/fne-config.example.yml configs/fne-sysview.example.yml configs/monitor-config.example.yml configs/iden_table.example.dat configs/RSSI.dat configs/rid_acl.example.dat configs/talkgroup_rules.example.yml configs/bridge-config.example.yml DESTINATION ${CMAKE_INSTALL_PREFIX}/etc)
install(PROGRAMS tools/start-dvm.sh tools/stop-dvm.sh tools/dvm-watchdog.sh tools/stop-watchdog.sh tools/fne-watchdog.sh tools/start-dvm-fne.sh DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(CODE "execute_process(COMMAND bash \"-c\" \"sed -i 's/filePath: ./filePath: \\\\/var\\\\/log\\\\//' /usr/local/etc/config.example.yml\")")
install(CODE "execute_process(COMMAND bash \"-c\" \"sed -i 's/activityFilePath: ./activityFilePath: \\\\/var\\\\/log\\\\//' /usr/local/etc/config.example.yml\")")
install(CODE "execute_process(COMMAND bash \"-c\" \"sed -i 's/file: iden_table.dat/file: \\\\/usr\\\\/local\\\\/etc\\\\/iden_table.dat/' /usr/local/etc/config.example.yml\")")
install(CODE "execute_process(COMMAND bash \"-c\" \"sed -i 's/file: rid_acl.dat/file: \\\\/usr\\\\/local\\\\/etc\\\\/rid_acl.dat/' /usr/local/etc/config.example.yml\")")
install(CODE "execute_process(COMMAND bash \"-c\" \"sed -i 's/file: talkgroup_rules.yml/file: \\\\/usr\\\\/local\\\\/etc\\\\/talkgroup_rules.yml/' /usr/local/etc/config.example.yml\")")

#
# Helper target to force strip binaries.
#
if (NOT TARGET strip)
    if (CROSS_COMPILE_ARM)
        if (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))
            add_custom_target(strip
                    COMMAND arm-linux-gnueabihf-strip -s dvmhost
                    COMMAND arm-linux-gnueabihf-strip -s dvmfne
                    COMMAND arm-linux-gnueabihf-strip -s dvmcmd
                    COMMAND arm-linux-gnueabihf-strip -s dvmmon
                    COMMAND arm-linux-gnueabihf-strip -s sysview
                    COMMAND arm-linux-gnueabihf-strip -s tged
                    COMMAND arm-linux-gnueabihf-strip -s peered
                    COMMAND arm-linux-gnueabihf-strip -s dvmbridge)
        else()
            add_custom_target(strip
                    COMMAND arm-linux-gnueabihf-strip -s dvmhost
                    COMMAND arm-linux-gnueabihf-strip -s dvmfne
                    COMMAND arm-linux-gnueabihf-strip -s dvmcmd
                    COMMAND arm-linux-gnueabihf-strip -s dvmbridge)
        endif (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))
    elseif (CROSS_COMPILE_AARCH64)
        if (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))
            add_custom_target(strip
                COMMAND aarch64-linux-gnu-strip -s dvmhost
                COMMAND aarch64-linux-gnu-strip -s dvmfne
                COMMAND aarch64-linux-gnu-strip -s dvmcmd
                COMMAND aarch64-linux-gnu-strip -s dvmmon
                COMMAND aarch64-linux-gnu-strip -s sysview
                COMMAND aarch64-linux-gnu-strip -s tged
                COMMAND aarch64-linux-gnu-strip -s peered
                COMMAND aarch64-linux-gnu-strip -s dvmbridge)
        else()
            add_custom_target(strip
                COMMAND aarch64-linux-gnu-strip -s dvmhost
                COMMAND aarch64-linux-gnu-strip -s dvmfne
                COMMAND aarch64-linux-gnu-strip -s dvmcmd
                COMMAND aarch64-linux-gnu-strip -s dvmbridge)
        endif (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))
    elseif (CROSS_COMPILE_RPI_ARM)
        if (NOT WITH_RPI_ARM_TOOLS)
            add_custom_target(strip
                COMMAND ${CMAKE_CURRENT_BINARY_DIR}/_deps/rpitools-src/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-strip -s dvmhost
                COMMAND ${CMAKE_CURRENT_BINARY_DIR}/_deps/rpitools-src/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-strip -s dvmfne
                COMMAND ${CMAKE_CURRENT_BINARY_DIR}/_deps/rpitools-src/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-strip -s dvmcmd
                COMMAND ${CMAKE_CURRENT_BINARY_DIR}/_deps/rpitools-src/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-strip -s dvmbridge)
        else()
            add_custom_target(strip
                COMMAND ${RPI_ARM_TOOLS}/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-strip -s dvmhost
                COMMAND ${RPI_ARM_TOOLS}/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-strip -s dvmfne
                COMMAND ${RPI_ARM_TOOLS}/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-strip -s dvmcmd
                COMMAND ${RPI_ARM_TOOLS}/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-strip -s dvmbridge)
        endif ()
    else()
        if (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))
            add_custom_target(strip
                COMMAND strip -s dvmhost
                COMMAND strip -s dvmfne
                COMMAND strip -s dvmcmd
                COMMAND strip -s dvmmon
                COMMAND strip -s sysview
                COMMAND strip -s tged
                COMMAND strip -s peered
                COMMAND strip -s dvmbridge)
        else()
            add_custom_target(strip
                COMMAND strip -s dvmhost
                COMMAND strip -s dvmfne
                COMMAND strip -s dvmcmd
                COMMAND strip -s dvmbridge)
        endif (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))
    endif (CROSS_COMPILE_ARM)
endif (NOT TARGET strip)

#/*
#** bryanb: Please do not change the following section unless adding or removing paths that need to be part of a build
#** these sections are maintained for internal use.
#*/

#
# Custom make target to perform a tarball packaging. This will ultimately contain the same type of pathing
# the non-standard legacy install to "/opt/dvm" does.
# 
if (NOT TARGET tarball)
    set(CMAKE_INSTALL_PREFIX_TARBALL "tar_build")
    if (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))
        add_custom_target(tarball
            COMMAND rm -rf ${CMAKE_INSTALL_PREFIX_TARBALL}
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/log
            COMMAND touch ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/log/INCLUDE_DIRECTORY
            COMMAND cp -v dvmhost ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v dvmcmd ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v dvmmon ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v sysview ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v tged ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v peered ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v dvmfne ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v dvmbridge ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp ${CMAKE_SOURCE_DIR}/tools/*.sh ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm
            COMMAND chmod +x ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/*.sh
            COMMAND cp -v ${CMAKE_SOURCE_DIR}/configs/*.yml ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/schema
            COMMAND cp -v ${CMAKE_SOURCE_DIR}/configs/schema/*.json ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/schema
            COMMAND cp -v ${CMAKE_SOURCE_DIR}/configs/*.dat ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw
            COMMAND if [ -e dvm-firmware_f4.elf ]\; then cp -v dvm-firmware_f4.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4.bin ]\; then cp -v dvm-firmware_f4.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-pog.elf ]\; then cp -v dvm-firmware_f4-pog.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-pog.bin ]\; then cp -v dvm-firmware_f4-pog.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_eda.elf ]\; then cp -v dvm-firmware_eda.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_eda.bin ]\; then cp -v dvm-firmware_eda.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-dvmv1.elf ]\; then cp -v dvm-firmware_f4-dvmv1.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-dvmv1.bin ]\; then cp -v dvm-firmware_f4-dvmv1.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_due.elf ]\; then cp -v dvm-firmware_due.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_due.bin ]\; then cp -v dvm-firmware_due.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware-hs_f1.elf ]\; then cp -v dvm-firmware-hs_f1.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware-hs_f1.bin ]\; then cp -v dvm-firmware-hs_f1.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e DVM-V24-stm32f103.elf ]\; then cp -v DVM-V24-stm32f103.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e DVM-V24-stm32f103.bin ]\; then cp -v DVM-V24-stm32f103.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND cd ${CMAKE_INSTALL_PREFIX_TARBALL} && tar czvf ../dvmhost_${CPACK_DEBIAN_PACKAGE_VERSION}_${ARCH}.tar.gz *
            COMMAND rm -rf ${CMAKE_INSTALL_PREFIX_TARBALL})
    else()
        add_custom_target(tarball
            COMMAND rm -rf ${CMAKE_INSTALL_PREFIX_TARBALL}
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/log
            COMMAND touch ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/log/INCLUDE_DIRECTORY
            COMMAND cp -v dvmhost ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v dvmcmd ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v dvmfne ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v dvmbridge ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp ${CMAKE_SOURCE_DIR}/tools/*.sh ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm
            COMMAND chmod +x ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/*.sh
            COMMAND cp -v ${CMAKE_SOURCE_DIR}/configs/*.yml ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/schema
            COMMAND cp -v ${CMAKE_SOURCE_DIR}/configs/schema/*.json ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/schema
            COMMAND cp -v ${CMAKE_SOURCE_DIR}/configs/*.dat ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw
            COMMAND if [ -e dvm-firmware_f4.elf ]\; then cp -v dvm-firmware_f4.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4.bin ]\; then cp -v dvm-firmware_f4.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-pog.elf ]\; then cp -v dvm-firmware_f4-pog.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-pog.bin ]\; then cp -v dvm-firmware_f4-pog.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_eda.elf ]\; then cp -v dvm-firmware_eda.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_eda.bin ]\; then cp -v dvm-firmware_eda.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-dvmv1.elf ]\; then cp -v dvm-firmware_f4-dvmv1.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-dvmv1.bin ]\; then cp -v dvm-firmware_f4-dvmv1.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_due.elf ]\; then cp -v dvm-firmware_due.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_due.bin ]\; then cp -v dvm-firmware_due.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware-hs_f1.elf ]\; then cp -v dvm-firmware-hs_f1.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware-hs_f1.bin ]\; then cp -v dvm-firmware-hs_f1.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e DVM-V24-stm32f103.elf ]\; then cp -v DVM-V24-stm32f103.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e DVM-V24-stm32f103.bin ]\; then cp -v DVM-V24-stm32f103.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND cd ${CMAKE_INSTALL_PREFIX_TARBALL} && tar czvf ../dvmhost_${CPACK_DEBIAN_PACKAGE_VERSION}_${ARCH}.tar.gz *
            COMMAND rm -rf ${CMAKE_INSTALL_PREFIX_TARBALL})
    endif (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))
endif (NOT TARGET tarball)

#
# Custom make target to perform a tarball packaging. This will ultimately contain the same type of pathing
# the non-standard legacy install to "/opt/dvm" does.
# 
if (NOT TARGET tarball_notools)
    set(CMAKE_INSTALL_PREFIX_TARBALL "tar_build")
    if (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))
        add_custom_target(tarball_notools
            COMMAND rm -rf ${CMAKE_INSTALL_PREFIX_TARBALL}
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/log
            COMMAND touch ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/log/INCLUDE_DIRECTORY
            COMMAND cp -v dvmhost ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v dvmcmd ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v dvmmon ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v sysview ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v tged ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v peered ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v dvmfne ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v dvmbridge ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v ${CMAKE_SOURCE_DIR}/configs/*.yml ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/schema
            COMMAND cp -v ${CMAKE_SOURCE_DIR}/configs/schema/*.json ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/schema
            COMMAND cp -v ${CMAKE_SOURCE_DIR}/configs/*.dat ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw
            COMMAND if [ -e dvm-firmware_f4.elf ]\; then cp -v dvm-firmware_f4.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4.bin ]\; then cp -v dvm-firmware_f4.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-pog.elf ]\; then cp -v dvm-firmware_f4-pog.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-pog.bin ]\; then cp -v dvm-firmware_f4-pog.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_eda.elf ]\; then cp -v dvm-firmware_eda.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_eda.bin ]\; then cp -v dvm-firmware_eda.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-dvmv1.elf ]\; then cp -v dvm-firmware_f4-dvmv1.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-dvmv1.bin ]\; then cp -v dvm-firmware_f4-dvmv1.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_due.elf ]\; then cp -v dvm-firmware_due.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_due.bin ]\; then cp -v dvm-firmware_due.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware-hs_f1.elf ]\; then cp -v dvm-firmware-hs_f1.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware-hs_f1.bin ]\; then cp -v dvm-firmware-hs_f1.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e DVM-V24-stm32f103.elf ]\; then cp -v DVM-V24-stm32f103.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e DVM-V24-stm32f103.bin ]\; then cp -v DVM-V24-stm32f103.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND cd ${CMAKE_INSTALL_PREFIX_TARBALL} && tar czvf ../dvmhost_${CPACK_DEBIAN_PACKAGE_VERSION}_${ARCH}.tar.gz *
            COMMAND rm -rf ${CMAKE_INSTALL_PREFIX_TARBALL})
    else()
        add_custom_target(tarball_notools
            COMMAND rm -rf ${CMAKE_INSTALL_PREFIX_TARBALL}
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/log
            COMMAND touch ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/log/INCLUDE_DIRECTORY
            COMMAND cp -v dvmhost ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v dvmcmd ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v dvmfne ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v dvmbridge ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/bin
            COMMAND cp -v ${CMAKE_SOURCE_DIR}/configs/*.yml ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/schema
            COMMAND cp -v ${CMAKE_SOURCE_DIR}/configs/schema/*.json ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/schema
            COMMAND cp -v ${CMAKE_SOURCE_DIR}/configs/*.dat ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm
            COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw
            COMMAND if [ -e dvm-firmware_f4.elf ]\; then cp -v dvm-firmware_f4.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4.bin ]\; then cp -v dvm-firmware_f4.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-pog.elf ]\; then cp -v dvm-firmware_f4-pog.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-pog.bin ]\; then cp -v dvm-firmware_f4-pog.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_eda.elf ]\; then cp -v dvm-firmware_eda.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_eda.bin ]\; then cp -v dvm-firmware_eda.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-dvmv1.elf ]\; then cp -v dvm-firmware_f4-dvmv1.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_f4-dvmv1.bin ]\; then cp -v dvm-firmware_f4-dvmv1.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_due.elf ]\; then cp -v dvm-firmware_due.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware_due.bin ]\; then cp -v dvm-firmware_due.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware-hs_f1.elf ]\; then cp -v dvm-firmware-hs_f1.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e dvm-firmware-hs_f1.bin ]\; then cp -v dvm-firmware-hs_f1.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e DVM-V24-stm32f103.elf ]\; then cp -v DVM-V24-stm32f103.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND if [ -e DVM-V24-stm32f103.bin ]\; then cp -v DVM-V24-stm32f103.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
            COMMAND cd ${CMAKE_INSTALL_PREFIX_TARBALL} && tar czvf ../dvmhost_${CPACK_DEBIAN_PACKAGE_VERSION}_${ARCH}.tar.gz *
            COMMAND rm -rf ${CMAKE_INSTALL_PREFIX_TARBALL})
    endif (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))
endif (NOT TARGET tarball_notools)

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
    COMMAND mkdir -p ${CMAKE_LEGACY_INSTALL_PREFIX}/schema
    COMMAND install -m 755 dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/bin
    COMMAND install -m 755 dvmcmd ${CMAKE_LEGACY_INSTALL_PREFIX}/bin
    COMMAND install -m 755 dvmmon ${CMAKE_LEGACY_INSTALL_PREFIX}/bin
    COMMAND install -m 755 sysview ${CMAKE_LEGACY_INSTALL_PREFIX}/bin
    COMMAND install -m 755 tged ${CMAKE_LEGACY_INSTALL_PREFIX}/bin
    COMMAND install -m 755 peered ${CMAKE_LEGACY_INSTALL_PREFIX}/bin
    COMMAND install -m 755 dvmfne ${CMAKE_LEGACY_INSTALL_PREFIX}/bin
    COMMAND install -m 755 dvmbridge ${CMAKE_LEGACY_INSTALL_PREFIX}/bin
    COMMAND install -m 644 ${CMAKE_SOURCE_DIR}/configs/config.example.yml ${CMAKE_LEGACY_INSTALL_PREFIX}/config.example.yml
    COMMAND install -m 644 ${CMAKE_SOURCE_DIR}/configs/fne-config.example.yml ${CMAKE_LEGACY_INSTALL_PREFIX}/fne-config.example.yml
    COMMAND install -m 644 ${CMAKE_SOURCE_DIR}/configs/fne-sysview.example.yml ${CMAKE_LEGACY_INSTALL_PREFIX}/fne-sysview.example.yml
    COMMAND install -m 644 ${CMAKE_SOURCE_DIR}/configs/monitor-config.example.yml ${CMAKE_LEGACY_INSTALL_PREFIX}/monitor-config.example.yml
    COMMAND install -m 644 ${CMAKE_SOURCE_DIR}/configs/iden_table.example.dat ${CMAKE_LEGACY_INSTALL_PREFIX}/iden_table.example.dat
    COMMAND install -m 644 ${CMAKE_SOURCE_DIR}/configs/RSSI.dat ${CMAKE_LEGACY_INSTALL_PREFIX}/RSSI.dat
    COMMAND install -m 644 ${CMAKE_SOURCE_DIR}/configs/rid_acl.example.dat ${CMAKE_LEGACY_INSTALL_PREFIX}/rid_acl.dat
    COMMAND install -m 644 ${CMAKE_SOURCE_DIR}/configs/talkgroup_rules.example.yml ${CMAKE_LEGACY_INSTALL_PREFIX}/talkgroup_rules.example.yml
    COMMAND install -m 644 ${CMAKE_SOURCE_DIR}/configs/bridge-config.example.yml ${CMAKE_LEGACY_INSTALL_PREFIX}/bridge-config.example.yml
    COMMAND install -m 644 ${CMAKE_SOURCE_DIR}/configs/schema/talkgroup_rules.yaml_schema.json ${CMAKE_LEGACY_INSTALL_PREFIX}/schema/talkgroup_rules.yaml_schema.json
    COMMAND install -m 755 ${CMAKE_SOURCE_DIR}/tools/start-dvm.sh ${CMAKE_LEGACY_INSTALL_PREFIX}
    COMMAND install -m 755 ${CMAKE_SOURCE_DIR}/tools/stop-dvm.sh ${CMAKE_LEGACY_INSTALL_PREFIX}
    COMMAND install -m 755 ${CMAKE_SOURCE_DIR}/tools/dvm-watchdog.sh ${CMAKE_LEGACY_INSTALL_PREFIX}
    COMMAND install -m 755 ${CMAKE_SOURCE_DIR}/tools/stop-watchdog.sh ${CMAKE_LEGACY_INSTALL_PREFIX}
    COMMAND install -m 755 ${CMAKE_SOURCE_DIR}/tools/fne-watchdog.sh ${CMAKE_LEGACY_INSTALL_PREFIX}
    COMMAND install -m 755 ${CMAKE_SOURCE_DIR}/tools/start-dvm-fne.sh ${CMAKE_LEGACY_INSTALL_PREFIX}
    COMMAND mkdir -p ${CMAKE_LEGACY_INSTALL_PREFIX}/fw
    COMMAND if [ -e dvm-firmware_f4.elf ]\; then install -m 644 dvm-firmware_f4.elf ${CMAKE_LEGACY_INSTALL_PREFIX}/fw/dvm-firmware_f4.elf\; fi
    COMMAND if [ -e dvm-firmware_f4.bin ]\; then install -m 644 dvm-firmware_f4.bin ${CMAKE_LEGACY_INSTALL_PREFIX}/fw/dvm-firmware_f4.bin\; fi
    COMMAND if [ -e dvm-firmware_f4-pog.elf ]\; then install -m 644 dvm-firmware_f4-pog.elf ${CMAKE_LEGACY_INSTALL_PREFIX}/fw/dvm-firmware_f4-pog.elf\; fi
    COMMAND if [ -e dvm-firmware_f4-pog.bin ]\; then install -m 644 dvm-firmware_f4-pog.bin ${CMAKE_LEGACY_INSTALL_PREFIX}/fwdvm-firmware_f4-pog.bin\; fi
    COMMAND if [ -e dvm-firmware_eda.elf ]\; then install -m 644 dvm-firmware_eda.elf ${CMAKE_LEGACY_INSTALL_PREFIX}/fw/dvm-firmware_eda.elf\; fi
    COMMAND if [ -e dvm-firmware_eda.bin ]\; then install -m 644 dvm-firmware_eda.bin ${CMAKE_LEGACY_INSTALL_PREFIX}/fw/dvm-firmware_eda.bin\; fi
    COMMAND if [ -e dvm-firmware_f4-dvmv1.elf ]\; then install -m 644 dvm-firmware_f4-dvmv1.elf ${CMAKE_LEGACY_INSTALL_PREFIX}/fw/dvm-firmware_f4-dvmv1.elf\; fi
    COMMAND if [ -e dvm-firmware_f4-dvmv1.bin ]\; then install -m 644 dvm-firmware_f4-dvmv1.bin ${CMAKE_LEGACY_INSTALL_PREFIX}/fw/dvm-firmware_f4-dvmv1.bin\; fi
    COMMAND if [ -e dvm-firmware_due.elf ]\; then install -m 644 dvm-firmware_due.elf ${CMAKE_LEGACY_INSTALL_PREFIX}/fw/dvm-firmware_due.elf\; fi
    COMMAND if [ -e dvm-firmware_due.bin ]\; then install -m 644 dvm-firmware_due.bin ${CMAKE_LEGACY_INSTALL_PREFIX}/fw/dvm-firmware_due.bin\; fi
    COMMAND if [ -e dvm-firmware-hs_f1.elf ]\; then install -m 644 dvm-firmware-hs_f1.elf ${CMAKE_LEGACY_INSTALL_PREFIX}/fw/dvm-firmware-hs_f1.elf\; fi
    COMMAND if [ -e dvm-firmware-hs_f1.bin ]\; then install -m 644 dvm-firmware-hs_f1.bin ${CMAKE_LEGACY_INSTALL_PREFIX}/fw/dvm-firmware-hs_f1.bin\; fi
    COMMAND if [ -e DVM-V24-stm32f103.elf ]\; then cp -v DVM-V24-stm32f103.elf ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi
    COMMAND if [ -e DVM-V24-stm32f103.bin ]\; then cp -v DVM-V24-stm32f103.bin ${CMAKE_INSTALL_PREFIX_TARBALL}/dvm/fw\; fi)

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
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/fne-sysview.example.yml
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/monitor-config.example.yml
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/iden_table.example.dat
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/RSSI.dat
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/rid_acl.example.dat
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/talkgroup_rules.example.yml
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/bridge-config.example.yml
    COMMAND chown dvmhost:dvmhost ${CMAKE_LEGACY_INSTALL_PREFIX}/log
    COMMAND cp ../linux/dvmhost.service /lib/systemd/system/
    COMMAND bash \"-c\" \"sed -i 's/\\\\/usr\\\\/local\\\\/bin/\\\\/opt\\\\/dvm\\\\/bin/' /lib/systemd/system/dvmhost.service\"
    COMMAND bash \"-c\" \"sed -i 's/\\\\/usr\\\\/local\\\\/etc/\\\\/opt\\\\/dvm/' /lib/systemd/system/dvmhost.service\")

#
# Firmware compilation
#
add_custom_target(dvmfw
    DEPENDS dvmfw-stm32f4 dvmfw-stm32f4-pog dvmfw-stm32f4-eda dvmfw-stm32f4-dvmv1 dvmfw-stm32fx dvmfw-v24
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/fw/modem
)
add_custom_target(dvmfw-clean
    DEPENDS dvmfw-stm32f4-clean dvmfw-stm32fx-clean dvmfw-v24-clean
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/fw/modem
)

add_custom_target(dvmfw-stm32f4
    COMMAND make -f Makefile.STM32F4 clean
    COMMAND make -f Makefile.STM32F4
    COMMAND cp dvm-firmware_f4.elf ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND cp dvm-firmware_f4.bin ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/fw/modem
)
add_custom_target(dvmfw-stm32f4-pog
    COMMAND make -f Makefile.STM32F4_POG clean
    COMMAND make -f Makefile.STM32F4_POG
    COMMAND cp dvm-firmware_f4-pog.elf ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND cp dvm-firmware_f4-pog.bin ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/fw/modem
)
add_custom_target(dvmfw-stm32f4-eda
    COMMAND make -f Makefile.STM32F4_EDA clean
    COMMAND make -f Makefile.STM32F4_EDA
    COMMAND cp dvm-firmware_eda.elf ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND cp dvm-firmware_eda.bin ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/fw/modem
)
add_custom_target(dvmfw-stm32f4-dvmv1
    COMMAND make -f Makefile.STM32F4_DVMV1 clean
    COMMAND make -f Makefile.STM32F4_DVMV1
    COMMAND cp dvm-firmware_f4-dvmv1.elf ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND cp dvm-firmware_f4-dvmv1.bin ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/fw/modem
)
add_custom_target(dvmfw-sam3x8
    COMMAND make -f Makefile.SAM3X8_DUE clean
    COMMAND make -f Makefile.SAM3X8_DUE
    COMMAND cp dvm-firmware_due.elf ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND cp dvm-firmware_due.bin ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/fw/modem
)
add_custom_target(dvmfw-stm32f4-clean
    COMMAND make -f Makefile clean
    COMMAND rm -f ${CMAKE_CURRENT_BINARY_DIR}/dvm-firmware*.elf
    COMMAND rm -f ${CMAKE_CURRENT_BINARY_DIR}/dvm-firmware*.bin
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/fw/modem
)

add_custom_target(dvmfw-stm32fx
    COMMAND make -f Makefile.STM32FX clean
    COMMAND make -f Makefile.STM32FX mmdvm-hs-hat-dual
    COMMAND cp dvm-firmware-hs_f1.elf ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND cp dvm-firmware-hs_f1.bin ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/fw/hotspot
)
add_custom_target(dvmfw-stm32fx-clean
    COMMAND make -f Makefile clean
    COMMAND rm -f ${CMAKE_CURRENT_BINARY_DIR}/dvm-firmware*.elf
    COMMAND rm -f ${CMAKE_CURRENT_BINARY_DIR}/dvm-firmware*.bin
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/fw/hotspot
)

add_custom_target(dvmfw-v24
    COMMAND make -f Makefile
    COMMAND cp build/DVM-V24-stm32f103.elf ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND cp build/DVM-V24-stm32f103.bin ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/fw/v24/fw
)
add_custom_target(dvmfw-v24-clean
    COMMAND make -f Makefile clean
    COMMAND rm -rf ${CMAKE_CURRENT_SOURCE_DIR}/src/fw/v24/fw/Build
    COMMAND rm -f ${CMAKE_CURRENT_BINARY_DIR}/DVM-V24*
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/fw/v24/fw
)
