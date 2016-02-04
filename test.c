#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "strings.h"
#include "block.h"
#include "unsigned.h"

int main() {
    struct strings *strings = strings_new();
    assert(strings);

    char buffer[12] = {'x'};
    const char *string;
    uint32_t id;

    unsigned count = 1000000;

    // test interning strings
    for (unsigned i = 1; i <= count; i++) {
        unsigned_string(buffer + 1, i);

        assert(strings_intern(strings, buffer, &id));
        assert(id == i);

        assert(strings_intern(strings, buffer, &id));
        assert(id == i);

        assert(strings_lookup(strings, buffer) == id);

        string = strings_lookup_id(strings, id);
        assert(string && !strcmp(buffer, string));
    }

    // test string iteration
    struct strings_cursor cursor;
    strings_cursor_init(&cursor, strings);
    while (strings_cursor_next(&cursor)) {
        id = strings_cursor_id(&cursor);
        string = strings_cursor_string(&cursor);
        assert(id && string);
        unsigned_string(buffer + 1, id);
        assert(!strcmp(buffer, string));
    }
    assert(cursor.id == count);

#ifdef INLINE_UNSIGNED
    // test interning unsigned int strings
    for (unsigned i = 1; i <= count; i++) {
        unsigned_string(buffer, i);

        assert(strings_intern(strings, buffer, &id));
        assert(i == (id & 0x7FFFFFFF));

        assert(strings_intern(strings, buffer, &id));
        assert(i == (id & 0x7FFFFFFF));

        assert(strings_lookup(strings, buffer) == id);

        string = strings_lookup_id(strings, id);
        assert(string && !strcmp(buffer, string));
    }
#endif

    assert(strings_intern(strings, "2147483648", &id));
    assert(id == count + 1);

    assert(strings_intern(strings, "4294967295", &id));
    assert(id == count + 2);

    assert(strings_intern(strings, "4294967296", &id));
    assert(id == count + 3);

    assert(strings_intern(strings, "2147483647", &id));
#ifdef INLINE_UNSIGNED
    assert(id == 0xFFFFFFFF);
#else
    assert(id == count + 4);
#endif

    char *large_string = malloc(BLOCK_PAGE_SIZE + 1);
    assert(large_string);
    memset(large_string, 'x', BLOCK_PAGE_SIZE);
    large_string[BLOCK_PAGE_SIZE] = '\0';
    assert(!strings_intern(strings, large_string, &id));
    free(large_string);

    strings_free(strings);
    return 0;
}
