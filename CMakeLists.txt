cmake_minimum_required(VERSION 3.5)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "./cmake")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -std=c++14")

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

set(EXAMPLE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/examples)

function(build_singlefile_example SOURCE)
	# use current directory of project as prefix (e.g. 01-init)
	get_filename_component(target_prefix ${CMAKE_CURRENT_SOURCE_DIR} NAME)
	string(REPLACE " " "_" target_prefix ${target_prefix})

	# use source file's name w/o extension as name (e.g. main)
	get_filename_component(target_shortname ${SOURCE} NAME_WE)

	# combine into full target name (01-init-main)
	set(TARGET ${target_prefix}-${target_shortname})

	add_executable(${TARGET} ${SOURCE})
	target_link_libraries(${TARGET} ${CURSES_LIBRARY} ${ADDITIONAL_LIBRARIES})
endfunction()

build_singlefile_example(main.cpp)
#build_singlefile_example(square.cpp)
#build_singlefile_example(line.cpp)


