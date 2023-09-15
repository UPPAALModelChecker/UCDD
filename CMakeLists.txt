cmake_minimum_required(VERSION 3.15)
project(UCDD VERSION 0.2.2 LANGUAGES CXX C)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(UCDD_WITH_TESTS "UCDD Unit tests" ON)
option(FIND_FATAL "Stop upon find_package errors" OFF)
include(cmake/sanitizer.cmake)

cmake_policy(SET CMP0048 NEW) # project() command manages VERSION variables
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(MULTI_TERMINAL 1)
CONFIGURE_FILE("src/config.h.cmake" "include/cdd/config.h")

if (UCDD_WITH_TESTS)
  include(cmake/doctest.cmake)
  enable_testing()
endif (UCDD_WITH_TESTS)

include(cmake/xxhash.cmake)
include(cmake/UUtils.cmake)
include(cmake/UDBM.cmake)

if(STATIC)
    # The project does not use neither threads nor networking, but
    # wsock32, ws2_32 and winpthread seem to be important for self-contained static binary
    set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ -lwsock32 -lws2_32 ${CMAKE_CXX_STANDARD_LIBRARIES}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")
endif(STATIC)

add_subdirectory("src")

if (UCDD_WITH_TESTS)
    add_subdirectory("test")
endif(UCDD_WITH_TESTS)

write_basic_package_version_file(${PROJECT_BINARY_DIR}/UCDDConfigVersion.cmake VERSION ${PACKAGE_VERSION} COMPATIBILITY SameMajorVersion)

install(DIRECTORY include DESTINATION .)
install(EXPORT UCDDConfig DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UCDD)
install(FILES ${PROJECT_BINARY_DIR}/UCDDConfigVersion.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UCDD)
install(FILES ${PROJECT_BINARY_DIR}/include/cdd/config.h DESTINATION include/cdd/)
