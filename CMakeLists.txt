cmake_minimum_required(VERSION 3.16.3)

project(
	SRB2RandoLogic
	LANGUAGES C CXX
	VERSION 1.0
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
	set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif(CCACHE_FOUND)

add_executable(srb2aplogic)

add_subdirectory(lib)
add_subdirectory(src)
