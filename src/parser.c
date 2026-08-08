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

AstNode *parse_expression(Parser *parser) {
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
            free(node);
            return NULL;
    }
    parser_advance(parser);
    return node;
}

