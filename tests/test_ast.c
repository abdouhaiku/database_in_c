//
//  Reconstructs the tree for:
//   SELECT id FROM users WHERE id = 1;
//
// and test ast_print + ast_destroy on it.
//

#include <stdlib.h>

#include "ast.h"

int main(void) {
    AstNode *where_left = malloc(sizeof(AstNode));
    where_left->type = EXPR_COLUMN;
    where_left->as.column.name = "id";
    where_left->as.column.length = 2;

    AstNode *where_right = malloc(sizeof(AstNode));
    where_right->type = EXPR_LITERAL_INT;
    where_right->as.literal_int = 1;

    AstNode *where = malloc(sizeof(AstNode));
    where->type = EXPR_EQUALS;
    where->as.equals.left = where_left;
    where->as.equals.right = where_right;

    AstNode *col = malloc(sizeof(AstNode));
    col->type = EXPR_COLUMN;
    col->as.column.name = "id";
    col->as.column.length = 2;

    AstNode *select = malloc(sizeof(AstNode));
    select->type = AST_SELECT;
    select->as.select.table = "users";
    select->as.select.table_length = 5;
    select->as.select.is_star = false;
    select->as.select.columns[0] = col;
    select->as.select.column_count = 1;
    select->as.select.where = where;

    ast_print(select);
    ast_destroy(select);

    return 0;
}
