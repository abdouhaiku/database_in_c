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
            // table/username/email are borrowed pointers into the SQL text, nothing owned to free
            break;
        case AST_SELECT:
            for (size_t i = 0; i < node->as.select.column_count; i++) {
                ast_destroy(node->as.select.columns[i]);
            }
            ast_destroy(node->as.select.where);
            break;
        case EXPR_LITERAL_INT:
        case EXPR_LITERAL_STRING:
        case EXPR_COLUMN:
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
        case EXPR_LITERAL_INT:
            return "EXPR_LITERAL_INT";
        case EXPR_LITERAL_STRING:
            return "EXPR_LITERAL_STRING";
        case EXPR_COLUMN:
            return "EXPR_COLUMN";
        case EXPR_EQUALS:
            return "EXPR_EQUALS";
    }
    return "AST_UNKNOWN";
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
            printf("id: %lld\n", (long long) node->as.insert.id);
            ast_print_indent(depth + 1);
            printf("username: %.*s\n", (int) node->as.insert.username_length, node->as.insert.username);
            ast_print_indent(depth + 1);
            printf("email: %.*s\n", (int) node->as.insert.email_length, node->as.insert.email);
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
        case EXPR_COLUMN:
            ast_print_indent(depth + 1);
            printf("name: %.*s\n", (int) node->as.column.length, node->as.column.name);
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
