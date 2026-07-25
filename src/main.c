#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>

#include "command.h"
#include "repl.h"
#include "table.h"


int main(void) {
    InputBuffer *input_buffer = new_input_buffer();

    Row *rows = malloc(4 * sizeof(Row));
    if (rows == NULL) {
        fprintf(stderr, "Failed to allocate row storage.\n");
        exit(EXIT_FAILURE);
    }

    //init table
    Table table = {
        rows,
        0,
        4
    };

    while (1) {
        print_prompt();

        if (read_input(input_buffer) != 0) {
            printf("\n");
            break;
        }

        if (input_buffer->input_length == 0) {
            continue;
        }

        if (input_buffer->buffer[0] == '.') {
            switch (do_meta_command(input_buffer)) {
                case COMMAND_SUCCESS:
                    close_input_buffer(input_buffer);
                    return EXIT_SUCCESS;
                case UNRECOGNIZED_COMMAND:
                    printf("Unrecognized command '%s'.\n", input_buffer->buffer);
                    continue;
                case ID_DUPLICATE_ERROR:
                    break;
            }
        }

        // process the command
        CommandResult command_result = process_command(input_buffer, &table);
        if (command_result == UNRECOGNIZED_COMMAND) {
            printf("Unrecognized statement: '%s'.\n", input_buffer->buffer);
        }
    }

    close_input_buffer(input_buffer);
    return EXIT_SUCCESS;
}
