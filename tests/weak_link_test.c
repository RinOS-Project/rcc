/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "linker.h"
#include "objfile.h"

static void write_weak_object(const char* path)
{
    ObjectFile* object = objfile_new(path, ARCH_X64);
    ObjSection* data = objfile_add_section(
        object, ".data", SECT_DATA, SECT_FLAG_WRITE | SECT_FLAG_ALLOC);
    section_add_byte(data, UINT8_C(0x11));
    objfile_add_symbol(object, "pick", SYM_WEAK, BIND_DATA, 0, 0u, 1u);
    assert(objfile_write(object, path));
    objfile_free(object);
}

static void write_strong_object(const char* path)
{
    ObjectFile* object = objfile_new(path, ARCH_X64);
    ObjSection* text = objfile_add_section(
        object, ".text", SECT_CODE, SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
    section_add_byte(text, UINT8_C(0xc3));
    objfile_add_symbol(object, "pick", SYM_GLOBAL, BIND_CODE, 0, 0u, 1u);
    assert(objfile_write(object, path));
    objfile_free(object);
}

int main(int argc, char** argv)
{
    Linker* linker;
    GlobalSymbol* selected;

    assert(argc == 3);
    write_weak_object(argv[1]);
    write_strong_object(argv[2]);

    linker = linker_new();
    assert(linker_add_object(linker, argv[1]));
    assert(linker_add_object(linker, argv[2]));
    assert(linker_merge_sections(linker));
    assert(linker_collect_symbols(linker));

    selected = linker->symbols;
    while (selected && strcmp(selected->name, "pick") != 0) {
        selected = selected->next;
    }
    assert(selected != NULL);
    assert(selected->type == SYM_GLOBAL);
    assert(selected->binding == BIND_CODE);
    assert(selected->size == 1u);
    assert(strcmp(selected->source, argv[2]) == 0);

    g_linker_opts.entry = "pick";
    assert(linker_layout(linker, UINT32_C(0x10000)));
    assert(selected->value == UINT32_C(0x10000));
    linker_free(linker);
    return 0;
}
