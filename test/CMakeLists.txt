# cdd tests...

set(libs UCDD UDBM UUtils::udebug UUtils::hash UUtils::base)

if (TESTING)
    file(GLOB test_sources "test_*.c" "test_*.cpp")

    foreach(source ${test_sources})
        get_filename_component(test_target ${source} NAME_WE)
        add_executable(${test_target} ${source})
        target_link_libraries(${test_target} PRIVATE ${libs})
        add_test(NAME ${test_target} COMMAND ${test_target})
        set_tests_properties(${test_target} PROPERTIES TIMEOUT 180) # "10": 102s on Linux32, 97s on Win32, 27s on Win64
    endforeach()
endif()
