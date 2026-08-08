//
// Created by Abdou on 08/08/2026.
//

#include <stdlib.h>

#include "ast.h"

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
