#!/bin/bash

PLATFORM=macos

BUILD_TYPE=Release
BUILD_DIR=build-lib-$PLATFORM

FLAGS_BUILD_TYPE="-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
CMAKE_PLATFORM_FLAGS="${FLAGS_BUILD_TYPE} -DCMAKE_TOOLCHAIN_FILE=../cmake-config/toolchain.cmake -DPLATFORM=MACOS64"


if [ ! -d "$2" ] ; then
    mkdir xcode-build
fi

cd xcode-build
cmake $CMAKE_PLATFORM_FLAGS -DAS_LIBRARY:BOOL=YES -G Xcode ..