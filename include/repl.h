//
// Created by Abdou on 25/07/2026.
//

#ifndef DATABASE_IN_C_REPL_H
#define DATABASE_IN_C_REPL_H
#include <stddef.h>
#include <sys/types.h>

typedef struct {
    char *buffer;
    size_t buffer_capacity;
    ssize_t input_length;
} InputBuffer;


InputBuffer *new_input_buffer(void);

void print_prompt(void);

int read_input(InputBuffer *input_buffer);

void close_input_buffer(InputBuffer *input_buffer);


#endif //DATABASE_IN_C_REPL_H
