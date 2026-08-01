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


uint32_t *leaf_node_num_cells(void *page);
uint8_t *leaf_node_cell(void *page, uint32_t cell_num);
int64_t *leaf_node_key(void *page, uint32_t cell_num);
uint8_t *leaf_node_value(void *page, uint32_t cell_num);
#endif //DATABASE_IN_C_BTREE_H
