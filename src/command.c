


#include "command.h"
#include "ast.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btree.h"
#include "parser.h"
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
    if (strncasecmp(input_buffer->buffer, ".tokens", 7) == 0) {
        char *sql_query = input_buffer->buffer + 7;
        debug_tokens(sql_query);
        return DEBUGS_TOKENS_SUCCESS;
    }
    if (strncasecmp(input_buffer->buffer, ".ast", 4) == 0) {
        char *sql_query = input_buffer->buffer + 4;
        debug_ast(sql_query);
        return DEBUG_AST_SUCCESS;
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

    Parser parser;
    parser_init(&parser, input_buffer->buffer);

    AstNode *tree = parser.current.type == TOKEN_SELECT
    ? parse_select_statement(&parser)
    : parse_insert_statement(&parser);

    if (tree == NULL) {
        return -1;
    }

    if (tree->type == AST_INSERT) {
        return insert_command(tree, table);
    }

    // TODO: add approprite function in SELECT to handle when where is NULL + columns is not empty & when when is not NULL
    if (tree->type == AST_SELECT && tree->as.select.is_star == true) {
        return select_all_command(table);
    }
}
