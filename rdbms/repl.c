#include "storage.h"
#include "parser.h"
#include "executor.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "main.h"

#define HISTORY_FILE ".nyotadb_history"

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>

static char* sql_keywords[] = {
    "SELECT", "FROM", "WHERE", "INSERT", "INTO", "VALUES", 
    "UPDATE", "SET", "DELETE", "CREATE", "TABLE", "DROP", 
    "INTEGER", "TEXT", "PRIMARY", "KEY", "NULL", "JOIN", NULL
};

// Autocomplete generator
char* sql_generator(const char* text, int state) {
    static int list_index, len;
    char* name;
    if (!state) {
        list_index = 0;
        len = strlen(text);
    }
    while ((name = sql_keywords[list_index++])) {
        if (strncasecmp(name, text, len) == 0) return SAFE_STRDUP(name);
    }
    return NULL;
}

char** sql_completer(const char* text, int start, int end) {
    rl_attempted_completion_over = 1;
    return rl_completion_matches(text, sql_generator);
}

void initialize_readline() {
    using_history();
    read_history(HISTORY_FILE);
    rl_attempted_completion_function = sql_completer;
}

void finish_readline() {
    write_history(HISTORY_FILE);
}
#else
// Fallback if no readline
void initialize_readline() {}
void finish_readline() {}
char* simple_readline(const char* prompt) {
    char buffer[2048];
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) return NULL;
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') buffer[len-1] = '\0';
    return SAFE_STRDUP(buffer);
}
#endif

int is_command_complete(const char* line) {
    if (!line || strlen(line) == 0) return 0;
    // Dot commands are immediately complete
    if (line[0] == '.') return 1;
    // SQL commands are complete if they end with a semicolon
    size_t len = strlen(line);
    while (len > 0 && isspace(line[len-1])) len--;
    return (len > 0 && line[len-1] == ';');
}

void print_result(QueryResult* result) {
    if (!result) {
        printf("ERROR: Null result\n");
        return;
    }
    
    if (result->error_message) {
        printf("ERROR: %s\n", result->error_message);
        return;
    }
    
    if (result->column_count == 0 || result->row_count == 0) {
        printf("Empty result set\n");
        return;
    }
    
    // Calculate column widths
    int col_widths[MAX_COLUMNS];
    for (uint32_t i = 0; i < result->column_count; i++) {
        col_widths[i] = strlen(result->column_names[i]);
    }
    
    for (uint32_t row = 0; row < result->row_count; row++) {
        for (uint32_t col = 0; col < result->column_count; col++) {
            if (result->rows[row][col]) {
                int len = strlen((char*)result->rows[row][col]);
                if (len > col_widths[col]) {
                    col_widths[col] = len;
                }
            }
        }
    }
    
    // Print header
    printf("+");
    for (uint32_t i = 0; i < result->column_count; i++) {
        for (int j = 0; j < col_widths[i] + 2; j++) printf("-");
        printf("+");
    }
    printf("\n");
    
    printf("|");
    for (uint32_t i = 0; i < result->column_count; i++) {
        printf(" %-*s |", col_widths[i], result->column_names[i]);
    }
    printf("\n");
    
    printf("+");
    for (uint32_t i = 0; i < result->column_count; i++) {
        for (int j = 0; j < col_widths[i] + 2; j++) printf("-");
        printf("+");
    }
    printf("\n");
    
    // Print rows
    for (uint32_t row = 0; row < result->row_count; row++) {
        printf("|");
        for (uint32_t col = 0; col < result->column_count; col++) {
            if (result->rows[row][col]) {
                printf(" %-*s |", col_widths[col], (char*)result->rows[row][col]);
            } else {
                printf(" %-*s |", col_widths[col], "NULL");
            }
        }
        printf("\n");
    }
    
    printf("+");
    for (uint32_t i = 0; i < result->column_count; i++) {
        for (int j = 0; j < col_widths[i] + 2; j++) printf("-");
        printf("+");
    }
    printf("\n");
    
    printf("%u row(s) in set\n\n", result->row_count);
}

void print_welcome() {
    printf("╔══════════════════════════════════════╗\n");
    printf("║         NyotaDB v0.1 - REPL          ║\n");
    printf("║    Simple RDBMS Implementation       ║\n");
    printf("╚══════════════════════════════════════╝\n");
    printf("\n");
    printf("Type commands ending with ';' or use:\n");
    printf("  HELP;     - Show this help\n");
    printf("  QUIT;     - Exit the REPL\n");
    printf("  CLEAR;    - Clear screen\n");
    printf("  SHOW TABLES; - List all tables\n");
    printf("\n");
}

void print_help() {
    printf("\nAvailable SQL commands:\n");
    printf("────────────────────────────────────────\n");
    printf("  CREATE TABLE table_name (\n");
    printf("      column_name DATA_TYPE [PRIMARY KEY],\n");
    printf("      ...\n");
    printf("  );\n\n");
    
    printf("  INSERT INTO table_name VALUES (value1, value2, ...);\n\n");
    
    printf("  SELECT column1, column2 FROM table_name\n");
    printf("      [WHERE condition];\n\n");
    
    printf("  DELETE FROM table_name [WHERE condition];\n\n");
    
    printf("  DROP TABLE table_name;\n\n");
    
    printf("  SHOW TABLES;\n\n");
    
    printf("Utility commands:\n");
    printf("────────────────────────────────────────\n");
    printf("  HELP;     - Show this help\n");
    printf("  QUIT;     - Exit\n");
    printf("  CLEAR;    - Clear screen\n");
    printf("  .tables   - List tables (alternative)\n");
    printf("  .schema table_name - Show table schema\n");
    printf("\n");
}

// Update handle_dot_command in repl.c
void handle_dot_command(StorageManager* sm, const char* command) {
    if (strcmp(command, ".tables") == 0 || strcasecmp(command, "SHOW TABLES;") == 0) {
        // Use the real implementation
        QueryResult* result = execute_show_tables(sm);
        if (result) {
            print_result(result);
            free_result(result);
        }
    }
    else if (strncmp(command, ".schema ", 8) == 0) {
        const char* table_name = command + 8;
        // Load and display schema
        TableSchema* schema = load_schema(sm, table_name);
        if (schema) {
            printf("Schema for table '%s':\n", table_name);
            printf("Columns: %u\n", schema->column_count);
            printf("Row size: %u bytes\n", schema->row_size);
            
            for (uint32_t i = 0; i < schema->column_count; i++) {
                printf("  %s: ", schema->columns[i].name);
                switch (schema->columns[i].type) {
                    case DT_INT: printf("INT"); break;
                    case DT_FLOAT: printf("FLOAT"); break;
                    case DT_STRING: printf("STRING(%u)", schema->columns[i].length); break;
                    case DT_BOOL: printf("BOOL"); break;
                }
                if (schema->columns[i].is_primary) printf(" PRIMARY KEY");
                if (schema->columns[i].is_unique) printf(" UNIQUE");
                printf("\n");
            }
            SAFE_FREE(schema);
        } else {
            printf("Table '%s' not found\n", table_name);
        }
    }
    else if (strcmp(command, ".clear") == 0 || strcasecmp(command, "CLEAR;") == 0) {
        printf("\033[2J\033[H"); // Clear screen
        print_welcome();
    }
    else if (strcmp(command, ".stats") == 0) {
        // Show database statistics
        printf("Database Statistics:\n");
        printf("  Total pages: %u\n", sm->header.page_count);
        printf("  Schema page: %u\n", sm->header.schema_page);
        printf("  Root page: %u\n", sm->header.root_page);
        printf("  Cache size: %u pages\n", sm->cache_size);
    }
    else {
        printf("Unknown dot command: %s\n", command);
        printf("Available dot commands:\n");
        printf("  .tables          - List all tables\n");
        printf("  .schema <table>  - Show table schema\n");
        printf("  .stats           - Show database statistics\n");
        printf("  .clear           - Clear screen\n");
    }
}

void run_repl() {
    StorageManager* sm = sm_open("nyotadb.db");
    if (!sm) {
        printf("ERROR: Failed to open/create database 'nyotadb.db'\n");
        return;
    }
    
    initialize_readline();
    print_welcome();
    
    char* line = NULL;
    char* full_statement = NULL;
    while (1) {
        const char* prompt = (full_statement == NULL) ? "nyotadb> " : "     ..> ";

#ifdef HAVE_READLINE
        line = readline(prompt);
#else
        line = simple_readline(prompt);
#endif
        
        if (!line) {
            printf("\n");
            break; // EOF (Ctrl+D)
        }

        if (strlen(line) == 0 && full_statement == NULL) {
            SAFE_FREE(line);
            continue;
        }

        if (full_statement == NULL) {
            full_statement = SAFE_STRDUP(line);
        } else {
            size_t new_size = strlen(full_statement) + strlen(line) + 2;
            full_statement = SAFE_REALLOC(full_statement, char, new_size);
            strcat(full_statement, " ");
            strcat(full_statement, line);
        }
        SAFE_FREE(line);

        if (!is_command_complete(full_statement)) continue;

#ifdef HAVE_READLINE
        add_history(full_statement);
        append_history(1, HISTORY_FILE);
#endif

        // Handle quit early
        if (full_statement[0] == '.' || 
            strcasecmp(full_statement, "HELP;") == 0 ||
            strcasecmp(full_statement, "QUIT;") == 0 ||
            strcasecmp(full_statement, "EXIT;") == 0 ||
            strcasecmp(full_statement, "CLEAR;") == 0 ||
            strcasecmp(full_statement, "SHOW TABLES;") == 0) {
            
            if (strcasecmp(full_statement, "HELP;") == 0) {
                print_help();
            }
            else if (strcasecmp(full_statement, "QUIT;") == 0 || strcasecmp(full_statement, "EXIT;") == 0) {
                break; // Break will reach the cleanup at the bottom
            }
            else if (strcasecmp(full_statement, "CLEAR;") == 0) {
                printf("\033[H\033[J"); // ANSI escape code to clear screen
            }
            else if (strcasecmp(full_statement, "SHOW TABLES;") == 0) {
                handle_dot_command(sm, ".tables");
            }
            else {
                handle_dot_command(sm, full_statement);
            }
        } else {
            SQLStatement* stmt = parse_sql(full_statement);
            if (stmt) {
                if (stmt->has_error) {
                    printf("Parse error: %s\n", stmt->error_message);
                } else {
                    QueryResult* result = NULL;
                    switch (stmt->type) {
            case STMT_CREATE_TABLE:
                result = execute_create_table(sm, stmt);
                break;
            case STMT_SELECT:
                result = execute_select(sm, stmt);
                break;
            case STMT_INSERT:
                result = execute_insert(sm, stmt);
                break;
            case STMT_UPDATE:
                result = execute_update(sm, stmt);
                break;
            case STMT_DELETE:
                result = execute_delete(sm, stmt);
                break;
            case STMT_DROP_TABLE:
                printf("DROP TABLE not yet implemented\n");
                break;
            case STMT_SHOW_TABLES:
                handle_dot_command(sm, ".tables");
                break;
            default:
                printf("Statement type '%s' not yet implemented\n", 
                       statement_type_to_string(stmt->type));
        }
        
        if (result) {
            print_result(result);
            free_result(result);
        }
                }
        free_sql_statement(stmt);
            }

        }
        SAFE_FREE(full_statement);
    }
    
    SAFE_FREE(full_statement);
    finish_readline();
    sm_close(sm);
    printf("Goodbye!\n");
}
