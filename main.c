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
    UNRECOGNIZED_COMMAND
} CommandResult;

typedef struct {
    long id;
    char username[25];
    char email[255];
} Row;

CommandResult do_meta_command(InputBuffer *input_buffer) {
    if (strcmp(input_buffer->buffer, ".exit") == 0) {
        return COMMAND_SUCCESS;
    }

    return UNRECOGNIZED_COMMAND;
}

void process_command(InputBuffer *input_buffer) {
    //split by whitespace first
    //variable to calculate the count of the tokens
    char *token;
    int count = 0;
    const char *delim = " \t\n";
    char *endptr;
    char *p = input_buffer->buffer;
    Row row;
    char* tokens[4];

    while ((token = strsep(&p, delim)) != NULL) {
        if (*token != '\0') {
            if (count <4) {
                tokens[count] = token;
                // Skip empty tokens from consecutive spaces
                printf("Token: %s\n", token);
                count++;

            }
        }
    }


    if (count < 4) {
        printf("Not enough arguments to insert!\n");
        return;
    }

    // Get first token
    if (strcmp(tokens[0], "insert") != 0) {
        printf("Wrong command, try again!\n");
        return;
    }

    //TODO : check if the ID is not duplicated
    errno = 0;
    long result = strtol(tokens[1], &endptr, 10);
    if (errno != 0) {
        perror("strtol");
        return;
    } else if (*endptr != '\0') {
        printf("Invalid number: '%s'\n", endptr);
        return;
    } else if (result < INT_MIN || result > INT_MAX) {
        printf("Value out of int range\n");
        return;
    } else {
        row.id = result;
        printf("Value = %ld\n", result);
    }
    if (strlen(tokens[2]) >= sizeof(row.username)) {
        printf("Username too long\n");
        return;
    }
    strncpy(row.username, tokens[2], sizeof(row.username) - 1);
    row.username[sizeof(row.username) - 1] = '\0';
    if (strlen(tokens[3]) >= sizeof(row.email)) {
        printf("Email too long! \n");
        return;
    }
    strncpy(row.email, tokens[3], sizeof(row.email));
    row.email[sizeof(row.email) - 1] = '\0';
}

int main(void) {
    InputBuffer *input_buffer = new_input_buffer();

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
        process_command(input_buffer);
        printf("Unrecognized statement: '%s'.\n", input_buffer->buffer);
    }

    close_input_buffer(input_buffer);
    return EXIT_SUCCESS;
}
