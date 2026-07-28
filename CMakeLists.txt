cmake_minimum_required(VERSION 4.2)
project(database_in_c C)

set(CMAKE_C_STANDARD 11)

add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)

add_executable(database_in_c
        src/repl.c
        src/table.c
        src/command.c
        src/main.c
        src/pager.c
        tests/Btree.c
)
target_include_directories(database_in_c PRIVATE include)