# SPDX-License-Identifier: GPL-2.0-only
#/*
# * Digital Voice Modem - CMake Build System
# * GPLv2 Open Source. Use is subject to license terms.
# * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
# *
# *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
# *
# */
include(src/CompilerOptions.cmake)

#
## common
#
include(src/common/CMakeLists.txt)
add_library(common STATIC ${common_SRC} ${common_INCLUDE})
if (COMPILE_WIN32)
    target_link_libraries(common PRIVATE asio::asio Threads::Threads)
else ()
    target_link_libraries(common PRIVATE asio::asio Threads::Threads util)
endif (COMPILE_WIN32)
target_include_directories(common PRIVATE src src/common)

#
## vocoder
#
include(src/vocoder/CMakeLists.txt)
add_library(vocoder STATIC ${vocoder_SRC} ${vocoder_INCLUDE})
target_include_directories(vocoder PRIVATE src src/vocoder)

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
    target_link_libraries(dvmhost PRIVATE common ${OPENSSL_LIBRARIES} asio::asio finalcut Threads::Threads util)
else ()
    if (COMPILE_WIN32)
        target_link_libraries(dvmhost PRIVATE common ${OPENSSL_LIBRARIES} asio::asio Threads::Threads)
    else ()
        target_link_libraries(dvmhost PRIVATE common ${OPENSSL_LIBRARIES} asio::asio Threads::Threads util)
    endif (COMPILE_WIN32)
endif (ENABLE_SETUP_TUI)
target_include_directories(dvmhost PRIVATE ${OPENSSL_INCLUDE_DIR} src src/host)

set(CPACK_SET_DESTDIR true)
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "dvmhost")
set(CPACK_DEBIAN_PACKAGE_NAME "dvmhost")

set(CPACK_PACKAGE_VENDOR "DVMProject")

set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "The DVM Host software provides the host computer implementation of a mixed-mode DMR, P25 and/or NXDN or dedicated-mode DMR, P25 or NXDN repeater system that talks to the actual modem hardware. The host software; is the portion of a complete Over-The-Air modem implementation that performs the data processing, decision making and FEC correction for a digital repeater.")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "DVMProject Authors")
set(CPACK_DEBIAN_PACKAGE_VERSION "R04Axx")
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
target_link_libraries(dvmfne PRIVATE common ${OPENSSL_LIBRARIES} asio::asio Threads::Threads)
target_include_directories(dvmfne PRIVATE ${OPENSSL_INCLUDE_DIR} src src/host src/fne)

#
## dvmmon
#
if (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))
    include(src/monitor/CMakeLists.txt)
    add_executable(dvmmon ${common_INCLUDE} ${dvmmon_SRC})
    target_link_libraries(dvmmon PRIVATE common ${OPENSSL_LIBRARIES} asio::asio finalcut Threads::Threads)
    target_include_directories(dvmmon PRIVATE ${OPENSSL_INCLUDE_DIR} src src/host src/monitor)
endif (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))

#
## sysview
#
if (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))
    include(src/sysview/CMakeLists.txt)
    add_executable(sysview ${common_INCLUDE} ${sysView_SRC})
    target_link_libraries(sysview PRIVATE common ${OPENSSL_LIBRARIES} asio::asio finalcut Threads::Threads)
    target_include_directories(sysview PRIVATE ${OPENSSL_INCLUDE_DIR} src src/host src/sysview)
endif (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))

#
## tged
#
if (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))
    include(src/tged/CMakeLists.txt)
    add_executable(tged ${common_INCLUDE} ${tged_SRC})
    target_link_libraries(tged PRIVATE common ${OPENSSL_LIBRARIES} asio::asio finalcut Threads::Threads)
    target_include_directories(tged PRIVATE ${OPENSSL_INCLUDE_DIR} src src/host src/tged)
endif (ENABLE_TUI_SUPPORT AND (NOT DISABLE_TUI_APPS))

#
## dvmcmd
#
include(src/remote/CMakeLists.txt)
add_executable(dvmcmd ${common_INCLUDE} ${dvmcmd_SRC})
target_link_libraries(dvmcmd PRIVATE common ${OPENSSL_LIBRARIES} asio::asio Threads::Threads)
target_include_directories(dvmcmd PRIVATE ${OPENSSL_INCLUDE_DIR} src src/remote)

#
## dvmbridge
#
include(src/bridge/CMakeLists.txt)
add_executable(dvmbridge ${common_INCLUDE} ${bridge_SRC})
if (COMPILE_WIN32)
    target_link_libraries(dvmbridge PRIVATE common vocoder ${OPENSSL_LIBRARIES} asio::asio Threads::Threads)
else ()
    target_link_libraries(dvmbridge PRIVATE common vocoder ${OPENSSL_LIBRARIES} dl asio::asio Threads::Threads)
endif (COMPILE_WIN32)
target_include_directories(dvmbridge PRIVATE ${OPENSSL_INCLUDE_DIR} src src/host src/bridge)
