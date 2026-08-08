//
// Created by Abdou on 08/08/2026.
//

#include "parser.h"

#include <stdlib.h>
#include <string.h>

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
    AstNode *node = malloc(sizeof(AstNode));
    if (parser->current.type == TOKEN_SELECT) {
        node->type = AST_SELECT;
        parser_advance(parser);
        // Get the column list
        size_t column_count = 0;
        AstNode *columns[3];
        while (parser->current.type != TOKEN_EOF && parser->current.type != TOKEN_FROM) {
            if (parser->current.type == TOKEN_COMMA) {
                parser_advance(parser);
            }
            else if (parser->current.type == TOKEN_IDENTIFIER) {
                columns[column_count] = parse_primary_expression(parser);
                column_count++;
            }
            else {
                //error while parsing
                return NULL;
            }
        }

        if (column_count > 0) {
            for (size_t i = 0; i < column_count; i++) {
                node->as.select.columns[i] = columns[i];
            }
        }
        if (parser->current.type == TOKEN_FROM) {
            parser_advance(parser);
        }

        // Get the name of the table
        if (parser->current.type == TOKEN_IDENTIFIER) {
            node->as.select.table = malloc(parser->current.text_length);
            strncpy(node->as.select.table, parser->current.text, parser->current.text_length);
            parser_advance(parser);
        }

        if (parser->current.type == TOKEN_WHERE) {
            node->as.select.where = parse_expression(parser);
        }
    }

}
