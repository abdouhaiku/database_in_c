#ifndef DATABASE_IN_C_TABLE_H
#define DATABASE_IN_C_TABLE_H

#include <stddef.h>

#include "command_result.h"
#include "pager.h"

typedef struct {
    long id;
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


#endif //DATABASE_IN_C_TABLE_H
