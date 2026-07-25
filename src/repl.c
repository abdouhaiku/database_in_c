
#include <stdlib.h>
#include <stdio.h>
#include "repl.h"


InputBuffer *new_input_buffer(void) {
    InputBuffer *input_buffer = malloc(sizeof(InputBuffer));
    if (input_buffer == NULL) {
        fprintf(stderr, "Failed to allocate InputBuffer.\n");
        exit(EXIT_FAILURE);
    }

    input_buffer->buffer = NULL;
    input_buffer->buffer_capacity = 0;
    input_buffer->input_length = 0;

    return input_buffer;
}


/* Returns 0 on success, -1 on EOF (e.g. Ctrl-D). */
int read_input(InputBuffer *input_buffer) {
    ssize_t bytes_read = getline(&input_buffer->buffer, &input_buffer->buffer_capacity, stdin);

    if (bytes_read < 0) {
        return -1;
    }

    if (bytes_read > 0 && input_buffer->buffer[bytes_read - 1] == '\n') {
        bytes_read--;
        input_buffer->buffer[bytes_read] = '\0';
    }

    input_buffer->input_length = bytes_read;
    return 0;
}

void close_input_buffer(InputBuffer *input_buffer) {
    free(input_buffer->buffer);
    free(input_buffer);
}

void print_prompt(void) {
    printf("miniDB> ");
}

