cmake_minimum_required(VERSION 3.19)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(TESTING OFF)

cmake_policy(SET CMP0048 NEW) # project() command manages VERSION variables
set(CMAKE_CXX_STANDARD 17)
set(MULTI_TERMINAL 1)
CONFIGURE_FILE("src/config.h.cmake" "include/cdd/config.h")

project(UCDD VERSION 0.2 LANGUAGES CXX C)
find_package(UUtils 1.1.0 REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/libs/UUtils)
find_package(UDBM 2.0.8 REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/libs/UDBM)

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
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

if(TESTING)
    enable_testing()
    add_subdirectory("test")
endif(TESTING)

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/UCDDConfigVersion.cmake VERSION ${PACKAGE_VERSION} COMPATIBILITY SameMajorVersion)

install(DIRECTORY include DESTINATION .)
install(TARGETS UCDD EXPORT UCDDConfig LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(EXPORT UCDDConfig DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UCDD)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/UCDDConfigVersion.cmake DESTINATION lib/cmake/UCDD )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/cdd/config.h DESTINATION include/cdd/)
