cmake_minimum_required(VERSION 3.4)
project(main)

set(CMAKE_CXX_FLAGS "-g -std=c++2a -Wall")

set(LOX_SRX_DIR "${PROJECT_SOURCE_DIR}/src")

set(lox_lib_SRC
  ${LOX_SRX_DIR}/chunk.cpp
)

add_library(lox_lib ${lox_lib_SRC})

add_executable(lox ${LOX_SRX_DIR}/main.cpp)
target_link_libraries(lox lox_lib)
