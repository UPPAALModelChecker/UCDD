cmake_minimum_required(VERSION 3.15)
project(UCDD VERSION 0.2.1 LANGUAGES CXX C)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(TESTING "Unit tests" OFF)
option(ASAN "Address Sanitizer" OFF)

cmake_policy(SET CMP0048 NEW) # project() command manages VERSION variables
set(CMAKE_CXX_STANDARD 17)
set(MULTI_TERMINAL 1)
CONFIGURE_FILE("src/config.h.cmake" "include/cdd/config.h")

set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${CMAKE_CURRENT_SOURCE_DIR}/libs")
find_package(xxHash 0.8.0 REQUIRED COMPONENTS xxhash PATHS ${CMAKE_CURRENT_SOURCE_DIR}/libs/xxHash)
find_package(UUtils 1.1.1 REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/libs/UUtils)
find_package(UDBM 2.0.11 REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/libs/UDBM)
if (TESTING)
    find_package(doctest 2.4.8 REQUIRED PATHS ${PROJECT_SOURCE_DIR}/libs/doctest)
endif(TESTING)

include_directories(
    PRIVATE
        # where the library itself will look for its internal headers
        ${CMAKE_BINARY_DIR}/include
    PUBLIC
        # where top-level project will look for the library's public headers
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        # where external projects will look for the library's public headers
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(STATIC)
    # The project does not use neither threads nor networking, but
    # wsock32, ws2_32 and winpthread seem to be important for self-contained static binary
    set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ -lwsock32 -lws2_32 ${CMAKE_CXX_STANDARD_LIBRARIES}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")
endif(STATIC)

add_subdirectory("src")

if(TESTING)
    enable_testing()
    add_subdirectory("test")
endif(TESTING)

if (ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif(ASAN)

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/UCDDConfigVersion.cmake VERSION ${PACKAGE_VERSION} COMPATIBILITY SameMajorVersion)

install(DIRECTORY include DESTINATION .)
install(TARGETS UCDD EXPORT UCDDConfig LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(EXPORT UCDDConfig DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UCDD)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/UCDDConfigVersion.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UCDD )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/cdd/config.h DESTINATION include/cdd/)
