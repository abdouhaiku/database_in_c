#include "pager.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <unistd.h>


Pager *pager_open(const char *filename) {
    errno = 0;
    int fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
        perror("Error opening the file\n");
        return NULL;
    }

    errno = 0;
    Pager *pager = malloc(sizeof(Pager));
    if (pager == NULL) {
        perror("malloc failed");
        close(fd);
        return NULL;
    }

    pager->fd = fd;
    // Get file length
    off_t file_length = lseek(fd, 0, SEEK_END);
    if (file_length == -1) {
        perror("lseek failed");
        close(fd);
        free(pager);
        return NULL;
    }

    pager->file_length = file_length;
    lseek(fd, 0, SEEK_SET);

    for (uint32_t i = 0; i < TABLES_MAX_PAGES; i++) {
        pager->pages[i] = NULL;
        pager->page_dirty[i] = false;
    }

    return pager;
}

void *pager_get_page(Pager *pager, uint32_t page_number) {
    if (page_number >= TABLES_MAX_PAGES) {
        printf("Error: Invalid page number\n");
        return NULL;
    }
    if (pager->pages[page_number] == NULL) {
        errno = 0;
        void *page = calloc(1, PAGE_SIZE);
        if (page == NULL) {
            perror("calloc error");
            free(page);
            return NULL;
        }

        // -1 because because even one extra byte requires another page
        size_t num_pages = (pager->file_length + PAGE_SIZE - 1) / PAGE_SIZE;
        if (page_number < num_pages) {
            //Reads from disk
            off_t offset = PAGE_SIZE * page_number;
            size_t bytes_in_file_from_offset = pager->file_length - offset;
            size_t expected = bytes_in_file_from_offset < PAGE_SIZE ? bytes_in_file_from_offset : PAGE_SIZE;

            lseek(pager->fd, offset, SEEK_SET);
            size_t bytes_read = read(pager->fd, page, PAGE_SIZE);

            if (bytes_read != expected) {
                fprintf(stderr, "Corrupt database file: expected %zu bytes for page %u, got %zd\n",
                        expected, page_number, bytes_read);
                free(page);
                return NULL;
            }
        }

        pager->pages[page_number] = page;
    }

    return pager->pages[page_number];
}

void pager_mark_dirty(Pager *pager, uint32_t page_number) {
    pager->page_dirty[page_number] = true;
}

int pager_flush(Pager *pager, uint32_t page_number) {
    // lseek to the position of the file
    errno = 0;
    off_t page_position = page_number * PAGE_SIZE;
    if (lseek(pager->fd, page_position, SEEK_SET) == -1) {
        perror("Error in seeking to the desired position");
        return -1;
    }

    errno = 0;
    if (write(pager->fd, pager->pages[page_number], PAGE_SIZE) == -1) {
        perror("Error in writing to the disk");
        return -1;
    }
    return 0;
}

void pager_close(Pager *pager) {
    for (size_t i = 0; i < TABLES_MAX_PAGES; i++) {
        if (pager->page_dirty[i]) {
            pager_flush(pager, (int) i);
        }
        // free pages
        if (pager->pages[i] != NULL) {
            free(pager->pages[i]);
        }

    }
    errno = 0;
    if (close(pager->fd) == -1) {
        perror("Error in closing the file");
    }
    // free the pager
    free(pager);
}
