set(libs UCDD UDBM UUtils::udebug UUtils::hash UUtils::base)

add_executable(test_cdd test_cdd.cpp)
target_link_libraries(test_cdd PRIVATE ${libs} doctest_with_main)
add_test(NAME test_cdd COMMAND test_cdd)
set_tests_properties(test_cdd PROPERTIES TIMEOUT 180) # "10": 102s on Linux32, 97s on Win32, 27s on Win64
