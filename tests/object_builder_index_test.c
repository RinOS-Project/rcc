#include "objfile.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCALE_STRING_COUNT 4096u
#define SCALE_SYMBOL_COUNT 128u

int main(int argc, char** argv) {
    ObjectFile* object;
    ObjectFile* roundtrip;
    ObjSection* text;
    uint32_t string_offsets[SCALE_STRING_COUNT];
    uint8_t code[SCALE_SYMBOL_COUNT * 4u] = {0};
    char name[64];

    assert(argc == 2);
    object = objfile_new(argv[1], ARCH_X64);
    text = objfile_add_section(object, ".text", SECT_CODE,
                               SECT_FLAG_ALLOC | SECT_FLAG_EXEC);
    assert(section_add_data(text, code, sizeof(code)) == 0u);

    for (uint32_t index = 0u; index < SCALE_STRING_COUNT; ++index) {
        snprintf(name, sizeof(name), "scale_string_%05u", index);
        string_offsets[index] = objfile_add_string(object, name);
        assert(string_offsets[index] != 0u);
        assert(objfile_add_string(object, name) == string_offsets[index]);
    }

    for (uint32_t index = 0u; index < SCALE_SYMBOL_COUNT; ++index) {
        snprintf(name, sizeof(name), "scale_symbol_%04u", index);
        objfile_add_symbol(object, name, SYM_UNDEF, BIND_DATA, -1, 0u, 0u);
        objfile_add_reloc(object, 0, (uint64_t)index * 4u, name,
                          RELOC_ABS32U, (int64_t)index);
    }
    assert(object->symbol_count == (int)SCALE_SYMBOL_COUNT);
    assert(objfile_write(object, argv[1]));
    objfile_free(object);

    roundtrip = objfile_read(argv[1]);
    assert(roundtrip != NULL);
    assert(roundtrip->section_count == 1);
    assert(roundtrip->symbol_count == (int)SCALE_SYMBOL_COUNT);
    for (uint32_t index = 0u; index < SCALE_STRING_COUNT; ++index) {
        snprintf(name, sizeof(name), "scale_string_%05u", index);
        assert(objfile_add_string(roundtrip, name) == string_offsets[index]);
    }
    for (uint32_t index = 0u; index < SCALE_SYMBOL_COUNT; ++index) {
        ObjReloc* relocation = roundtrip->sections_by_index[0]->relocs;
        ObjSymbol* symbol;
        uint32_t current = 0u;
        snprintf(name, sizeof(name), "scale_symbol_%04u", index);
        symbol = objfile_find_symbol(roundtrip, name);
        assert(symbol != NULL);
        while (relocation && current < index) {
            relocation = relocation->next;
            ++current;
        }
        assert(relocation != NULL);
        assert(relocation->offset == (uint64_t)index * 4u);
        assert(strcmp(relocation->symbol_name, name) == 0);
    }
    objfile_free(roundtrip);
    assert(remove(argv[1]) == 0);
    puts("Object builder string and append index round-trip passed");
    return 0;
}
