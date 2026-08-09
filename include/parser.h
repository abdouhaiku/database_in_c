//
// Created by Abdou on 08/08/2026.
//


#ifndef DATABASE_IN_C_PARSER_H
#define DATABASE_IN_C_PARSER_H
#include "ast.h"

typedef struct {
    Tokenizer tokenizer;
    Token current;
    char error[256]; // set by the parse_* functions on failure; empty when there's no error
} Parser;

void parser_init(Parser *parser, const char *sql);
void parser_advance(Parser *parser);
AstNode *parse_expression(Parser *parser);
AstNode *parse_select_statement(Parser *parser);
AstNode *parse_insert_statement(Parser *parser);

#endif //DATABASE_IN_C_PARSER_H
