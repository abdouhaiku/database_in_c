#include "btree.h"

#include <stdio.h>
#include <string.h>

#include "command_result.h"
#include "table.h"


void leaf_node_init(void *page) {
    *((uint8_t *) page + NODE_TYPE_OFFSET) = NODE_LEAF;
    *((uint8_t *) page + IS_ROOT_OFFSET) = 1;
    uint32_t *num_cells_ptr = leaf_node_num_cells(page);
    *num_cells_ptr = 0;
    *(uint32_t *) ((uint8_t *) page + NEXT_LEAF_OFFSET) = 0;
}

void internal_node_init(void *page, int is_root) {
    *((uint8_t *) page + NODE_TYPE_OFFSET) = NODE_INTERNAL;
    *((uint8_t *) page + IS_ROOT_OFFSET) = is_root;
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

CommandResult leaf_node_insert(Table *table,void *page, uint32_t leaf_page_num, uint32_t cell_num, int64_t key, const Row *row) {
    if (*leaf_node_num_cells(page) >= LEAF_NODE_MAX_CELLS) {
        return LEAF_FULL_ERROR;
    }

    uint32_t num_cells = *leaf_node_num_cells(page);
    // Move data by cell_num + 1 to complete the shifting operation
    memmove(leaf_node_cell(page, cell_num + 1),
            leaf_node_cell(page, cell_num),
            (num_cells - cell_num) * LEAF_NODE_CELL_SIZE);
    int64_t *keyptr = leaf_node_key(page, cell_num);
    *keyptr = key;

    serialize_row(row, leaf_node_value(page, cell_num));
    *leaf_node_num_cells(page) = num_cells + 1;
    //mark the page as dirty
    pager_mark_dirty(table->pager, leaf_page_num);

    return COMMAND_SUCCESS;
}


void debug_leaf_node(void *page, Table* table) {
    // Search for the first leaf, and keep visiting the next leaf
    void *curr = page;
    while (*((uint8_t*) curr + NODE_TYPE_OFFSET) == NODE_INTERNAL) {
        curr = pager_get_page(table->pager, *internal_node_value(curr, 0));
    }
    // curr is the first leaf
    int i = 1;
    while (curr != NULL) {
        uint32_t num_cells = *leaf_node_num_cells(curr);
        printf("Leaf %d ( size %u)\n", i, num_cells);
        for (uint32_t i = 0; i < num_cells; i++) {
            printf("- %lld\n", *leaf_node_key(curr, i));
        }
        i++;
        uint32_t next_leaf_num = *(uint32_t*)((uint8_t*) curr + NEXT_LEAF_OFFSET);
        if (next_leaf_num == 0) break;
        curr = pager_get_page(table->pager, next_leaf_num);
    }
}


uint32_t *internal_node_num_cells(void *page) {
    return (uint32_t *) ((uint8_t *) page + INTERNAL_NODE_NUM_KEYS_OFFSET);
}

uint8_t *internal_node_cell(void *page, uint32_t cell_num) {
    return (uint8_t *) page + INTERNAL_NODE_HEADER_SIZE + (cell_num * INTERNAL_NODE_CELL_SIZE);
}

int64_t *internal_node_key(void *page, uint32_t cell_num) {
    return (int64_t *) internal_node_cell(page, cell_num);
}

uint32_t *internal_node_value(void *page, uint32_t cell_num) {
    return (uint32_t *) ((uint8_t *) internal_node_key(page, cell_num) + INTERNAL_NODE_KEY_SIZE);
}

uint32_t internal_node_find(void *page, int64_t key) {
    uint32_t low = 0;
    uint32_t high = *internal_node_num_cells(page);

    while (low < high) {
        uint32_t mid = low + ((high - low) / 2);
        int64_t key_value = *internal_node_key(page, mid);

        if (key == key_value) {
            return mid;
        }
        if (key_value < key) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    // the index of the first existing key that's ≥ the new key
    return low;
}

uint8_t *leaf_node_for_key(Pager *pager, void *page, int64_t key, uint32_t *out_page_num) {
    uint8_t *curr = page; // page is the root node
    *out_page_num = 0;
    while (*(curr + NODE_TYPE_OFFSET) != NODE_LEAF) {
        uint32_t num_cells = *internal_node_num_cells(curr);
        uint32_t i = num_cells;
        while (i > 0 && *internal_node_key(curr, i - 1) > key) {
            i--;
        } //Get the associated page from the cell_num=i
        uint32_t child_page_num = (i == num_cells)
            ? *(uint32_t *) ((uint8_t *) curr + INTERNAL_NODE_RIGHT_CHILD_OFFSET)
            : *internal_node_value(curr, i);
        *out_page_num = child_page_num;
        curr = (uint8_t *) pager_get_page(pager, child_page_num);
        if (curr == NULL) {
            return NULL;
        }
    }
    return curr;
}

CommandResult internal_node_insert(Pager *pager, void *page, uint32_t page_num, uint32_t cell_num, int64_t key,
                                   uint32_t left_child_page_num, uint32_t right_child_page_num) {
    if (*internal_node_num_cells(page) >= INTERNAL_NODE_MAX_KEYS) {
        return INTERNAL_NODE_FULL_ERROR;
    }


    uint32_t num_cells = *internal_node_num_cells(page);
    // Move data by cell_num + 1 to complete the shifting operation
    memmove(internal_node_cell(page, cell_num + 1),
            internal_node_cell(page, cell_num),
            (num_cells - cell_num) * INTERNAL_NODE_CELL_SIZE);

    if (cell_num == num_cells) {
        // Inserting past every existing key: the new right child becomes the node's
        // new rightmost child, replacing what RIGHT_CHILD_OFFSET used to hold.
        *(uint32_t *) ((uint8_t *) page + INTERNAL_NODE_RIGHT_CHILD_OFFSET) = right_child_page_num;
    } else {
        *internal_node_value(page, cell_num + 1) = right_child_page_num;
    }

    *internal_node_key(page, cell_num) = key;
    *internal_node_value(page, cell_num) = left_child_page_num;
    *internal_node_num_cells(page) = num_cells + 1;
    pager_mark_dirty(pager, page_num);
    return COMMAND_SUCCESS;
}


CommandResult split_internal_node(Pager *pager, void *old_page, uint32_t old_page_num, int64_t new_key,
                                  uint32_t left_child_page_num, uint32_t right_child_page_num) {
    int64_t temp_keys[INTERNAL_NODE_MAX_KEYS + 1];
    uint32_t temp_children[INTERNAL_NODE_MAX_KEYS + 2];

    uint32_t cell_num = internal_node_find(old_page, new_key);

    //Copy all the children node in temp_keys
    for (uint32_t i = 0; i < *internal_node_num_cells(old_page); i++) {
        temp_keys[i] = *internal_node_key(old_page, i);
        temp_children[i] = *internal_node_value(old_page, i);
    }

    temp_children[INTERNAL_NODE_MAX_KEYS] = *(uint32_t *) ((uint8_t *) old_page + INTERNAL_NODE_RIGHT_CHILD_OFFSET);

    memmove(temp_keys+cell_num+1,
            temp_keys+cell_num,
            (INTERNAL_NODE_MAX_KEYS - cell_num) * sizeof(int64_t));

    memmove(temp_children+cell_num+1,
            temp_children+cell_num,
            (INTERNAL_NODE_MAX_KEYS - cell_num + 1) * sizeof(uint32_t));

    temp_keys[cell_num] = new_key;
    temp_children[cell_num] = left_child_page_num;
    temp_children[cell_num + 1] = right_child_page_num;

    uint32_t mid = INTERNAL_NODE_MAX_KEYS / 2;
    int64_t promoted = temp_keys[mid];

    if (*((uint8_t*)old_page + IS_ROOT_OFFSET) == 1) {
        // old_page stays root forever, it can't become a child of anything, so
        // both halves need brand-new pages, and old_page itself gets rewritten as the new root.
        uint32_t left_internal_page_num = pager->num_pages;
        void *left_internal_page = pager_get_page(pager, left_internal_page_num);
        if (left_internal_page == NULL) {
            return -1;
        }
        uint32_t right_internal_page_num = pager->num_pages;
        void *right_internal_page = pager_get_page(pager, right_internal_page_num);
        if (right_internal_page == NULL) {
            return -1;
        }
        internal_node_init(left_internal_page, 0);
        internal_node_init(right_internal_page, 0);

        for (uint32_t i = 0; i < mid; i++) {
            *internal_node_key(left_internal_page, i) = temp_keys[i];
            *internal_node_value(left_internal_page, i) = temp_children[i];
        }
        for (uint32_t i = 0; i < INTERNAL_NODE_MAX_KEYS - mid; i++) {
            *internal_node_key(right_internal_page, i) = temp_keys[mid + 1 + i];
            *internal_node_value(right_internal_page, i) = temp_children[mid + 1 + i];
        }

        *(uint32_t *) ((uint8_t *) left_internal_page + INTERNAL_NODE_RIGHT_CHILD_OFFSET) = temp_children[mid];
        *(uint32_t *) ((uint8_t *) right_internal_page + INTERNAL_NODE_RIGHT_CHILD_OFFSET) = temp_children[INTERNAL_NODE_MAX_KEYS + 1];
        *internal_node_num_cells(left_internal_page) = mid;
        *internal_node_num_cells(right_internal_page) = INTERNAL_NODE_MAX_KEYS - mid;
        *(uint32_t *) ((uint8_t *) left_internal_page + PARENT_POINTER_OFFSET) = old_page_num;
        *(uint32_t *) ((uint8_t *) right_internal_page + PARENT_POINTER_OFFSET) = old_page_num;

        // old_page is becoming the parent of BOTH halves now (it wasn't the parent of either
        // before it was the whole node), so every one of its old children needs its parent
        // pointer updated, not just the ones that moved to the right side.
        for (uint32_t j = 0; j <= INTERNAL_NODE_MAX_KEYS + 1; j++) {
            void *moved_child = pager_get_page(pager, temp_children[j]);
            if (moved_child == NULL) {
                return -1;
            }
            uint32_t new_parent = (j <= mid) ? left_internal_page_num : right_internal_page_num;
            *(uint32_t *) ((uint8_t *) moved_child + PARENT_POINTER_OFFSET) = new_parent;
        }

        // Rewrite old_page itself as the new internal root, pointing at both new children
        memset((uint8_t *) old_page + INTERNAL_NODE_HEADER_SIZE, 0, INTERNAL_NODE_SPACE_FOR_CELLS);
        internal_node_init(old_page, 1);
        *internal_node_key(old_page, 0) = promoted;
        *internal_node_value(old_page, 0) = left_internal_page_num;
        *internal_node_num_cells(old_page) = 1;
        *(uint32_t *) ((uint8_t *) old_page + INTERNAL_NODE_RIGHT_CHILD_OFFSET) = right_internal_page_num;

        // mark the new right/left + root dirty
        pager_mark_dirty(pager, left_internal_page_num);
        pager_mark_dirty(pager, right_internal_page_num);
        pager_mark_dirty(pager, old_page_num);
        return COMMAND_SUCCESS;
    }

    uint32_t* parent_page_num = (uint32_t *) ((uint8_t *) old_page + PARENT_POINTER_OFFSET);
    if (parent_page_num == NULL) {
        return -1;
    }

    memset((uint8_t*)old_page + INTERNAL_NODE_HEADER_SIZE, 0, INTERNAL_NODE_SPACE_FOR_CELLS);
    uint32_t right_internal_page_num = pager->num_pages;
    void *right_internal_page = pager_get_page(pager, pager->num_pages);
    if (right_internal_page == NULL) {
        return -1;
    }
    void *left_internal_page = old_page;
    uint32_t left_internal_page_num = old_page_num;
    internal_node_init(right_internal_page, 0);
    internal_node_init(left_internal_page, 0);
    // Left gets temp[0 .. mid-1] mid entries, same indices as temp itself
    for (uint32_t i = 0; i < mid; i++) {
        *internal_node_key(left_internal_page, i) = temp_keys[i];
        *internal_node_value(left_internal_page, i) = temp_children[i];
    }

    // Right gets temp[mid+1 .. INTERNAL_NODE_MAX_KEYS] skipping the promoted key at mid
    // but starting at right's OWN index 0, not offset by mid
    for (uint32_t i = 0; i < INTERNAL_NODE_MAX_KEYS - mid; i++) {
        *internal_node_key(right_internal_page, i) = temp_keys[mid + 1 + i];
        *internal_node_value(right_internal_page, i) = temp_children[mid + 1 + i];
    }

    *(uint32_t *) ((uint8_t *) left_internal_page + INTERNAL_NODE_RIGHT_CHILD_OFFSET) = temp_children[mid];
    *(uint32_t *) ((uint8_t *) right_internal_page + INTERNAL_NODE_RIGHT_CHILD_OFFSET) = temp_children[INTERNAL_NODE_MAX_KEYS + 1];

    // Every child from mid+1 onward (the cell-paired ones plus the trailing right_child)
    // moved from old_page to right_internal_page fix up their own parent pointer to match.
    // Children 0..mid stayed with old_page/left_internal_page, whose page number didn't change,
    // so their parent pointer is still correct as-is.
    for (uint32_t j = mid + 1; j <= INTERNAL_NODE_MAX_KEYS + 1; j++) {
        void *moved_child = pager_get_page(pager, temp_children[j]);
        if (moved_child == NULL) {
            return -1;
        }
        *(uint32_t *) ((uint8_t *) moved_child + PARENT_POINTER_OFFSET) = right_internal_page_num;
    }

    *(uint32_t *) ((uint8_t *) right_internal_page + PARENT_POINTER_OFFSET) = *parent_page_num;
    *(uint32_t *) ((uint8_t *) left_internal_page + PARENT_POINTER_OFFSET) = *parent_page_num;
    *internal_node_num_cells(left_internal_page) = mid;
    *internal_node_num_cells(right_internal_page) = INTERNAL_NODE_MAX_KEYS - mid;

    void *parent_page = pager_get_page(pager, *parent_page_num);
    if (parent_page == NULL) {
        return -1;
    }
    //mark all the pages left+right dirty
    pager_mark_dirty(pager, left_internal_page_num);
    pager_mark_dirty(pager, right_internal_page_num);

    uint32_t new_cell_num = internal_node_find(parent_page, promoted);
    if (internal_node_insert(pager, parent_page, *parent_page_num, new_cell_num, promoted, old_page_num, right_internal_page_num) ==
        INTERNAL_NODE_FULL_ERROR) {
            return split_internal_node(pager, parent_page, *parent_page_num, promoted, left_internal_page_num, right_internal_page_num);
        }

    return COMMAND_SUCCESS;

}


CommandResult split_leaf_node(Pager *pager, void *old_page, uint32_t old_page_num, int64_t new_key,
                              const Row *new_row) {
    typedef struct {
        Row row;
        int64_t key;
    } NodeEntry;

    uint32_t left_child_page_num = 0;
    uint32_t right_child_page_num = 0;
    //copy all the keys of the old page plus the new keys into a temp buffer
    NodeEntry temp[LEAF_NODE_MAX_CELLS + 1];
    for (uint32_t i = 0; i < LEAF_NODE_MAX_CELLS; i++) {
        temp[i].key = *leaf_node_key(old_page, i);
        deserialize_row(leaf_node_value(old_page, i), &temp[i].row);
    }
    //insert the new key into the correct position
    uint32_t i = LEAF_NODE_MAX_CELLS;
    while (i > 0 && temp[i - 1].key > new_key) {
        temp[i] = temp[i - 1];
        i--;
    }
    temp[i].key = new_key;
    memcpy(&temp[i].row, new_row, sizeof(Row));

    // All the keys are inserted we can get the promoted key
    uint32_t mid = (LEAF_NODE_MAX_CELLS + 1) / 2;
    NodeEntry promoted = temp[mid];
    // create a new leaf node
    right_child_page_num = pager->num_pages;
    void *right_child_page = pager_get_page(pager, right_child_page_num);
    if (right_child_page == NULL) {
        return -1;
    }
    leaf_node_init(right_child_page);
    *((uint8_t *) right_child_page + IS_ROOT_OFFSET) = 0;
    uint32_t right_num_cells = LEAF_NODE_MAX_CELLS - mid + 1;
    int is_root_split = *((uint8_t *) old_page + IS_ROOT_OFFSET) == 1;
    *(uint32_t *) ((uint8_t *) right_child_page + PARENT_POINTER_OFFSET) = is_root_split
        ? old_page_num
        : *(uint32_t *) ((uint8_t *) old_page + PARENT_POINTER_OFFSET);

    //copy all the right cells to the new leaf
    for (uint32_t index = 0; index < right_num_cells; index++) {
        *leaf_node_key(right_child_page, index) = temp[mid + index].key;
        serialize_row(&temp[mid + index].row, leaf_node_value(right_child_page, index));
        *leaf_node_num_cells(right_child_page) = *leaf_node_num_cells(right_child_page) + 1;
    }
    pager_mark_dirty(pager, right_child_page_num);


    // If it is a root and a leaf
    if (*((uint8_t *) old_page + IS_ROOT_OFFSET) == 1) {
        left_child_page_num = pager->num_pages;
        void *left_child = pager_get_page(pager, left_child_page_num);
        if (left_child == NULL) {
            return -1;
        }
        leaf_node_init(left_child);
        *((uint8_t *) left_child + IS_ROOT_OFFSET) = 0;
        //copy all of content of the old page to the left child page
        for (uint32_t index = 0; index < mid; index++) {
            *leaf_node_key(left_child, index) = temp[index].key;
            serialize_row(&temp[index].row, leaf_node_value(left_child, index));
        }
        *leaf_node_num_cells(left_child) = mid;
        *(uint32_t *) ((uint8_t *) left_child + NEXT_LEAF_OFFSET) = right_child_page_num;
        //Update the parent of the left child
        *(uint32_t *) ((uint8_t *) left_child + PARENT_POINTER_OFFSET) = 0;
        pager_mark_dirty(pager, left_child_page_num);

        // Reset the old page which is the new root
        memset((uint8_t*)old_page + LEAF_NODE_HEADER_SIZE, 0, LEAF_NODE_SPACE_FOR_CELLS);
        leaf_node_init(old_page);
        *((uint8_t *) old_page + NODE_TYPE_OFFSET) = NODE_INTERNAL;
        *internal_node_key(old_page, 0) = promoted.key;
        //page number of the left child
        *internal_node_value(old_page, 0) = left_child_page_num;
        *internal_node_num_cells(old_page) = 1;
        *(uint32_t *) ((uint8_t *) left_child + NEXT_LEAF_OFFSET) = right_child_page_num;
        *(uint32_t *) ((uint8_t *) old_page + INTERNAL_NODE_RIGHT_CHILD_OFFSET) = right_child_page_num;
        pager_mark_dirty(pager, old_page_num);
        return COMMAND_SUCCESS;
    }

    //if the node in old page is a leaf but not a node, reset the old page and leave the left half + insert the
    //promoted key in parent of the node page which is an internal node
    left_child_page_num = old_page_num;
    memset((uint8_t*)old_page + LEAF_NODE_HEADER_SIZE, 0, LEAF_NODE_SPACE_FOR_CELLS);
    *leaf_node_num_cells(old_page) = mid;
    for (uint32_t index = 0; index < mid; index++) {
        *leaf_node_key(old_page, index) = temp[index].key;
        serialize_row(&temp[index].row, leaf_node_value(old_page, index));
    }
    pager_mark_dirty(pager, left_child_page_num);
    uint32_t parent_page_num = *(uint32_t *) ((uint8_t *) old_page + PARENT_POINTER_OFFSET);

    //in case there is more than one leaf
    // Leaf A (page 5) --next_leaf--> Leaf B (page 3) --next_leaf--> Leaf C (page 7) --next_leaf--> 0
    //old page is Leaf B
    uint32_t old_next_leaf = *(uint32_t *) ((uint8_t *) old_page + NEXT_LEAF_OFFSET); // save 7, before it's gone
    *(uint32_t *) ((uint8_t *) right_child_page + NEXT_LEAF_OFFSET) = old_next_leaf; // right_child now points at C
    *(uint32_t *) ((uint8_t *) old_page + NEXT_LEAF_OFFSET) = right_child_page_num;
    // old_page now points at right_child
    //insert the seperator key inside the parent node
    void *parent_page = pager_get_page(pager, parent_page_num);
    if (parent_page == NULL) {
        return -1;
    }
    uint32_t cell_num = internal_node_find(parent_page, promoted.key);
    if (internal_node_insert(pager, parent_page, parent_page_num, cell_num, promoted.key, left_child_page_num, right_child_page_num) ==
        INTERNAL_NODE_FULL_ERROR) {
        return split_internal_node(pager, parent_page, parent_page_num,
            promoted.key, left_child_page_num, right_child_page_num);
    }

    return COMMAND_SUCCESS;
}
