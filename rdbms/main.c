#include <stdio.h>
#include <string.h>
#include "storage.h"


int main(int argc, char* argv[]) {
    StorageManager* sm = sm_open("test.db");
    if (!sm) {
        fprintf(stderr, "Failed to open/create database\n");
        return 1;
    }
    
    // Check command line arguments
    if (argc > 1 && strcmp(argv[1], "--web") == 0) {
        // Web server mode
        run_webserver(sm);
    } else {
        // REPL mode (default)
        run_repl(sm);
    }
    
    sm_close(sm);
    return 0;
}