#ifndef DATABASE_IN_C_TABLE_H
#define DATABASE_IN_C_TABLE_H

#include <stddef.h>
#include <stdint.h>

#include "ast.h"
#include "command_result.h"
#include "pager.h"

#define ROW_SIZE 288
#define ROWS_PER_PAGE (PAGE_SIZE / ROW_SIZE)

typedef struct {
    int64_t id;
    char username[25];
    char email[255];
} Row;

typedef struct {
    Pager *pager;
    uint32_t root_page_num;
} Table;

CommandResult insert_command(AstNode *tree, Table *table);

CommandResult select_all_command(Table *table);
CommandResult select_by_id(Table *table, int64_t key);
CommandResult select_columns_or_filter(Table *table, AstNode *tree);

void serialize_row(const Row *row, uint8_t *destination);
void deserialize_row(const uint8_t *source, Row *row);

#endif //DATABASE_IN_C_TABLE_H
