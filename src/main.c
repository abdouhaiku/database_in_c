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
    Pager *pager = pager_open("db.bin");
    if (pager == NULL) {
        return -1;
    }

    Table *table = table_open("db.bin");
    if (table == NULL) {
        printf("Problem in creating the table object\n");
        return -1;
    }

    while (1) {
        print_prompt();

        if (read_input(input_buffer) != 0) {
            printf("\n");
            close_input_buffer(input_buffer);
            table_close(table);
            return EXIT_SUCCESS;
        }

        if (input_buffer->input_length == 0) {
            continue;
        }

        if (input_buffer->buffer[0] == '.') {
            switch (do_meta_command(input_buffer,table)) {
                case COMMAND_SUCCESS:
                    close_input_buffer(input_buffer);
                    table_close(table);
                    return EXIT_SUCCESS;
                case UNRECOGNIZED_COMMAND:
                    printf("Unrecognized command '%s'.\n", input_buffer->buffer);
                    continue;
                case ID_DUPLICATE_ERROR:
                    break;
                case DEBUG_BTREE_SUCCESS:
                    continue;
                case DEBUGS_TOKENS_SUCCESS:
                    continue;
                case DEBUG_AST_SUCCESS:
                    continue;
            }
        }

        // process the command
        CommandResult command_result = process_command(input_buffer,pager);
        if (command_result == UNRECOGNIZED_COMMAND) {
            printf("Unrecognized statement: '%s'.\n", input_buffer->buffer);
        }
    }
}
