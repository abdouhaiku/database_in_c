//
// Created by Abdou on 01/08/2026.
//

#ifndef DATABASE_IN_C_BTREE_H
#define DATABASE_IN_C_BTREE_H

#include <stdint.h>
#include "table.h"

#define NODE_TYPE_OFFSET        0   // uint8_t, 1 byte
#define IS_ROOT_OFFSET          1   // uint8_t, 1 byte
#define PARENT_POINTER_OFFSET   2   // uint32_t, 4 bytes
#define NUM_CELLS_OFFSET        6   // uint32_t, 4 bytes
#define NEXT_LEAF_OFFSET        10  // uint32_t, 4 bytes
#define LEAF_NODE_HEADER_SIZE   14  // sum of the above

#define KEY_SIZE                8                                   // sizeof(int64_t)
#define KEY_OFFSET               0
#define VALUE_SIZE                ROW_SIZE
#define VALUE_OFFSET             (KEY_OFFSET + KEY_SIZE)             // 8
#define LEAF_NODE_CELL_SIZE      (KEY_SIZE + VALUE_SIZE)             // 296
#define LEAF_NODE_SPACE_FOR_CELLS (PAGE_SIZE - LEAF_NODE_HEADER_SIZE) // 4082
#define LEAF_NODE_MAX_CELLS      (LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE) // 13

// Internal keys offsets + size
#define INTERNAL_NODE_NUM_KEYS_OFFSET 6  // uint32_t, 4 bytes long same as NUM_CELLS_OFFSET
#define INTERNAL_NODE_RIGHT_CHILD_OFFSET 10 // uint32_t, 4 bytes long same as NEXNEXT_LEAF_OFFSET
#define INTERNAL_NODE_HEADER_SIZE 14
#define INTERNAL_NODE_CHILD_SIZE 4   // uint32_t, the size of the page number
#define INTERNAL_NODE_KEY_SIZE 8    //uint64_t, the size of the seperator key
#define INTERNAL_NODE_CELL_SIZE (INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE) // 12
#define INTERNAL_NODE_SPACE_FOR_CELLS (PAGE_SIZE - INTERNAL_NODE_HEADER_SIZE) //4082
#define INTERNAL_NODE_MAX_KEYS (INTERNAL_NODE_SPACE_FOR_CELLS / INTERNAL_NODE_CELL_SIZE) //340


#define TABLE_COUNT_OFFSET      0
#define CATALOG_ENTRIES_OFFSET  4   // page offset where table entry 0 begins

#define TABLE_NAME_OFFSET       0
#define ROOT_PAGE_NUMBER_OFFSET 32
#define COLUMN_COUNT_OFFSET     36
#define TABLE_COLUMNS_OFFSET    37
#define TABLE_HEADER_SIZE       37
#define COLUMN_NAME_SIZE        32
#define COLUMN_TYPE_SIZE        1
#define TABLE_ENTRY_SIZE        301 // (33 * 8) + TABLE_HEADER_SIZE
typedef enum {
    NODE_LEAF,
    NODE_INTERNAL
} NodeType;


typedef enum {
    COLUMN_INTEGER,
    COLUMN_TEXT,
    COLUMN_BOOLEAN
} ColumnType;

void leaf_node_init(void *page);

uint32_t *leaf_node_num_cells(void *page);

uint8_t *leaf_node_cell(void *page, uint32_t cell_num);

int64_t *leaf_node_key(void *page, uint32_t cell_num);

uint8_t *leaf_node_value(void *page, uint32_t cell_num);

uint32_t leaf_node_find(void *page, int64_t key);

// return the index where the node exist or the node where the key should be inserted
CommandResult leaf_node_insert(Table *table, void *page, uint32_t leaf_page_num, uint32_t cell_num, int64_t key, const Row *row);

void debug_leaf_node(void *page, Pager *pager);

// internal node accessors
void internal_node_init(void *page, int is_root);
uint32_t *internal_node_num_cells(void *page);

uint8_t *internal_node_cell(void *page, uint32_t cell_num);

int64_t *internal_node_key(void *page, uint32_t cell_num);

uint32_t *internal_node_value(void *page, uint32_t cell_num);

uint32_t internal_node_find(void *page, int64_t key);

CommandResult internal_node_insert(Pager *pager, void *page, uint32_t page_num, uint32_t cell_num, int64_t key,
                                   uint32_t left_child_page_num, uint32_t right_child_page_num);

CommandResult split_leaf_node(Pager *pager, void *old_page, uint32_t old_page_num, int64_t new_key, const Row *new_row);

CommandResult split_internal_node(Pager *pager, void *old_page, uint32_t old_page_num, int64_t new_key,
                                  uint32_t left_child_page_num, uint32_t right_child_page_num);

uint8_t *leaf_node_for_key(Pager *pager, void *page, int64_t key, uint32_t *out_page_num);

void catalog_node_init(void *page);

uint32_t *catalog_node_num_tables(void *page);

uint32_t catalog_node_find_table(void *page, const char *table, size_t table_length);

uint8_t* catalog_node_table(void *page, uint32_t table_num);

uint32_t *catalog_node_root_page(void *page, uint32_t table_num);

uint8_t *catalog_node_column_count(void *page, uint32_t table_num);

void open_database_and_initialize_catalog(char* filename, Pager *pager);
#endif //DATABASE_IN_C_BTREE_H
