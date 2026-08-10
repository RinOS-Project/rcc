/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "linker.h"
#include "objfile.h"

static ObjSymbol* find_object_symbol(ObjectFile* object, const char* name)
{
    ObjSymbol* symbol;
    for (symbol = object->symbols; symbol; symbol = symbol->next) {
        if (strcmp(symbol->name, name) == 0) return symbol;
    }
    return NULL;
}

static void write_wide_object(const char* path)
{
    ObjectFile* object = objfile_new(path, ARCH_X64);
    ObjSection* text = objfile_add_section(
        object, ".text", SECT_CODE, SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
    uint64_t zero = 0u;
    section_add_data(text, &zero, sizeof(zero));
    objfile_add_symbol(object, "main", SYM_GLOBAL, BIND_CODE, 0, 0u, 8u);
    objfile_add_symbol(object, "wide", SYM_GLOBAL, BIND_ABS, -1,
                       UINT64_C(0x100000123), UINT64_C(0x200000045));
    objfile_add_reloc(object, 0, 0u, "wide", RELOC_ABS64,
                      INT64_C(0x200000005));
    assert(objfile_write(object, path));
    objfile_free(object);
}

int main(int argc, char** argv)
{
    ObjectFile* roundtrip;
    ObjSymbol* wide;
    Linker* linker;
    LinkedSection* text;
    uint64_t patched = 0u;

    assert(argc == 2);
    write_wide_object(argv[1]);

    roundtrip = objfile_read(argv[1]);
    assert(roundtrip != NULL);
    wide = find_object_symbol(roundtrip, "wide");
    assert(wide != NULL);
    assert(wide->value == UINT64_C(0x100000123));
    assert(wide->size == UINT64_C(0x200000045));
    assert(roundtrip->sections != NULL && roundtrip->sections->relocs != NULL);
    assert(roundtrip->sections->relocs->addend == INT64_C(0x200000005));
    objfile_free(roundtrip);

    memset(&g_linker_opts, 0, sizeof(g_linker_opts));
    g_linker_opts.arch = ARCH_X64;
    g_linker_opts.arch_explicit = true;
    g_linker_opts.entry = "main";
    linker = linker_new();
    assert(linker_add_object(linker, argv[1]));
    assert(linker_merge_sections(linker));
    assert(linker_collect_symbols(linker));
    assert(linker_resolve_symbols(linker));
    assert(linker_layout(linker, UINT64_C(0x400000000)));
    assert(linker->entry_addr == UINT64_C(0x400000000));
    assert(linker_apply_relocations(linker));

    text = linker->sections;
    while (text && strcmp(text->name, ".text") != 0) text = text->next;
    assert(text != NULL);
    memcpy(&patched, text->data, sizeof(patched));
    assert(patched == UINT64_C(0x300000128));

    linker->relocs->type = RELOC_ABS32;
    assert(!linker_apply_relocations(linker));
    g_linker_opts.arch = ARCH_X86;
    assert(!linker_layout(linker, UINT64_C(0xC0000000)));
    linker_free(linker);
    return 0;
}
