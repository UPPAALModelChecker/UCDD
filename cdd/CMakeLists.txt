file(GLOB cdd_source "*.c" "*.cpp" "*.h")

add_library(cdd ${cdd_source})

add_subdirectory(tests)
# list(REMOVE_ITEM MODULES cdd)
# target_link_libraries(cdd ${MODULES})
