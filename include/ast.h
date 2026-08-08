//
// Created by Abdou on 07/08/2026.
//

#ifndef DATABASE_IN_C_AST_H
#define DATABASE_IN_C_AST_H
#include <stdbool.h>
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

typedef enum {
    AST_INSERT, AST_SELECT,
    EXPR_LITERAL_INT, EXPR_LITERAL_STRING, EXPR_COLUMN, EXPR_EQUALS
} AstNodeType;

typedef struct AstNode AstNode;

struct AstNode {
    AstNodeType type;
    union {
        struct { const char *table; size_t table_length;
            int64_t id; const char *username; size_t username_length;
            const char *email; size_t email_length; } insert;
        struct {
            const char *table; size_t table_length;
            bool is_star;             // true for SELECT *
            AstNode *columns[3];      // up to 3 column references — id/username/email are the
            size_t column_count;      // only columns that can exist, no catalog for now
            AstNode *where;           // nullable
        } select;
        int64_t literal_int;
        struct { const char *text; size_t length; } literal_string;
        struct { const char *name; size_t length; } column;
        struct { AstNode *left; AstNode *right; } equals;
    } as;
};

void ast_destroy(AstNode *node);

void ast_print(AstNode *node);
#endif //DATABASE_IN_C_AST_H
