#include "btree.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

static void node_to_page(BTreeNode* node, Page* page);
static void page_to_node(Page* page, BTreeNode* node);
static uint32_t create_new_node(StorageManager* sm, bool is_leaf);
static uint32_t key_to_hash(DataType type, void* key);
static int compare_hashes(uint32_t hash1, uint32_t hash2);

BTreeIndex* btree_create_index(TableSchema* schema, uint32_t key_column) {
    BTreeIndex* index = SAFE_MALLOC(BTreeIndex, 1);
    
    index->schema = schema;
    index->key_column = key_column;
    
    return index;
}

uint32_t btree_search(StorageManager* sm, BTreeIndex* index, void* key) {
    if (sm->header.root_page == 0) return 0;

    // 1. Hash the key once at the start
    DataType key_type = index->schema->columns[index->key_column].type;
    uint32_t search_hash = key_to_hash(key_type, key);

    uint32_t current_page_id = sm->header.root_page;

    while (true) {
        Page* page = sm_get_page(sm, current_page_id);
        if (!page) return 0;

        BTreeNode node;
        page_to_node(page, &node);

        int i =  0;
        while (i < (int)node.num_keys && search_hash > node.keys[i]) {
            i++;
        }

        if (i < (int)node.num_keys && search_hash == node.keys[i]) {
            return node.values[i];
        }

        if (node.is_leaf) {
            return 0;
        }

        current_page_id = node.children[i];
    }
}

static void btree_split_child(StorageManager* sm, BTreeNode* parent, int i, BTreeNode* child) {
    uint32_t new_node_id = create_new_node(sm, child->is_leaf);
    Page* new_page = sm_get_page(sm, new_node_id);
    BTreeNode new_node;
    page_to_node(new_page, &new_node);
    
    int t = BTREE_ORDER / 2; // Midpoint
    new_node.num_keys = t - 1;
    
    // Copyseond half of keys/values to new node
    for (int j = 0; j < t - 1; j++) {
        new_node.keys[j] = child->keys[j + t];
        new_node.values[j] = child->values[j + t];
    }
    
    // Copy children if not leaf
    if (!child->is_leaf) {
        for (int j = 0; j < t; j++) {
            new_node.children[j] = child->children[j + t];
        }
    }
    
    child->num_keys = t - 1;
    
    // Shift parent children to make room for the new node
    for (int j = parent->num_keys; j >= i + 1; j--) {
        parent->children[j + 1] = parent->children[j];
    }

    parent->children[i + 1] = new_node_id;
    
    // Shift parent keys to move the median key up
    for (int j = parent->num_keys - 1; j >= i; j--) {
        parent->keys[j + 1] = parent->keys[j];
        parent->values[j + 1] = parent->values[j];
    }
    parent->keys[i] = child->keys[t - 1];
    parent->values[i] = child->values[t - 1];
    parent->num_keys++;
    
    // Persist all changes
    node_to_page(&new_node, new_page);
    new_page->is_dirty = true;
    
    Page* child_page = sm_get_page(sm, child->page_id);
    node_to_page(child, child_page);
    child_page->is_dirty = true;
}

static void btree_insert_nonfull(StorageManager* sm, uint32_t page_id, uint32_t key, uint32_t value) {
    Page* p = sm_get_page(sm, page_id);
    BTreeNode node;
    page_to_node(p, &node);
    
    int i = node.num_keys - 1;
    
    if (node.is_leaf) {
        // Shift keys to the right to insert new key
        while (i >= 0 && compare_hashes(key, node.keys[i]) < 0) {
            node.keys[i + 1] = node.keys[i];
            node.values[i + 1] = node.values[i];
            i--;
        }
        node.keys[i + 1] = key;
        node.values[i + 1] = value;
        node.num_keys++;
        
        node_to_page(&node, p);
        p->is_dirty = true;
    } else {
        // Find child to descend into
        while (i >= 0 && key < node.keys[i]) {
            i--;
        }
        i++;
        
        Page* child_p = sm_get_page(sm, node.children[i]);
        BTreeNode child;
        page_to_node(child_p, &child);
        
        if (child.num_keys == BTREE_ORDER - 1) {
            btree_split_child(sm, &node, i, &child);
            // After split, check which child to go to
            if (compare_hashes(key, node.keys[i]) > 0) {
                i++;
            }
        }
        btree_insert_nonfull(sm, node.children[i], key, value);
        
        // Parent might have been modified by split
        node_to_page(&node, p);
        p->is_dirty = true;
    }
}

bool btree_insert(StorageManager* sm, BTreeIndex* index, void* key, uint32_t value_page) {
    if (!sm || !index || !key) return false;

    DataType key_type = index->schema->columns[index->key_column].type;
    uint32_t key_hash = key_to_hash(key_type, key);

    // Initial Tree Creation
    if (sm->header.root_page == 0) {
        sm->header.root_page = create_new_node(sm, true);
        index->root_page = sm->header.root_page;
    }

    Page* root_p = sm_get_page(sm, index->root_page);
    BTreeNode root;
    page_to_node(root_p, &root);
    
    if (root.num_keys == BTREE_ORDER - 1) {
        // Root is full, need to split and increase height
        uint32_t new_root_id = create_new_node(sm, false);
        Page* new_root_p = sm_get_page(sm, new_root_id);
        BTreeNode new_root;
        page_to_node(new_root_p, &new_root);
        
        new_root.children[0] = index->root_page;
        btree_split_child(sm, &new_root, 0, &root);

        // Update both the index and the storage header
        sm->header.root_page = new_root_id;
        index->root_page = new_root_id;

        btree_insert_nonfull(sm, new_root_id, key_hash, value_page);
        
        node_to_page(&new_root, new_root_p);
        new_root_p->is_dirty = true;
    } else {
        btree_insert_nonfull(sm, sm->header.root_page, key_hash, value_page);
    }
    
    return true;
}

bool btree_delete(StorageManager* sm, BTreeIndex* index, void* key) {
    if (!sm || !index || !key) return false;
    
    DataType key_type = index->schema->columns[index->key_column].type;
    uint32_t key_hash = key_to_hash(key_type, key);
    
    printf("B-Tree delete for key_hash %u (not fully implemented)\n", key_hash);
    
    // TODO: Implement proper B-Tree deletion
    // This is complex and includes:
    // 1. Find the node containing the key
    // 2. If leaf: remove key, handle underflow
    // 3. If internal: replace with predecessor/successor
    // 4. Handle merging/redistribution
    
    return true;
}

void btree_free_index(BTreeIndex* index) {
    if (!index) return;
    
    printf("Freeing B-Tree index\n");
    free(index);
}


static inline uint32_t fnv1a_hash(const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t hash = 2166136261u;

    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }

    return hash;
}

static uint32_t key_to_hash(DataType type, void* key){
    switch (type) {
        case DT_INT:
            return fnv1a_hash(key, sizeof(int));
        case DT_FLOAT:
            return fnv1a_hash(key, sizeof(float));
        case DT_STRING:
            return fnv1a_hash(key, strlen((char*)key));
        default:
            return 0;
    }
}

static int compare_hashes(uint32_t hash1, uint32_t hash2) {
    if (hash1 < hash2) return -1;
    if (hash1 > hash2) return 1;
    return 0;
}

static void node_to_page(BTreeNode* node, Page* page) {
    uint8_t* cursor = page->data;

    memcpy(cursor, &node->num_keys, sizeof(uint32_t));
    cursor += sizeof(uint32_t);

    memcpy(cursor, &node->is_leaf, sizeof(bool));
    cursor += sizeof(bool);

    memcpy(cursor, node->keys, sizeof(uint32_t) * (BTREE_ORDER - 1));
    cursor += sizeof(uint32_t) * (BTREE_ORDER - 1);
    
    memcpy(cursor, node->values, sizeof(uint32_t) * (BTREE_ORDER - 1));
    cursor += sizeof(uint32_t) * (BTREE_ORDER - 1);
    
    memcpy(cursor, node->children, sizeof(uint32_t) * BTREE_ORDER);
}

static void page_to_node(Page* page, BTreeNode* node) {
    uint8_t* cursor = page->data;
    node->page_id = page->page_id;
    
    memcpy(&node->num_keys, cursor, sizeof(uint32_t)); cursor += sizeof(uint32_t);
    memcpy(&node->is_leaf, cursor, sizeof(bool));      cursor += sizeof(bool);
    
    memcpy(node->keys, cursor, sizeof(uint32_t) * (BTREE_ORDER - 1));
    cursor += sizeof(uint32_t) * (BTREE_ORDER - 1);
    
    memcpy(node->values, cursor, sizeof(uint32_t) * (BTREE_ORDER - 1));
    cursor += sizeof(uint32_t) * (BTREE_ORDER - 1);
    
    memcpy(node->children, cursor, sizeof(uint32_t) * BTREE_ORDER);
}

static uint32_t create_new_node(StorageManager* sm, bool is_leaf) {
    uint32_t page_id = sm_allocate_page(sm);
    Page* page = sm_get_page(sm, page_id);
    
    BTreeNode node;
    memset(&node, 0, sizeof(BTreeNode));
    node.page_id = page_id;
    node.is_leaf = is_leaf;
    node.num_keys = 0;
    
    node_to_page(&node, page);
    page->is_dirty = true;
    
    return page_id;

}
