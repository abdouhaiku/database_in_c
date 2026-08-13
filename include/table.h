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

#define ROW_SIZE sizeof(Row)
#define ROWS_PER_PAGE (PAGE_SIZE / ROW_SIZE)

typedef struct {
    Pager *pager;
    uint32_t root_page_num;
    const char* table_name;
    size_t table_length;
} Table;

CommandResult insert_command(AstNode *tree, Table *table);

CommandResult select_all_command(Table *table);

CommandResult select_by_id(Table *table, int64_t key);

CommandResult select_columns_or_filter(Table *table, AstNode *tree);

CommandResult create_table(Pager *pager, AstNode *tree);

void serialize_row(const Row *row, uint8_t *destination);

void deserialize_row(const uint8_t *source, Row *row, Table *table);
#endif //DATABASE_IN_C_TABLE_H
