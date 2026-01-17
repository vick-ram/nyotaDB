#include "executor.h"
#include "btree.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

static uint32_t get_column_size(ColumnDef* column) {
    switch (column->type) {
        case DT_INT: return sizeof(int);
        case DT_FLOAT: return sizeof(float);
        case DT_STRING: return column->length;
        case DT_BOOL: return sizeof(bool);
        default: return 0;
    }
}

static uint32_t get_column_offset(TableSchema* schema, uint32_t column_index) {
    uint32_t offset = sizeof(bool) + sizeof(uint32_t) + sizeof(uint32_t);
    
    for (uint32_t i = 0; i < column_index; i++) {
        offset += get_column_size(&schema->columns[i]);
    }
    
    return offset;
}

QueryResult* execute_create_table(StorageManager* sm, SQLStatement* stmt) {
    QueryResult* result = malloc(sizeof(QueryResult));
    memset(result, 0, sizeof(QueryResult));
    
    // Calculate row size
    stmt->create_schema.row_size = calculate_row_size(&stmt->create_schema);

    // Save the schema to disk
    if (!save_schema(sm, &stmt->create_schema)) {
        result->error_message = strdup("Failed to save schema");
        return result;
    }

    // Find primary key column index
    uint32_t pk_index = 0;
    for (uint32_t i = 0; i < stmt->create_schema.column_count; i++) {
        if (stmt->create_schema.columns[i].is_primary) {
            pk_index = i;
            break;
        }
    }

    // Create B-Tree index for primary key
    if (stmt->create_schema.primary_key_index != -1) {
        BTreeIndex* pk_btree_index = btree_create_index(&stmt->create_schema, 
                                                  stmt->create_schema.primary_key_index);
        result->success_message = strdup("Table created successfully");
        btree_free_index(pk_btree_index);
    }

    result->column_count = 1;
    strcpy(result->column_names[0], "status");
    result->row_count = 1;
    
    // Allocate space for the result grid
    result->rows = malloc(sizeof(void**));
    result->rows[0] = malloc(sizeof(void*));
    
    char* msg = malloc(100);
    snprintf(msg, 100, "Table '%s' created successfully (Row size: %u bytes)", 
             stmt->create_schema.name, stmt->create_schema.row_size);
    result->success_message = msg;
    result->rows[0][0] = strdup(msg);
    
    return result;
}

QueryResult* execute_select(StorageManager* sm, SQLStatement* stmt) {
    QueryResult* result = calloc(1, sizeof(QueryResult));

    // Load schema
    TableSchema* schema = load_schema(sm, stmt->select_table);
    if (!schema) {
        result->error_message = strdup("Table not found");
        return result;
    }

    // setup result columns
    if (stmt->select_column_count == 1 && strcmp(stmt->select_columns[0], "*") == 0) {
        // Select all columns
        result->column_count = schema->column_count;
        for (uint32_t i = 0; i < schema->column_count; i++) {
            strcpy(result->column_names[i], schema->columns[i].name);
        }
    
    } else {
        // specific columns
        result->column_count = stmt->select_column_count;
        for (uint32_t i = 0; i < stmt->select_column_count; i++) {
            strcpy(result->column_names[i], stmt->select_columns[i]);
        }
    }

    // Simple table scan (no WHERE optimization yet)
    uint32_t current_page = sm->header.root_page;
    uint32_t rows_found = 0;
    uint32_t max_rows = 100; // simple limit


    result->rows = malloc(max_rows * sizeof(void**));

    while (current_page != 0 && rows_found < max_rows) {
        Page* page = sm_get_page(sm, current_page);
        if (!page) break;

        // Simple scan: each row is stored sequentially
        uint32_t row_offset = 0;
        while (row_offset + schema->row_size <= PAGE_SIZE) {
            // Check if this is a valid row start (not all zeros)
            bool all_zeros = true;
            for (uint32_t i = 0; i < 8 && row_offset + i < PAGE_SIZE; i++) {
                if (page->data[row_offset + i] != 0) {
                    all_zeros = false;
                    break;
                }
            }
            
            if (all_zeros) {
                // Skip empty space
                row_offset += schema->row_size;
                continue;
            }

            bool deleted = *(bool*)(page->data + row_offset);
            uint32_t row_id = *(uint32_t*)(page->data + row_offset + sizeof(bool));
            
            if (!deleted) {
                // Apply WHERE filter if present
                bool include = true;
                if (stmt->where_value && stmt->where_column[0] != '\0') {
                    // Find the column to filter
                    for (uint32_t i = 0; i < schema->column_count; i++) {
                        if (strcmp(schema->columns[i].name, stmt->where_column) == 0) {
                            uint32_t col_offset = get_column_offset(schema, i);
                            uint32_t col_size = get_column_size(&schema->columns[i]);
                            
                            if (col_size == sizeof(int)) {
                                int row_value;
                                memcpy(&row_value, page->data + row_offset + col_offset, sizeof(int));
                                int filter_value = *(int*)stmt->where_value;
                                if (row_value != filter_value) {
                                    include = false;
                                }
                            }
                            break;
                        }
                    }
                }
                
                if (include) {
                    // Extract selected columns
                    void** row = malloc(result->column_count * sizeof(void*));
                    
                    for (uint32_t col = 0; col < result->column_count; col++) {
                        // Find column index in schema
                        for (uint32_t i = 0; i < schema->column_count; i++) {
                            if (strcmp(schema->columns[i].name, result->column_names[col]) == 0) {
                                uint32_t col_offset = get_column_offset(schema, i);
                                uint32_t col_size = get_column_size(&schema->columns[i]);
                                
                                row[col] = malloc(col_size + 1); // +1 for string null terminator
                                memcpy(row[col], page->data + row_offset + col_offset, col_size);
                                
                                // Null terminate if it's a string
                                if (schema->columns[i].type == DT_STRING) {
                                    ((char*)row[col])[col_size] = '\0';
                                }
                                break;
                            }
                        }
                    }
                    
                    result->rows[rows_found++] = row;
                    if (rows_found >= max_rows) break;
                }
            }
            
            row_offset += schema->row_size;
        }
        
        // Get next page (simplified linked list)
        // Look for next page pointer at end of current page
        if (PAGE_SIZE >= sizeof(uint32_t)) {
            current_page = *(uint32_t*)(page->data + PAGE_SIZE - sizeof(uint32_t));
        } else {
            current_page = 0;
        }
    }
    
    result->row_count = rows_found;
    free(schema);
    return result;
}

QueryResult* execute_insert(StorageManager* sm, SQLStatement* stmt) {
    QueryResult* result = malloc(sizeof(QueryResult));
    memset(result, 0, sizeof(QueryResult));
    
    // Load schema to validate
    TableSchema* schema = load_schema(sm, stmt->insert_table);
    if (!schema) {
        result->error_message = strdup("Table not found");
        return result;
    }
    
    // Validate value count matches column count
    if (stmt->insert_value_count != schema->column_count) {
        result->error_message = strdup("Value count doesn't match column count");
        free(schema);
        return result;
    }
    
    // Find primary key column
    int pk_index = -1;
    for (uint32_t i = 0; i < schema->column_count; i++) {
        if (schema->columns[i].is_primary) {
            pk_index = i;
            break;
        }
    }
    
    // If primary key exists, check for duplicates
    if (pk_index >= 0) {
        BTreeIndex* pk_index_ptr = btree_create_index(schema, pk_index);
        if (btree_search(sm, pk_index_ptr, stmt->insert_values[pk_index]) != 0) {
            result->error_message = strdup("Primary key violation - duplicate value");
            btree_free_index(pk_index_ptr);
            free(schema);
            return result;
        }
        btree_free_index(pk_index_ptr);
    }
    
    // Allocate new page if needed
    if (sm->header.root_page == 0) {
        sm->header.root_page = sm_allocate_page(sm);
    }
    
    // Find page with free space
    uint32_t current_page = sm->header.root_page;
    Page* page = sm_get_page(sm, current_page);
    
    // Find free space in page
    uint32_t free_offset = 0;
    bool found_space = false;
    
    while (free_offset + schema->row_size <= PAGE_SIZE) {
        bool slot_used = *(bool*)(page->data + free_offset);
        if (!slot_used) {
            found_space = true;
            break;
        }
        free_offset += schema->row_size;
    }
    
    if (!found_space) {
        // Allocate new page
        uint32_t new_page = sm_allocate_page(sm);
        // Link pages (simplified)
        *(uint32_t*)(page->data + PAGE_SIZE - sizeof(uint32_t)) = new_page;
        page->is_dirty = true;
        
        current_page = new_page;
        page = sm_get_page(sm, current_page);
        free_offset = 0;
    }
    
    // Serialize and insert row
    void* row_data = serialize_row(schema, stmt->insert_values);
    memcpy(page->data + free_offset, row_data, schema->row_size);
    page->is_dirty = true;
    
    // Update B-Tree index if primary key exists
    if (pk_index >= 0) {
        BTreeIndex* pk_index_ptr = btree_create_index(schema, pk_index);
        btree_insert(sm, pk_index_ptr, stmt->insert_values[pk_index], current_page);
        btree_free_index(pk_index_ptr);
    }
    
    free(row_data);
    free(schema);
    
    result->column_count = 1;
    strcpy(result->column_names[0], "rows_affected");
    result->row_count = 1;
    result->rows = malloc(sizeof(void**));
    result->rows[0] = malloc(sizeof(void*));
    result->rows[0][0] = strdup("1");
    
    return result;
}

// Add to executor.c
QueryResult* execute_update(StorageManager* sm, SQLStatement* stmt) {
    QueryResult* result = malloc(sizeof(QueryResult));
    memset(result, 0, sizeof(QueryResult));
    
    // Load schema
    TableSchema* schema = load_schema(sm, stmt->update_table);
    if (!schema) {
        result->error_message = strdup("Table not found");
        return result;
    }
    
    // Validate that all columns exist
    for (uint32_t i = 0; i < stmt->update_column_count; i++) {
        bool column_found = false;
        for (uint32_t j = 0; j < schema->column_count; j++) {
            if (strcmp(stmt->update_columns[i], schema->columns[j].name) == 0) {
                column_found = true;
                break;
            }
        }
        if (!column_found) {
            result->error_message = malloc(100);
            snprintf(result->error_message, 100, "Column '%s' not found", stmt->update_columns[i]);
            free(schema);
            return result;
        }
    }
    
    // Simple table scan to find rows to update
    uint32_t current_page = sm->header.root_page;
    uint32_t rows_updated = 0;
    
    while (current_page != 0) {
        Page* page = sm_get_page(sm, current_page);
        if (!page) break;
        
        uint32_t row_offset = 0;
        
        while (row_offset + schema->row_size <= PAGE_SIZE) {
            // Check if this is a valid row
            bool all_zeros = true;
            for (uint32_t i = 0; i < 8 && row_offset + i < PAGE_SIZE; i++) {
                if (page->data[row_offset + i] != 0) {
                    all_zeros = false;
                    break;
                }
            }
            
            if (all_zeros) {
                row_offset += schema->row_size;
                continue;
            }
            
            bool deleted = *(bool*)(page->data + row_offset);
            uint32_t row_id = *(uint32_t*)(page->data + row_offset + sizeof(bool));
            
            if (!deleted) {
                // Check WHERE condition if present
                bool should_update = true;
                
                if (stmt->where_value && stmt->where_column[0] != '\0') {
                    should_update = false;
                    
                    // Find the column to filter
                    for (uint32_t i = 0; i < schema->column_count; i++) {
                        if (strcmp(schema->columns[i].name, stmt->where_column) == 0) {
                            uint32_t col_offset = get_column_offset(schema, i);
                            uint32_t col_size = get_column_size(&schema->columns[i]);
                            
                            if (col_size == sizeof(int)) {
                                int row_value;
                                memcpy(&row_value, page->data + row_offset + col_offset, sizeof(int));
                                int filter_value = *(int*)stmt->where_value;
                                
                                // Simple equality check for now
                                if (row_value == filter_value) {
                                    should_update = true;
                                }
                            }
                            break;
                        }
                    }
                }
                
                // Update the row if it matches WHERE condition
                if (should_update) {
                    // For each column to update
                    for (uint32_t i = 0; i < stmt->update_column_count; i++) {
                        // Find column index in schema
                        for (uint32_t j = 0; j < schema->column_count; j++) {
                            if (strcmp(stmt->update_columns[i], schema->columns[j].name) == 0) {
                                uint32_t col_offset = get_column_offset(schema, j);
                                uint32_t col_size = get_column_size(&schema->columns[j]);
                                
                                // Update the value
                                if (stmt->update_values[i]) {
                                    memcpy(page->data + row_offset + col_offset, 
                                           stmt->update_values[i], col_size);
                                }
                                break;
                            }
                        }
                    }
                    
                    page->is_dirty = true;
                    rows_updated++;
                }
            }
            
            row_offset += schema->row_size;
        }
        
        // Get next page
        if (PAGE_SIZE >= sizeof(uint32_t)) {
            current_page = *(uint32_t*)(page->data + PAGE_SIZE - sizeof(uint32_t));
        } else {
            current_page = 0;
        }
    }
    
    free(schema);
    
    // Return result
    result->column_count = 1;
    strcpy(result->column_names[0], "rows_updated");
    result->row_count = 1;
    result->rows = malloc(sizeof(void**));
    result->rows[0] = malloc(sizeof(void*));
    
    char* msg = malloc(20);
    snprintf(msg, 20, "%u", rows_updated);
    result->rows[0][0] = msg;
    
    return result;
}

QueryResult* execute_delete(StorageManager* sm, SQLStatement* stmt) {
    QueryResult* result = malloc(sizeof(QueryResult));
    memset(result, 0, sizeof(QueryResult));
    
    result->column_count = 1;
    strcpy(result->column_names[0], "rows_affected");
    result->row_count = 1;
    result->rows = malloc(sizeof(void**));
    result->rows[0] = malloc(sizeof(void*));
    
    // Simple implementation: mark as deleted
    if (stmt->where_value && stmt->table_name[0] != '\0') {
        TableSchema* schema = load_schema(sm, stmt->table_name);
        if (schema) {
            // Scan and mark matching rows as deleted
            uint32_t current_page = sm->header.root_page;
            uint32_t deleted_count = 0;
            
            while (current_page != 0) {
                Page* page = sm_get_page(sm, current_page);
                uint32_t row_offset = 0;
                
                while (row_offset + schema->row_size <= PAGE_SIZE) {
                    bool deleted = *(bool*)(page->data + row_offset);
                    
                    if (!deleted) {
                        // Check if this row matches WHERE condition
                        bool match = true;
                        if (stmt->where_column[0] != '\0') {
                            for (uint32_t i = 0; i < schema->column_count; i++) {
                                if (strcmp(schema->columns[i].name, stmt->where_column) == 0) {
                                    uint32_t col_offset = get_column_offset(schema, i);
                                    uint32_t col_size = get_column_size(&schema->columns[i]);
                                    
                                    if (col_size == sizeof(int)) {
                                        int row_value;
                                        memcpy(&row_value, page->data + row_offset + col_offset, sizeof(int));
                                        int filter_value = *(int*)stmt->where_value;
                                        if (row_value != filter_value) {
                                            match = false;
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                        
                        if (match) {
                            // Mark as deleted
                            *(bool*)(page->data + row_offset) = true;
                            page->is_dirty = true;
                            deleted_count++;
                        }
                    }
                    
                    row_offset += schema->row_size;
                }
                
                current_page = *(uint32_t*)(page->data + PAGE_SIZE - sizeof(uint32_t));
            }
            
            free(schema);
            
            char* msg = malloc(20);
            snprintf(msg, 20, "%u", deleted_count);
            result->rows[0][0] = msg;
            return result;
        }
    }
    
    result->rows[0][0] = strdup("0");
    return result;
}

bool save_schema(StorageManager* sm, TableSchema* schema) {
    if (!sm || !schema) return false;
    
    printf("DEBUG: Saving schema for table '%s'\n", schema->name);

    // Use the schema page from header
    uint32_t schema_page_id = sm->header.schema_page;
    if (schema_page_id == 0) {
        // Allocate first schema page (page 1)
        // schema_page_id = sm_allocate_page(sm);
        schema_page_id = 1; // Schema always starts at page 1
        sm->header.schema_page = schema_page_id;
        // sm->header.is_dirty = true;

        printf("DEBUG: Allocated schema page %u\n", schema_page_id);

        // Make sure page 1 exists
        if (schema_page_id >= sm->header.page_count) {
            sm_allocate_page(sm); // This will create page 1
        }
    }
    
    Page* schema_page = sm_get_page(sm, schema_page_id);
    if (!schema_page) {
        printf("DEBUG: Failed to get schema page %u\n", schema_page_id);
        return false;
    }

    // Calculate offset in page
    uint32_t offset = 0;
    char stored_name[MAX_TABLE_NAME];
    bool found_slot = false;

    printf("DEBUG: Searching for slot in schema page...\n");
    
    while (offset + sizeof(TableSchema) <= PAGE_SIZE) {
        // Read table name
        memcpy(stored_name, schema_page->data + offset, MAX_TABLE_NAME);

        printf("DEBUG: Slot at offset %u: '%s'\n", offset, stored_name);
        
        if (stored_name[0] == '\0' || strcmp(stored_name, schema->name) == 0) {
            // Empty slot found or schema exists
            found_slot = true;
            printf("DEBUG: Found slot at offset %u\n", offset);
            break;
        }
        
        offset += sizeof(TableSchema);
    }

    if (!found_slot) {
        printf("DEBUG: No free slot found in schema page\n");
        return false;
    }
    
    if (offset + sizeof(TableSchema) > PAGE_SIZE) {
        // Page full - need to handle in real implementation
        printf("DEBUG: Page full at offset %u\n", offset);
        return false;
    }
    
    // Save the schema at the calculated offset
    printf("DEBUG: Saving schema at offset %u (size: %lu)\n", offset, sizeof(TableSchema));
    memcpy(schema_page->data + offset, schema, sizeof(TableSchema));
    schema_page->is_dirty = true;
    
    return true;
}

TableSchema* load_schema(StorageManager* sm, const char* table_name) {
    if (sm->header.schema_page == 0) {
        return NULL;
    }
    
    Page* schema_page = sm_get_page(sm, sm->header.schema_page);
    if (!schema_page) return NULL;
    
    uint32_t offset = 0;
    char stored_name[MAX_TABLE_NAME];
    
    while (offset + sizeof(TableSchema) <= PAGE_SIZE) {
        memcpy(stored_name, schema_page->data + offset, MAX_TABLE_NAME);
        
        if (stored_name[0] == '\0') {
            // End of schemas
            break;
        }
        
        if (strcmp(stored_name, table_name) == 0) {
            // Found the schema
            TableSchema* schema = malloc(sizeof(TableSchema));
            memcpy(schema, schema_page->data + offset, sizeof(TableSchema));
            return schema;
        }
        
        offset += sizeof(TableSchema);
    }
    
    return NULL;
}

uint32_t calculate_row_size(TableSchema* schema) {
    uint32_t row_size = 0;
    
    // Add size for each column
    for (uint32_t i = 0; i < schema->column_count; i++) {
        row_size += get_column_size(&schema->columns[i]);
    }
    
    // Add overhead: deleted flag, row_id, next_row pointer
    row_size += sizeof(bool) + sizeof(uint32_t) + sizeof(uint32_t);
    
    return row_size;
}

void* serialize_row(TableSchema* schema, void** values) {
    uint32_t row_size = calculate_row_size(schema);
    uint8_t* row_data = malloc(row_size);
    uint32_t offset = 0;
    
    // Start with deleted flag = false
    bool deleted = false;
    memcpy(row_data + offset, &deleted, sizeof(bool));
    offset += sizeof(bool);
    
    // Row ID (will be set by storage manager)
    uint32_t row_id = 0; // Temporary - should be assigned
    memcpy(row_data + offset, &row_id, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Next row pointer (for linked list of rows)
    uint32_t next_row = 0;
    memcpy(row_data + offset, &next_row, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Serialize column values
    for (uint32_t i = 0; i < schema->column_count; i++) {
        uint32_t col_size = get_column_size(&schema->columns[i]);
        
        if (!values[i]) {
            // NULL value handling
            memset(row_data + offset, 0, col_size);
        } else {
            memcpy(row_data + offset, values[i], col_size);
        }
        offset += col_size;
    }

    return row_data;
}

void** deserialize_row(TableSchema* schema, void* row_data) {
    uint8_t* data = (uint8_t*)row_data;
    uint32_t offset = sizeof(bool) + sizeof(uint32_t) + sizeof(uint32_t); // Skip overhead
    
    void** values = malloc(schema->column_count * sizeof(void*));
    
    for (uint32_t i = 0; i < schema->column_count; i++) {
        uint32_t col_size = get_column_size(&schema->columns[i]);
        values[i] = malloc(col_size + 1); // +1 for string null terminator
        memcpy(values[i], data + offset, col_size);
        
        // Null terminate if it's a string
        if (schema->columns[i].type == DT_STRING) {
            ((char*)values[i])[col_size] = '\0';
        }
        
        offset += col_size;
    }
    
    return values;
}

uint32_t count_tables(StorageManager* sm) {
    if (sm->header.schema_page == 0) return 0;

    Page* schema_page = sm_get_page(sm, sm->header.schema_page);
    if (!schema_page) return 0;

    uint32_t table_count = 0;
    uint32_t offset = 0;
    char table_name[MAX_TABLE_NAME];

    while (offset + sizeof(TableSchema) <= PAGE_SIZE) {
        memcpy(table_name, schema_page->data + offset, MAX_TABLE_NAME);

        if (table_name[0] == '\0') {
            break; // Empty slot
        }

        table_count++;
        offset += sizeof(TableSchema);
    }

    return table_count;

}

uint32_t get_all_tables(StorageManager* sm, char table_names[][MAX_TABLE_NAME], uint32_t max_tables) {
    if (sm->header.schema_page == 0) return 0;

    Page* schema_page = sm_get_page(sm, sm->header.schema_page);
    if (!schema_page) return 0;
    
    uint32_t count = 0;
    uint32_t offset = 0;
    
    while (offset + sizeof(TableSchema) <= PAGE_SIZE && count < max_tables) {
        char table_name[MAX_TABLE_NAME];
        memcpy(table_name, schema_page->data + offset, MAX_TABLE_NAME);
        
        if (table_name[0] == '\0') {
            break; // Empty slot
        }
        
        strcpy(table_names[count], table_name);
        count++;
        offset += sizeof(TableSchema);
    }
    
    return count;
}

QueryResult* execute_show_tables(StorageManager* sm) {
    QueryResult* result = malloc(sizeof(QueryResult));
    memset(result, 0, sizeof(QueryResult));
    
    result->column_count = 1;
    strcpy(result->column_names[0], "Tables");
    
    // Get all table names
    char table_names[100][MAX_TABLE_NAME];
    uint32_t table_count = get_all_tables(sm, table_names, 100);
    
    if (table_count == 0) {
        result->row_count = 1;
        result->rows = malloc(sizeof(void**));
        result->rows[0] = malloc(sizeof(void*));
        result->rows[0][0] = strdup("No tables found");
        return result;
    }
    
    result->row_count = table_count;
    result->rows = malloc(table_count * sizeof(void**));
    
    for (uint32_t i = 0; i < table_count; i++) {
        result->rows[i] = malloc(sizeof(void*));
        result->rows[i][0] = strdup(table_names[i]);
    }
    
    return result;
}


// Free query result memory
void free_result(QueryResult* result) {
    if (!result) return;
    
    for (uint32_t i = 0; i < result->row_count; i++) {
        if (result->rows[i]) {
            for (uint32_t j = 0; j < result->column_count; j++) {
                free(result->rows[i][j]);
            }
            free(result->rows[i]);
        }
    }
    free(result->rows);
    free(result->success_message);
    free(result->error_message);
    free(result);
}
