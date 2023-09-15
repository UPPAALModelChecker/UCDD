#!/usr/bin/env bash
#set -euxo pipefail
set -euo pipefail

if [ $# -eq 0 ] ; then
  echo "Script $0 compiles and installs dependent libraries for a set of targets specified as arguments."
  echo "Possible arguments:"
  echo "  linux64 win64 macos64"
  echo "The script is sensitive to CMake environment variables like:"
  echo "  CMAKE_TOOLCHAIN_FILE CMAKE_BUILD_TYPE CMAKE_PREFIX_PATH"
  machine=$(uname -m)
  kernel=$(uname -s)
  targets=${machine,,}-${kernel,,}
  echo "Guessing target: $targets"
else
  targets="$@"
fi

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -z "${CMAKE_BUILD_TYPE+x}" ]; then
    export CMAKE_BUILD_TYPE=Release
elif [ "$CMAKE_BUILD_TYPE" != Release ]; then
    echo "WARNING: building libs with CMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE"
fi

for target in $targets ; do
    echo "GETLIBS for $target"
    BUILD_TARGET=$target
    SOURCES="$PROJECT_DIR/local/sources"
    mkdir -p "$SOURCES"
    PREFIX="$PROJECT_DIR/local/${BUILD_TARGET}"
    # export CMAKE_PREFIX_PATH="$PREFIX"
    export CMAKE_INSTALL_PREFIX="$PREFIX"

    ## DOCTEST
    NAME=doctest
    VERSION=2.4.11
    LIBRARY="${NAME}-${VERSION}"
    ARCHIVE="$LIBRARY.tgz"
    SHA256=632ed2c05a7f53fa961381497bf8069093f0d6628c5f26286161fbd32a560186
    SOURCE="${SOURCES}/$LIBRARY"
    BUILD="${PREFIX}/build-$LIBRARY"
    if [ -r "${CMAKE_INSTALL_PREFIX}/include/doctest/doctest.h" ]; then
        echo "$LIBRARY is already installed in $CMAKE_INSTALL_PREFIX"
    else
        pushd "${SOURCES}"
        [ -r "${ARCHIVE}" ] || curl -sL "https://github.com/doctest/doctest/archive/refs/tags/v$VERSION.tar.gz" -o "${ARCHIVE}"
        if [ -n "$(command -v sha256sum)" ]; then echo "$SHA256 $ARCHIVE" | sha256sum --check ; fi
        [ -d "${SOURCE}" ] || tar xf "${ARCHIVE}"
        popd
        echo "Building $LIBRARY in $BUILD from $SOURCE"
        echo "  CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE:-(unset)}"
        echo "  CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-(unset)}"
        echo "  CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH:-(unset)}"
        echo "  CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX:-(unset)}"
        cmake -S "$SOURCE" -B "$BUILD" -DDOCTEST_WITH_TESTS=OFF -DDOCTEST_WITH_MAIN_IN_STATIC_LIB=ON
        cmake --build "$BUILD" --config $CMAKE_BUILD_TYPE
        cmake --install "$BUILD" --config $CMAKE_BUILD_TYPE --prefix "${CMAKE_INSTALL_PREFIX}"
        rm -Rf "$BUILD"
        rm -Rf "$SOURCE"
    fi

    ## XXHASH (in src/kernel.c)
    NAME=xxHash
    VERSION=0.8.2
    LIBRARY="${NAME}-${VERSION}"
    ARCHIVE="$LIBRARY.tgz"
    SHA256=baee0c6afd4f03165de7a4e67988d16f0f2b257b51d0e3cb91909302a26a79c4
    SOURCE="${SOURCES}/$LIBRARY"
    BUILD="${PREFIX}/build-$LIBRARY"
    if [ -r "${CMAKE_INSTALL_PREFIX}/include/xxhash.h" ]; then
        echo "$LIBRARY is already installed in $CMAKE_INSTALL_PREFIX"
    else
        pushd "$SOURCES"
        [ -r "${ARCHIVE}" ] || curl -sL "https://github.com/Cyan4973/xxHash/archive/refs/tags/v$VERSION.tar.gz" -o "${ARCHIVE}"
        if [ -n "$(command -v sha256sum)" ]; then echo "$SHA256 $ARCHIVE" | sha256sum --check ; fi
        [ -d "$SOURCE" ] || tar xf "${ARCHIVE}"
        popd
        echo "Building $LIBRARY in $BUILD from $SOURCE"
        echo "  CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE:-(unset)}"
        echo "  CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-(unset)}"
        echo "  CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH:-(unset)}"
        echo "  CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX:-(unset)}"
        cmake -S "$SOURCE/cmake_unofficial" -B "$BUILD" -DBUILD_SHARED_LIBS=OFF
        cmake --build "$BUILD" --config $CMAKE_BUILD_TYPE
        cmake --install "$BUILD" --config $CMAKE_BUILD_TYPE --prefix "${CMAKE_INSTALL_PREFIX}"
        rm -Rf "$BUILD"
        rm -Rf "$SOURCE"
    fi

    # UUtils various low level Uppaal utilities
    NAME=UUtils
    VERSION=2.0.5
    LIBRARY="${NAME}-${VERSION}"
    ARCHIVE="${LIBRARY}.tar.gz"
    SHA256=e213f936af73344de071a7794233a328028045c08df58ac9c637a0e6a2ad7b3f
    SOURCE="${SOURCES}/${LIBRARY}"
    BUILD="${PREFIX}/build-${LIBRARY}"
    if [ -r "$PREFIX/include/base/Enumerator.h" ]; then
      echo "$LIBRARY is already installed in $PREFIX"
    else
      pushd "$SOURCES"
      #git clone -b dev-branch --single-branch --depth 1 https://github.com/UPPAALModelChecker/UUtils.git "$LIBRARY"
      [ -r "$ARCHIVE" ] || curl -sL "https://github.com/UPPAALModelChecker/UUtils/archive/refs/tags/v${VERSION}.tar.gz" -o "$ARCHIVE"
      if [ -n "$(command -v sha256sum)" ]; then echo "$SHA256 $ARCHIVE" | sha256sum --check ; fi
      [ -d "$SOURCE" ] || tar -xf "$ARCHIVE"
      popd
      echo "Building $LIBRARY in $BUILD"
      echo "  CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE:-(unset)}"
      echo "  CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-(unset)}"
      echo "  CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH:-(unset)}"
      echo "  CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX:-(unset)}"
      echo "  CMAKE_GENERATOR=${CMAKE_GENERATOR:-(unset)}"
      cmake -S "$SOURCE" -B "$BUILD" -DUUtils_WITH_TESTS=OFF -DUUtils_WITH_BENCHMARKS=OFF
      cmake --build "$BUILD" --config $CMAKE_BUILD_TYPE
      cmake --install "$BUILD" --config $CMAKE_BUILD_TYPE --prefix="$CMAKE_INSTALL_PREFIX"
      rm -Rf "$BUILD"
      rm -Rf "$SOURCE"
    fi

    # UDBM Uppaal Difference Bound Matrix library
    NAME=UDBM
    VERSION=2.0.13
    LIBRARY="${NAME}-${VERSION}"
    ARCHIVE="${LIBRARY}.tar.gz"
    SHA256=dd5d2b13997aae1669d6efca0901a5d44c323469d0fd945e518bffbfb058f414
    SOURCE="${SOURCES}/${LIBRARY}"
    BUILD="${PREFIX}/build-${LIBRARY}"
    if [ -r "$PREFIX/include/dbm/config.h" ]; then
      echo "$LIBRARY is already installed in $PREFIX"
    else
      pushd "$SOURCES"
      #git clone -b dev-branch --single-branch --depth 1 https://github.com/UPPAALModelChecker/UDBM.git "$LIBRARY"
      [ -r "$ARCHIVE" ] || curl -sL "https://github.com/UPPAALModelChecker/UDBM/archive/refs/tags/v${VERSION}.tar.gz" -o "$ARCHIVE"
      if [ -n "$(command -v sha256sum)" ]; then echo "$SHA256 $ARCHIVE" | sha256sum --check ; fi
      [ -d "$SOURCE" ] || tar -xf "$ARCHIVE"
      popd
      echo "Building $LIBRARY in $BUILD"
      echo "  CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE:-(unset)}"
      echo "  CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-(unset)}"
      echo "  CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH:-(unset)}"
      echo "  CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX:-(unset)}"
      echo "  CMAKE_GENERATOR=${CMAKE_GENERATOR:-(unset)}"
      cmake -S "$SOURCE" -B "$BUILD" -DUDBM_WITH_TESTS=OFF
      cmake --build "$BUILD" --config $CMAKE_BUILD_TYPE
      cmake --install "$BUILD" --config $CMAKE_BUILD_TYPE --prefix="$CMAKE_INSTALL_PREFIX"
      rm -Rf "$BUILD"
      rm -Rf "$SOURCE"
    fi

#    ## BOOST
#    NAME=boost
#    VERSION=1.83.0
#    LIBRARY="${NAME}-${VERSION}"
#    ARCHIVE="$LIBRARY.tar.xz"
#    SHA256=c5a0688e1f0c05f354bbd0b32244d36085d9ffc9f932e8a18983a9908096f614
#    SOURCE="${SOURCES}/$LIBRARY"
#    BUILD="${PREFIX}/build-$LIBRARY"
#    if [ -r "${CMAKE_INSTALL_PREFIX}/include/boost/math/distributions/arcsine.hpp" ] ; then
#        echo "$LIBRARY is already installed in $CMAKE_INSTALL_PREFIX"
#    else
#        pushd "$SOURCES"
#        [ -r "${ARCHIVE}" ] || curl -sL "https://github.com/boostorg/boost/releases/download/$LIBRARY/$LIBRARY.tar.xz" -o "${ARCHIVE}"
#        if [ -n "$(command -v sha256sum)" ]; then echo "$SHA256 $ARCHIVE" | sha256sum --check ; fi
#        [ -d "${SOURCE}" ] || tar xf "${ARCHIVE}"
#        popd
#        echo "Building $LIBRARY in $BUILD from $SOURCE"
#        echo "  CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE:-(unset)}"
#        echo "  CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-(unset)}"
#        echo "  CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH:-(unset)}"
#        echo "  CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX:-(unset)}"
#        cmake -S "$SOURCE" -B "$BUILD" -DBUILD_SHARED_LIBS=OFF \
#          -DBOOST_INCLUDE_LIBRARIES="headers;math" -DBOOST_ENABLE_MPI=OFF -DBOOST_ENABLE_PYTHON=OFF \
#          -DBOOST_RUNTIME_LINK=static -DBUILD_TESTING=OFF -DBOOST_USE_STATIC_LIBS=ON -DBOOST_USE_DEBUG_LIBS=ON \
#          -DBOOST_USE_RELEASE_LIBS=ON -DBOOST_USE_STATIC_RUNTIME=ON -DBOOST_INSTALL_LAYOUT=system -DBOOST_ENABLE_CMAKE=ON
#        cmake --build "$BUILD" --config $CMAKE_BUILD_TYPE
#        cmake --install "$BUILD" --config $CMAKE_BUILD_TYPE --prefix "${CMAKE_INSTALL_PREFIX}"
#        rm -Rf "$BUILD"
#        rm -Rf "$SOURCE"
#    fi
#
#    ## Google Benchmark
#    NAME=benchmark
#    VERSION=1.8.3 # v1.8.2 fails with "-lrt not found" on win64
#    LIBRARY="${NAME}-${VERSION}"
#    ARCHIVE="${LIBRARY}.tar.gz"
#    SHA256=6bc180a57d23d4d9515519f92b0c83d61b05b5bab188961f36ac7b06b0d9e9ce
#    SOURCE="${SOURCES}/${LIBRARY}"
#    BUILD="${PREFIX}/build-${LIBRARY}"
#    if [ -r "${CMAKE_INSTALL_PREFIX}/include/benchmark/benchmark.h" ] ; then
#        echo "$LIBRARY is already installed in $CMAKE_INSTALL_PREFIX"
#    else
#        pushd "$SOURCES"
#        [ -r "$ARCHIVE" ] || curl -sL "https://github.com/google/benchmark/archive/refs/tags/v${VERSION}.tar.gz" -o "$ARCHIVE"
#        if [ -n "$(command -v sha256sum)" ]; then echo "$SHA256 $ARCHIVE" | sha256sum --check ; fi
#        [ -d "$LIBRARY" ] || tar -xf "$ARCHIVE"
#        popd
#        echo "Building $LIBRARY in $BUILD from $SOURCE"
#        echo "  CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE:-(unset)}"
#        echo "  CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-(unset)}"
#        echo "  CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH:-(unset)}"
#        echo "  CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX:-(unset)}"
#        cmake -S "$SOURCE" -B "$BUILD" -DBUILD_SHARED_LIBS=OFF \
#          -DBENCHMARK_ENABLE_TESTING=OFF -DBENCHMARK_ENABLE_EXCEPTIONS=ON -DBENCHMARK_ENABLE_LTO=OFF \
#          -DBENCHMARK_USE_LIBCXX=OFF -DBENCHMARK_ENABLE_WERROR=ON -DBENCHMARK_FORCE_WERROR=OFF
#        cmake --build "$BUILD" --config $CMAKE_BUILD_TYPE
#        cmake --install "$BUILD" --config $CMAKE_BUILD_TYPE --prefix "${CMAKE_INSTALL_PREFIX}"
#        rm -Rf "$BUILD"
#        rm -Rf "$SOURCE"
#    fi
    echo "GETLIBS $target success!"
done
