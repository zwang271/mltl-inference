cmake_minimum_required(VERSION 3.4)
project(libmltl)

set(CMAKE_CXX_STANDARD 17)

find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SOURCES 
    src/pybind/libmltl_pybind.cc
    src/parser.cc
    src/ast.cc
)

pybind11_add_module(libmltl ${SOURCES})

target_link_libraries(libmltl PRIVATE pybind11::module)