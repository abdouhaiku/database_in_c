#include "btree.h"


void leaf_node_init(void *page) {
    *((uint8_t *) page + NODE_TYPE_OFFSET) = NODE_LEAF;
    *((uint8_t *) page + IS_ROOT_OFFSET) = 1;
    uint32_t *num_cells_ptr = leaf_node_num_cells(page);
    *num_cells_ptr = 0;
    *(uint32_t *) ((uint8_t *) page + NEXT_LEAF_OFFSET) = 0;
}

uint32_t *leaf_node_num_cells(void *page) {
    //reminder: page come as a void*, we need to cast it to (uint8_t *) so
    // that we can advance the pointer to the offset number. pointer arithmetic would not be possible without this cast
    return (uint32_t *) ((uint8_t *) page + NUM_CELLS_OFFSET);
}

uint8_t *leaf_node_cell(void *page, uint32_t cell_num) {
    return (uint8_t *) page + LEAF_NODE_HEADER_SIZE + (cell_num * LEAF_NODE_CELL_SIZE);
}

int64_t *leaf_node_key(void *page, uint32_t cell_num) {
    return (int64_t *) leaf_node_cell(page, cell_num);
}

uint8_t *leaf_node_value(void *page, uint32_t cell_num) {
    return leaf_node_cell(page, cell_num) + VALUE_OFFSET;
}


uint32_t leaf_node_find(void *page, int64_t key) {
    uint32_t low = 0;
    uint32_t high = *leaf_node_num_cells(page);

    while (low < high) {
        uint32_t mid = low + (high - low) / 2;

        int64_t node_key = *leaf_node_key(page, mid);
        if (node_key == key) {
            return mid;
        }
        if (node_key < key) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    // if there is no exact match return low which will be the insertion point
    return low;

}
