# DVM Host Express Makefile
# An express Makefile for easily creating binaries for various architectures.
# Author: K4YT3X <i@k4yt3x.com>

# This Makefile helps building the dvmcmd and dvmhost binaries for all supported architectures.
# Built binaries will be saved to build/${ARCH}. E.g., The binaries built with `make aarch64`
# will be saved to build/aarch64.

all: prepare amd64 arm aarch64 armhf
	@echo 'All builds completed successfully'

amd64:
	@echo 'Compiling for AMD64'
	mkdir -p "build/$@" && cd "build/$@" \
		&& cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-s" -DCMAKE_CXX_FLAGS="-s" ../.. \
		&& make -j $(nproc)
	@echo 'Successfully compiled for AMD64'

arm:
	@echo 'Cross-Compiling for ARM'
	mkdir -p "build/$@" && cd "build/$@" \
		&& cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-s" -DCMAKE_CXX_FLAGS="-s" \
			-DCROSS_COMPILE_ARM=1 ../.. \
		&& make -j $(nproc)
	@echo 'Successfully compiled for ARM'

aarch64:
	@echo 'Cross-Compiling for AARCH64'
	mkdir -p "build/$@" && cd "build/$@" \
		&& cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-s" -DCMAKE_CXX_FLAGS="-s" \
			-DCROSS_COMPILE_AARCH64=1 ../.. \
		&& make -j $(nproc)
	@echo 'Successfully compiled for AARCH64'

armhf:
	@echo 'Cross-Compiling for ARMHF'
	mkdir -p "build/$@" && cd "build/$@" \
		&& cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-s" -DCMAKE_CXX_FLAGS="-s" \
			-DCROSS_COMPILE_RPI_ARM=1 ../.. \
		&& make -j $(nproc)
	@echo 'Successfully compiled for ARMHF'

clean:
	@echo 'Removing all temporary files'
	git clean -ffxd

export_compile_commands:
	@echo 'Exporting CMake compile commands'
	cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 .
	git checkout HEAD -- Makefile

prepare:
	# if the system is Debian
	grep 'ID_LIKE=debian' /etc/os-release > /dev/null 2>&1 \
		&& echo 'Preparing dependencies for Debian' \
		&& export DEBIAN_FRONTEND=noninteractive \
		&& apt-get update \
		&& apt-get install -y git build-essential cmake libasio-dev \
			g++-arm-linux-gnueabihf gcc-arm-linux-gnueabihf g++-aarch64-linux-gnu
	# mark directory safe for Git if running in container
	[ ! -z "${container}" ] && git config --global --add safe.directory \
		"$(abspath $(dir $(lastword $(MAKEFILE_LIST))))"
