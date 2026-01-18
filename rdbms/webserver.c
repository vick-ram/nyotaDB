#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include "storage.h"
#include "parser.h"
#include "executor.h"
#include <ctype.h>
#include "main.h"

#define PORT 8082
#define BUFFER_SIZE 8192
#define MAX_CLIENTS 10

static int server_fd_global = -1; // Global to access in signal handler

// HTML/CSS/JS for the web interface
const char* HTML_PAGE = 
"<!DOCTYPE html>"
"<html lang=\"en\">"
"<head>"
"    <meta charset=\"UTF-8\">"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"    <title>NyotaDB - Web Interface</title>"
"    <style>"
"        * { margin: 0; padding: 0; box-sizing: border-box; }"
"        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; "
"               background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; "
"               padding: 20px; color: #333; }"
"        .container { max-width: 1200px; margin: 0 auto; }"
"        .header { text-align: center; margin-bottom: 40px; color: white; }"
"        .header h1 { font-size: 3rem; margin-bottom: 10px; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }"
"        .header p { font-size: 1.2rem; opacity: 0.9; }"
"        .main { display: grid; grid-template-columns: 1fr 2fr; gap: 30px; }"
"        @media (max-width: 768px) { .main { grid-template-columns: 1fr; } }"
"        .panel { background: white; border-radius: 10px; padding: 25px; "
"                 box-shadow: 0 10px 30px rgba(0,0,0,0.2); }"
"        .panel h2 { color: #667eea; margin-bottom: 20px; padding-bottom: 10px; "
"                    border-bottom: 2px solid #f0f0f0; }"
"        .query-input textarea { width: 100%; height: 150px; padding: 15px; "
"                                border: 2px solid #e0e0e0; border-radius: 8px; "
"                                font-family: 'Courier New', monospace; font-size: 14px; "
"                                resize: vertical; margin-bottom: 15px; }"
"        .query-input textarea:focus { outline: none; border-color: #667eea; }"
"        button { background: #667eea; color: white; border: none; padding: 12px 25px; "
"                 border-radius: 8px; cursor: pointer; font-size: 16px; "
"                 transition: background 0.3s; margin-right: 10px; }"
"        button:hover { background: #5a67d8; }"
"        button.secondary { background: #48bb78; }"
"        button.secondary:hover { background: #38a169; }"
"        .results { overflow-x: auto; }"
"        table { width: 100%; border-collapse: collapse; margin-top: 20px; }"
"        th { background: #667eea; color: white; padding: 15px; text-align: left; }"
"        td { padding: 12px 15px; border-bottom: 1px solid #e0e0e0; }"
"        tr:hover { background: #f8f9fa; }"
"        .error { background: #fed7d7; color: #742a2a; padding: 15px; "
"                 border-radius: 8px; margin-top: 20px; border-left: 4px solid #fc8181; }"
"        .success { background: #c6f6d5; color: #22543d; padding: 15px; "
"                   border-radius: 8px; margin-top: 20px; border-left: 4px solid #48bb78; }"
"        .examples { margin-top: 20px; }"
"        .example { background: #f7fafc; padding: 10px; border-radius: 5px; "
"                   margin-bottom: 10px; cursor: pointer; border-left: 3px solid #667eea; }"
"        .example:hover { background: #edf2f7; }"
"        .example code { font-family: 'Courier New', monospace; color: #2d3748; }"
"    </style>"
"</head>"
"<body>"
"    <div class=\"container\">"
"        <div class=\"header\">"
"            <h1>NyotaDB Web Interface</h1>"
"            <p>A simple RDBMS with web-based SQL interface</p>"
"        </div>"
"        <div class=\"main\">"
"            <div class=\"panel\">"
"                <h2>SQL Query</h2>"
"                <div class=\"query-input\">"
"                    <textarea id=\"sqlInput\" placeholder=\"Enter SQL query here...\">"
"CREATE TABLE users (id INT PRIMARY KEY, name STRING(50), age INT);</textarea>"
"                </div>"
"                <div>"
"                    <button onclick=\"executeQuery()\">Execute Query</button>"
"                    <button class=\"secondary\" onclick=\"clearResults()\">Clear Results</button>"
"                </div>"
"                <div class=\"examples\">"
"                    <h3>Example Queries:</h3>"
"                    <div class=\"example\" onclick=\"document.getElementById('sqlInput').value = this.querySelector('code').textContent\">"
"                        <code>CREATE TABLE users (id INT PRIMARY KEY, name STRING(50), age INT);</code>"
"                    </div>"
"                    <div class=\"example\" onclick=\"document.getElementById('sqlInput').value = this.querySelector('code').textContent\">"
"                        <code>INSERT INTO users VALUES (1, 'Alice', 30);</code>"
"                    </div>"
"                    <div class=\"example\" onclick=\"document.getElementById('sqlInput').value = this.querySelector('code').textContent\">"
"                        <code>SELECT * FROM users;</code>"
"                    </div>"
"                    <div class=\"example\" onclick=\"document.getElementById('sqlInput').value = this.querySelector('code').textContent\">"
"                        <code>SELECT name, age FROM users WHERE age > 25;</code>"
"                    </div>"
"                </div>"
"            </div>"
"            <div class=\"panel\">"
"                <h2>Results</h2>"
"                <div id=\"results\">"
"                    <p>Results will appear here...</p>"
"                </div>"
"            </div>"
"        </div>"
"    </div>"
"    <script>"
"        async function executeQuery() {"
"            const sql = document.getElementById('sqlInput').value.trim();"
"            if (!sql) {"
"                showError('Please enter a SQL query');"
"                return;"
"            }"
"            "
"            const resultsDiv = document.getElementById('results');"
"            resultsDiv.innerHTML = '<p>Executing query...</p>';"
"            "
"            try {"
"                const response = await fetch('/api/query', {"
"                    method: 'POST',"
"                    headers: { 'Content-Type': 'application/json' },"
"                    body: JSON.stringify({ query: sql })"
"                });"
"                "
"                const data = await response.json();"
"                "
"                if (data.error) {"
"                    showError('Error: ' + data.error);"
"                } else if (data.results) {"
"                    displayResults(data.results);"
"                } else if (data.message) {"
"                    showSuccess(data.message);"
"                }"
"            } catch (error) {"
"                showError('Network error: ' + error.message);"
"            }"
"        }"
"        "
"        function displayResults(results) {"
"            let html = '';"
"            "
"            if (results.rows && results.rows.length > 0) {"
"                html += '<div class=\"success\">' + results.rowCount + ' row(s) returned</div>';"
"                html += '<div class=\"results\"><table>';"
"                html += '<thead><tr>';"
"                results.columns.forEach(col => {"
"                    html += '<th>' + col + '</th>';"
"                });"
"                html += '</tr></thead><tbody>';"
"                "
"                results.rows.forEach(row => {"
"                    html += '<tr>';"
"                    row.forEach(cell => {"
"                        html += '<td>' + (cell || 'NULL') + '</td>';"
"                    });"
"                    html += '</tr>';"
"                });"
"                html += '</tbody></table></div>';"
"            } else {"
"                html += '<p>Empty result set</p>';"
"            }"
"            "
"            document.getElementById('results').innerHTML = html;"
"        }"
"        "
"        function showError(message) {"
"            document.getElementById('results').innerHTML = "
"                '<div class=\"error\">' + message + '</div>';"
"        }"
"        "
"        function showSuccess(message) {"
"            document.getElementById('results').innerHTML = "
"                '<div class=\"success\">' + message + '</div>';"
"        }"
"        "
"        function clearResults() {"
"            document.getElementById('results').innerHTML = '<p>Results cleared</p>';"
"        }"
"        "
"        // Allow Ctrl+Enter to execute query"
"        document.getElementById('sqlInput').addEventListener('keydown', function(e) {"
"            if (e.ctrlKey && e.key === 'Enter') {"
"                executeQuery();"
"            }"
"        });"
"    </script>"
"</body>"
"</html>";

// Convert QueryResult to JSON
char* result_to_json(QueryResult* result) {
    if (!result) {
        return SAFE_STRDUP("{\"error\":\"Null result\"}");
    }
    
    if (result->error_message) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "{\"error\":\"%s\"}", result->error_message);
        return SAFE_STRDUP(buffer);
    }
    
    // Calculate needed buffer size
    size_t buffer_size = 1024; // Start with reasonable size
    char* json = SAFE_MALLOC(char, buffer_size);
    if (!json) return NULL;
    
    int pos = 0;
    
    pos += snprintf(json + pos, buffer_size - pos, 
                   "{\"columns\":[");
    
    for (uint32_t i = 0; i < result->column_count; i++) {
        pos += snprintf(json + pos, buffer_size - pos, 
                       "\"%s\"%s", 
                       result->column_names[i],
                       (i < result->column_count - 1) ? "," : "");
        
        // Reallocate if needed
        if (pos >= buffer_size - 100) {
            buffer_size *= 2;
            json = SAFE_REALLOC(json, char, buffer_size);
            if (!json) return NULL;
        }
    }
    
    pos += snprintf(json + pos, buffer_size - pos, "],\"rows\":[");
    
    for (uint32_t row = 0; row < result->row_count; row++) {
        pos += snprintf(json + pos, buffer_size - pos, "[");
        
        for (uint32_t col = 0; col < result->column_count; col++) {
            if (result->rows[row][col]) {
                // Escape quotes in string values
                char* value = (char*)result->rows[row][col];
                char* escaped = SAFE_MALLOC(char, strlen(value) * 2 + 3);
                if (escaped) {
                    char* dest = escaped;
                    *dest++ = '"';
                    for (char* src = value; *src; src++) {
                        if (*src == '"' || *src == '\\') {
                            *dest++ = '\\';
                        }
                        *dest++ = *src;
                    }
                    *dest++ = '"';
                    *dest = '\0';
                    
                    pos += snprintf(json + pos, buffer_size - pos, "%s", escaped);
                    SAFE_FREE(escaped);
                } else {
                    pos += snprintf(json + pos, buffer_size - pos, "\"\"");
                }
            } else {
                pos += snprintf(json + pos, buffer_size - pos, "null");
            }
            
            pos += snprintf(json + pos, buffer_size - pos, 
                           "%s", (col < result->column_count - 1) ? "," : "");
            
            // Reallocate if needed
            if (pos >= buffer_size - 100) {
                buffer_size *= 2;
                json = SAFE_REALLOC(json, char, buffer_size);
                if (!json) return NULL;
            }
        }
        
        pos += snprintf(json + pos, buffer_size - pos, "]%s", 
                       (row < result->row_count - 1) ? "," : "");
    }
    
    pos += snprintf(json + pos, buffer_size - pos, 
                   "],\"rowCount\":%u}", result->row_count);
    
    return json;
}

// Handle HTTP request
void handle_request(int client_fd, StorageManager* sm) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
    
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    
    // Parse request line
    char* method = strtok(buffer, " ");
    char* path = strtok(NULL, " ");
    
    if (!method || !path) {
        const char* response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        write(client_fd, response, strlen(response));
        close(client_fd);
        return;
    }
    
    // Find body
    char* body = strstr(buffer, "\r\n\r\n");
    if (body) {
        body += 4;
    }
    
    // Handle routes
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        // Serve HTML page
        char response_header[256];
        snprintf(response_header, sizeof(response_header),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: %lu\r\n\r\n",
                strlen(HTML_PAGE));
        
        write(client_fd, response_header, strlen(response_header));
        write(client_fd, HTML_PAGE, strlen(HTML_PAGE));
    }
    else if (strcmp(path, "webapp/index.html") == 0) {
        // Serve static files from webapp
        char file_path[256];
        snprintf(file_path, sizeof(file_path), "webapp%s", path + 7); // Remove "/webapp" prefix

        FILE* file = fopen(file_path, "rb");
        printf("File found or not=====", file_path);
        if (!file) {
            const char* response = 
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/html\r\n\r\n"
                "<h1>404 Not Found</h1>";
            write(client_fd, response, strlen(response));
            close(client_fd);
            return;
        }

        // Get file size
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        // Determine content type
        const char* content_type = "text/plain";
        if (strstr(file_path, ".html")) content_type = "text/html";
        else if (strstr(file_path, ".css")) content_type = "text/css";
        else if (strstr(file_path, ".js")) content_type = "application/javascript";
        else if (strstr(file_path, ".json")) content_type = "application/json";
        
        // Send header
        char response_header[256];
        snprintf(response_header, sizeof(response_header),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %ld\r\n\r\n",
                content_type, file_size);
        
        write(client_fd, response_header, strlen(response_header));
        
        // Send file content
        char file_buffer[BUFFER_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(file_buffer, 1, BUFFER_SIZE, file)) > 0) {
            write(client_fd, file_buffer, bytes_read);
        }
        
        fclose(file);
    }
    else if (strcmp(path, "/api/query") == 0 && strcmp(method, "POST") == 0) {
        // Handle SQL query API
        char* query = NULL;
        
        // Parse JSON body (simple parsing)
        if (body) {
            char* query_start = strstr(body, "\"query\"");
            if (query_start) {
                query_start = strchr(query_start, ':');
                if (query_start) {
                    query_start++;
                    while (*query_start && isspace(*query_start)) query_start++;
                    
                    if (*query_start == '"') {
                        query_start++;
                        char* query_end = strchr(query_start, '"');
                        if (query_end) {
                            *query_end = '\0';
                            query = query_start;
                        }
                    }
                }
            }
        }
        
        if (!query) {
            const char* response = 
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Type: application/json\r\n\r\n"
                "{\"error\":\"No query provided\"}";
            write(client_fd, response, strlen(response));
            close(client_fd);
            return;
        }
        
        // Execute query
        SQLStatement* stmt = parse_sql(query);
        char* json_response = NULL;
        
        if (!stmt || stmt->has_error) {
            json_response = SAFE_MALLOC(char, 256);
            if (json_response) {
                snprintf(json_response, 256, 
                        "{\"error\":\"Parse error: %s\"}", 
                        stmt ? stmt->error_message : "Failed to parse");
            }
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
                case STMT_DELETE:
                    result = execute_delete(sm, stmt);
                    break;
                default:
                    json_response = SAFE_STRDUP("{\"error\":\"Unsupported statement type\"}");
                    break;
            }
            
            if (result) {
                json_response = result_to_json(result);
                free_result(result);
            } else if (!json_response) {
                json_response = SAFE_STRDUP("{\"message\":\"Query executed successfully\"}");
            }
            
            free_sql_statement(stmt);
        }
        
        if (!json_response) {
            json_response = SAFE_STRDUP("{\"error\":\"Internal server error\"}");
        }
        
        // Send response
        char response_header[256];
        snprintf(response_header, sizeof(response_header),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %lu\r\n"
                "Access-Control-Allow-Origin: *\r\n\r\n",
                strlen(json_response));
        
        write(client_fd, response_header, strlen(response_header));
        write(client_fd, json_response, strlen(json_response));
        
        SAFE_FREE(json_response);
    }
    else {
        // 404 Not Found
        const char* response = 
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n\r\n"
            "<h1>404 Not Found</h1>";
        write(client_fd, response, strlen(response));
    }
    
    close(client_fd);
}

void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\nShutting down nyotaDB web server...\n");
        if (server_fd_global >= 0) {
            close(server_fd_global);
            server_fd_global = -1;
        }
        exit(0);
    }
}

void run_webserver(StorageManager* sm) {
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    // Create socket
    if ((server_fd_global = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd_global, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Failed to set socket options");
        close(server_fd_global);
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Bind
    if (bind(server_fd_global, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Failed to bind socket");
        close(server_fd_global);
        exit(EXIT_FAILURE);
    }
    
    // Listen
    if (listen(server_fd_global, MAX_CLIENTS) < 0) {
        perror("Failed to listen on socket");
        close(server_fd_global);
        exit(EXIT_FAILURE);
    }
    
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║        NyotaDB Web Server v0.1              ║\n");
    printf("║                                              ║\n");
    printf("║  • Web Interface: http://localhost:%d       ║\n", PORT);
    printf("║  • API Endpoint: http://localhost:%d/api/query ║\n", PORT);
    printf("║  • Press Ctrl+C to stop server               ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");
    
    while (1) {
        int client_fd = accept(server_fd_global, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_fd < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, exit loop
                break;
            }
            perror("Failed to accept connection");
            continue;
        }
        
        handle_request(client_fd, sm);
    }

    // Close socket if loop exists
    if (server_fd_global != -1) {
        close(server_fd_global);
        server_fd_global = -1;
    }
}