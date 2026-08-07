//
// Created by Abdou on 07/08/2026.
//

#include "ast.h"

void tokenizer_init(Tokenizer *t, const char *sql) {
    t->text = sql;
    t->cursor = sql;
    t->line = 1;
    t->column = 1;
}

void get_next_token(Tokenizer *tokenizer, Token *out_token) {
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

    
    out_token->type = TOKEN_INVALID;
    out_token->text_length = 1;
}
