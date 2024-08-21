cmake_minimum_required(VERSION 3.5)
project(MemoryTracker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(.)

set(MM_HDRS
    MemoryTracker.h
    malloc_allocator.h
)

set(MM_SRCS
    dummy.cpp
)

#add_subdirectory(tests)

add_library(${PROJECT_NAME} ${MM_HDRS} ${MM_SRCS})


