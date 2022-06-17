file(GLOB cdd_source "*.c" "*.cpp" "*.h")
add_library(UCDD STATIC)

target_sources(UCDD PRIVATE ${cdd_source})
target_link_libraries(UCDD UDBM)

if(STATIC)
	set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ -lwsock32 -lws2_32 ${CMAKE_CXX_STANDARD_LIBRARIES}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")
endif(STATIC)
