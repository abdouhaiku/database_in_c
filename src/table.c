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

void deserialize_row(const uint8_t *source, Row *row, Table *table);

CommandResult insert_command(AstNode *tree, Table *table) {
    // Validate the values
    // 1. First check if the values count matches the table defintion columns.
    void *catalog_page = pager_get_page(table->pager, 0);
    uint32_t table_num = catalog_node_find_table(catalog_page, tree->as.insert.table, tree->as.insert.table_length);
    if (*catalog_node_column_count(pager_get_page(table->pager, 0), table_num) != tree->as.insert.values_count) {
        printf("The values count does not match the column count definitions! \n");
        return -1;
    }

    for (size_t i = 0; i < tree->as.insert.values_count; i++) {
        uint8_t stored_type = *(catalog_node_table(catalog_page, table_num) + TABLE_COLUMNS_OFFSET + (i * COLUMN_SIZE) +
                                COLUMN_NAME_SIZE);

        bool matches =
                (stored_type == COLUMN_INTEGER && tree->as.insert.values[i]->type == EXPR_LITERAL_INT) ||
                (stored_type == COLUMN_TEXT && tree->as.insert.values[i]->type == EXPR_LITERAL_STRING) ||
                (stored_type == COLUMN_BOOLEAN && tree->as.insert.values[i]->type == EXPR_LITERAL_BOOLEAN);

        if (!matches) {
            printf("Type mismatch, exit the program\n");
            return -1;
        }
    }

    Row row;
    memset(&row, 0, sizeof(row)); // zero every value slot so unused text bytes are real padding
    row.value_count = tree->as.insert.values_count;
    // Get the primary column
    int primary_column_index = *catalog_node_primary_key_column(catalog_page, table_num);
    // Get the column value from the tree
    int64_t primary_key = tree->as.insert.values[primary_column_index]->as.literal_int;

    uint32_t leaf_page_num;
    void *leaf_page = leaf_node_for_key(table->pager, pager_get_page(table->pager, table->root_page_num),
                                        primary_key, &leaf_page_num, table->root_page_num);

    if (leaf_page == NULL) {
        printf("Could not get the leaf page\n");
        return -1;
    }

    uint32_t cell_num = leaf_node_find(leaf_page, primary_key);
    if (cell_num < *leaf_node_num_cells(leaf_page) && *leaf_node_key(leaf_page, cell_num) == primary_key) {
        printf("Id %ld is duplicated\n", primary_key);
        return ID_DUPLICATE_ERROR;
    }

    for (size_t i = 0; i < tree->as.insert.values_count; i++) {
        switch (tree->as.insert.values[i]->type) {
            case EXPR_LITERAL_INT:
                row.values[i].type = INTEGER_TYPE;
                row.values[i].integer = tree->as.insert.values[i]->as.literal_int;
                break;
            case EXPR_LITERAL_STRING:
                if (tree->as.insert.values[i]->as.literal_string.length > VALUE_TEXT_MAX_LEN) {
                    printf("Text value too long\n");
                    return -1;
                }
                row.values[i].type = TEXT_TYPE;
                memcpy(row.values[i].string.text, tree->as.insert.values[i]->as.literal_string.text,
                       tree->as.insert.values[i]->as.literal_string.length);
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
        insert_result = split_leaf_node(table, leaf_page, leaf_page_num, primary_key, &row);
    }
    if (insert_result != COMMAND_SUCCESS) {
        printf("Error: cannot insert\n");
        return insert_result;
    }
    return COMMAND_SUCCESS;
}

// column.name/literal_string.text are borrowed, non-null-terminated pointers
// into the raw input line.  comparisons must be length-bounded, never strcmp which causes the bug in earlier versions.
bool column_name_is(const AstNode *column, const char *name) {
    return column->as.column.length == strlen(name) &&
           strncasecmp(column->as.column.name, name, column->as.column.length) == 0;
}

static bool literal_string_equals(const AstNode *literal, const char *str) {
    return literal->as.literal_string.length == strlen(str) &&
           strncmp(literal->as.literal_string.text, str, literal->as.literal_string.length) == 0;
}

static bool row_matches_where(const AstNode *where, const Row *row, void *catalog_page, size_t table_num) {
    if (where == NULL) {
        return true;
    }
    const AstNode *column = where->as.equals.left;
    const AstNode *value = where->as.equals.right;
    for (size_t row_idx = 0; row_idx < row->value_count; row_idx++) {
        char column_name[32];
        memcpy(&column_name, catalog_node_table(catalog_page, table_num) + TABLE_COLUMNS_OFFSET +
               (row_idx*COLUMN_SIZE), 32);
        if (column_name_is(column, column_name)) {
            switch (row->values[row_idx].type) {
                case INTEGER_TYPE:
                    return value->as.literal_int == row->values[row_idx].integer;
                case TEXT_TYPE:
                    return literal_string_equals(value, row->values[row_idx].string.text);
                case BOOLEAN_TYPE:
                    return value->as.literal_boolean.bool_value == row->values[row_idx].bool_value;
            }
        }
    }
    return false;
}

CommandResult create_table(Pager *pager, AstNode *tree) {
    void *catalog_page = pager_get_page(pager, 0);
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
    memcpy(catalog_node_table(catalog_page, number_of_tables) + COLUMN_COUNT_OFFSET,
           &tree->as.create_table.column_count, 1);
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
    for (size_t i = 0; i < row->value_count; i++) {
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

void deserialize_row(const uint8_t *source, Row *row, Table *table) {
    // Get the column count from the table
    void *catalog_page = pager_get_page(table->pager, 0);
    uint32_t table_num = catalog_node_find_table(catalog_page, table->table_name, table->table_length);
    // Get the column defintions from the catalog page
    uint8_t column_count = *catalog_node_column_count(pager_get_page(table->pager, 0), table_num);

    row->value_count = column_count;
    int offset = 0;
    for (size_t i = 0; i < column_count; i++) {
        uint8_t stored_type = *(catalog_node_table(catalog_page, table_num) + TABLE_COLUMNS_OFFSET + (i * COLUMN_SIZE) +
                                COLUMN_NAME_SIZE);
        switch (stored_type) {
            case COLUMN_INTEGER:
                row->values[i].type = INTEGER_TYPE;
                memcpy(&row->values[i].integer, source + offset, sizeof(int64_t));
                offset += sizeof(int64_t);
                break;
            case COLUMN_TEXT:
                row->values[i].type = TEXT_TYPE;
                memcpy(row->values[i].string.text, source + offset, VALUE_TEXT_MAX_LEN);
                row->values[i].string.length = strnlen(row->values[i].string.text, VALUE_TEXT_MAX_LEN);
                offset += VALUE_TEXT_MAX_LEN;
                break;
            case COLUMN_BOOLEAN:
                row->values[i].type = BOOLEAN_TYPE;
                memcpy(&row->values[i].bool_value, source + offset, 1);
                offset += 1;
                break;
        }
    }
}


bool table_scan_next(Cursor *cursor, Table *table, Row *out_row) {
    if (cursor->as.table_scan.done) {
        return false;
    }

    // Get the page number
    uint32_t cell_num = cursor->as.table_scan.current_cell_num;
    uint32_t page_num = cursor->as.table_scan.current_page_num;

    void *curr = pager_get_page(table->pager, page_num);
    if (curr == NULL) {
        return false;
    }
    while (*((uint8_t *) curr + NODE_TYPE_OFFSET) == NODE_INTERNAL) {
        page_num = *internal_node_value(curr, 0);
        curr = pager_get_page(table->pager, page_num);
        if (curr == NULL) {
            return false;
        }
        cell_num = 0;
    }
    uint32_t num_cells = *leaf_node_num_cells(curr);
    if (cell_num >= num_cells) {
        uint32_t next_leaf_num = *(uint32_t *) ((uint8_t *) curr + NEXT_LEAF_OFFSET);
        if (next_leaf_num == 0) {
            cursor->as.table_scan.done = true;
            return false;
        }
        page_num = next_leaf_num;
        curr = pager_get_page(table->pager, page_num);
        if (curr == NULL) {
            return false;
        }
        cell_num = 0;
    }
    deserialize_row(leaf_node_value(curr, cell_num), out_row, table);
    cursor->as.table_scan.current_page_num = page_num;
    cursor->as.table_scan.current_cell_num = cell_num + 1;
    return true;
}

bool pk_lookup_next(Cursor *cursor, Table *table, Row *out_row) {
    if (cursor->as.pk_lookup.done) {
        return false;
    }
    int64_t key = cursor->as.pk_lookup.key;
    void *root_page = pager_get_page(table->pager, cursor->as.pk_lookup.root_page_num);
    if (root_page == NULL) {
        return false;
    }
    uint32_t out_page_num;
    void *leaf_page = leaf_node_for_key(table->pager, root_page, key, &out_page_num, cursor->as.pk_lookup.root_page_num);
    uint32_t cell_num = leaf_node_find(leaf_page, key);
    if (cell_num >= *leaf_node_num_cells(leaf_page) || *leaf_node_key(leaf_page, cell_num) != key) {
        cursor->as.pk_lookup.done = true;
        return false;
    }
    deserialize_row(leaf_node_value(leaf_page, cell_num), out_row, table);
    cursor->as.pk_lookup.done = true;
    return true;
}

bool cursor_next(Cursor *cursor, Table *table, Row *out_row) {
    switch (cursor->type) {
        case CURSOR_TABLE_SCAN:
            return table_scan_next(cursor, table, out_row);
        case CURSOR_PK_LOOKUP:
            return pk_lookup_next(cursor, table, out_row);
        case CURSOR_FILTER:
            return filter_next(cursor, table, out_row);
        case CURSOR_PROJECTION:
            return projection_next(cursor, table, out_row);
    }
    return false;
}


bool filter_next(Cursor *cursor, Table *table, Row *out_row) {
    void *catalog_page = pager_get_page(table->pager, 0);
    uint32_t table_num = catalog_node_find_table(catalog_page, table->table_name, table->table_length);
    while (cursor_next(cursor->as.filter.child, table, out_row)) {
        if (row_matches_where(cursor->as.filter.condition, out_row, catalog_page, table_num)) {
            return true;
        }
    }
    return false;
}

bool projection_next(Cursor *cursor, Table *table, Row *out_row) {
    Row full_row;
    if (!cursor_next(cursor->as.projection.child, table, &full_row)) {
        return false;
    }

    void *catalog_page = pager_get_page(table->pager, 0);
    uint32_t table_num = catalog_node_find_table(catalog_page, table->table_name, table->table_length);
    uint8_t *catalog_entry = catalog_node_table(catalog_page, table_num);

    out_row->value_count = cursor->as.projection.column_count;
    for (size_t i = 0; i < cursor->as.projection.column_count; i++) {
        AstNode *column = cursor->as.projection.columns[i];
        for (size_t j = 0; j < full_row.value_count; j++) {
            char name[32];
            memcpy(name, catalog_entry + TABLE_COLUMNS_OFFSET + (j * COLUMN_SIZE), 32);
            if (column_name_is(column, name)) {
                out_row->values[i] = full_row.values[j];
                break;
            }
        }
    }
    return true;
}
