#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>
#include "storage.h"

typedef enum {
    STMT_SELECT,
    STMT_INSERT,
    STMT_UPDATE,
    STMT_DELETE,
    STMT_CREATE_TABLE,
    STMT_DROP_TABLE,
    STMT_CREATE_INDEX,
    STMT_SHOW_TABLES,
    STMT_UNKNOWN
} StatementType;

typedef enum {
    OP_EQUALS,
    OP_NOT_EQUALS,
    OP_GREATER,
    OP_LESS,
    OP_GREATER_EQUAL,
    OP_LESS_EQUAL,
    OP_LIKE
} OperatorType;

typedef struct {
    char column[MAX_COLUMN_NAME];
    OperatorType op;
    void* value;
    DataType value_type;
} WhereClause;

typedef struct {
    StatementType type;

    // For CREATE TABLE
    TableSchema create_schema;

    // For SELECT
    char select_table[MAX_TABLE_NAME];
    char select_columns[MAX_COLUMNS][MAX_COLUMN_NAME];
    uint32_t select_column_count;
    WhereClause where_clause;
    bool has_where;

    // For INSERT
    char insert_table[MAX_TABLE_NAME];
    void** insert_values;
    DataType* insert_value_types;
    uint32_t insert_value_count;

    // For DELETE/UPDATE
    char table_name[MAX_TABLE_NAME];
    
    // For multiple WHERE conditions (simplified to single for now)
    char where_column[MAX_COLUMN_NAME];
    OperatorType where_operator;
    void* where_value;
    DataType where_value_type;
    
    // For UPDATE
    char update_columns[MAX_COLUMNS][MAX_COLUMN_NAME];
    void** update_values;
    uint32_t update_column_count;
    
    // Error information
    char error_message[256];
    bool has_error;
} SQLStatement;

typedef struct
{
    char* buffer;
    size_t length;
    size_t position;
} Tokenizer;

SQLStatement* parse_sql(const char* sql);
void free_sql_statement(SQLStatement* statement);
const char* statement_type_to_string(StatementType type);

// Tokenizer functions
Tokenizer* tokenizer_create(const char* sql);
void tokenizer_free(Tokenizer* t);
char* tokenizer_next(Tokenizer* t);
char* tokenizer_peek(Tokenizer* t);
bool tokenizer_has_more(Tokenizer* t);

// Helper functions
DataType parse_data_type(const char* type_str);
OperatorType parse_operator(const char* op_str);
void* parse_value(const char* value_str, DataType type);

#endif // PARSER_H