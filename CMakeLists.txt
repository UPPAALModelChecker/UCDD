cmake_minimum_required(VERSION 3.15)
project(UCDD VERSION 0.2 LANGUAGES CXX C)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(TESTING OFF)

cmake_policy(SET CMP0048 NEW) # project() command manages VERSION variables
set(CMAKE_CXX_STANDARD 17)
set(MULTI_TERMINAL 1)
CONFIGURE_FILE("src/config.h.cmake" "include/cdd/config.h")

set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${CMAKE_CURRENT_SOURCE_DIR}/libs")
find_package(xxHash 0.8.0 REQUIRED COMPONENTS xxhash)
find_package(UUtils 1.1.1 REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/libs/UUtils)
find_package(UDBM 2.0.11 REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/libs/UDBM)

# Bit of a hack. UUtils requires xxhash, but can fetch it like this.
# But it can also find it in the system. If UDBM can find it in the system
# we assume that UUtils found it in the system, and all is linked properly
# If not, we also need to include it, such that we can find it when it's included
# in include/hash/compute.h
find_library(xxhash NAMES xxhash xxHash)
if(NOT xxhash)
    message("XXHash not found. Downloading via fetchContent")
    include(FetchContent)
    FetchContent_Declare(xxhashc
        GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
        GIT_TAG 35b0373c697b5f160d3db26b1cbb45a0d5ba788c
        )
    FetchContent_MakeAvailable(xxhashc)
    include_directories("${xxhashc_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/include")
else()
    include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
endif()

file(GLOB cdd_source "src/*.c" "src/*.cpp" "src/*.h")
add_library(UCDD STATIC)

target_sources(UCDD PRIVATE ${cdd_source})
target_include_directories(UCDD
    PRIVATE
        # where the library itself will look for its internal headers
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}/include
    PUBLIC
        # where top-level project will look for the library's public headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # where external projects will look for the library's public headers
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(UCDD UDBM)

if(STATIC)
	set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ -lwsock32 -lws2_32 ${CMAKE_CXX_STANDARD_LIBRARIES}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")
endif(STATIC)

if(TESTING)
    enable_testing()
    add_subdirectory("test")
endif(TESTING)

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/UCDDConfigVersion.cmake VERSION ${PACKAGE_VERSION} COMPATIBILITY SameMajorVersion)

install(DIRECTORY include DESTINATION .)
install(TARGETS UCDD EXPORT UCDDConfig LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(EXPORT UCDDConfig DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UCDD)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/UCDDConfigVersion.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UCDD )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/cdd/config.h DESTINATION include/cdd/)
