#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include "command.h"
#include "table.h"

#include <limits.h>
#include <string.h>

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
            printf("Id %ld is duplicated\n",result );
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
