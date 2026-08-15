//
// Created by Abdou on 08/08/2026.
//

#include <stdlib.h>

#include "ast.h"

#include <stdio.h>
#include <string.h>

#include "btree.h"
#include "pager.h"

void ast_destroy(AstNode *node) {
    if (node == NULL) {
        return;
    }

    switch (node->type) {
        case AST_INSERT:
            for (size_t i = 0; i < node->as.insert.values_count; i++) {
                ast_destroy(node->as.insert.values[i]);
            }
            break;
        case AST_SELECT:
            for (size_t i = 0; i < node->as.select.column_count; i++) {
                ast_destroy(node->as.select.columns[i]);
            }
            ast_destroy(node->as.select.where);
            break;
        case AST_CREATE_TABLE:
            for (size_t i = 0; i < node->as.create_table.column_count; i++) {
                ast_destroy(node->as.create_table.columns_definitions[i]);
            }
            break;
        case EXPR_LITERAL_INT:
        case EXPR_LITERAL_STRING:
        case EXPR_LITERAL_BOOLEAN:
        case EXPR_COLUMN:
        case EXPR_COLUMN_DEFINITION:
            // leaves nothing owned to free besides the node itself
            break;
        case EXPR_EQUALS:
            ast_destroy(node->as.equals.left);
            ast_destroy(node->as.equals.right);
            break;
    }

    free(node);
}

char *ast_node_type_by_name(AstNodeType type) {
    switch (type) {
        case AST_INSERT:
            return "AST_INSERT";
        case AST_SELECT:
            return "AST_SELECT";
        case AST_CREATE_TABLE:
            return "AST_CREATE_TABLE";
        case EXPR_LITERAL_INT:
            return "EXPR_LITERAL_INT";
        case EXPR_LITERAL_STRING:
            return "EXPR_LITERAL_STRING";
        case EXPR_LITERAL_BOOLEAN:
            return "EXPR_LITERAL_BOOLEAN";
        case EXPR_COLUMN:
            return "EXPR_COLUMN";
        case EXPR_EQUALS:
            return "EXPR_EQUALS";
        case EXPR_COLUMN_DEFINITION:
            return "EXPR_COLUMN_DEFINITION";
    }
    return "AST_UNKNOWN";
}

static const char *column_type_name(ColumnType type) {
    switch (type) {
        case COLUMN_INTEGER: return "INTEGER";
        case COLUMN_TEXT: return "TEXT";
        case COLUMN_BOOLEAN: return "BOOLEAN";
    }
    return "UNKNOWN";
}

static void ast_print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
}

static void ast_print_node(AstNode *node, int depth) {
    if (node == NULL) {
        ast_print_indent(depth);
        printf("NULL\n");
        return;
    }

    ast_print_indent(depth);
    printf("AstNode { type: %s }\n", ast_node_type_by_name(node->type));

    switch (node->type) {
        case AST_INSERT:
            ast_print_indent(depth + 1);
            printf("table: %.*s\n", (int) node->as.insert.table_length, node->as.insert.table);
            ast_print_indent(depth + 1);
            printf("values: [\n");
            for (size_t i = 0; i < node->as.insert.values_count; i++) {
                ast_print_node(node->as.insert.values[i], depth + 2);
            }
            ast_print_indent(depth + 1);
            printf("]\n");
            break;
        case AST_SELECT:
            ast_print_indent(depth + 1);
            printf("table: %.*s\n", (int) node->as.select.table_length, node->as.select.table);
            ast_print_indent(depth + 1);
            printf("is_star: %s\n", node->as.select.is_star ? "true" : "false");
            ast_print_indent(depth + 1);
            printf("where:\n");
            ast_print_node(node->as.select.where, depth + 2);
            ast_print_indent(depth + 1);
            printf("columns: [\n");
            for (size_t i = 0; i < node->as.select.column_count; i++) {
                ast_print_node(node->as.select.columns[i], depth + 2);
            }
            ast_print_indent(depth + 1);
            printf("]\n");
            break;
        case EXPR_LITERAL_INT:
            ast_print_indent(depth + 1);
            printf("value: %lld\n", (long long) node->as.literal_int);
            break;
        case EXPR_LITERAL_STRING:
            ast_print_indent(depth + 1);
            printf("value: %.*s\n", (int) node->as.literal_string.length, node->as.literal_string.text);
            break;
        case EXPR_LITERAL_BOOLEAN:
            ast_print_indent(depth + 1);
            printf("value: %s\n", node->as.literal_boolean.bool_value ? "true" : "false");
            break;
        case EXPR_COLUMN:
            ast_print_indent(depth + 1);
            printf("name: %.*s\n", (int) node->as.column.length, node->as.column.name);
            break;
        case AST_CREATE_TABLE:
            ast_print_indent(depth + 1);
            printf("table: %.*s\n", (int) node->as.create_table.table_length, node->as.create_table.table);
            ast_print_indent(depth + 1);
            printf("columns: [\n");
            for (size_t i = 0; i < node->as.create_table.column_count; i++) {
                ast_print_node(node->as.create_table.columns_definitions[i], depth + 2);
            }
            ast_print_indent(depth + 1);
            printf("]\n");
            break;
        case EXPR_COLUMN_DEFINITION:
            ast_print_indent(depth + 1);
            printf("name: %.*s\n", (int) node->as.column_definition.length, node->as.column_definition.name);
            ast_print_indent(depth + 1);
            printf("type: %s\n", column_type_name(node->as.column_definition.columnType));
            ast_print_indent(depth + 1);
            printf("is_primary: %s\n", node->as.column_definition.is_primary ? "true" : "false");
            break;
        case EXPR_EQUALS:
            ast_print_indent(depth + 1);
            printf("left:\n");
            ast_print_node(node->as.equals.left, depth + 2);
            ast_print_indent(depth + 1);
            printf("right:\n");
            ast_print_node(node->as.equals.right, depth + 2);
            break;
    }
}

void ast_print(AstNode *node) {
    ast_print_node(node, 0);
}


void print_table_schema(Pager *pager, char *table_name) {
    Token out_token;
    memset(&out_token, 0, sizeof(Token));
    Tokenizer tokenizer;
    tokenizer_init(&tokenizer, table_name);
    while (out_token.type != TOKEN_IDENTIFIER && out_token.type != TOKEN_EOF) {
        get_next_token(&tokenizer, &out_token);
    }
    if (out_token.type != TOKEN_IDENTIFIER) {
        printf("Not a real table name, please try again!\n");
        return;
    }
    // Get the catalog page
    void *catalog_page = pager_get_page(pager, 0);
    uint32_t table_num = catalog_node_find_table(catalog_page, out_token.text, out_token.text_length);
    if (table_num == UINT32_MAX) {
        printf("The table does not exist! \n");
        return;
    }
    int column_count = *catalog_node_column_count(catalog_page, table_num);
    void *table = catalog_node_table(catalog_page, table_num);
    for (int i = 0; i < column_count; i++) {
        char name[32];
        ColumnType column_type;
        memcpy(name, (uint8_t*) table + TABLE_HEADER_SIZE + (i * COLUMN_SIZE), 32);
        column_type = *((uint8_t *) table + TABLE_HEADER_SIZE + (i * COLUMN_SIZE) + COLUMN_NAME_SIZE);

        printf("Column Name %s, its type is %s\n", name, column_type_name(column_type));
    }
}

void build_plan(PlanNode *plan_node, AstNode *tree, Table *table) {
    void *catalog_page = pager_get_page(table->pager, 0);
    uint32_t table_num = catalog_node_find_table(catalog_page, table->table_name, table->table_length);
    switch (tree->type) {
        case AST_INSERT:
            plan_node->type = PLAN_INSERT;
            // Get the table root page num
            plan_node->as.insert.root_page_num = table->root_page_num;
            plan_node->as.insert.values = tree->as.insert.values;
            plan_node->as.insert.value_count = tree->as.insert.values_count;
            break;
        case AST_SELECT: {
            PlanNode *table_scan = malloc(sizeof(PlanNode));
            table_scan->type = PLAN_TABLE_SCAN;
            table_scan->as.table_scan.root_page_num = table->root_page_num;
            if (tree->as.select.column_count > 0) {
                plan_node->type = PLAN_PROJECTION;
                plan_node->as.projection.column_count = tree->as.select.column_count;
                plan_node->as.projection.columns = tree->as.select.columns;
                if (tree->as.select.where != NULL) {
                    PlanNode *where_child = malloc(sizeof(PlanNode));
                    // Check if the key is primary
                    if (is_primary_key(catalog_page, table_num,
                                       tree->as.select.where->as.equals.left->as.column.name,
                                       tree->as.select.where->as.equals.left->as.column.length)) {
                        where_child->as.pk_lookup.key = tree->as.select.where->as.equals.right->as.literal_int;
                        where_child->type = PLAN_PK_LOOKUP;
                        where_child->as.pk_lookup.root_page_num = table->root_page_num;
                        plan_node->as.projection.child = where_child;
                        free(table_scan); // since it wont be used
                    } else {
                        where_child->type = PLAN_FILTER;
                        where_child->as.filter.condition = tree->as.select.where;
                        where_child->as.filter.child = table_scan;
                        plan_node->as.projection.child = where_child;
                    }
                } else {
                    plan_node->as.projection.child = table_scan;
                }
            } else if (tree->as.select.where != NULL) {
                if (is_primary_key(catalog_page, table_num,
                                   tree->as.select.where->as.equals.left->as.column.name,
                                   tree->as.select.where->as.equals.left->as.column.length)) {
                    plan_node->type = PLAN_PK_LOOKUP;
                    plan_node->as.pk_lookup.key = tree->as.select.where->as.equals.right->as.literal_int;
                    plan_node->as.pk_lookup.root_page_num = table->root_page_num;
                    free(table_scan);
                } else {
                    plan_node->type = PLAN_FILTER;
                    plan_node->as.filter.condition = tree->as.select.where;
                    plan_node->as.filter.child = table_scan;
                }
            } else {
                plan_node->type = PLAN_TABLE_SCAN;
                plan_node->as.table_scan.root_page_num = table->root_page_num;
                free(table_scan);
            }
            break;
        }
    }
}

void plan_destroy(PlanNode *node) {
    if (node == NULL) {
        return;
    }

    switch (node->type) {
        case PLAN_PK_LOOKUP:
        case PLAN_TABLE_SCAN:
        case PLAN_INSERT:
            break;
        case PLAN_FILTER:
            plan_destroy(node->as.filter.child);
            break;
        case PLAN_PROJECTION:
            plan_destroy(node->as.projection.child);
            break;
    }
    free(node);
}

void build_cursor(Cursor* cursor, PlanNode *plan) {
    switch (plan->type) {
        case PLAN_TABLE_SCAN:
            cursor->type = CURSOR_TABLE_SCAN;
            cursor->as.table_scan.current_page_num = plan->as.table_scan.root_page_num;
            cursor->as.table_scan.current_cell_num = 0;
            cursor->as.table_scan.done = false;
            break;
        case PLAN_PK_LOOKUP :
            cursor->type = CURSOR_PK_LOOKUP;
            cursor->as.pk_lookup.root_page_num = plan->as.pk_lookup.root_page_num;
            cursor->as.pk_lookup.key = plan->as.pk_lookup.key;
            cursor->as.pk_lookup.done = false;
            break;
        case PLAN_FILTER : {
            cursor->type = CURSOR_FILTER;
            Cursor *child = malloc(sizeof(Cursor));
            build_cursor(child, plan->as.filter.child);
            cursor->as.filter.child = child;
            cursor->as.filter.condition = plan->as.filter.condition;
            break;
        }
        case PLAN_PROJECTION: {
            cursor->type = CURSOR_PROJECTION;
            Cursor *child = malloc(sizeof(Cursor));
            build_cursor(child, plan->as.projection.child);
            cursor->as.projection.child = child;
            cursor->as.projection.columns = plan->as.projection.columns;
            cursor->as.projection.column_count = plan->as.projection.column_count;
            break;
        }
    }
}

void cursor_destroy(Cursor *cursor) {
    if (cursor == NULL) {
        return;
    }

    switch (cursor->type) {
        case CURSOR_TABLE_SCAN:
        case CURSOR_PK_LOOKUP:
            break;
        case CURSOR_FILTER:
            cursor_destroy(cursor->as.filter.child);
            break;
        case CURSOR_PROJECTION:
            cursor_destroy(cursor->as.projection.child);
            break;
    }
    free(cursor);
}