cmake_minimum_required(VERSION 4.2)
project(database_in_c C)

set(CMAKE_C_STANDARD 11)

add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)

add_executable(database_in_c main.c)

enable_testing()
add_executable(test_input_reading tests/test_input_reading.c
        tests/test_spliting.c)
add_test(NAME test_input_reading COMMAND test_input_reading)
