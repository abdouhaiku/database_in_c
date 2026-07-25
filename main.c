#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>

typedef struct {
    char *buffer;
    size_t buffer_capacity;
    ssize_t input_length;
} InputBuffer;

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

void close_input_buffer(InputBuffer *input_buffer) {
    free(input_buffer->buffer);
    free(input_buffer);
}

void print_prompt(void) {
    printf("miniDB> ");
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

typedef enum {
    COMMAND_SUCCESS,
    UNRECOGNIZED_COMMAND,
    ID_DUPLICATE_ERROR
} CommandResult;

typedef struct {
    long id;
    char username[25];
    char email[255];
} Row;

typedef struct {
    Row *rows;
    size_t num_rows;
    size_t capacity;
} Table;

CommandResult do_meta_command(InputBuffer *input_buffer) {
    if (strcmp(input_buffer->buffer, ".exit") == 0) {
        return COMMAND_SUCCESS;
    }

    return UNRECOGNIZED_COMMAND;
}


CommandResult insert_command(char **tokens, Table *table, int total_tokens) {
    if (total_tokens > 4) {
        printf("Too many arguments to insert!\n");
        return COMMAND_SUCCESS;
    }

    if (table->num_rows == table->capacity) {
        //reset errno
        errno = 0;
        Row *tmp = realloc(table->rows, table->capacity * 2 * sizeof(Row));
        if (tmp == NULL) {
            perror("Reallocation failed, try again\n");
            return COMMAND_SUCCESS;
        }
        table->capacity = 2 * table->capacity;
        table->rows = tmp;
    }
    errno = 0;
    char *endptr;
    long result = strtol(tokens[1], &endptr, 10);
    if (errno != 0) {
        perror("strtol");
        return COMMAND_SUCCESS;
    } else if (*endptr != '\0') {
        printf("Invalid number: '%s'\n", endptr);
        return COMMAND_SUCCESS;
    } else if (result < INT_MIN || result > INT_MAX) {
        printf("Value out of int range\n");
        return COMMAND_SUCCESS;
    } else {
        table->rows[table->num_rows].id = result;
        printf("Value = %ld\n", result);
    }

    // check if the id is not duplicate
    for (size_t i = 0; i < table->num_rows; i++) {
        if (table->rows[i].id == result) {
            printf("Id %ld is duplicated",result );
            return ID_DUPLICATE_ERROR;
        }
    }
    if (strlen(tokens[2]) >= sizeof(table->rows[table->num_rows].username)) {
        printf("Username too long\n");
        return COMMAND_SUCCESS;
    }
    strncpy(table->rows[table->num_rows].username, tokens[2],
            sizeof(table->rows[table->num_rows].username) - 1);
    table->rows[table->num_rows].username[sizeof(table->rows[table->num_rows].username) - 1] = '\0';
    if (strlen(tokens[3]) >= sizeof(table->rows[table->num_rows].email)) {
        printf("Email too long! \n");
        return COMMAND_SUCCESS;
    }
    strncpy(table->rows[table->num_rows].email, tokens[3], sizeof(table->rows[table->num_rows].email));
    table->rows[table->num_rows].email[sizeof(table->rows[table->num_rows].email) - 1] = '\0';
    table->num_rows++;
    return COMMAND_SUCCESS;
}

CommandResult select_all_command(Table *table) {
    for (size_t i = 0; i < table->num_rows; i++) {
        printf("Row %zu : %ld, %s, %s\n", i, table->rows[i].id, table->rows[i].username, table->rows[i].email);
    }
    return COMMAND_SUCCESS;
}

CommandResult process_command(InputBuffer *input_buffer, Table *table) {
    //split by whitespace first
    //variable to calculate the count of the tokens
    char *token;
    int total_tokens = 0;
    const char *delim = " \t\n";
    char *p = input_buffer->buffer;
    char *tokens[4];

    while ((token = strsep(&p, delim)) != NULL) {
        if (*token != '\0') {
            total_tokens++;
            if (total_tokens <= 4) {
                tokens[total_tokens - 1] = token;
                // Skip empty tokens from consecutive spaces
                printf("Token: %s\n", token);
            }
        }
    }


    if (total_tokens < 1) {
        printf("No command entered!\n");
        return COMMAND_SUCCESS;
    }

    // Get first token
    if (strcmp(tokens[0], "insert") != 0 && strcmp(tokens[0], "select") != 0) {
        printf("Wrong command, try again!\n");
        return UNRECOGNIZED_COMMAND;
    }

    if (strcmp(tokens[0], "insert") == 0) {
        if (total_tokens < 4) {
            printf("Not enough arguments to insert!\n");
            return COMMAND_SUCCESS;
        }
        return insert_command(tokens, table, total_tokens);
    }

    // select takes no arguments
    if (total_tokens > 1) {
        printf("Too many arguments to select!\n");
        return COMMAND_SUCCESS;
    }
    return select_all_command(table);
}

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
