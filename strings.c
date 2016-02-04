#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "strings.h"
#include "block.h"
#include "tree.h"
#include "branch.h"

#ifdef INLINE_UNSIGNED
#include "unsigned.h"

const static uint32_t unsigned_tag = 0x80000000;

static int is_small_unsigned(const char *string)
{
    char c;
    int numeric = 0;
    const char *ptr = string;
    while ((c = *ptr++)) {
        if (c < '0' || c > '9' || numeric > 10) {
            numeric = -1;
            break;
        }
        numeric++;
    }
    return numeric >= 1 && \
        (numeric < 10 || (numeric == 10 && string[0] <= '2'));
}

static uint32_t to_unsigned(const char *string)
{
    uint32_t number = 0;
    char c;
    while ((c = *string++))
        number = number * 10 + (c - '0');
    return number;
}
#endif

typedef struct tree_node_s tree_node_t;
struct tree_node_s {
    uint32_t hash;
    uint32_t id;
    const char *string;
    rb_node(tree_node_t) tree_link;
    tree_node_t *next;
};

typedef rb_tree(tree_node_t) tree_t;

static int tree_cmp(tree_node_t *a, tree_node_t *b)
{
    return a->hash < b->hash ? -1 : (a->hash > b->hash ? 1 : 0);
}

rb_gen(static, tree_, tree_t, tree_node_t, tree_link, tree_cmp)

struct strings {
    struct block *hashes;
    struct block *strings;
    struct block *tree_nodes;
    tree_t hash_map;
    uint32_t total;
#ifdef INLINE_UNSIGNED
    char buffer[11];
#endif
};

struct strings *strings_new()
{
    struct strings *strings = malloc(sizeof(*strings));
    if (!strings)
        return NULL;

    strings->hashes = block_new();
    strings->strings = block_new();
    strings->tree_nodes = block_new();
    if (!strings->hashes || !strings->strings || !strings->tree_nodes)
        goto error;

    tree_new(&strings->hash_map);

    strings->total = 0;

    return strings;

error:
    if (strings->hashes)
        block_free(strings->hashes);
    if (strings->strings)
        block_free(strings->strings);
    if (strings->tree_nodes)
        block_free(strings->tree_nodes);
    free(strings);
    return NULL;
}

void strings_free(struct strings *strings)
{
    block_free(strings->hashes);
    block_free(strings->strings);
    block_free(strings->tree_nodes);
    free(strings);
}

static uint32_t strings_hash(const char *string)
{
    uint32_t hash = 5381;
    char c;
    while ((c = *string++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

static tree_node_t *create_node(struct strings *strings, uint32_t hash,
                                const char *string)
{
    tree_node_t *node = block_alloc(strings->tree_nodes, sizeof(*node));
    if (UNLIKELY(!node))
        return NULL;

    int len = strlen(string);
    void *string_ptr = block_alloc(strings->strings, len + 1);
    if (UNLIKELY(!string_ptr))
        goto error;
    memcpy(string_ptr, string, len + 1);

    uint32_t *hash_ptr = block_alloc(strings->hashes, sizeof(*hash_ptr));
    if (UNLIKELY(!hash_ptr))
        goto error;
    *hash_ptr = hash;

    node->hash = hash;
    node->id = ++strings->total;
    node->string = (const char *)string_ptr;
    node->next = NULL;

#ifdef INLINE_UNSIGNED
    assert(node->id < unsigned_tag);
#else
    assert(node->id);
#endif

    return node;

error:
    free(node);
    return NULL;
}

static tree_node_t *find_node(const struct strings *strings, uint32_t hash)
{
    tree_node_t key = {hash};
    return tree_search((tree_t *)&strings->hash_map, &key);
}

__attribute__ ((noinline))
static bool strings_intern_collision(struct strings *strings,
                                     tree_node_t *node, const char *string,
                                     uint32_t hash, uint32_t *id)
{
    for (;;) {
        if (!strcmp(node->string, string))
            break;
        if (!node->next) {
            node = node->next = create_node(strings, hash, string);
            if (UNLIKELY(!node))
                return false;
            break;
        }
        node = node->next;
    }
    *id = node->id;
    return true;
}

bool strings_intern(struct strings *strings, const char *string, uint32_t *id)
{
#ifdef INLINE_UNSIGNED
    if (is_small_unsigned(string)) {
        uint32_t number = to_unsigned(string);
        if (LIKELY(number < unsigned_tag)) {
            *id = number | unsigned_tag;
            return true;
        }
    }
#endif

    uint32_t hash = strings_hash(string);
    tree_node_t *node = find_node(strings, hash);
    if (node) {
        if (UNLIKELY(strcmp(node->string, string)))
            return strings_intern_collision(strings, node, string, hash, id);
    } else {
        if (UNLIKELY(!(node = create_node(strings, hash, string))))
            return false;
        tree_insert(&strings->hash_map, node);
    }

    *id = node->id;

    return true;
}

uint32_t strings_lookup(const struct strings *strings, const char *string)
{
#ifdef INLINE_UNSIGNED
    if (is_small_unsigned(string)) {
        uint32_t number = to_unsigned(string);
        if (LIKELY(number < unsigned_tag))
            return number | unsigned_tag;
    }
#endif

    uint32_t hash = strings_hash(string);
    tree_node_t *node = find_node(strings, hash);
    if (node)
        do {
            if (!strcmp(node->string, string))
                return node->id;
        } while ((node = node->next));

    return 0;
}

#ifdef INLINE_UNSIGNED
const char *strings_lookup_id(struct strings *strings, uint32_t id)
#else
const char *strings_lookup_id(const struct strings *strings, uint32_t id)
#endif
{
#ifdef INLINE_UNSIGNED
    if (id & unsigned_tag) {
        unsigned_string(strings->buffer, id & ~unsigned_tag);
        return strings->buffer;
    }
#endif

    if (UNLIKELY(id > strings->total))
        return NULL;

    size_t hashes_per_page = BLOCK_PAGE_SIZE / sizeof(uint32_t);
    size_t offset = (size_t)(id - 1);
    size_t page_num = offset / hashes_per_page;
    size_t page_offset = offset % hashes_per_page;
    void *page = strings->hashes->pages[page_num];
    uint32_t hash =
        *(const uint32_t *)((uintptr_t)page + page_offset * sizeof(uint32_t));

    tree_node_t *node = find_node(strings, hash);
    do {
        if (LIKELY(node->id == id))
            return node->string;
    } while ((node = node->next));

    return NULL;
}
