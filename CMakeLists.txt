cmake_minimum_required(VERSION 3.16)
project(bf LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# libbf as a static library (C only — keep vendor/libbf PRIVATE
# to avoid its "version" file shadowing the C++ <version> header)
add_library(libbf STATIC
    vendor/libbf/libbf.c
    vendor/libbf/cutils.c
)
target_include_directories(libbf PRIVATE vendor/libbf)

# Main executable
add_executable(round src/round.cpp)
target_include_directories(round PRIVATE
    include
    vendor/retain-recall/include
    vendor   # so bf.hpp can #include "libbf/libbf.h"
)
target_link_libraries(round PRIVATE libbf)
