#include "parser.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "main.h"

static bool parse_create_table(Tokenizer *t, SQLStatement *stmt);
static bool parse_select(Tokenizer *t, SQLStatement *stmt);
static bool parse_insert(Tokenizer *t, SQLStatement *stmt);
static bool parse_update(Tokenizer *t, SQLStatement *stmt);
static bool parse_delete(Tokenizer *t, SQLStatement *stmt);
static bool parse_where_clause(Tokenizer *t, SQLStatement *stmt);
static bool parse_value_list(Tokenizer *t, SQLStatement *stmt, bool for_insert);
static bool expect_token(Tokenizer *t, SQLStatement *stmt, const char *expected, const char *error_msg);
static bool parse_join_clause(Tokenizer *t, SQLStatement *stmt);
static bool parse_drop_table(Tokenizer* t, SQLStatement* stmt);

SQLStatement *parse_sql(const char *sql)
{
    SQLStatement *stmt = SAFE_CALLOC(SQLStatement, 1);
    Tokenizer *t = tokenizer_create(sql);

    char *token = tokenizer_next(t);
    if (!token)
    {
        stmt->type = STMT_UNKNOWN;
        snprintf(stmt->error_message, sizeof(stmt->error_message), "Empty statement");
        stmt->has_error = true;
        tokenizer_free(t);
        return stmt;
    }

    bool parse_success = false;

    if (strcasecmp(token, "SELECT") == 0)
    {
        parse_success = parse_select(t, stmt);
    }
    else if (strcasecmp(token, "INSERT") == 0)
    {
        parse_success = parse_insert(t, stmt);
    }
    else if (strcasecmp(token, "CREATE") == 0)
    {
        parse_success = parse_create_table(t, stmt);
    }
    else if (strcasecmp(token, "DELETE") == 0)
    {
        parse_success = parse_delete(t, stmt);
    }
    else if (strcasecmp(token, "UPDATE") == 0)
    {
        parse_success = parse_update(t, stmt);
    }
    else if (strcasecmp(token, "DROP") == 0)
    {
        parse_success = parse_drop_table(t, stmt);
    }
    else if (strcasecmp(token, "SHOW") == 0)
    {
        token = tokenizer_next(t);
        if (token && strcasecmp(token, "TABLES") == 0)
        {
            stmt->type = STMT_SHOW_TABLES;
            parse_success = true;
        }
    }
    else
    {
        stmt->type = STMT_UNKNOWN;
        snprintf(stmt->error_message, sizeof(stmt->error_message), "Unknown command: %s", token);
        stmt->has_error = true;
    }

    SAFE_FREE(token);

    // Check for trailing semicolon (optional but good practice)
    if (parse_success)
    {
        char *peek = tokenizer_peek(t);
        if (peek && strcmp(peek, ";") == 0)
        {
            char *colon = tokenizer_next(t);
            SAFE_FREE(colon); // Consume semicolon
        }

        // Check for extra tokens
        char *extra = tokenizer_next(t);
        if (extra)
        {
            snprintf(stmt->error_message, sizeof(stmt->error_message),
                     "Unexpected token after statement: %s", extra);
            stmt->has_error = true;
            SAFE_FREE(extra);
        }
    }

    tokenizer_free(t);
    return stmt;
}

static bool parse_create_table(Tokenizer *t, SQLStatement *stmt)
{
    stmt->type = STMT_CREATE_TABLE;
    stmt->create_schema.primary_key_index = -1;

    // Expect "TABLE"
    if (!expect_token(t, stmt, "TABLE", "Expected TABLE after CREATE"))
    {
        return false;
    }

    // Parse table name
    char *table_name = tokenizer_next(t);
    if (!table_name)
    {
        snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected table name");
        return false;
    }
    strncpy(stmt->create_schema.name, table_name, MAX_TABLE_NAME - 1);
    SAFE_FREE(table_name);

    // Expect "("
    if (!expect_token(t, stmt, "(", "Expected '(' after table name"))
    {
        return false;
    }

    uint32_t col_idx = 0;
    bool parsing_columns = true;

    while (parsing_columns && col_idx < MAX_COLUMNS)
    {
        // Parse column name
        char *col_name = tokenizer_next(t);
        if (!col_name)
        {
            snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected column name");
            return false;
        }
        strncpy(stmt->create_schema.columns[col_idx].name, col_name, MAX_COLUMN_NAME - 1);
        SAFE_FREE(col_name);

        // Parse data type
        char *type_str = tokenizer_next(t);
        if (!type_str)
        {
            snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected data type");
            return false;
        }

        stmt->create_schema.columns[col_idx].type = parse_data_type(type_str);

        // Handle type-specific parsing
        if (stmt->create_schema.columns[col_idx].type == DT_STRING)
        {
            // Check for length specification
            char *peek = tokenizer_peek(t);
            if (peek && strcmp(peek, "(") == 0)
            {
                char *brack = tokenizer_next(t);
                SAFE_FREE(brack); // Consume "("

                char *length_str = tokenizer_next(t);
                if (!length_str)
                {
                    snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected string length");
                    SAFE_FREE(type_str);
                    return false;
                }

                stmt->create_schema.columns[col_idx].length = atoi(length_str);
                SAFE_FREE(length_str);

                // Expect ")"
                if (!expect_token(t, stmt, ")", "Expected ')' after string length"))
                {
                    SAFE_FREE(type_str);
                    return false;
                }
            }
            else
            {
                stmt->create_schema.columns[col_idx].length = MAX_STRING_LEN;
            }
        }
        else
        {
            stmt->create_schema.columns[col_idx].length = 0;
        }
        SAFE_FREE(type_str);

        // Check for constraints
        char *next = tokenizer_peek(t);
        while (next && strcasecmp(next, ",") != 0 && strcmp(next, ")") != 0)
        {
            char *constraint = tokenizer_next(t);

            if (strcasecmp(constraint, "PRIMARY") == 0)
            {
                char *key_token = tokenizer_next(t);
                if (!key_token || strcasecmp(key_token, "KEY") != 0)
                {
                    snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected KEY after PRIMARY");
                    SAFE_FREE(constraint);
                    SAFE_FREE(key_token);
                    return false;
                }
                SAFE_FREE(key_token);

                stmt->create_schema.columns[col_idx].is_primary = true;
                stmt->create_schema.primary_key_index = col_idx;
            }
            else if (strcasecmp(constraint, "UNIQUE") == 0)
            {
                stmt->create_schema.columns[col_idx].is_unique = true;
            }
            else if (strcasecmp(constraint, "NOT") == 0)
            {
                char *null_token = tokenizer_next(t);
                if (!null_token || strcasecmp(null_token, "NULL") != 0)
                {
                    snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected NULL after NOT");
                    SAFE_FREE(constraint);
                    SAFE_FREE(null_token);
                    return false;
                }
                SAFE_FREE(null_token);
                stmt->create_schema.columns[col_idx].nullable = false;
            }

            SAFE_FREE(constraint);
            next = tokenizer_peek(t);
        }

        // Check for comma or closing paren
        next = tokenizer_peek(t);
        if (next && strcmp(next, ",") == 0)
        {
            char *comma = tokenizer_next(t);
            SAFE_FREE(comma); // Consume comma
            col_idx++;
        }
        else if (next && strcmp(next, ")") == 0)
        {
            char *paren = tokenizer_next(t);
            SAFE_FREE(paren); // Consume ")"
            parsing_columns = false;
            col_idx++;
        }
        else if (!next)
        {
            snprintf(stmt->error_message, sizeof(stmt->error_message), "Unexpected end of statement");
            return false;
        }
    }

    stmt->create_schema.column_count = col_idx;
    return true;
}

static bool parse_select(Tokenizer *t, SQLStatement *stmt)
{
    stmt->type = STMT_SELECT;
    stmt->has_join = false;

    // Parse columns
    char *token = tokenizer_next(t);
    if (!token)
    {
        snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected column list or *");
        return false;
    }

    if (strcmp(token, "*") == 0)
    {
        strcpy(stmt->select_columns[0], "*");
        stmt->select_column_count = 1;
        SAFE_FREE(token);
        token = tokenizer_next(t); // Get next token for FROM
    }
    else
    {
        uint32_t col_idx = 0;
        while (token && strcasecmp(token, "FROM") != 0)
        {
            if (strcmp(token, ",") != 0)
            {
                if (col_idx >= MAX_COLUMNS)
                {
                    snprintf(stmt->error_message, sizeof(stmt->error_message), "Too many columns");
                    SAFE_FREE(token);
                    return false;
                }
                strncpy(stmt->select_columns[col_idx], token, MAX_COLUMN_NAME - 1);
                col_idx++;
            }
            SAFE_FREE(token);
            token = tokenizer_next(t);
        }
        stmt->select_column_count = col_idx;
    }

    // Expect "FROM"
    if (!token || strcasecmp(token, "FROM") != 0)
    {
        snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected FROM after column list");
        if (token)
            SAFE_FREE(token);
        return false;
    }
    SAFE_FREE(token);

    // Parse table name
    char *table_name = tokenizer_next(t);
    if (!table_name)
    {
        snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected table name");
        return false;
    }

    // Check for JOIN keyword
    char *next = tokenizer_peek(t);
    if (next && (strcasecmp(next, "JOIN") == 0 ||
                 strcasecmp(next, "INNER") == 0 ||
                 strcasecmp(next, "LEFT") == 0 ||
                 strcasecmp(next, "RIGHT") == 0 ||
                 strcasecmp(next, "FULL") == 0))
    {
        // Store left table name
        strncpy(stmt->join_clause.left_table, table_name, MAX_TABLE_NAME - 1);
        strncpy(stmt->select_table, table_name, MAX_TABLE_NAME - 1);
        SAFE_FREE(table_name);
        
        // Parse JOIN clause
        if (!parse_join_clause(t, stmt)) {
            return false;
        }
    } else {
        strncpy(stmt->select_table, table_name, MAX_TABLE_NAME - 1);
        SAFE_FREE(table_name);
    }

    // Check for WHERE clause
    char *where_token = tokenizer_peek(t);
    if (where_token && strcasecmp(where_token, "WHERE") == 0)
    {
        char *where_token = tokenizer_next(t);
        SAFE_FREE(where_token); // Consume WHERE
        return parse_where_clause(t, stmt);
    }

    return true;
}

static bool parse_insert(Tokenizer *t, SQLStatement *stmt)
{
    stmt->type = STMT_INSERT;

    // Expect "INTO"
    if (!expect_token(t, stmt, "INTO", "Expected INTO after INSERT"))
    {
        return false;
    }

    // Parse table name
    char *table_name = tokenizer_next(t);
    if (!table_name)
    {
        snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected table name");
        return false;
    }
    strncpy(stmt->insert_table, table_name, MAX_TABLE_NAME - 1);
    SAFE_FREE(table_name);

    // Check for column list (optional)
    char *peek = tokenizer_peek(t);
    if (peek && strcmp(peek, "(") == 0)
    {
        // TODO: Parse column list
        char *open_paren = tokenizer_next(t);
        SAFE_FREE(open_paren); // Consume "("
        // Skip column list for now
        while ((peek = tokenizer_next(t)) && strcmp(peek, ")") != 0)
        {
            SAFE_FREE(peek);
        }
        SAFE_FREE(peek); // Free the ")"
    }

    // Expect "VALUES"
    if (!expect_token(t, stmt, "VALUES", "Expected VALUES after table name"))
    {
        return false;
    }

    // Expect "("
    if (!expect_token(t, stmt, "(", "Expected '(' after VALUES"))
    {
        return false;
    }

    // Parse value list
    return parse_value_list(t, stmt, true);
}

static bool parse_update(Tokenizer *t, SQLStatement *stmt)
{
    stmt->type = STMT_UPDATE;

    // Parse table name
    char *table_name = tokenizer_next(t);
    if (!table_name)
    {
        snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected table name");
        return false;
    }
    strncpy(stmt->update_table, table_name, MAX_TABLE_NAME - 1);
    SAFE_FREE(table_name);

    // Expect "SET"
    if (!expect_token(t, stmt, "SET", "Expected SET after table name"))
    {
        return false;
    }

    // Parse SET clause
    uint32_t col_idx = 0;
    bool parsing_set = true;

    while (parsing_set && col_idx < MAX_COLUMNS)
    {
        // Parse column name
        char *col_name = tokenizer_next(t);
        if (!col_name)
        {
            snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected column name in SET");
            return false;
        }
        strncpy(stmt->update_columns[col_idx], col_name, MAX_COLUMN_NAME - 1);
        SAFE_FREE(col_name);

        // Expect "="
        if (!expect_token(t, stmt, "=", "Expected = after column name"))
        {
            return false;
        }

        // Parse value
        char *value_str = tokenizer_next(t);
        if (!value_str)
        {
            snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected value after =");
            return false;
        }

        // Allocate space for update values if needed
        if (col_idx == 0)
        {
            stmt->update_values = SAFE_MALLOC(void *, MAX_COLUMNS);
            if (!stmt->update_values)
            {
                SAFE_FREE(value_str);
                return false;
            }
        }

        // Parse value based on type (simplified - we'll need schema to know actual type)
        // For now, handle integers and strings
        if (value_str[0] == '\'' || value_str[0] == '"')
        {
            // String value
            char *str_val = SAFE_MALLOC(char, strlen(value_str) - 1);
            if (str_val)
            {
                strncpy(str_val, value_str + 1, strlen(value_str) - 2);
                str_val[strlen(value_str) - 2] = '\0';
                stmt->update_values[col_idx] = str_val;
            }
        }
        else
        {
            // Try integer
            int *int_val = SAFE_MALLOC(int, 1);
            if (int_val)
            {
                *int_val = atoi(value_str);
                stmt->update_values[col_idx] = int_val;
            }
        }
        SAFE_FREE(value_str);

        col_idx++;

        // Check for comma or WHERE
        char *next = tokenizer_peek(t);
        if (next && strcmp(next, ",") == 0)
        {
            char *comma = tokenizer_next(t);
            SAFE_FREE(comma); // Consume comma
        }
        else if (next && strcasecmp(next, "WHERE") == 0)
        {
            char *where_token = tokenizer_next(t);
            SAFE_FREE(where_token); // Consume WHERE
            parsing_set = false;
        }
        else if (!next)
        {
            snprintf(stmt->error_message, sizeof(stmt->error_message), "Unexpected end of SET clause");
            return false;
        }
    }

    stmt->update_column_count = col_idx;

    // Parse WHERE clause if present
    char *where_token = tokenizer_peek(t);
    if (where_token && strcasecmp(where_token, "WHERE") == 0)
    {
        char *where_token = tokenizer_next(t);
        SAFE_FREE(where_token); // Consume WHERE
        return parse_where_clause(t, stmt);
    }

    return true;
}

static bool parse_delete(Tokenizer *t, SQLStatement *stmt)
{
    stmt->type = STMT_DELETE;

    // Expect "FROM"
    if (!expect_token(t, stmt, "FROM", "Expected FROM after DELETE"))
    {
        return false;
    }

    // Parse table name
    char *table_name = tokenizer_next(t);
    if (!table_name)
    {
        snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected table name");
        return false;
    }
    strncpy(stmt->table_name, table_name, MAX_TABLE_NAME - 1);
    SAFE_FREE(table_name);

    // Check for WHERE clause
    char *where_token = tokenizer_peek(t);
    if (where_token && strcasecmp(where_token, "WHERE") == 0)
    {
        char *where_token = tokenizer_next(t);
        SAFE_FREE(where_token); // Consume WHERE
        return parse_where_clause(t, stmt);
    }

    return true;
}

static bool parse_where_clause(Tokenizer *t, SQLStatement *stmt)
{
    stmt->has_where = true;

    // Parse column name
    char *column = tokenizer_next(t);
    if (!column)
    {
        snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected column name in WHERE clause");
        return false;
    }
    strncpy(stmt->where_column, column, MAX_COLUMN_NAME - 1);
    SAFE_FREE(column);

    // Parse operator
    char *op_str = tokenizer_next(t);
    if (!op_str)
    {
        snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected operator in WHERE clause");
        return false;
    }
    stmt->where_operator = parse_operator(op_str);
    SAFE_FREE(op_str);

    // Parse value
    char *value_str = tokenizer_next(t);
    if (!value_str)
    {
        snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected value in WHERE clause");
        return false;
    }

    // For now, assume integer values
    // In a real parser, you'd determine the type from schema
    stmt->where_value_type = DT_INT;
    int *int_val = SAFE_MALLOC(int, 1);
    *int_val = atoi(value_str);
    stmt->where_value = int_val;

    SAFE_FREE(value_str);
    return true;
}

static bool parse_value_list(Tokenizer *t, SQLStatement *stmt, bool for_insert)
{
    uint32_t value_count = 0;
    bool parsing_values = true;

    while (parsing_values && value_count < MAX_COLUMNS)
    {
        char *value_str = tokenizer_next(t);
        if (!value_str)
        {
            snprintf(stmt->error_message, sizeof(stmt->error_message), "Expected value");
            return false;
        }

        if (for_insert)
        {
            // Allocate space for values
            if (value_count == 0)
            {
                stmt->insert_values = SAFE_MALLOC(void *, MAX_COLUMNS);
                stmt->insert_value_types = SAFE_MALLOC(DataType, MAX_COLUMNS);
            }

            // Try to parse as different types
            // For now, assume strings are quoted and integers are not
            if (value_str[0] == '\'' || value_str[0] == '"')
            {
                // String value
                char *str_val = SAFE_MALLOC(char, strlen(value_str) - 1);
                strncpy(str_val, value_str + 1, strlen(value_str) - 2);
                str_val[strlen(value_str) - 2] = '\0';
                stmt->insert_values[value_count] = str_val;
                stmt->insert_value_types[value_count] = DT_STRING;
            }
            else
            {
                // Try integer
                int *int_val = SAFE_MALLOC(int, 1);
                *int_val = atoi(value_str);
                stmt->insert_values[value_count] = int_val;
                stmt->insert_value_types[value_count] = DT_INT;
            }
        }

        SAFE_FREE(value_str);
        value_count++;

        // Check for comma or closing paren
        char *next = tokenizer_peek(t);
        if (next && strcmp(next, ",") == 0)
        {
            char *comma = tokenizer_next(t);
            SAFE_FREE(comma); // Consume comma
        }
        else if (next && strcmp(next, ")") == 0)
        {
            char *closing_paren = tokenizer_next(t);
            SAFE_FREE(closing_paren); // Consume ")"
            parsing_values = false;
        }
        else if (!next)
        {
            snprintf(stmt->error_message, sizeof(stmt->error_message), "Unexpected end of value list");
            return false;
        }
    }

    if (for_insert)
    {
        stmt->insert_value_count = value_count;
    }

    return true;
}

static bool parse_join_clause(Tokenizer *t, SQLStatement *stmt)
{
    stmt->has_join = true;

    char *join_type = tokenizer_peek(t);
    if (join_type && (strcasecmp(join_type, "INNER") == 0 ||
                      strcasecmp(join_type, "LEFT") == 0 ||
                      strcasecmp(join_type, "RIGHT") == 0 ||
                      strcasecmp(join_type, "FULL") == 0))
    {

        char *jt = tokenizer_next(t);
        SAFE_FREE(jt); // Consume JOIN type
        if (strcasecmp(join_type, "INNER") == 0)
        {
            stmt->join_clause.type = JOIN_INNER;
        }
        else if (strcasecmp(join_type, "LEFT") == 0)
        {
            stmt->join_clause.type = JOIN_LEFT;
        }
        else if (strcasecmp(join_type, "RIGHT") == 0)
        {
            stmt->join_clause.type = JOIN_RIGHT;
        }
        else if (strcasecmp(join_type, "FULL") == 0)
        {
            stmt->join_clause.type = JOIN_FULL;
        }
    }
    else
    {
        stmt->join_clause.type = JOIN_INNER;
    }

    // Expect "JOIN"
    if (!expect_token(t, stmt, "JOIN", "Expected JOIN after JOIN type"))
    {
        return false;
    }

    // Parse right table
    char *right_table = tokenizer_next(t);
    if (!right_table)
    {
        snprintf(stmt->error_message, sizeof(stmt->error_message),
                 "Expected table name after JOIN");
        return false;
    }
    strncpy(stmt->join_clause.right_table, right_table, MAX_TABLE_NAME - 1);
    SAFE_FREE(right_table);

    // Expect "ON"
    if (!expect_token(t, stmt, "ON", "Expected ON after JOIN table"))
    {
        return false;
    }

    // Parse ON condition: left.column = right.column
    char *left_column = tokenizer_next(t);
    if (!left_column)
    {
        snprintf(stmt->error_message, sizeof(stmt->error_message),
                 "Expected left column in ON clause");
        return false;
    }

    // Check for table prefix
    char *dot = strchr(left_column, '.');
    if (dot)
    {
        *dot = '\0';
        // left_column now has table name
        char *col_name = dot + 1;
        strncpy(stmt->join_clause.on_left, col_name, MAX_COLUMN_NAME - 1);
    }
    else
    {
        strncpy(stmt->join_clause.on_left, left_column, MAX_COLUMN_NAME - 1);
    }
    SAFE_FREE(left_column);

    // Expect "="
    if (!expect_token(t, stmt, "=", "Expected = in ON clause"))
    {
        return false;
    }

    // Parse right column
    char *right_column = tokenizer_next(t);
    if (!right_column)
    {
        snprintf(stmt->error_message, sizeof(stmt->error_message),
                 "Expected right column in ON clause");
        return false;
    }

    dot = strchr(right_column, '.');
    if (dot)
    {
        *dot = '\0';
        char *col_name = dot + 1;
        strncpy(stmt->join_clause.on_right, col_name, MAX_COLUMN_NAME - 1);
    }
    else
    {
        strncpy(stmt->join_clause.on_right, right_column, MAX_COLUMN_NAME - 1);
    }
    SAFE_FREE(right_column);

    return true;
}

static bool parse_drop_table(Tokenizer* t, SQLStatement* stmt) {
    stmt->type = STMT_DROP_TABLE;

    // Parse table name
    char* table_name = tokenizer_next(t);
    if (!table_name) {
        snprintf(stmt->error_message, sizeof(stmt->error_message),
                "Expected table name");
        return false;
    }
    strncpy(stmt->drop_table, table_name, MAX_TABLE_NAME - 1);
    SAFE_FREE(table_name);
    
    return true;
}

static bool expect_token(Tokenizer *t, SQLStatement *stmt, const char *expected, const char *error_msg)
{
    char *token = tokenizer_next(t);
    if (!token || strcasecmp(token, expected) != 0)
    {
        if (token)
            SAFE_FREE(token);
        snprintf(stmt->error_message, sizeof(stmt->error_message), "%s", error_msg);
        stmt->has_error = true;
        return false;
    }
    SAFE_FREE(token);
    return true;
}

DataType parse_data_type(const char *type_str)
{
    if (strcasecmp(type_str, "INT") == 0 || strcasecmp(type_str, "INTEGER") == 0)
    {
        return DT_INT;
    }
    else if (strcasecmp(type_str, "FLOAT") == 0 || strcasecmp(type_str, "REAL") == 0)
    {
        return DT_FLOAT;
    }
    else if (strcasecmp(type_str, "STRING") == 0 || strcasecmp(type_str, "VARCHAR") == 0 ||
             strcasecmp(type_str, "TEXT") == 0 || strcasecmp(type_str, "CHAR") == 0)
    {
        return DT_STRING;
    }
    else if (strcasecmp(type_str, "BOOL") == 0 || strcasecmp(type_str, "BOOLEAN") == 0)
    {
        return DT_BOOL;
    }
    return DT_INT; // Default
}

OperatorType parse_operator(const char *op_str)
{
    if (strcmp(op_str, "=") == 0)
        return OP_EQUALS;
    if (strcmp(op_str, "!=") == 0 || strcasecmp(op_str, "<>") == 0)
        return OP_NOT_EQUALS;
    if (strcmp(op_str, ">") == 0)
        return OP_GREATER;
    if (strcmp(op_str, "<") == 0)
        return OP_LESS;
    if (strcmp(op_str, ">=") == 0)
        return OP_GREATER_EQUAL;
    if (strcmp(op_str, "<=") == 0)
        return OP_LESS_EQUAL;
    if (strcasecmp(op_str, "LIKE") == 0)
        return OP_LIKE;
    return OP_EQUALS; // Default
}

Tokenizer *tokenizer_create(const char *sql)
{
    Tokenizer *t = SAFE_MALLOC(Tokenizer, 1);
    t->buffer = SAFE_STRDUP(sql);
    t->length = strlen(sql);
    t->position = 0;
    return t;
}

char *tokenizer_next(Tokenizer *t)
{
    // Skip whitespace
    while (t->position < t->length && isspace(t->buffer[t->position]))
    {
        t->position++;
    }

    if (t->position >= t->length)
        return NULL;

    char c = t->buffer[t->position];

    // Handle string literals
    if (c == '\'' || c == '"')
    {
        char quote = c;
        size_t start = t->position;
        t->position++; // Skip opening quote

        while (t->position < t->length && t->buffer[t->position] != quote)
        {
            // Handle escape sequences
            if (t->buffer[t->position] == '\\' && t->position + 1 < t->length)
            {
                t->position++; // Skip escape char
            }
            t->position++;
        }

        if (t->position >= t->length)
        {
            // Unterminated string
            t->position = start;
            return NULL;
        }

        t->position++; // Skip closing quote

        size_t len = t->position - start;
        char *token = SAFE_MALLOC(char, len + 1);
        strncpy(token, t->buffer + start, len);
        token[len] = '\0';
        return token;
    }

    // Handle special characters
    if (strchr(",()=*><;", c) != NULL)
    {
        // Check for two-character operators
        if ((c == '!' || c == '<' || c == '>') && t->position + 1 < t->length &&
            t->buffer[t->position + 1] == '=')
        {
            char *token = SAFE_MALLOC(char, 3);
            token[0] = c;
            token[1] = '=';
            token[2] = '\0';
            t->position += 2;
            return token;
        }

        char *token = SAFE_MALLOC(char, 2);
        token[0] = c;
        token[1] = '\0';
        t->position++;
        return token;
    }

    // Parse identifier or keyword
    size_t start = t->position;
    while (t->position < t->length && !isspace(t->buffer[t->position]) &&
           strchr(",()=*><;", t->buffer[t->position]) == NULL)
    {
        t->position++;
    }

    size_t len = t->position - start;
    char *token = SAFE_MALLOC(char, len + 1);
    strncpy(token, t->buffer + start, len);
    token[len] = '\0';
    return token;
}

char *tokenizer_peek(Tokenizer *t)
{
    size_t old_pos = t->position;
    char *token = tokenizer_next(t);
    t->position = old_pos;
    return token;
}

bool tokenizer_has_more(Tokenizer *t)
{
    size_t old_pos = t->position;

    // Skip whitespace
    while (t->position < t->length && isspace(t->buffer[t->position]))
    {
        t->position++;
    }

    bool has_more = (t->position < t->length);
    t->position = old_pos;
    return has_more;
}

void tokenizer_free(Tokenizer *t)
{
    SAFE_FREE(t->buffer);
    SAFE_FREE(t);
}

void free_sql_statement(SQLStatement *statement)
{
    if (!statement)
        return;

    // Free WHERE value
    if (statement->where_value)
    {
        SAFE_FREE(statement->where_value);
    }

    // Free INSERT values
    if (statement->insert_values)
    {
        for (uint32_t i = 0; i < statement->insert_value_count; i++)
        {
            if (statement->insert_values[i])
            {
                SAFE_FREE(statement->insert_values[i]);
            }
        }
        SAFE_FREE(statement->insert_values);
    }

    if (statement->insert_value_types)
    {
        SAFE_FREE(statement->insert_value_types);
    }

    // Free UPDATE values (if implemented)
    if (statement->update_values)
    {
        for (uint32_t i = 0; i < statement->update_column_count; i++)
        {
            if (statement->update_values[i])
            {
                SAFE_FREE(statement->update_values[i]);
            }
        }
        SAFE_FREE(statement->update_values);
    }

    SAFE_FREE(statement);
}

const char *statement_type_to_string(StatementType type)
{
    switch (type)
    {
    case STMT_SELECT:
        return "SELECT";
    case STMT_INSERT:
        return "INSERT";
    case STMT_UPDATE:
        return "UPDATE";
    case STMT_DELETE:
        return "DELETE";
    case STMT_CREATE_TABLE:
        return "CREATE TABLE";
    case STMT_DROP_TABLE:
        return "DROP TABLE";
    case STMT_CREATE_INDEX:
        return "CREATE INDEX";
    case STMT_SHOW_TABLES:
        return "SHOW TABLES";
    case STMT_UNKNOWN:
        return "UNKNOWN";
    default:
        return "UNKNOWN";
    }
}
