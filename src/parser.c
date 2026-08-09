//
// Created by Abdou on 08/08/2026.
//

#include "parser.h"

#include <stdlib.h>

void parser_init(Parser *parser, const char *sql) {
    Tokenizer *tokenizer = malloc(sizeof(Tokenizer));
    tokenizer_init(tokenizer, sql);
    parser->tokenizer = *tokenizer;
    get_next_token(tokenizer, &parser->current);
}

void parser_advance(Parser *parser) {
    get_next_token(&parser->tokenizer, &parser->current);
}

static AstNode *parse_primary_expression(Parser *parser) {
    AstNode *node = malloc(sizeof(AstNode));

    switch (parser->current.type) {
        case TOKEN_INTEGER:
            node->type = EXPR_LITERAL_INT;
            node->as.literal_int = parser->current.int_value;
            break;
        case TOKEN_STRING:
            node->type = EXPR_LITERAL_STRING;
            node->as.literal_string.text = parser->current.text;
            node->as.literal_string.length = parser->current.text_length;
            break;
        case TOKEN_IDENTIFIER:
            node->type = EXPR_COLUMN;
            node->as.column.name = parser->current.text;
            node->as.column.length = parser->current.text_length;
            break;
        default:
            // Not a literal or column reference - syntax error. Full error
            // reporting is #41's job; for now, NULL signals "not an expression."
            free(node);
            return NULL;
    }
    parser_advance(parser);
    return node;
}

AstNode *parse_expression(Parser *parser) {
    AstNode *left = parse_primary_expression(parser);
    if (left == NULL) {
        return NULL;
    }

    // peek at whatever parse_primary_expression's advance left us sitting on.
    if (parser->current.type != TOKEN_EQUAL) {
        return left;
    }
    parser_advance(parser); // consume '='

    AstNode *right = parse_primary_expression(parser);
    if (right == NULL) {
        ast_destroy(left);
        return NULL;
    }

    AstNode *equals = malloc(sizeof(AstNode));
    equals->type = EXPR_EQUALS;
    equals->as.equals.left = left;
    equals->as.equals.right = right;
    return equals;
}

AstNode *parse_select_statement(Parser *parser) {
    if (parser->current.type != TOKEN_SELECT) {
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    node->type = AST_SELECT;
    node->as.select.is_star = false;
    node->as.select.column_count = 0;
    node->as.select.where = NULL;
    parser_advance(parser);

    if (parser->current.type == TOKEN_STAR) {
        node->as.select.is_star = true;
        parser_advance(parser);
    } else {
        // Column list
        while (parser->current.type != TOKEN_EOF && parser->current.type != TOKEN_FROM) {
            if (parser->current.type == TOKEN_COMMA) {
                parser_advance(parser);
                continue;
            }
            if (parser->current.type != TOKEN_IDENTIFIER) {
                // Unexpected token where a column name or comma was expected.
                ast_destroy(node);
                return NULL;
            }
            if (node->as.select.column_count >= 3) {
                // Too many columns, only id/username/email can ever exist.
                ast_destroy(node);
                return NULL;
            }
            node->as.select.columns[node->as.select.column_count] = parse_primary_expression(parser);
            node->as.select.column_count++;
        }
    }

    if (parser->current.type != TOKEN_FROM) {
        // FROM is required.
        ast_destroy(node);
        return NULL;
    }
    parser_advance(parser);

    if (parser->current.type != TOKEN_IDENTIFIER) {
        // Table name is required after FROM.
        ast_destroy(node);
        return NULL;
    }
    node->as.select.table = parser->current.text;
    node->as.select.table_length = parser->current.text_length;
    parser_advance(parser);

    if (parser->current.type == TOKEN_WHERE) {
        parser_advance(parser); // consume WHERE itself before parsing the condition
        node->as.select.where = parse_expression(parser);
    }

    return node;
}
