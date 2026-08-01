#ifndef DATABASE_IN_C_TABLE_H
#define DATABASE_IN_C_TABLE_H

#include <stddef.h>
#include <stdint.h>
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
    size_t num_rows;
} Table;

CommandResult insert_command(char **tokens, Table *table, int total_tokens);

CommandResult select_all_command(Table *table);

Table *table_open(const char *filename);
void   table_close(Table *table);
void serialize_row(const Row *row, uint8_t *destination);

#endif //DATABASE_IN_C_TABLE_H
