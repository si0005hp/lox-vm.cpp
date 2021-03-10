cmake_minimum_required(VERSION 3.4)
project(main)

set(CMAKE_CXX_FLAGS "-g -std=c++2a -Wall")

set(LOX_SRX_DIR "${PROJECT_SOURCE_DIR}/src")

set(lox_lib_SRC
  ${LOX_SRX_DIR}/chunk.cpp
  ${LOX_SRX_DIR}/memory.cpp
  ${LOX_SRX_DIR}/debug.cpp
  ${LOX_SRX_DIR}/vm.cpp
  ${LOX_SRX_DIR}/compiler.cpp
  ${LOX_SRX_DIR}/scanner.cpp
  ${LOX_SRX_DIR}/value.cpp
  ${LOX_SRX_DIR}/object.cpp
  ${LOX_SRX_DIR}/table.cpp
)

add_library(lox_lib ${lox_lib_SRC})

add_executable(lox ${LOX_SRX_DIR}/main.cpp)
target_link_libraries(lox lox_lib)
