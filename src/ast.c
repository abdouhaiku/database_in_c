//
// Created by Abdou on 08/08/2026.
//

#include <stdlib.h>

#include "ast.h"

#include <stdio.h>

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

char* ast_node_type_by_name(AstNodeType type) {
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
        case COLUMN_TEXT:    return "TEXT";
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
