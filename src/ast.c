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
            // table/username/email are borrowed pointers into the SQL text - nothing owned to free
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
    }
}

/*
AstNode
├── type: AST_SELECT
└── as.select:
    ├── table: "TABLE"
    ├── is_star: false
    ├── where: NULL
    └── columns: [
          AstNode { type: EXPR_COLUMN, as.column: { name: "EMAIL" } },
          AstNode { type: EXPR_COLUMN, as.column: { name: "USERNAME" } },
          AstNode { type: EXPR_COLUMN, as.column: { name: "ID" } }
        ]
*/
void ast_print(AstNode *node) {
    printf("%s\n", "AstNode");
    printf("%s %s\n", "├── type:", ast_node_type_by_name(node->type));
    if (node->type == AST_INSERT) {
        printf("%s\n", "└── as.INSERT:");
        printf("\t%s %s\n","├── table:", node->as.insert.table);
        printf("\t%s %lld\n","├── id:", node->as.insert.id);
        printf("\t%s %s\n","├── username:", node->as.insert.username);
        printf("\t%s %s\n","├── email:", node->as.insert.email);
    }
    else if (node->type == AST_SELECT) {
        printf("%s\n", "└── as.SELECT:");
        printf("\t%s %s\n","├── table:", node->as.insert.table);
        printf("\t%s %s\n","├── is_start:", node->as.select.is_star ? "true" : "false");
        if (node->as.select.where != NULL) {
            printf("\t%s %s\n","├── WHERE:");
            // Get left literal string
            char *left = node->as.select.where->as.equals.left->as.literal_string.text;
            int64_t right = node->as.select.where->as.equals.right->as.literal_int;
            printf("\t\t%s %s\n","├──── left:", left);
            printf("\t\t%s %lld\n","├──── right:", right);
        }
        else {
            printf("\t%s\n","├── WHERE: NULL");
        }
        if (node->as.select.column_count>0) {
            printf("\t%s\n","├── columns: [");
            for (int i = 0; i<node->as.select.column_count; i++) {
                printf("\t\t%s%s\n","AstNode { type: EXPR_COLUMN, as.column: { name: ",
                    node->as.select.columns[i]->as.column.name );
            }
        }

    }
}
