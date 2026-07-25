#ifndef DATABASE_IN_C_TABLE_H
#define DATABASE_IN_C_TABLE_H

#include <stddef.h>

#include "command_result.h"

typedef struct {
    long id;
    char username[25];
    char email[255];
} Row;

typedef struct {
    Row *rows;
    size_t num_rows;
    size_t capacity;
} Table;

CommandResult insert_command(char **tokens, Table *table, int total_tokens);

CommandResult select_all_command(Table *table);


#endif //DATABASE_IN_C_TABLE_H
