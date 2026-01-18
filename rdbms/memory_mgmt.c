#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "main.h"

/**
 * @brief Checks for integer overflow during multiplication
 */
static int will_overflow(size_t a, size_t b) {
    if (a > 0 && b > SIZE_MAX / a) return 1;
    return 0;
}

/**
 * @brief Safely allocates memory. Aborts program if allocation fails.
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated memory
 */
void* safe_malloc(size_t size) {
    if (size == 0) {
        fprintf(stderr, "Warning: Attempting to allocate 0 bytes.\n");
        return NULL;
    }

    void* ptr = malloc(size);
    if (ptr == NULL && size != 0) {
        fprintf(stderr, "Error: Memory allocation failed for %zu bytes.\n", size);
        exit(EXIT_FAILURE); // Prevent program from continuing with a NULL pointer
    }
    return ptr;
}

void* safe_calloc(size_t num, size_t size) {
    if (num == 0 || size == 0) {
        fprintf(stderr, "Warning: Attempting to allocate 0 bytes with safe_calloc.\n");
        return NULL;
    }

    void* ptr = calloc(num, size);
    if (ptr == NULL) {
        fprintf(stderr, "Fatal Error: Memory allocation failed for %zu elements of size %zu.\n", num, size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

/**
 * @brief Safely frees memory and sets the pointer to NULL.
 * @param ptr_ref A pointer to the pointer variable (to modify the original).
 */
void safe_free(void** ptr_ref) {
    if (ptr_ref == NULL) {
        fprintf(stderr, "Warning: NULL pointer passed to safe_free.\n");
        return;
    }
    
    if (*ptr_ref != NULL) {
        free(*ptr_ref);
        *ptr_ref = NULL;
    }
}

/**
 * @brief Safely reallocates memory.
 * @param ptr Pointer to previously allocated memory.
 * @param new_size New size in bytes.
 * @return Pointer to the reallocated memory.
 */
void* safe_realloc(void* ptr, size_t new_size) {
    if (new_size == 0) {
        safe_free(&ptr);
        return NULL;
    }
    
    void* new_ptr = realloc(ptr, new_size);
    if (new_ptr == NULL) {
        fprintf(stderr, "Fatal Error: Memory reallocation failed for %zu bytes.\n", new_size);
        safe_free(&ptr); // Free old pointer before exiting
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

/**
 * @brief Duplicates a string safely.
 * @param str String to duplicate.
 * @return Newly allocated string copy.
 */
char* safe_strdup(const char* str) {
    if (str == NULL) {
        return NULL;
    }
    
    size_t len = strlen(str) + 1;
    char* new_str = (char*)safe_malloc(len);
    strcpy(new_str, str);
    return new_str;
}

/**
 * @brief Allocates a 2D array safely.
 * @param rows Number of rows.
 * @param cols Number of columns.
 * @param element_size Size of each element.
 * @return Pointer to the 2D array.
 */
void** safe_malloc_2d(size_t rows, size_t cols, size_t element_size) {
    if (will_overflow(rows, sizeof(void*)) || will_overflow(cols, element_size)) {
        fprintf(stderr, "Fatal: 2D allocation size overflow\n");
        exit(EXIT_FAILURE);
    }
    // Allocate row pointers
    void** array = (void**)safe_malloc(rows * sizeof(void*));
    
    // Allocate each row
    for (size_t i = 0; i < rows; i++) {
        array[i] = safe_malloc(cols * element_size);
    }
    
    return array;
}

/**
 * @brief Frees a 2D array safely.
 * @param array Pointer to the 2D array.
 * @param rows Number of rows.
 */
void safe_free_2d(void*** array_ref, size_t rows) {
    if (array_ref == NULL || *array_ref == NULL) {
        return;
    }
    
    void** array = *array_ref;
    for (size_t i = 0; i < rows; i++) {
        safe_free(&array[i]);
    }
    safe_free((void**)array_ref);
}

/**
 * @brief Checks if a pointer is NULL and logs an error.
 * @param ptr Pointer to check.
 * @param name Name of the pointer for error message.
 * @return 1 if valid, 0 if NULL.
 */
int check_pointer(const void* ptr, const char* name) {
    if (ptr == NULL) {
        fprintf(stderr, "Error: Pointer '%s' is NULL.\n", name);
        return 0;
    }
    return 1;
}

