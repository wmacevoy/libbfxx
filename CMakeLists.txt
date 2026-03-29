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

# facts as a static library 
add_library(facts STATIC
    vendor/facts/src/facts.c
    vendor/facts/src/facts.cpp
)
target_include_directories(facts PRIVATE vendor/facts/include)

add_executable(round src/round.cpp)
target_include_directories(round PRIVATE
    include
    vendor/retain-recall/include
    vendor   # so bf.hpp can #include "libbf/libbf.h"
)
target_link_libraries(round PRIVATE libbf)

add_executable(numeric src/numeric.cpp)
target_include_directories(numeric PRIVATE
    include
    vendor/retain-recall/include
    vendor
)
target_link_libraries(numeric PRIVATE libbf)

set(TEST_INCLUDES
    include
    vendor/retain-recall/include
    vendor/facts/include
    vendor
)
set(TEST_LIBS libbf facts)

add_executable(test_bf src/test_bf.cpp)
target_include_directories(test_bf PRIVATE ${TEST_INCLUDES})
target_link_libraries(test_bf PRIVATE ${TEST_LIBS})

add_executable(test_bf_dec src/test_bf_dec.cpp)
target_include_directories(test_bf_dec PRIVATE ${TEST_INCLUDES})
target_link_libraries(test_bf_dec PRIVATE ${TEST_LIBS})

add_executable(test_numeric src/test_numeric.cpp)
target_include_directories(test_numeric PRIVATE ${TEST_INCLUDES})
target_link_libraries(test_numeric PRIVATE ${TEST_LIBS})
