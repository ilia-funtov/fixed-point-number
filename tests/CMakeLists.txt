cmake_minimum_required(VERSION 3.0.0)
project(fixed_point_number_tests)

set(CMAKE_CXX_STANDARD 17)

include(CTest)
enable_testing()

add_executable(fixed_point_number_tests fixed_point_number_tests.cpp)
target_include_directories(fixed_point_number_tests PRIVATE ../include)
target_include_directories(fixed_point_number_tests PRIVATE ../dependencies)

if (MSVC)
    # warning level 4
    add_compile_options(/W4)
else()
    # lots of warnings
    add_compile_options(-Wall -Wextra -pedantic)
endif()

include(CTest)
enable_testing()

add_test(Unit-tests fixed_point_number_tests)