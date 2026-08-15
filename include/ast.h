//
// Created by Abdou on 07/08/2026.
//

#ifndef DATABASE_IN_C_AST_H
#define DATABASE_IN_C_AST_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pager.h"
#include "table.h"

typedef enum {
    COLUMN_INTEGER,
    COLUMN_TEXT,
    COLUMN_BOOLEAN
} ColumnType;

typedef enum {
    TOKEN_NONE = 0,TOKEN_SELECT, TOKEN_FROM, TOKEN_WHERE, TOKEN_INSERT, TOKEN_INTO, TOKEN_VALUE,
    TOKEN_IDENTIFIER, TOKEN_INTEGER, TOKEN_STRING,
    TOKEN_COMMA, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_SEMICOLON, TOKEN_EQUAL, TOKEN_STAR,
    TOKEN_EOF, TOKEN_INVALID, TOKEN_CREATE, TOKEN_TABLE, TOKEN_TYPE_TEXT, TOKEN_TYPE_BOOLEAN, TOKEN_TYPE_INTEGER,
    TOKEN_PRIMARY, TOKEN_KEY, TOKEN_TRUE, TOKEN_FALSE
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
const char* token_type_name(Token *token);
void debug_tokens(char* sql);
void debug_ast(char *sql);

#define MAX_TABLE_COLUMNS 8 // matches the catalog's per-table column-slot capacity

typedef enum {
    AST_INSERT, AST_SELECT, AST_CREATE_TABLE,
    EXPR_LITERAL_INT, EXPR_LITERAL_STRING, EXPR_LITERAL_BOOLEAN,  EXPR_COLUMN, EXPR_EQUALS, EXPR_COLUMN_DEFINITION
} AstNodeType;

typedef struct AstNode AstNode;
typedef struct PlanNode PlanNode;

struct AstNode {
    AstNodeType type;
    union {
        struct { const char *table; size_t table_length;
            AstNode * values[MAX_TABLE_COLUMNS];
            size_t values_count; } insert;
        struct {
            const char *table; size_t table_length;
            bool is_star;             // true for SELECT *
            AstNode *columns[MAX_TABLE_COLUMNS];
            size_t column_count;
            AstNode *where;           // nullable
        } select;
        struct {
            const char *table; size_t table_length;
            AstNode *columns_definitions[MAX_TABLE_COLUMNS];
            size_t column_count;
        } create_table;
        int64_t literal_int;
        struct { const char *text; size_t length; } literal_string;
        struct { const char *name; size_t length; } column;
        struct {bool bool_value;} literal_boolean;
        struct { const char *name; size_t length; ColumnType columnType; bool is_primary; } column_definition;
        struct { AstNode *left; AstNode *right; } equals;
    } as;
};

typedef struct { const char *name; size_t length; } Column;

typedef enum {
    PLAN_TABLE_SCAN,
    PLAN_PK_LOOKUP,
    PLAN_FILTER,
    PLAN_PROJECTION,
    PLAN_INSERT
} PlanNodeType;

typedef struct PlanNode {
    PlanNodeType type;
    union {
        struct {
            uint32_t root_page_num;
        } table_scan;

        struct {
            uint32_t root_page_num;
            int64_t key;
        } pk_lookup;

        struct {
            struct PlanNode *child;
            AstNode *condition;      // points at the WHERE clause's EXPR_EQUALS node
        } filter;

        struct {
            struct PlanNode *child;
            AstNode **columns;       // points at tree->as.select.columns
            size_t column_count;
        } projection;

        struct {
            uint32_t root_page_num;
            AstNode **values;        // points at tree->as.insert.values
            size_t value_count;
        } insert;
    } as;
} PlanNode;

void build_plan(PlanNode* plan_node, AstNode *tree, Table* table);

void ast_destroy(AstNode *node);

void ast_print(AstNode *node);

void print_table_schema(Pager *pager, char* table_name);

void plan_destroy(PlanNode *node); 

#endif //DATABASE_IN_C_AST_H
