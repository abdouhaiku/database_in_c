//
// Created by Abdou on 07/08/2026.
//

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

int is_digits(const char *str, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)str[i])) {
            return 0;
        }
    }

    return 1;
}

void tokenizer_init(Tokenizer *t, const char *sql) {
    t->text = sql;
    t->cursor = sql;
    t->line = 1;
    t->column = 1;
}

void get_next_token(Tokenizer *tokenizer, Token *out_token) {
    out_token->type = TOKEN_NONE;
    while (*tokenizer->cursor &&
           (*tokenizer->cursor == ' ' || *tokenizer->cursor == '\t' ||
            *tokenizer->cursor == '\r' || *tokenizer->cursor == '\n')) {
        if (*tokenizer->cursor == '\n') {
            tokenizer->cursor++;
            tokenizer->column = 1;
            tokenizer->line++;
        } else {
            tokenizer->cursor++;
            tokenizer->column++;
        }
    }

    // reached either end of input or a real, non-whitespace character.
    out_token->byte_offset = (size_t) (tokenizer->cursor - tokenizer->text);
    out_token->line = tokenizer->line;
    out_token->column = tokenizer->column;
    out_token->text = tokenizer->cursor;

    if (*tokenizer->cursor == '\0') {
        out_token->type = TOKEN_EOF;
        out_token->text_length = 0;
        return;
    }

    switch (*tokenizer->cursor) {
        case ',':
            out_token->type = TOKEN_COMMA;
            break;
        case ';':
            out_token->type = TOKEN_SEMICOLON;
            break;
        case '(':
            out_token->type = TOKEN_LPAREN;
            break;
        case ')':
            out_token->type = TOKEN_RPAREN;
            break;
        case '=':
            out_token->type = TOKEN_EQUAL;
            break;
    }
    if (out_token->type != TOKEN_NONE) {
        out_token->text_length = 1;
        tokenizer->cursor++;
        tokenizer->column++;
        return;
    }

    //if non of these cases applies
    const char* c = tokenizer->cursor;
    while (isalnum(*c) || *c =='_') {
        //advance c
        c++;
    }
    out_token->text_length = c - tokenizer->cursor;
    out_token->text = tokenizer->cursor;
    tokenizer->cursor = tokenizer->cursor + out_token->text_length;

#define IS_KEYWORD(kw) \
    (out_token->text_length == strlen(kw) && strncasecmp(out_token->text, kw, out_token->text_length) == 0)

    if (IS_KEYWORD("SELECT")) {
        out_token->type = TOKEN_SELECT;
    }
    else if (IS_KEYWORD("FROM")) {
        out_token->type = TOKEN_FROM;
    }
    else if (IS_KEYWORD("WHERE")) {
        out_token->type = TOKEN_WHERE;
    }
    else if (IS_KEYWORD("INSERT")) {
        out_token->type = TOKEN_INSERT;
    }
    else if (IS_KEYWORD("INTO")) {
        out_token->type = TOKEN_INTO;
    }
    else if (IS_KEYWORD("VALUES")) {
        out_token->type = TOKEN_VALUE;
    }
    else if (is_digits(out_token->text, out_token->text_length)) {
        out_token->type = TOKEN_INTEGER;
        char temp[out_token->text_length + 1];
        memcpy(temp, out_token->text, out_token->text_length);
        temp[out_token->text_length] = '\0';
        long value = strtol(temp, NULL, 10);
        out_token->int_value = value;
    }
    else {
        out_token->type = TOKEN_IDENTIFIER;
    }

#undef IS_KEYWORD
}
