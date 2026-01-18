#ifndef MAIN_H
#define MAIN_H

#include <stdlib.h>

void* safe_malloc(size_t size);
void* safe_calloc(size_t num, size_t size);
void* safe_realloc(void* ptr, size_t new_size);
void safe_free(void** ptr_ref);
char* safe_strdup(const char* str);
void** safe_malloc_2d(size_t rows, size_t cols, size_t element_size);
void safe_free_2d(void*** array_ref, size_t rows);
int check_pointer(const void* ptr, const char* name);


#define SAFE_MALLOC(type, count) (type*)safe_malloc((count) * sizeof(type))
#define SAFE_CALLOC(type, count) (type*)safe_calloc(count, sizeof(type))
#define SAFE_REALLOC(ptr, type, count) (type*)safe_realloc(ptr, (count) * sizeof(type))
#define SAFE_FREE(ptr) safe_free((void**)&ptr)
#define SAFE_STRDUP(str) safe_strdup(str)
#define SAFE_MALLOC_2D(rows, cols, element_size) safe_malloc_2d(rows, cols, element_size)
#define SAFE_FREE_2D(array_ref, rows) safe_free_2d((void***)&array_ref, rows)
#define CHECK_POINTER(ptr, name) check_pointer(ptr, name)
#endif /* MAIN_H */
