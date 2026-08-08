//
// Created by Abdou on 07/08/2026.
//

#ifndef DATABASE_IN_C_AST_H
#define DATABASE_IN_C_AST_H
#include <stddef.h>
#include <stdint.h>

typedef enum {
    TOKEN_NONE = 0,TOKEN_SELECT, TOKEN_FROM, TOKEN_WHERE, TOKEN_INSERT, TOKEN_INTO, TOKEN_VALUE,
    TOKEN_IDENTIFIER, TOKEN_INTEGER, TOKEN_STRING,
    TOKEN_COMMA, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_SEMICOLON, TOKEN_EQUAL,
    TOKEN_EOF, TOKEN_INVALID
} TokenType;

typedef struct {
    TokenType type;
    const char *text;   // pointer into the original SQL string, or a copy
    size_t text_length;
    int64_t int_value;    // only meaningful when type == TOKEN_INTEGER
    size_t byte_offset;
    int line;
    int column;
} Token;


typedef struct {
    const char* text;
    const char* cursor; // pointer to the next token to be processed
    int line;
    int column;
} Tokenizer;


void get_next_token(Tokenizer *tokenizer, Token *out_token);
void tokenizer_init(Tokenizer *t, const char *sql);
void debug_tokens(char* sql);
#endif //DATABASE_IN_C_AST_H
