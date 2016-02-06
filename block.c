#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "branch.h"
#include "page.h"

struct block *block_new() {
    struct block *block = malloc(sizeof(*block));
    if (!block) {
        return NULL;
    }

    block->pages = malloc(sizeof(*block->pages));
    block->offsets = malloc(sizeof(*block->offsets));
    if (!block->pages || !block->offsets) {
        goto error;
    }

    block->pages[0] = page_alloc(BLOCK_PAGE_SIZE);
    if (!block->pages[0]) {
        goto error;
    }

    block->offsets[0] = 0;
    block->size = 1;
    block->count = 1;

    return block;

error:
    if (block->pages) {
        free(block->pages);
    }
    if (block->offsets) {
        free(block->offsets);
    }
    free(block);
    return NULL;
}

void block_free(struct block *block) {
    for (size_t i = 0; i < block->count; i++) {
        page_free(block->pages[i], BLOCK_PAGE_SIZE);
    }
    free(block->offsets);
    free(block->pages);
    free(block);
}

__attribute__ ((noinline))
static void *add_page(struct block *block) {
    if (UNLIKELY(block->count == block->size)) {
        size_t new_size = block->size * 2;
        void **pages = realloc(block->pages, sizeof(*pages) * new_size);
        if (UNLIKELY(!pages)) {
            return NULL;
        }
        block->pages = pages;
        size_t *offsets = realloc(block->offsets, sizeof(*offsets) * new_size);
        if (UNLIKELY(!offsets)) {
            return NULL;
        }
        block->offsets = offsets;
        block->size = new_size;
    }
    void *page = page_alloc(BLOCK_PAGE_SIZE);
    if (UNLIKELY(!page)) {
        return NULL;
    }
    block->pages[block->count] = page;
    block->offsets[block->count] = 0;
    block->count++;
    return page;
}

void *block_alloc(struct block *block, size_t size) {
    if (UNLIKELY(size > BLOCK_PAGE_SIZE)) {
        return NULL;
    }
    size_t offset = block->offsets[block->count - 1];
    void *page;
    if (UNLIKELY(BLOCK_PAGE_SIZE - offset < size)) {
        page = add_page(block);
        if (UNLIKELY(!page)) {
            return NULL;
        }
        offset = 0;
    } else {
        page = block->pages[block->count - 1];
    }
    block->offsets[block->count - 1] += size;
    return (void *)((uintptr_t)page + offset);
}
