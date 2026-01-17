#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "storage.h"
#include "parser.h"

typedef struct QueryResult
{
    uint32_t column_count;
    char column_names[MAX_COLUMNS][MAX_COLUMN_NAME];
    void*** rows; // Array of pointers to rows
    uint32_t row_count;
    char* success_message;
    char* error_message;
} QueryResult;

QueryResult* execute_create_table(StorageManager* sm, SQLStatement* stmt);
QueryResult* execute_insert(StorageManager* sm, SQLStatement* stmt);
QueryResult* execute_select(StorageManager* sm, SQLStatement* stmt);
QueryResult* execute_update(StorageManager* sm, SQLStatement* stmt);
QueryResult* execute_delete(StorageManager* sm, SQLStatement* stmt);
uint32_t count_tables(StorageManager* sm);
uint32_t get_all_tables(StorageManager* sm, char table_names[][MAX_TABLE_NAME], uint32_t max_tables);
QueryResult* execute_show_tables(StorageManager* sm);

// Helper functions
TableSchema* load_schema(StorageManager* sm, const char* table_name);
bool save_schema(StorageManager* sm, TableSchema* schema);
uint32_t calculate_row_size(TableSchema* schema);
void* serialize_row(TableSchema* schema, void** values);
void** deserialize_row(TableSchema* schema, void* row_data);
void free_result(QueryResult* result);


#endif // EXECUTOR_H