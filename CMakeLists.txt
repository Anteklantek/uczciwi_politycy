cmake_minimum_required(VERSION 3.10)
project(Uczciwi_politycy C)

find_package(MPI REQUIRED)
include_directories(${MPI_INCLUDE_PATH})

set(CMAKE_C_COMPILER mpicc)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")


set(SOURCE_FILES main.cpp)
add_executable(Uczciwi_politycy ${SOURCE_FILES})