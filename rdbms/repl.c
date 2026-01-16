#include "storage.h"
#include "parser.h"
#include "executor.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#else
// Simple fallback input if readline is not available
char* simple_readline(const char* prompt) {
    static char buffer[1024];
    printf("%s", prompt);
    fflush(stdout);
    
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return NULL;
    }
    
    // Remove newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') {
        buffer[len-1] = '\0';
    }
    
    return strdup(buffer);
}
#endif

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

void handle_dot_command(StorageManager* sm, const char* command) {
    if (strcmp(command, ".tables") == 0 || strcasecmp(command, "SHOW TABLES;") == 0) {
        // TODO: Implement table listing
        printf("+----------------------+\n");
        printf("| Tables in database   |\n");
        printf("+----------------------+\n");
        printf("| (Not implemented)    |\n");
        printf("+----------------------+\n");
    }
    else if (strncmp(command, ".schema ", 8) == 0) {
        const char* table_name = command + 8;
        // TODO: Load and display schema
        printf("Schema for table '%s':\n", table_name);
        printf("(Not implemented)\n");
    }
    else if (strcmp(command, ".clear") == 0 || strcasecmp(command, "CLEAR;") == 0) {
        printf("\033[2J\033[H"); // Clear screen
        print_welcome();
    }
    else {
        printf("Unknown dot command: %s\n", command);
        printf("Available dot commands: .tables, .schema <table>, .clear\n");
    }
}

void run_repl() {
    StorageManager* sm = sm_open("nyotadb.db");
    if (!sm) {
        printf("ERROR: Failed to open/create database 'nyotadb.db'\n");
        return;
    }
    
    print_welcome();
    
    char* line = NULL;
    while (1) {
#ifdef HAVE_READLINE
        line = readline("nyotadb> ");
#else
        line = simple_readline("nyotadb> ");
#endif
        
        if (!line) {
            printf("\n");
            break; // EOF (Ctrl+D)
        }
        
        // Skip empty lines
        if (strlen(line) == 0) {
            free(line);
            continue;
        }
        
#ifdef HAVE_READLINE
        if (strlen(line) > 0) {
            add_history(line);
        }
#endif
        
        // Handle dot commands and special commands
        if (line[0] == '.' || 
            strcasecmp(line, "HELP;") == 0 ||
            strcasecmp(line, "QUIT;") == 0 ||
            strcasecmp(line, "EXIT;") == 0 ||
            strcasecmp(line, "CLEAR;") == 0 ||
            strcasecmp(line, "SHOW TABLES;") == 0) {
            
            if (strcasecmp(line, "HELP;") == 0) {
                print_help();
            }
            else if (strcasecmp(line, "QUIT;") == 0 || strcasecmp(line, "EXIT;") == 0) {
                free(line);
                break;
            }
            else {
                handle_dot_command(sm, line);
            }
            
            free(line);
            continue;
        }
        
        // Parse SQL statement
        SQLStatement* stmt = parse_sql(line);
        if (!stmt) {
            printf("ERROR: Failed to parse statement\n");
            free(line);
            continue;
        }
        
        if (stmt->has_error) {
            printf("Parse error: %s\n", stmt->error_message);
            free_sql_statement(stmt);
            free(line);
            continue;
        }
        
        // Execute statement
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
        
        free_sql_statement(stmt);
        free(line);
    }
    
    sm_close(sm);
    printf("Goodbye! Database saved to 'nyotadb.db'\n");
}

// int main(int argc, char* argv[]) {
//     if (argc > 1 && strcmp(argv[1], "--web") == 0) {
//         // Run in web server mode
//         StorageManager* sm = sm_open("nyotadb.db");
//         if (!sm) {
//             printf("ERROR: Failed to open database for web server\n");
//             return 1;
//         }
//         run_webserver(sm);
//         sm_close(sm);
//     } else {
//         // Run in REPL mode
//         run_repl();
//     }
//     return 0;
// }