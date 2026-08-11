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
    Row row;

    errno = 0;
    long result = tree->as.insert.id;
    row.id = result;
    printf("Value = %ld\n", result);
    
    uint32_t leaf_page_num;
    void *leaf_page = leaf_node_for_key(table->pager, pager_get_page(table->pager,table->root_page_num),
        row.id, &leaf_page_num);

    if (leaf_page == NULL) {
        printf("Could not get the leaf page\n");
        return -1;
    }
    uint32_t cell_num = leaf_node_find(leaf_page, row.id);
    if (cell_num < *leaf_node_num_cells(leaf_page) && *leaf_node_key(leaf_page, cell_num) == row.id) {
        printf("Id %ld is duplicated\n", row.id);
        return ID_DUPLICATE_ERROR;
    }

    if (tree->as.insert.username_length >= sizeof(row.username)) {
        printf("Username too long\n");
        return COMMAND_SUCCESS;
    }
    memcpy(row.username, tree->as.insert.username, tree->as.insert.username_length);
    row.username[tree->as.insert.username_length] = '\0';
    if (tree->as.insert.email_length >= sizeof(row.email)) {
        printf("Email too long! \n");
        return COMMAND_SUCCESS;
    }
    memcpy(row.email, tree->as.insert.email, tree->as.insert.email_length);
    row.email[tree->as.insert.email_length] = '\0';
    // get the leaf node
    CommandResult insert_result = leaf_node_insert(table, leaf_page, leaf_page_num, cell_num, row.id, &row);
    if (insert_result == LEAF_FULL_ERROR) {
        insert_result= split_leaf_node(table->pager, leaf_page, leaf_page_num, row.id, &row);
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


