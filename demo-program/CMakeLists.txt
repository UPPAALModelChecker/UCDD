file(GLOB demo-program_source "*.c" "*.cpp" "*.h")

#add_library(demo-program ${demo-program_source} )


if (CMAKE_SYSTEM_NAME STREQUAL Windows)
  target_link_libraries(base udebug iphlpapi psapi)
else()
  target_link_libraries(base udebug)
endif()

add_executable(demo-program demo-program.cpp)
target_link_libraries(demo-program cdd dbm hash)


