#!/bin/bash

set -e

# OSNAME=`uname`

if [ `uname` == "Darwin" ]; then
    PLATFORM=macos
else
    PLATFORM=linux
fi




BUILD_DIR=build-$PLATFORM

FLAGS_BUILD_TYPE="-DCMAKE_BUILD_TYPE=Debug"
CMAKE_PLATFORM_FLAGS="${FLAGS_BUILD_TYPE} -DCMAKE_TOOLCHAIN_FILE=cmake-config/toolchain.cmake -DPLATFORM=MACOS64"

if [ ! -d "$BUILD_DIR" ]; then
  mkdir $BUILD_DIR
fi

pushd $BUILD_DIR

if [ ! -d "out" ]; then
  mkdir out
fi

pushd out

cmake $CMAKE_PLATFORM_FLAGS  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=./bin ../..
make -j8

popd
popd

echo BUILD SUCCESS!