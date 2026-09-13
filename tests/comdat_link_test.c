/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "linker.h"
#include "objfile.h"

static void write_comdat_object(const char* path, uint16_t arch, uint8_t marker,
                                int discarded_undefined)
{
    ObjectFile* object = objfile_new(path, arch);
    ObjSection* text = objfile_add_section(
        object, ".text.pick", SECT_CODE, SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
    ObjSection* rodata = objfile_add_section(
        object, ".rodata.pick", SECT_RODATA, SECT_FLAG_ALLOC);
    uint64_t zero = 0u;
    uint32_t pointer_size = arch == ARCH_X64 ? 8u : 4u;
    RelocType pointer_reloc = arch == ARCH_X64 ? RELOC_ABS64 : RELOC_ABS32U;

    assert(objfile_set_comdat(text, "pick.group", RO_COMDAT_SELECT_ANY));
    assert(objfile_set_comdat(rodata, "pick.group", RO_COMDAT_SELECT_ANY));
    assert(!objfile_set_comdat(text, "pick.group", 99u));
    section_add_data(text, &zero, pointer_size);
    section_add_byte(text, marker);
    section_add_byte(rodata, marker);
    objfile_add_symbol(object, "pick", SYM_GLOBAL, BIND_CODE,
                       0, pointer_size, 1u);
    objfile_add_symbol(object, "pick_data", SYM_GLOBAL, BIND_DATA,
                       1, 0u, 1u);
    objfile_add_reloc(object, 0, 0u, "pick_data", pointer_reloc, 0);
    if (discarded_undefined) {
        objfile_add_symbol(object, "discarded_dependency", SYM_UNDEF,
                           BIND_CODE, -1, 0u, 0u);
        objfile_add_reloc(object, 0, 0u, "discarded_dependency",
                          pointer_reloc, 0);
    }
    assert(objfile_write(object, path));
    objfile_free(object);
}

static void assert_round_trip(const char* path, uint16_t arch)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* section;
    assert(object != NULL);
    assert(object->arch == arch);
    assert(object->section_count == 2);
    for (section = object->sections; section; section = section->next) {
        assert((section->flags & SECT_FLAG_COMDAT) != 0u);
        assert(section->comdat_selection == RO_COMDAT_SELECT_ANY);
        assert(strcmp(section->comdat_key, "pick.group") == 0);
    }
    objfile_free(object);
}

static LinkedSection* find_section(Linker* linker, const char* name)
{
    LinkedSection* section;
    for (section = linker->sections; section; section = section->next) {
        if (strcmp(section->name, name) == 0) return section;
    }
    return NULL;
}

static GlobalSymbol* find_global(Linker* linker, const char* name)
{
    GlobalSymbol* symbol;
    for (symbol = linker->symbols; symbol; symbol = symbol->next) {
        if (strcmp(symbol->name, name) == 0) return symbol;
    }
    return NULL;
}

static void assert_first_group_wins(const char* first, const char* second,
                                    uint16_t arch, uint8_t marker)
{
    Linker* linker = linker_new();
    LinkedSection* text;
    LinkedSection* rodata;
    GlobalSymbol* pick;
    GlobalSymbol* pick_data;
    uint64_t relocated = 0u;
    uint32_t pointer_size = arch == ARCH_X64 ? 8u : 4u;

    memset(&g_linker_opts, 0, sizeof(g_linker_opts));
    g_linker_opts.entry = "pick";
    assert(linker_add_object(linker, first));
    assert(linker_add_object(linker, second));
    assert(linker_merge_sections(linker));
    assert(linker->section_count == 2);
    assert(linker->reloc_count == 1);
    assert(linker_collect_symbols(linker));
    assert(linker->symbol_count == 2);
    assert(linker_resolve_symbols(linker));

    text = find_section(linker, ".text.pick");
    rodata = find_section(linker, ".rodata.pick");
    pick = find_global(linker, "pick");
    pick_data = find_global(linker, "pick_data");
    assert(text != NULL && rodata != NULL);
    assert(pick != NULL && pick_data != NULL);
    assert(text->size == pointer_size + 1u &&
           text->memory_size == pointer_size + 1u);
    assert(rodata->size == 1u && rodata->memory_size == 1u);
    assert((text->flags & SECT_FLAG_COMDAT) == 0u);
    assert((rodata->flags & SECT_FLAG_COMDAT) == 0u);
    assert(text->data[pointer_size] == marker);
    assert(rodata->data[0] == marker);
    assert(strcmp(pick->source, first) == 0);
    assert(strcmp(pick_data->source, first) == 0);

    assert(linker_layout(linker, UINT32_C(0x10000)));
    assert(linker_apply_relocations(linker));
    memcpy(&relocated, text->data, pointer_size);
    assert(relocated == pick_data->value);
    linker_free(linker);
}

static uint8_t* read_bytes(const char* path, size_t* size)
{
    FILE* file = fopen(path, "rb");
    long length;
    uint8_t* bytes;
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    length = ftell(file);
    assert(length > 0);
    assert(fseek(file, 0, SEEK_SET) == 0);
    bytes = malloc((size_t)length);
    assert(bytes != NULL);
    assert(fread(bytes, (size_t)length, 1, file) == 1);
    assert(fclose(file) == 0);
    *size = (size_t)length;
    return bytes;
}

static void assert_malformed_metadata_rejected(const char* path)
{
    size_t size;
    uint8_t* bytes = read_bytes(path, &size);
    RoHeader header;
    RoSection original;
    RoSection malformed;
    ObjectFile* object;

    memcpy(&header, bytes, sizeof(header));
    memcpy(&original, bytes + (size_t)header.section_off, sizeof(original));
    assert((original.flags & SECT_FLAG_COMDAT) != 0u);

    malformed = original;
    malformed.reserved0 = 99u;
    memcpy(bytes + (size_t)header.section_off, &malformed, sizeof(malformed));
    object = objfile_read_memory(bytes, size, "bad-comdat-selection.ro");
    assert(object == NULL);

    malformed = original;
    malformed.reserved1 = header.strtab_size;
    memcpy(bytes + (size_t)header.section_off, &malformed, sizeof(malformed));
    object = objfile_read_memory(bytes, size, "bad-comdat-key.ro");
    assert(object == NULL);

    malformed = original;
    malformed.flags &= ~SECT_FLAG_COMDAT;
    memcpy(bytes + (size_t)header.section_off, &malformed, sizeof(malformed));
    object = objfile_read_memory(bytes, size, "reserved-without-comdat.ro");
    assert(object == NULL);

    free(bytes);
}

static void write_duplicate_object(const char* path, uint16_t arch,
                                   uint8_t marker)
{
    ObjectFile* object = objfile_new(path, arch);
    ObjSection* data = objfile_add_section(
        object, ".data", SECT_DATA, SECT_FLAG_WRITE | SECT_FLAG_ALLOC);
    section_add_byte(data, marker);
    objfile_add_symbol(object, "duplicate", SYM_GLOBAL, BIND_DATA,
                       0, 0u, 1u);
    assert(objfile_write(object, path));
    objfile_free(object);
}

static void assert_non_comdat_duplicate_rejected(const char* first,
                                                  const char* second)
{
    Linker* linker = linker_new();
    memset(&g_linker_opts, 0, sizeof(g_linker_opts));
    assert(linker_add_object(linker, first));
    assert(linker_add_object(linker, second));
    assert(linker_merge_sections(linker));
    assert(!linker_collect_symbols(linker));
    linker_free(linker);
}

int main(int argc, char** argv)
{
    uint16_t arch;
    assert(argc == 6);
    assert(strcmp(argv[5], "x86") == 0 || strcmp(argv[5], "x64") == 0);
    arch = strcmp(argv[5], "x64") == 0 ? ARCH_X64 : ARCH_X86;
    write_comdat_object(argv[1], arch, UINT8_C(0x11), 0);
    write_comdat_object(argv[2], arch, UINT8_C(0x22), 0);
    assert_round_trip(argv[1], arch);
    assert_round_trip(argv[2], arch);
    assert_first_group_wins(argv[1], argv[2], arch, UINT8_C(0x11));
    assert_first_group_wins(argv[2], argv[1], arch, UINT8_C(0x22));
    assert_malformed_metadata_rejected(argv[1]);

    write_comdat_object(argv[2], arch, UINT8_C(0x22), 1);
    assert_first_group_wins(argv[1], argv[2], arch, UINT8_C(0x11));

    write_duplicate_object(argv[3], arch, UINT8_C(0x33));
    write_duplicate_object(argv[4], arch, UINT8_C(0x44));
    assert_non_comdat_duplicate_rejected(argv[3], argv[4]);
    return 0;
}
