#!/usr/bin/env bash
set -eo pipefail

PROJECT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

if [ $# -eq 0 ]; then
    targets="x86_64-linux-libs-release x86_64-w64-mingw32-libs-release"
else
    targets="$@"
fi

function show_cmake_vars() {
    for var in CMAKE_TOOLCHAIN_FILE CMAKE_BUILD_TYPE CMAKE_PREFIX_PATH CMAKE_INSTALL_PREFIX \
               CMAKE_GENERATOR CMAKE_BUILD_PARALLEL_LEVEL; do
        echo "  $var=${!var:- (unset)}"
    done
}

for target in $targets ; do
    unset BUILD_EXTRA
    unset CMAKE_BUILD_TYPE
    case $target in
        x86_64-linux-gcc14*)
            PLATFORM=x86_64-linux-gcc14
            ;;
        linux64*|x86_64-linux*)
            PLATFORM=x86_64-linux
            ;;
        i686-linux-gcc14*)
            PLATFORM=i686-linux-gcc14
            ;;
        linux32*|i686-linux*)
            PLATFORM=i686-linux
            ;;
        win64*|x86_64-w64-mingw32*)
            PLATFORM=x86_64-w64-mingw32
            export WINEPATH=$("$PROJECT_DIR"/winepath-for $PLATFORM)
            ;;
        win32*|i686-w64-mingw32*)
            PLATFORM=i686-w64-mingw32
            export WINEPATH=$("$PROJECT_DIR"/winepath-for $PLATFORM)
            ;;
        darwin-brew-gcc14*)
            PLATFORM=darwin-brew-gcc14
            ;;
        darwin*)
            PLATFORM=darwin
            ;;
        x86_64-darwin-brew-gcc14*)
            PLATFORM=x86_64-darwin-brew-gcc14
            ;;
        macos*|x86_64-darwin*)
            PLATFORM=x86_64-darwin
            ;;
        *)
            echo "Unrecognized target platform: $target"
            exit 1
    esac
    export CMAKE_TOOLCHAIN_FILE="$PROJECT_DIR/cmake/toolchain/$PLATFORM.cmake"
    BUILD_DIR="build-$PLATFORM"

    case $target in
        *-libs*)
            CMAKE_BUILD_TYPE=Release ./getlibs.sh $PLATFORM
            BUILD_EXTRA="$BUILD_EXTRA -DFIND_FATAL=ON"
            export CMAKE_PREFIX_PATH="$PROJECT_DIR/local/$PLATFORM"
            ;;
    esac

    case $target in
        *-ubsan-*)
            BUILD_EXTRA="$BUILD_EXTRA -DUBSAN=ON"
            BUILD_DIR="${BUILD_DIR}-ubsan"
            ;;
    esac
    case $target in
        *-lsan-*)
            BUILD_EXTRA="$BUILD_EXTRA -DLSAN=ON"
            BUILD_DIR="${BUILD_DIR}-lsan"
            ;;
    esac
    case $target in
        *-asan-*)
            BUILD_EXTRA="$BUILD_EXTRA -DASAN=ON"
            BUILD_DIR="${BUILD_DIR}-asan"
            ;;
    esac
    case $target in
        *-debug)
            export CMAKE_BUILD_TYPE=Debug
            BUILD_DIR="${BUILD_DIR}-debug"
            ;;
        *-release)
            export CMAKE_BUILD_TYPE=Release
            BUILD_DIR="${BUILD_DIR}-release"
            ;;
        *)
            export CMAKE_BUILD_TYPE=Release
            BUILD_DIR="${BUILD_DIR}-release"
            echo "Unrecognized build type: $target, assuming $CMAKE_BUILD_TYPE"
    esac
    echo "Building $target${BUILD_EXTRA:+ with$BUILD_EXTRA} into $BUILD_DIR"
    show_cmake_vars
    cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" $BUILD_EXTRA
    cmake --build "$BUILD_DIR" --config $CMAKE_BUILD_TYPE
    ctest --test-dir "$BUILD_DIR" -C $CMAKE_BUILD_TYPE --output-on-failure
done
