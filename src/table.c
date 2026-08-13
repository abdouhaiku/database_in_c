#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include "command.h"
#include "table.h"

#include <limits.h>
#include <string.h>

#include "ast.h"
#include "btree.h"
#include "pager.h"




void serialize_row(const Row *row, uint8_t *destination);
void deserialize_row(const uint8_t *source, Row *row);

CommandResult insert_command(AstNode *tree, Table *table) {
    // Validate the values
     // 1. First check if the values count matches the table defintion columns.
    void* catalog_page = pager_get_page(table->pager, 0);
    uint32_t table_num = catalog_node_find_table(catalog_page, tree->as.insert.table, tree->as.insert.table_length);
    if (*catalog_node_column_count(pager_get_page(table->pager, 0),table_num) != tree->as.insert.values_count) {
        printf("The values count does not match the column count definitions! \n");
        return -1;
    }

    for (size_t i = 0; i< tree->as.insert.values_count; i++) {
        uint8_t stored_type = *(catalog_node_table(catalog_page, table_num) + TABLE_COLUMNS_OFFSET + (i*COLUMN_SIZE) + COLUMN_NAME_SIZE);

        bool matches =
            (stored_type == COLUMN_INTEGER && tree->as.insert.values[i]->type == EXPR_LITERAL_INT)   ||
            (stored_type == COLUMN_TEXT    && tree->as.insert.values[i]->type == EXPR_LITERAL_STRING) ||
            (stored_type == COLUMN_BOOLEAN && tree->as.insert.values[i]->type == EXPR_LITERAL_BOOLEAN);

        if (!matches) {
            printf("Type mismatch, exit the program\n");
            return -1;
        }
    }

    Row row;
    // Get the primary column
    int primary_column_index = *catalog_node_primary_key_column(catalog_page, table_num);
    // Get the column value from the tree
    int64_t primary_key = tree->as.insert.values[primary_column_index]->as.literal_int;

    uint32_t leaf_page_num;
    void *leaf_page = leaf_node_for_key(table->pager, pager_get_page(table->pager,table->root_page_num),
        primary_key, &leaf_page_num);

    if (leaf_page == NULL) {
        printf("Could not get the leaf page\n");
        return -1;
    }

    uint32_t cell_num = leaf_node_find(leaf_page, primary_key);
    if (cell_num < *leaf_node_num_cells(leaf_page) && *leaf_node_key(leaf_page, cell_num) == primary_key) {
        printf("Id %ld is duplicated\n", primary_key);
        return ID_DUPLICATE_ERROR;
    }

    for (size_t i = 0; i< tree->as.insert.values_count; i++) {
        switch (tree->as.insert.values[i]->type) {
            case EXPR_LITERAL_INT:
                row.values[i].type = INTEGER_TYPE;
                row.values[i].integer = tree->as.insert.values[i]->as.literal_int;
                break;
            case EXPR_LITERAL_STRING:
                row.values[i].type = TEXT_TYPE;
                memcpy(row.values[i].string.text, tree->as.insert.values[i]->as.literal_string.text, tree->as.insert.values[i]->as.literal_string.length);
                row.values[i].string.length = tree->as.insert.values[i]->as.literal_string.length;
                break;
            case EXPR_LITERAL_BOOLEAN:
                row.values[i].type = BOOLEAN_TYPE;
                row.values[i].bool_value = tree->as.insert.values[i]->as.literal_boolean.bool_value;
                break;
        }
    }

    CommandResult insert_result = leaf_node_insert(table, leaf_page, leaf_page_num, cell_num, primary_key, &row);
    if (insert_result == LEAF_FULL_ERROR) {
        insert_result= split_leaf_node(table->pager, leaf_page, leaf_page_num, primary_key, &row);
    }
    if (insert_result != COMMAND_SUCCESS) {
        printf("Error: cannot insert\n");
        return insert_result;
    }
    return COMMAND_SUCCESS;
}

CommandResult select_all_command(Table *table) {
    // Find the first leaf
    void *curr = pager_get_page(table->pager, table->root_page_num);
    if (curr == NULL) {
        return -1;
    }
    uint32_t current_count = 0;
    while (*((uint8_t*) curr + NODE_TYPE_OFFSET) == NODE_INTERNAL) {
        curr = pager_get_page(table->pager, *internal_node_value(curr, 0));
    }
    // curr is the first leaf node page
    while (curr != NULL) {
        uint32_t num_cells = *leaf_node_num_cells(curr);
        for (uint32_t i = 0; i< num_cells; i++) {
            Row row;
            deserialize_row(leaf_node_value(curr, i), &row);
            printf("Row %u : %ld, %s, %s\n", current_count, row.id, row.username, row.email);
            current_count++;
        }
        uint32_t next_leaf_num = *(uint32_t*)((uint8_t*) curr + NEXT_LEAF_OFFSET);
        if (next_leaf_num == 0) break;
        curr = pager_get_page(table->pager, next_leaf_num);
    }

    return COMMAND_SUCCESS;
}
CommandResult select_by_id(Table *table, int64_t key) {
    uint32_t out_page_num;
    void *leaf_page = leaf_node_for_key(table->pager, pager_get_page(table->pager, table->root_page_num), key, &out_page_num);
    uint32_t cell_num = leaf_node_find(leaf_page, key);
    if (cell_num >= *leaf_node_num_cells(leaf_page) || *leaf_node_key(leaf_page, cell_num) != key) {
        printf("No ID matches this key %ld", key);
        return -1;
    }
    Row row;
    deserialize_row(leaf_node_value(leaf_page, cell_num), &row);
    printf("Row that matches the ID %ld is : %s, %s\n", row.id, row.username, row.email);
    return COMMAND_SUCCESS;
}

// column.name/literal_string.text are borrowed, non-null-terminated pointers
// into the raw input line.  comparisons must be length-bounded, never strcmp which causes the bug in earlier versions.
static bool column_name_is(const AstNode *column, const char *name) {
    return column->as.column.length == strlen(name) &&
           strncasecmp(column->as.column.name, name, column->as.column.length) == 0;
}

static bool literal_string_equals(const AstNode *literal, const char *str) {
    return literal->as.literal_string.length == strlen(str) &&
           strncmp(literal->as.literal_string.text, str, literal->as.literal_string.length) == 0;
}

static bool row_matches_where(const AstNode *where, const Row *row) {
    if (where == NULL) {
        return true;
    }
    const AstNode *column = where->as.equals.left;
    const AstNode *value = where->as.equals.right;
    if (column_name_is(column, "id")) {
        return value->as.literal_int == row->id;
    }
    if (column_name_is(column, "username")) {
        return literal_string_equals(value, row->username);
    }
    if (column_name_is(column, "email")) {
        return literal_string_equals(value, row->email);
    }
    return false;
}

CommandResult select_columns_or_filter(Table *table, AstNode *tree) {
    // Find the first leaf
    void *curr = pager_get_page(table->pager, 0);
    if (curr == NULL) {
        return -1;
    }
    while (*((uint8_t*) curr + NODE_TYPE_OFFSET) == NODE_INTERNAL) {
        curr = pager_get_page(table->pager, *internal_node_value(curr, 0));
    }
    while (curr != NULL) {
        uint32_t num_cells = *leaf_node_num_cells(curr);
        for (uint32_t i = 0; i< num_cells; i++) {
            Row row;
            deserialize_row(leaf_node_value(curr, i), &row);
            if (!row_matches_where(tree->as.select.where, &row)) {
                continue;
            }
            if (tree->as.select.column_count > 0) {
                for (size_t col_idx = 0; col_idx < tree->as.select.column_count; col_idx++) {
                    AstNode *column = tree->as.select.columns[col_idx];
                    if (column_name_is(column, "id")) {
                        printf("%ld ", row.id);
                    }
                    else if (column_name_is(column, "username")) {
                        printf("%s ", row.username);
                    }
                    else if (column_name_is(column, "email")) {
                        printf("%s ", row.email);
                    }
                }
                printf("\n");
            }
            else {
                printf("%ld, %s, %s\n", row.id, row.username, row.email);
            }
        }
        uint32_t next_leaf_num = *(uint32_t*)((uint8_t*) curr + NEXT_LEAF_OFFSET);
        if (next_leaf_num == 0) break;
        curr = pager_get_page(table->pager, next_leaf_num);
    }
    return COMMAND_SUCCESS;
}

CommandResult create_table(Pager *pager, AstNode *tree) {
    void* catalog_page = pager_get_page(pager, 0);
    uint32_t number_of_tables = *catalog_node_num_tables(catalog_page);
    if (number_of_tables >= MAX_CATALOG_TABLES) {
        printf("MAX TABLES NUMBER REACHED! \n");
        return COMMAND_SUCCESS;
    }

    uint32_t page_num = pager_allocate_page(pager);
    leaf_node_init(pager_get_page(pager, page_num));

    memcpy(catalog_node_table(catalog_page, number_of_tables) + TABLE_NAME_OFFSET, tree->as.create_table.table,
        tree->as.create_table.table_length);
    memcpy(catalog_node_table(catalog_page, number_of_tables) + ROOT_PAGE_NUMBER_OFFSET, &page_num, 4);
    memcpy(catalog_node_table(catalog_page, number_of_tables) + COLUMN_COUNT_OFFSET, &tree->as.create_table.column_count, 1);
    for (size_t i = 0; i < tree->as.create_table.column_count; i++) {
        AstNode *column_def = tree->as.create_table.columns_definitions[i];
        //0-31 (table name) -> 32-35 (root page number) -> 36 (column count ) -> 37 (primary key index) -> 38 ( columns slots starts)
        uint8_t *slot = catalog_node_table(catalog_page, number_of_tables) + TABLE_COLUMNS_OFFSET
            + (i * (COLUMN_NAME_SIZE + COLUMN_TYPE_SIZE));

        size_t name_length = column_def->as.column_definition.length;
        if (name_length > COLUMN_NAME_SIZE) {
            printf("Column name too long\n");
            return COMMAND_SUCCESS;
        }
        memset(slot, 0, COLUMN_NAME_SIZE); // zero-pad so catalog_node_find_table's strlen stays correct
        memcpy(slot, column_def->as.column_definition.name, name_length); // 3, 8
        memcpy(slot + COLUMN_NAME_SIZE, &column_def->as.column_definition.columnType, COLUMN_TYPE_SIZE);

        if (column_def->as.column_definition.is_primary) {
            uint8_t index = (uint8_t) i;
            memcpy(catalog_node_primary_key_column(catalog_page, number_of_tables), &index, 1);
        }
    }

    *catalog_node_num_tables(catalog_page) = number_of_tables + 1;
    pager_mark_dirty(pager, 0);
    pager_mark_dirty(pager, page_num);

    return COMMAND_SUCCESS;
}


void serialize_row(const Row *row, uint8_t *destination) {
    int offset = 0;
    for (size_t i = 0; i< row->value_count; i++) {
        switch (row->values[i].type) {
            case INTEGER_TYPE:
                memcpy(destination + offset, &row->values[i].integer, sizeof(int64_t));
                offset += sizeof(int64_t);
                break;
            case TEXT_TYPE:
                memcpy(destination + offset, &row->values[i].string.text, VALUE_TEXT_MAX_LEN);
                offset += VALUE_TEXT_MAX_LEN;
                break;
            case BOOLEAN_TYPE:
                memcpy(destination + offset, &row->values[i].bool_value, 1);
                offset += 1;
                break;
        }
    }
}

void deserialize_row(const uint8_t *source, Row *row) {
    memcpy(&row->id, source, 8);
    memcpy(&row->username, source + 8, 25);
    memcpy(&row->email, source + 33, 255);
}

