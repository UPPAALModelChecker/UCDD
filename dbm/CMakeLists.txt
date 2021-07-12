set(DBM_VERSION "2.0.8" PARENT_SCOPE)
file(GLOB dbm_source "*.c" "*.cpp" "*.h")

add_library(dbm ${dbm_source} )
target_link_libraries(dbm base udebug hash)

add_subdirectory(tests)
