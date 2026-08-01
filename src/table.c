#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include "command.h"
#include "table.h"

#include <limits.h>
#include <string.h>
#include "btree.h"
#include "pager.h"



static void *row_slot(Table *table, size_t row_number);

void serialize_row(const Row *row, uint8_t *destination);

void deserialize_row(const uint8_t *source, Row *row);


Table *table_open(const char *filename) {
    errno = 0;
    Table *table = malloc(sizeof(Table));
    if (table == NULL) {
        perror("Error in allocating the table struct\n");
        free(table);
        return NULL;
    }
    errno = 0;
    Pager *pager = pager_open(filename);
    if (pager == NULL) {
        free((table));
        return NULL;
    }

    bool is_new_file = (pager->file_length == 0);
    errno = 0;
    void* page0 = pager_get_page(pager, 0);
    if (page0 == NULL) {
        perror("Error in getting the page\n");
        pager_close(pager);
        free(table);
        return NULL;
    }

    if (is_new_file) {
        leaf_node_init(page0);
    }
    table->num_rows = pager->file_length / ROW_SIZE;
    table->pager = pager;
    return table;
}


CommandResult insert_command(char **tokens, Table *table, int total_tokens) {
    Row row;
    if (total_tokens > 4) {
        printf("Too many arguments to insert!\n");
        return COMMAND_SUCCESS;
    }

    errno = 0;
    char *endptr;
    long result = strtol(tokens[1], &endptr, 10);
    if (errno != 0) {
        perror("strtol");
        return COMMAND_SUCCESS;
    } else if (*endptr != '\0') {
        printf("Invalid number: '%s'\n", endptr);
        return COMMAND_SUCCESS;
    } else if (result < INT_MIN || result > INT_MAX) {
        printf("Value out of int range\n");
        return COMMAND_SUCCESS;
    } else {
        row.id = result;
        printf("Value = %ld\n", result);
    }

    void *page = pager_get_page(table->pager, 0);   // for now the one leaf always lives at page 0
    if (page == NULL) {
        return -1;
    }
    uint32_t cell_num = leaf_node_find(page, row.id);
    if (cell_num < *leaf_node_num_cells(page) && *leaf_node_key(page, cell_num) == row.id) {
        printf("Id %ld is duplicated\n", row.id);
        return ID_DUPLICATE_ERROR;
    }

    if (strlen(tokens[2]) >= sizeof(row.username)) {
        printf("Username too long\n");
        return COMMAND_SUCCESS;
    }
    strncpy(row.username, tokens[2],
            sizeof(row.username) - 1);
    row.username[sizeof(row.username) - 1] = '\0';
    if (strlen(tokens[3]) >= sizeof(row.email)) {
        printf("Email too long! \n");
        return COMMAND_SUCCESS;
    }
    strncpy(row.email, tokens[3], sizeof(row.email));
    row.email[sizeof(row.email) - 1] = '\0';
    CommandResult insert_result = leaf_node_insert(page, cell_num, row.id, &row);
    if (insert_result == LEAF_FULL_ERROR) {
        printf("Error: leaf node full, cannot insert\n");
        return insert_result;
    }
    pager_mark_dirty(table->pager, 0);
    return COMMAND_SUCCESS;
}

CommandResult select_all_command(Table *table) {
    for (size_t i = 0; i < table->num_rows; i++) {
        // deserialize the row
        Row row;
        memset(&row, 0, sizeof(row));
        uint8_t *row_ptr = row_slot(table, i);
        if (row_ptr == NULL) {
            return -1; 
        }
        deserialize_row(row_ptr, &row);
        printf("Row %zu : %ld, %s, %s\n", i, row.id, row.username, row.email);
    }
    return COMMAND_SUCCESS;
}


static void *row_slot(Table *table, size_t row_number) {
    uint32_t page_number = row_number / ROWS_PER_PAGE;
    void *page = pager_get_page(table->pager, page_number);
    if (page == NULL) {
        return NULL;
    }
    return (uint8_t *) page + (row_number % ROWS_PER_PAGE) * ROW_SIZE;
}

void serialize_row(const Row *row, uint8_t *destination) {
    memcpy(destination, &row->id, 8);
    memcpy(destination + 8, row->username, 25);
    memcpy(destination + 33, row->email, 255);
}

void deserialize_row(const uint8_t *source, Row *row) {
    memcpy(&row->id, source, 8);
    memcpy(&row->username, source + 8, 25);
    memcpy(&row->email, source + 33, 255);
}

void table_close(Table *table) {
    pager_close(table->pager);
    free(table);
}


