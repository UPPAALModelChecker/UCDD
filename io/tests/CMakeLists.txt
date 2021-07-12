set(libs io base udebug)

if(WIN32)
  list(APPEND libs psapi ws2_32 iphlpapi)
endif(WIN32)

if (FALSE) # disable outdated license scheme

file(GLOB test_sources "test_*.c" "test_*.cpp")

foreach(source ${test_sources})
  get_filename_component(test_target ${source} NAME_WE)
  add_executable(${test_target} ${source})
  target_link_libraries(${test_target} ${libs})
endforeach()

add_test(NAME io_key     COMMAND test_key)
add_test(NAME io_license COMMAND test_license)

endif(FALSE)
