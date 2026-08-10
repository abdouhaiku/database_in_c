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

CommandResult do_meta_command(InputBuffer *input_buffer, Table *table) {
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
    Parser parser;
    parser_init(&parser, input_buffer->buffer);

    AstNode *tree = parser.current.type == TOKEN_SELECT
                        ? parse_select_statement(&parser)
                        : parse_insert_statement(&parser);

    if (tree == NULL) {
        printf("%s\n", parser.error);
        return COMMAND_SUCCESS;
    }

    CommandResult result;
    if (tree->type == AST_INSERT) {
        result = insert_command(tree, table);
    } else if (tree->as.select.is_star && tree->as.select.where == NULL) {
        result = select_all_command(table);
    } else if (tree->as.select.where != NULL || tree->as.select.column_count > 0) {
        result = select_columns_or_filter(table, tree);
    } else {
        // TODO: SELECT with an explicit column list and/or a WHERE clause
        // needs new execution functions only SELECT * is wired up so far.
        printf("This SELECT form isn't supported yet ");
        result = COMMAND_SUCCESS;
    }
    ast_destroy(tree);
    return result;
}
