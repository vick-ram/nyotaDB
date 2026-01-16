// btree.h

#ifndef BTREE_H
#define BTREE_H

#include "storage.h"

#define BTREE_ORDER 4

typedef struct BTreeNode {
    uint32_t keys[BTREE_ORDER - 1];
    uint32_t children[BTREE_ORDER];
    uint32_t values[BTREE_ORDER - 1];
    uint32_t num_keys;
    bool is_leaf;
    uint32_t page_id;
} BTreeNode;

typedef struct {
    uint32_t root_page;
    TableSchema* schema;
    uint32_t key_column; // which column we are indexing on
} BTreeIndex;

BTreeIndex* btree_create_index(TableSchema* schema, uint32_t key_column);
bool btree_insert(StorageManager* sm, BTreeIndex* index, void* key, uint32_t value_page);
uint32_t btree_search(StorageManager* sm, BTreeIndex* index, void* key);
bool btree_delete(StorageManager* sm, BTreeIndex* index, void* key);
void btree_free_index(BTreeIndex* index);

#endif // BTREE_H