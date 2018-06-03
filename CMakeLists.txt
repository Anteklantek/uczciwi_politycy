cmake_minimum_required(VERSION 3.7)
project(Uczciwi_politycy)

find_package(MPI REQUIRED)
include_directories(${MPI_INCLUDE_PATH})

set(CMAKE_C_COMPILER mpicc)
set(CMAKE_CXX_COMPILER mpicxx)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")


set(SOURCE_FILES main.cpp)
add_executable(Uczciwi_politycy ${SOURCE_FILES})