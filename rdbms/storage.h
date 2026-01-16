// Storage
#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_CACHE_PAGES 100

#define PAGE_SIZE 4096
#define MAX_TABLE_NAME 64
#define MAX_COLUMN_NAME 32
#define MAX_COLUMNS 32
#define MAX_STRING_LEN 255

typedef struct PageStruct PageStruct;

// Data types supported
typedef enum {
    DT_INT,
    DT_FLOAT,
    DT_STRING,
    DT_BOOL
} DataType;

// Column definition
typedef struct {
    char name[MAX_COLUMN_NAME];
    DataType type;
    uint32_t length; // For strings
    bool is_primary;
    bool is_unique;
    bool nullable;
} ColumnDef;

// Table schema definition
typedef struct {
    char name[MAX_TABLE_NAME];
    uint32_t column_count;
    ColumnDef columns[MAX_COLUMNS];
    uint32_t primary_key_index;
    uint32_t row_size; // Size of a single row in bytes
} TableSchema;

// Page structure
// typedef struct
// {
//     uint8_t data[PAGE_SIZE];
//     uint32_t page_id;
//     bool is_dirty;

//     // LRU pointers
//     struct Page* prev;
//     struct Page* next;
// } Page;
struct PageStruct {
    uint8_t data[PAGE_SIZE];
    uint32_t page_id;
    bool is_dirty;
    
    // LRU pointers
    PageStruct* prev;
    PageStruct* next;
};

typedef struct PageStruct Page;

// Record (row) structure
typedef struct {
    uint32_t row_id;
    uint8_t *data; // Pointer to row data
    bool deleted;
} Record;

// Database file header
typedef struct {
    uint32_t magic_number;
    uint32_t page_count;
    uint32_t root_page;
    uint32_t first_free_page;
    uint32_t schema_page;
} DBHeader;

typedef struct
{
    int fd;
    DBHeader header;

    Page* pages[100];
    uint32_t cache_size;

    // LRU list
    Page* lru_head; // Most recent
    Page* lru_tail; // Least recent
} StorageManager;

StorageManager* sm_open(const char* filename);
Page* sm_get_page(StorageManager* sm, uint32_t page_id);
void sm_persist_page(StorageManager* sm, Page* page);
void sm_close(StorageManager* sm);
uint32_t sm_allocate_page(StorageManager* sm);
void print_schema(TableSchema* schema);

void run_repl();
void run_webserver(StorageManager* sm);

#endif // STORAGE_H