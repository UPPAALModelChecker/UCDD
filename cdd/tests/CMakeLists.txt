# cdd tests...

set(libs cdd dbm udebug hash base)

if (TESTING)
  file(GLOB test_sources "test_*.c" "test_*.cpp")

  foreach(source ${test_sources})
    get_filename_component(test_target ${source} NAME_WE)
    add_executable(${test_target} ${source})
    target_link_libraries(${test_target} ${libs})
  endforeach()

  add_test(NAME cdd_bf_0  COMMAND test_bf  0)
  add_test(NAME cdd_bf_1  COMMAND test_bf  1)
  add_test(NAME cdd_bf_10 COMMAND test_bf 10)
  set_tests_properties(cdd_bf_10 PROPERTIES TIMEOUT 60)  # 49s

  add_test(NAME cdd_cdd_0  COMMAND test_cdd  0)
  add_test(NAME cdd_cdd_1  COMMAND test_cdd  1)
  add_test(NAME cdd_cdd_10 COMMAND test_cdd 10)
  set_tests_properties(cdd_cdd_10 PROPERTIES TIMEOUT 180) # 102s on Linux32, 97s on Win32, 27s on Win64

  if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	  set_tests_properties(
	    cdd_cdd_10  # assertion fail
	    PROPERTIES DISABLED TRUE)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	  set_tests_properties(
        #	  cdd_cdd_10
	    PROPERTIES DISABLED TRUE)
    endif()
  elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	  set_tests_properties(
	    cdd_cdd_10  # assertion fail
	    PROPERTIES DISABLED TRUE)
    endif()
  endif()
endif()
