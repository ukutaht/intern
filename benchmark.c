#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "strings.h"
#include "unsigned.h"

int main() {
    struct strings *strings = strings_new();
    assert(strings);

    char buffer[12] = {'x'};
    uint32_t count = 5000000;
    size_t string_bytes = 0;

    for (uint32_t id = 1; id <= count; id++) {
        unsigned_string(buffer + 1, id);
        string_bytes += strlen(buffer);
        assert(strings_intern(strings, buffer) == id);
        assert(strcmp(strings_lookup_id(strings, id), buffer) == 0);
    }

    string_bytes += count;
    size_t allocated_bytes = strings_allocated_bytes(strings);
    size_t overhead = allocated_bytes - string_bytes;
    double overhead_per_string = (double)overhead / (double)count;

    printf("Interned 5M unique strings\n");
    printf("Overhead per string: %.1f bytes\n", overhead_per_string);

    // strings_free(strings);
    return 0;
}
