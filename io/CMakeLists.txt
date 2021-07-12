file(GLOB io_source "*.c" "*.cpp" "*.h")

add_library(io ${io_source})

add_subdirectory(tests)
