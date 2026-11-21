# SPDX-License-Identifier: GPL-2.0-only
#/*
# * Digital Voice Modem - CMake Build System
# * GPLv2 Open Source. Use is subject to license terms.
# * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
# *
# *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
# *
# */

#
## callsim
#
include(tools/callsim/CMakeLists.txt)
add_executable(callsim ${common_INCLUDE} ${callSim_SRC})
target_link_libraries(callsim PRIVATE common ${OPENSSL_LIBRARIES} ${LIBDW_LIBRARY} asio::asio Threads::Threads)
target_include_directories(callsim PRIVATE ${OPENSSL_INCLUDE_DIR} ${LIBDW_INCLUDE_DIR} src src/host tools/callsim src)

#
# TSBK/CSBK decoder
#
include(tools/tsbkdecode/CMakeLists.txt)
add_executable(tsbkdecode ${common_INCLUDE} ${tsbkDecode_SRC})
target_link_libraries(tsbkdecode PRIVATE common ${OPENSSL_LIBRARIES} ${LIBDW_LIBRARY} asio::asio Threads::Threads)
target_include_directories(tsbkdecode PRIVATE ${OPENSSL_INCLUDE_DIR} ${LIBDW_INCLUDE_DIR} tools/tsbkdecode src)
