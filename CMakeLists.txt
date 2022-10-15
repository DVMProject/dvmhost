cmake_minimum_required(VERSION 3.18.4)

# dvmhost
file(GLOB dvmhost_SRC
    # DMR module
    "dmr/*.h"
    "dmr/*.cpp"
    "dmr/acl/*.h"
    "dmr/acl/*.cpp"
    "dmr/data/*.h"
    "dmr/data/*.cpp"
    "dmr/edac/*.h"
    "dmr/edac/*.cpp"
    "dmr/lc/*.h"
    "dmr/lc/*.cpp"
    "dmr/lc/csbk/*.h"
    "dmr/lc/csbk/*.cpp"
    "dmr/packet*.h"
    "dmr/packet/*.cpp"

    # P25 module
    "p25/*.h"
    "p25/*.cpp"
    "p25/acl/*.h"
    "p25/acl/*.cpp"
    "p25/data/*.h"
    "p25/data/*.cpp"
    "p25/dfsi/*.h"
    "p25/dfsi/*.cpp"
    "p25/dfsi/packet/*.h"
    "p25/dfsi/packet/*.cpp"
    "p25/edac/*.h"
    "p25/edac/*.cpp"
    "p25/lc/*.h"
    "p25/lc/*.cpp"
    "p25/lc/tsbk/*.h"
    "p25/lc/tsbk/*.cpp"
    "p25/lookups/*.h"
    "p25/lookups/*.cpp"
    "p25/packet/*.h"
    "p25/packet/*.cpp"

    # NXDN module
    "nxdn/*.h"
    "nxdn/*.cpp"
    "nxdn/acl/*.h"
    "nxdn/acl/*.cpp"
    "nxdn/channel/*.h"
    "nxdn/channel/*.cpp"
    "nxdn/lc/*.h"
    "nxdn/lc/*.cpp"
    "nxdn/packet/*.h"
    "nxdn/packet/*.cpp"

    # Core
    "edac/*.h"
    "edac/*.cpp"
    "host/*.h"
    "host/*.cpp"
    "host/calibrate/*.h"
    "host/calibrate/*.cpp"
    "host/setup/*.h"
    "host/setup/*.cpp"
    "lookups/*.h"
    "lookups/*.cpp"
    "modem/*.h"
    "modem/*.cpp"
    "modem/port/*.h"
    "modem/port/*.cpp"
    "network/*.h"
    "network/*.cpp"
    "network/json/*.h"
    "yaml/*.h"
    "yaml/*.cpp"
    "*.h"
    "*.cpp"
)

# dvmcmd
file(GLOB dvmcmd_SRC
    "network/UDPSocket.h"
    "network/UDPSocket.cpp"
    "remote/*.h"
    "remote/*.cpp"
    "edac/SHA256.h"
    "edac/SHA256.cpp"
    "Defines.h"
    "Thread.h"
    "Thread.cpp"
    "Log.h"
    "Log.cpp"
    "Utils.h"
    "Utils.cpp"
)

# dvmcmd
file(GLOB dvmtests_SRC
    "tests/nulltest.cpp"
)

# digital mode options
option(ENABLE_DMR "Enable DMR Digtial Mode" on)
option(ENABLE_P25 "Enable P25 Digital Mode" on)
option(ENABLE_NXDN "Enable NXDN Digital Mode" off)
option(ENABLE_DFSI_SUPPORT "Enable P25 DFSI Transport Support" off)

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

# debug options
option(DEBUG_DMR_PDU_DATA "" off)
option(DEBUG_CRC "" off)
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
option(DEBUG_P25_DFSI "" off)
option(DEBUG_RINGBUFFER "" off)

if (DEBUG_DMR_PDU_DATA)
    add_definitions(-DDEBUG_DMR_PDU_DATA)
endif (DEBUG_DMR_PDU_DATA)
if (DEBUG_CRC)
    add_definitions(-DDEBUG_CRC)
endif (DEBUG_CRC)
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
if (DEBUG_P25_DFSI)
    add_definitions(-DDEBUG_P25_DFSI)
endif (DEBUG_P25_DFSI)
if (DEBUG_RINGBUFFER)
    add_definitions(-DDEBUG_RINGBUFFER)
endif (DEBUG_RINGBUFFER)

# cross-compile options
option(CROSS_COMPILE_ARM "Cross-compile for 32-bit ARM" off)
option(CROSS_COMPILE_AARCH64 "Cross-compile for 64-bit ARM" off)
option(CROSS_COMPILE_RPI_ARM "Cross-compile for (old RPi) 32-bit ARM" off)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

message(CHECK_START "Cross compiling for 32-bit ARM")
if (CROSS_COMPILE_ARM)
    set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
    set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
    message(CHECK_PASS "yes")
else ()
    message(CHECK_PASS "no")
endif (CROSS_COMPILE_ARM)
message(CHECK_START "Cross compiling for 64-bit ARM")
if (CROSS_COMPILE_AARCH64)
    set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
    set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
    message(CHECK_PASS "yes")
else ()
    message(CHECK_PASS "no")
endif (CROSS_COMPILE_AARCH64)
message(CHECK_START "Cross compiling for (old RPi) 32-bit ARM")
if (CROSS_COMPILE_RPI_ARM)
    set(CMAKE_C_COMPILER /opt/tools/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc)
    set(CMAKE_CXX_COMPILER /opt/tools/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++)
    message(CHECK_PASS "yes")
else ()
    message(CHECK_PASS "no")
endif (CROSS_COMPILE_RPI_ARM)

set(THREADS_PREFER_PTHREAD_FLAG ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY .)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3 -Wall -std=c++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -Wall -std=c++11")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g -O3 -Wall -std=c++11 -s")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O3 -Wall -std=c++11 -s")

set(GIT_VER "")
set(GIT_VER_HASH "")
execute_process(COMMAND git describe --abbrev=8 --dirty --always --tags WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_VER OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND git describe --abbrev=8 --always --tags WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_VER_HASH OUTPUT_STRIP_TRAILING_WHITESPACE)

add_definitions(-D__GIT_VER__="${GIT_VER}")
add_definitions(-D__GIT_VER_HASH__="${GIT_VER_HASH}")

project(dvmhost)
find_package(Threads REQUIRED)
add_executable(dvmhost ${dvmhost_SRC})
target_include_directories(dvmhost PRIVATE .)
target_link_libraries(dvmhost PRIVATE Threads::Threads util)

project(dvmcmd)
find_package(Threads REQUIRED)
add_executable(dvmcmd ${dvmcmd_SRC})
target_link_libraries(dvmcmd PRIVATE Threads::Threads)
target_include_directories(dvmcmd PRIVATE .)

project(dvmtest)
Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.0.1 # or a later release
)

FetchContent_MakeAvailable(Catch2)
find_package(Threads REQUIRED)
add_executable(dvmtests ${dvmtests_SRC})
target_link_libraries(dvmtests PRIVATE Catch2::Catch2WithMain Threads::Threads util)
target_include_directories(dvmtests PRIVATE .)
