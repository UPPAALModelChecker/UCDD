file(GLOB cdd_source "*.c" "*.cpp" "*.h")
add_library(UCDD STATIC ${cdd_source})
target_link_libraries(UCDD PUBLIC xxHash PRIVATE UDBM)
add_library(UCDD::UCDD ALIAS UCDD)

target_include_directories(UCDD
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include> # cdd/config.h
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

install(TARGETS UCDD xxHash EXPORT UCDDConfig FILE_SET xxhash_headers)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/cdd DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
