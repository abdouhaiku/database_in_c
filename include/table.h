#ifndef DATABASE_IN_C_TABLE_H
#define DATABASE_IN_C_TABLE_H

#include <stddef.h>
#include <stdint.h>

#include "ast.h"
#include "command_result.h"
#include "pager.h"

#define VALUE_TEXT_MAX_LEN 255 // one global cap for every TEXT column, for now

typedef enum {
    INTEGER_TYPE,
    TEXT_TYPE,
    BOOLEAN_TYPE
} ValueType;

typedef struct {
    ValueType type;
    union {
        int64_t integer;
        struct {
            char text[VALUE_TEXT_MAX_LEN];
            size_t length;
        } string;

        bool bool_value;
    };
} Value;

typedef struct {
    Value values[MAX_TABLE_COLUMNS];
    size_t value_count;
} Row;

typedef struct Table {
    Pager *pager;
    uint32_t root_page_num;
    const char* table_name;
    size_t table_length;
} Table;

CommandResult insert_command(AstNode *tree, Table *table);

CommandResult create_table(Pager *pager, AstNode *tree);

void serialize_row(const Row *row, uint8_t *destination);

void deserialize_row(const uint8_t *source, Row *row, Table *table);

bool column_name_is(const AstNode *column, const char *name);

bool table_scan_next(Cursor *cursor, Table *table, Row *out_row);

bool pk_lookup_next(Cursor *cursor, Table *table, Row *out_row);

bool filter_next(Cursor *cursor, Table *table, Row *out_row);

bool projection_next(Cursor *cursor, Table *table, Row *out_row);

bool cursor_next(Cursor *cursor, Table *table, Row *out_row);
#endif //DATABASE_IN_C_TABLE_H
