#!/usr/bin/env bash
set -euo pipefail

# Cursed line that should also work on macos
# https://stackoverflow.com/questions/59895/how-can-i-get-the-source-directory-of-a-bash-script-from-within-the-script-itsel
SOURCE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
mkdir -p "$SOURCE_DIR/libs/sources";
CMAKE_PREFIX_PATH="$SOURCE_DIR/libs"

CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH"
if [ -z "${CMAKE_TOOLCHAIN_FILE+x}" ]; then
	echo "Not using a custom toolchain";
else
	echo "Using toolchain $CMAKE_TOOLCHAIN_FILE";
	CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE";
fi

cd "$SOURCE_DIR/libs/sources";

# DOCTEST
PREFIX="$CMAKE_PREFIX_PATH/doctest"
if [ ! -d "$PREFIX" ]; then
    wget -nv -N https://github.com/doctest/doctest/archive/refs/tags/v2.4.8.tar.gz
    tar -xf v2.4.8.tar.gz
    SRC_DIR="$SOURCE_DIR/libs/sources/doctest-2.4.8"
    BLD_DIR="$SRC_DIR/build"
    cmake $CMAKE_ARGS -DCMAKE_INSTALL_PREFIX="$PREFIX" -DBUILD_SHARED_LIBS=OFF "$SRC_DIR" -B "$BLD_DIR"
    cmake --build "$BLD_DIR" --config Release
    cmake --install "$BLD_DIR" --config Release
else
    echo "$PREFIX already exists, skipping"
fi

# XXHASH
PREFIX="$CMAKE_PREFIX_PATH/xxHash"
if [ ! -d "$PREFIX" ]; then
    wget -nv -N https://github.com/Cyan4973/xxHash/archive/refs/tags/v0.8.0.tar.gz
    tar -xf v0.8.0.tar.gz
    SRC_DIR="$SOURCE_DIR/libs/sources/xxHash-0.8.0"
    BLD_DIR="$SRC_DIR/build"
    cmake $CMAKE_ARGS -DCMAKE_INSTALL_PREFIX="$PREFIX" -DBUILD_SHARED_LIBS=OFF "$SRC_DIR/cmake_unofficial" -B "$BLD_DIR"
    cmake --build "$BLD_DIR" --config Release
    cmake --install "$BLD_DIR" --config Release
else
    echo "$PREFIX already exists, skipping"
fi

# UUtils
PREFIX="$CMAKE_PREFIX_PATH/UUtils"
if [ ! -d "$PREFIX" ]; then
    wget -nv -N https://github.com/UPPAALModelChecker/UUtils/archive/refs/tags/v1.1.1.tar.gz
    tar -xf v1.1.1.tar.gz
    SRC_DIR="$SOURCE_DIR/libs/sources/UUtils-1.1.1"
    BLD_DIR="$SRC_DIR/build"
    cmake $CMAKE_ARGS -DCMAKE_INSTALL_PREFIX="$PREFIX" "$SRC_DIR" -B "$BLD_DIR"
    cmake --build "$BLD_DIR" --config Release
    cmake --install "$BLD_DIR" --config Release
else
    echo "$PREFIX already exists, skipping"
fi

# UDBM
PREFIX="$CMAKE_PREFIX_PATH/UDBM"
if [ ! -d "$PREFIX" ]; then
    wget -nv -N https://github.com/UPPAALModelChecker/UDBM/archive/refs/tags/v2.0.11.tar.gz
    tar -xf v2.0.11.tar.gz
    SRC_DIR="$SOURCE_DIR/libs/sources/UDBM-2.0.11"
    BLD_DIR="$SRC_DIR/build"
    cmake $CMAKE_ARGS -DCMAKE_INSTALL_PREFIX="$PREFIX" "$SRC_DIR" -B "$BLD_DIR"
    cmake --build "$BLD_DIR" --config Release
    cmake --install "$BLD_DIR" --config Release
else
    echo "$PREFIX already exists, skipping"
fi
