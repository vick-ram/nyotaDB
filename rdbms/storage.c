#include "storage.h"
#include "btree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static void lru_remove(StorageManager* sm, Page* page);
static void lru_insert_front(StorageManager* sm, Page* page);
static void lru_touch(StorageManager* sm, Page* page);
static void evict_lru_page(StorageManager* sm);

StorageManager* sm_open(const char* filename) {
    StorageManager* sm = (StorageManager*)malloc(sizeof(StorageManager));
    if (!sm) return NULL;
    
    sm->cache_size = 0;
    sm->lru_head = sm->lru_tail = NULL;
    memset(sm->pages, 0, sizeof(sm->pages));


    // Open or create file
    sm->fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (sm->fd < 0) {
        free(sm);
        return NULL;
    }

    // Check if file is new
    off_t file_size = lseek(sm->fd, 0, SEEK_END);
    if (file_size == 0) {
        // Initialize new database
        sm->header.magic_number = 0x0042444D;
        sm->header.page_count = 1;
        sm->header.root_page = 0;
        sm->header.first_free_page = 0;
        sm->header.schema_page = 0;

        // Write header
        lseek(sm->fd, 0, SEEK_SET);
        write(sm->fd, &sm->header, sizeof(DBHeader));

        // Initialize first page (schema page)
        Page* first_page = (Page*)malloc(sizeof(Page));
        first_page->page_id = 0;
        sm->pages[0] = first_page;
    } else {
        // Read existing header
        lseek(sm->fd, 0, SEEK_SET);
        read(sm->fd, &sm->header, sizeof(DBHeader));
    
        if (sm->header.magic_number != 0x0042444D) {
            close(sm->fd);
            free(sm);
            return NULL;
        }
    }

    return sm;
}

Page* sm_get_page(StorageManager* sm, uint32_t page_id) {
    // cache lookup
    for (uint32_t i = 0; i < sm->cache_size; i++) {
        Page* page = sm->pages[i];
        if (page && page->page_id == page_id) {
            lru_touch(sm, page);
            return page;
        }
    }

    // Evict if cache is full
    if (sm->cache_size >= MAX_CACHE_PAGES) {
        evict_lru_page(sm);
    }


    // Read from disk
    off_t offset = page_id * PAGE_SIZE + sizeof(DBHeader);
    lseek(sm->fd, offset, SEEK_SET);

    Page* page = (Page*)malloc(sizeof(Page));
    page->page_id = page_id;
    page->is_dirty = false;
    page->prev = page->next = NULL;

    ssize_t bytes_read = read(sm->fd, page->data, PAGE_SIZE);
    if (bytes_read != PAGE_SIZE) {
        free(page);
        return NULL;
    }

    sm->pages[sm->cache_size++] = page;
    lru_insert_front(sm, page);

    return page;
}

void sm_persist_page(StorageManager* sm, Page* page) {
    if (!page->is_dirty) return;

    off_t offset = page->page_id * PAGE_SIZE + sizeof(DBHeader);
    lseek(sm->fd, offset, SEEK_SET);
    write(sm->fd, page->data, PAGE_SIZE);
    page->is_dirty = false;
}

void sm_close(StorageManager* sm) {
    // Persist all dirty pages
    for (uint32_t i = 0; i < sm->cache_size; i++) {
        if (sm->pages[i] && sm->pages[i]->is_dirty) {
            sm_persist_page(sm, sm->pages[i]);
        }
        free(sm->pages[i]);
    }

    // Update header
    lseek(sm->fd, 0, SEEK_SET);
    write(sm->fd, &sm->header, sizeof(DBHeader));

    close(sm->fd);
    free(sm);
}

uint32_t sm_allocate_page(StorageManager* sm) {
    uint32_t new_page_id = sm->header.page_count;

    off_t offset = sizeof(DBHeader) + new_page_id * PAGE_SIZE;
    lseek(sm->fd, offset - 1, SEEK_SET);
    write(sm->fd, "\0", 1);

    sm->header.page_count++;

    // Initialize new page
    Page* page = malloc(sizeof(Page));
    memset(page->data, 0, PAGE_SIZE);
    page->page_id = new_page_id;
    page->is_dirty = true;
    
    // Add to cache
    if (sm->cache_size < 100) {
        sm->pages[sm->cache_size++] = page;
    }
    
    return new_page_id;

}

static void lru_remove(StorageManager* sm, Page* page) {
    if (page->prev) ((Page*)page->prev)->next = page->next;
    if (page->next) ((Page*)page->next)->prev = page->prev;
    
    if (sm->lru_head == page) sm->lru_head = (Page*)page->next;
    if (sm->lru_tail == page) sm->lru_tail = (Page*)page->prev;

    page->prev = page->next = NULL;

}

static void lru_insert_front(StorageManager* sm, Page* page) {
    if (!page) return;
    
    page->prev = NULL;
    page->next = sm->lru_head;
    
    if (sm->lru_head) {
        sm->lru_head->prev = page;
    }
    sm->lru_head = page;
    
    if (!sm->lru_tail) {
        sm->lru_tail = page;
    }
}

static void lru_touch(StorageManager* sm, Page* page) {
    lru_remove(sm, page);
    lru_insert_front(sm, page);
}

static void evict_lru_page(StorageManager* sm) {
    Page* victim = sm->lru_tail;
    if (!victim) return;
    
    if (victim->is_dirty) sm_persist_page(sm, victim);
    
    for (uint32_t i = 0; i < sm->cache_size; i++) {
        if (sm->pages[i] == victim) {
            sm->pages[i] = sm->pages[--sm->cache_size];
            break;
        }
    }

    lru_remove(sm, victim);
    free(victim);

}
