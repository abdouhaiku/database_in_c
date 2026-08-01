
#include "btree.h"

uint32_t *leaf_node_num_cells(void *page) {
    //reminder: page come as a void*, we need to cast it to (uint8_t *) so
    // that we can advance the pointer to the offset number. pointer arithmetic would not be possible without this cast
    return (uint32_t *)((uint8_t *)page + NUM_CELLS_OFFSET);
}


uint8_t *leaf_node_cell(void *page, uint32_t cell_num) {
    return (uint8_t*)page + LEAF_NODE_HEADER_SIZE + (cell_num * LEAF_NODE_CELL_SIZE);
}
int64_t *leaf_node_key(void *page, uint32_t cell_num) {
    return (int64_t*) leaf_node_cell(page, cell_num);
}
uint8_t *leaf_node_value(void *page, uint32_t cell_num) {
    return leaf_node_cell(page, cell_num) + VALUE_OFFSET;
}
