#!/usr/bin/env bash
#set -euxo pipefail
set -euo pipefail

if [ $# -eq 0 ] ; then
  echo "Script $0 compiles and installs dependent libraries for a set of targets specified as arguments."
  echo "Possible arguments:"
  toolchains=$(cd cmake/toolchain/ ; ls *.cmake)
  for toolchain in $toolchains ; do
    echo "  ${toolchain%%.cmake}"
  done
  echo "The script is sensitive to CMake environment variables like:"
  echo "   CMAKE_TOOLCHAIN_FILE CMAKE_BUILD_TYPE CMAKE_PREFIX_PATH"
  exit 1
else
  targets="$@"
fi

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -z "${CMAKE_BUILD_TYPE+x}" ]; then
    export CMAKE_BUILD_TYPE=Release
elif [ "$CMAKE_BUILD_TYPE" != Release ]; then
    echo "WARNING: building libs with CMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE"
fi

if [ -n "${CMAKE_TOOLCHAIN_FILE+x}" ]; then
    TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN_FILE"
fi

function show_cmake_vars() {
    for var in CMAKE_TOOLCHAIN_FILE CMAKE_BUILD_TYPE CMAKE_PREFIX_PATH CMAKE_INSTALL_PREFIX \
               CMAKE_GENERATOR CMAKE_BUILD_PARALLEL_LEVEL; do
        echo "  $var=${!var:- (unset)}"
    done
}

for target in $targets ; do
    echo "GETLIBS for $target"
    BUILD_TARGET=$target
    SOURCES="$PROJECT_DIR/local/sources"
    mkdir -p "$SOURCES"
    PREFIX="$PROJECT_DIR/local/${BUILD_TARGET}"
    # export CMAKE_PREFIX_PATH="$PREFIX"
    export CMAKE_INSTALL_PREFIX="$PREFIX"
    if [ -z "${TOOLCHAIN_FILE+x}" ] && [ -r "$PROJECT_DIR/cmake/toolchain/${target}.cmake" ] ; then
        export CMAKE_TOOLCHAIN_FILE="$PROJECT_DIR/cmake/toolchain/${target}.cmake"
    fi

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
        show_cmake_vars
        cmake -S "$SOURCE" -B "$BUILD" -DDOCTEST_WITH_TESTS=OFF -DDOCTEST_WITH_MAIN_IN_STATIC_LIB=ON
        cmake --build "$BUILD" --config $CMAKE_BUILD_TYPE
        cmake --install "$BUILD" --config $CMAKE_BUILD_TYPE --prefix "${CMAKE_INSTALL_PREFIX}"
        rm -Rf "$BUILD"
        rm -Rf "$SOURCE"
    fi

    ## XXHASH (in src/kernel.c)
    NAME=xxHash
    VERSION=0.8.3
    LIBRARY="${NAME}-${VERSION}"
    ARCHIVE="$LIBRARY.tgz"
    SHA256=aae608dfe8213dfd05d909a57718ef82f30722c392344583d3f39050c7f29a80
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
        show_cmake_vars
        cmake -S "$SOURCE/cmake_unofficial" -B "$BUILD" -DBUILD_SHARED_LIBS=OFF
        cmake --build "$BUILD" --config $CMAKE_BUILD_TYPE
        cmake --install "$BUILD" --config $CMAKE_BUILD_TYPE --prefix "${CMAKE_INSTALL_PREFIX}"
        rm -Rf "$BUILD"
        rm -Rf "$SOURCE"
    fi

    ## BOOST for UUtils
    NAME=boost
    VERSION=1.88.0
    LIBRARY="${NAME}-${VERSION}"
    ARCHIVE="${LIBRARY}-cmake.tar.xz"
    SHA256=f48b48390380cfb94a629872346e3a81370dc498896f16019ade727ab72eb1ec
    SOURCE="${SOURCES}/${LIBRARY}"
    BUILD="${PREFIX}/build-${LIBRARY}"
    if [ -r "${CMAKE_INSTALL_PREFIX}/include/boost/math/distributions/arcsine.hpp" ] ; then
        echo "$LIBRARY is already installed in $CMAKE_INSTALL_PREFIX"
    else
        pushd "$SOURCES"
        [ -r "${ARCHIVE}" ] || curl -sL "https://github.com/boostorg/boost/releases/download/${LIBRARY}/${ARCHIVE}" -o "${ARCHIVE}"
        if [ -n "$(command -v shasum)" ]; then
            echo "$SHA256  $ARCHIVE" | shasum -a256 --check -
        fi
        [ -d "${SOURCE}" ] || tar xf "${ARCHIVE}"
        popd
        echo "Building $LIBRARY in $BUILD from $SOURCE"
        show_cmake_vars
        cmake -S "$SOURCE" -B "$BUILD" -DBUILD_SHARED_LIBS=OFF \
          -DBOOST_INCLUDE_LIBRARIES="headers;math" -DBOOST_ENABLE_MPI=OFF -DBOOST_ENABLE_PYTHON=OFF \
          -DBOOST_RUNTIME_LINK=static -DBUILD_TESTING=OFF -DBOOST_INSTALL_LAYOUT=system
        cmake --build "$BUILD" --config $CMAKE_BUILD_TYPE
        cmake --install "$BUILD" --config $CMAKE_BUILD_TYPE --prefix "${CMAKE_INSTALL_PREFIX}"
        rm -Rf "$BUILD"
        rm -Rf "$SOURCE"
    fi

    # UUtils various low level Uppaal utilities
    NAME=UUtils
    VERSION=2.0.7
    LIBRARY="${NAME}-${VERSION}"
    ARCHIVE="${LIBRARY}.tar.gz"
    SHA256=52a823768398707eee42392182c320141662e74e6bd8eaac1a0eca22d8a27bcd
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
      show_cmake_vars
      cmake -S "$SOURCE" -B "$BUILD" -DUUtils_WITH_TESTS=OFF -DUUtils_WITH_BENCHMARKS=OFF
      cmake --build "$BUILD" --config $CMAKE_BUILD_TYPE
      cmake --install "$BUILD" --config $CMAKE_BUILD_TYPE --prefix="$CMAKE_INSTALL_PREFIX"
      rm -Rf "$BUILD"
      rm -Rf "$SOURCE"
    fi

    # UDBM Uppaal Difference Bound Matrix library
    NAME=UDBM
    VERSION=2.0.15
    LIBRARY="${NAME}-${VERSION}"
    ARCHIVE="${LIBRARY}.tar.gz"
    SHA256=56a630731f05ef8c2c7122ca2ca225f7608e1ff1efb4b980f0ee6bdcc388efa5
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
      show_cmake_vars
      cmake -S "$SOURCE" -B "$BUILD" -DUDBM_WITH_TESTS=OFF -DUDBM_CLANG_TIDY=OFF
      cmake --build "$BUILD" --config $CMAKE_BUILD_TYPE
      cmake --install "$BUILD" --config $CMAKE_BUILD_TYPE --prefix="$CMAKE_INSTALL_PREFIX"
      rm -Rf "$BUILD"
      rm -Rf "$SOURCE"
    fi
    echo "GETLIBS $target success!"
done
