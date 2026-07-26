//
// Created by Abdou on 25/07/2026.
//

#ifndef DATABASE_IN_C_PAGER_H
#define DATABASE_IN_C_PAGER_H
#include <stdbool.h>
#include <stdint.h>

#define TABLES_MAX_PAGES 100
#define PAGE_SIZE 4096


typedef struct {
    int fd;
    uint32_t file_length;
    void *pages[TABLES_MAX_PAGES];
    bool page_dirty[TABLES_MAX_PAGES];
} Pager;

Pager *pager_open(const char *filename);

void *pager_get_page(Pager *pager, uint32_t page_number);

void pager_mark_dirty(Pager *pager, uint32_t page_number);

int pager_flush(Pager *pager, uint32_t page_number);

void pager_close(Pager *pager);


#endif //DATABASE_IN_C_PAGER_H
