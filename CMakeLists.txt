cmake_minimum_required(VERSION 3.5)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "./cmake")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

set(EXAMPLE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/examples)

add_executable(labyrinth 
  main.cpp
  )

target_link_libraries(labyrinth ${CURSES_LIBRARY})

