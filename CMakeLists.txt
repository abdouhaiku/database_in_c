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
        include/btree.h
        src/btree.c
        include/ast.h
        src/lexer.c
        src/ast.c
)
target_include_directories(database_in_c PRIVATE include)

add_executable(btree_study tests/Btree.c)

add_executable(test_ast tests/test_ast.c src/ast.c)
target_include_directories(test_ast PRIVATE include)