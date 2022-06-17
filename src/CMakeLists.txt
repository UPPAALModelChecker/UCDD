file(GLOB cdd_source "*.c" "*.cpp" "*.h")
add_library(UCDD STATIC)

target_sources(UCDD PRIVATE ${cdd_source})
target_link_libraries(UCDD UDBM)
