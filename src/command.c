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

CommandResult do_meta_command(InputBuffer *input_buffer, Pager *pager) {
    if (strcmp(input_buffer->buffer, ".exit") == 0) {
        return COMMAND_SUCCESS;
    }
    if (strcmp(input_buffer->buffer, ".btree") == 0) {
        void *page = pager_get_page(pager, 0);
        if (page == NULL) {
            printf("Error reading leaf node\n");
            return DEBUG_BTREE_SUCCESS;
        }
        debug_leaf_node(page, pager);
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
    if (strncasecmp(input_buffer->buffer, ".tables", 7) == 0) {
        // List all the tables from the catalog page
        void *catalog_page = pager_get_page(pager, 0);
        uint32_t num_of_tables = *catalog_node_num_tables(catalog_page);
        printf("The list of tables is the following : \n");
        for (uint32_t i = 0; i< num_of_tables; i++) {
            char name[32];
            memcpy(name, (uint8_t*)catalog_page + CATALOG_ENTRIES_OFFSET + (i* TABLE_ENTRY_SIZE), 32);
            printf("Table %s\n", name);
        }
        return DEBUG_AST_SUCCESS;
    }
    if (strncasecmp(input_buffer->buffer, ".schema", 7) == 0) {
        char *table_name = input_buffer->buffer + 7;
        print_table_schema(pager,table_name);
        return DEBUG_AST_SUCCESS;
    }



    return UNRECOGNIZED_COMMAND;
}


CommandResult process_command(InputBuffer *input_buffer, Pager *pager) {
    Parser parser;
    parser_init(&parser, input_buffer->buffer);
    AstNode *tree;
    switch (parser.current.type) {
        case TOKEN_SELECT:
            tree = parse_select_statement(&parser);
            break;
        case TOKEN_CREATE:
            tree = parse_create_statement(&parser);
            break;
        case TOKEN_INSERT:
            tree = parse_insert_statement(&parser);
            break;
        default:
            printf("Statement is not supported\n");
            return -1;
    }


    if (tree == NULL) {
        printf("%s\n", parser.error);
        return COMMAND_SUCCESS;
    }


    // Get the table
    void* catalog_page = pager_get_page(pager, 0);

    if (tree->type == AST_CREATE_TABLE) {
        uint32_t existing = catalog_node_find_table(catalog_page, tree->as.create_table.table, tree->as.create_table.table_length);
        if (existing != UINT32_MAX) {
            printf("Table already exists!\n");
            ast_destroy(tree);
            return COMMAND_SUCCESS;
        }
        CommandResult result = create_table(pager, tree); // 2
        ast_destroy(tree);
        return result;
    }

    uint32_t table_num = catalog_node_find_table(catalog_page,
        tree->type == AST_SELECT ? tree->as.select.table : tree->as.insert.table,
        tree->type == AST_SELECT ? tree->as.select.table_length : tree->as.insert.table_length
    );

    if (table_num == UINT32_MAX) {
        printf("Table is not found\n");
        ast_destroy(tree);
        return COMMAND_SUCCESS;
    }

    Table table = {
        .pager = pager,
        .root_page_num = *catalog_node_root_page(catalog_page, table_num),
        .table_name = tree->type == AST_SELECT ? tree->as.select.table : tree->as.insert.table,
        .table_length = tree->type == AST_SELECT ? tree->as.select.table_length : tree->as.insert.table_length
    };

    // Build Plan
    PlanNode *plan_node = malloc(sizeof(PlanNode));
    build_plan(plan_node, tree, &table);

    CommandResult result = COMMAND_SUCCESS;
    if (tree->type == AST_INSERT) {
        result = insert_command(tree, &table);
    } else if ((tree->as.select.where != NULL || tree->as.select.column_count > 0)
               && !validate_columns(&table, tree, table_num)) {
        printf("Column(s) entered do not exist!\n");
    } else {
        Cursor *cursor = malloc(sizeof(Cursor));
        build_cursor(cursor, plan_node);

        Row row;
        while (cursor_next(cursor, &table, &row)) {
            for (size_t i = 0; i < row.value_count; i++) {
                switch (row.values[i].type) {
                    case INTEGER_TYPE:
                        printf("%ld ", row.values[i].integer);
                        break;
                    case TEXT_TYPE:
                        printf("%s ", row.values[i].string.text);
                        break;
                    case BOOLEAN_TYPE:
                        row.values[i].bool_value == 1 ? printf("TRUE ") : printf("FALSE ");
                        break;
                }
            }
            printf("\n");
        }
        cursor_destroy(cursor);
    }

    plan_destroy(plan_node);
    ast_destroy(tree);
    return result;
}
