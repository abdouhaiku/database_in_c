


#include "command.h"
#include "ast.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btree.h"
#include "repl.h"
#include "table.h"

CommandResult do_meta_command(InputBuffer *input_buffer,Table *table) {
    if (strcmp(input_buffer->buffer, ".exit") == 0) {
        return COMMAND_SUCCESS;
    }
    if (strcmp(input_buffer->buffer, ".btree") == 0) {
        void *page = pager_get_page(table->pager, 0);
        if (page == NULL) {
            printf("Error reading leaf node\n");
            return DEBUG_BTREE_SUCCESS;
        }
        debug_leaf_node(page, table);
        return DEBUG_BTREE_SUCCESS;
    }
    if (strncasecmp(input_buffer->buffer, ".tokens", 6) == 0) {
        char *sql_query = input_buffer->buffer + 7;
        debug_tokens(sql_query);
        return DEBUGS_TOKENS_SUCCESS;
    }

    return UNRECOGNIZED_COMMAND;
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

    // select takes either no arguments (select all) or one id to look up
    if (total_tokens > 2) {
        printf("Too many arguments to select!\n");
        return COMMAND_SUCCESS;
    }
    if (total_tokens == 2) {
        errno = 0;
        char *endptr;
        long key = strtol(tokens[1], &endptr, 10);
        if (errno != 0 || *endptr != '\0') {
            printf("Invalid id: '%s'\n", tokens[1]);
            return COMMAND_SUCCESS;
        }
        return select_by_id(table, key);
    }
    return select_all_command(table);
}
