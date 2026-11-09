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

static void write_scaling_object(const char* path, uint16_t arch,
                                 unsigned int first_group,
                                 unsigned int end_group,
                                 uint8_t marker_base)
{
    ObjectFile* object = objfile_new(path, arch);
    uint32_t pointer_size = arch == ARCH_X64 ? 8u : 4u;
    RelocType pointer_reloc = arch == ARCH_X64 ? RELOC_ABS64 : RELOC_ABS32U;

    for (unsigned int group = first_group; group < end_group; ++group) {
        char key[64];
        char function_name[64];
        char data_name[64];
        int text_index = object->section_count;
        int data_index = text_index + 1;
        ObjSection* text = objfile_add_section(
            object, ".text.scaling", SECT_CODE,
            SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
        ObjSection* rodata = objfile_add_section(
            object, ".rodata.scaling", SECT_RODATA, SECT_FLAG_ALLOC);
        uint64_t zero = 0u;
        uint8_t text_marker = (uint8_t)(marker_base + group % 47u);
        uint8_t data_marker = (uint8_t)(marker_base + 0x30u + group % 47u);

        (void)snprintf(key, sizeof(key), "scaling.group.%08u", group);
        (void)snprintf(function_name, sizeof(function_name),
                       "scale_func_%08u", group);
        (void)snprintf(data_name, sizeof(data_name),
                       "scale_data_%08u", group);
        assert(objfile_set_comdat(text, key, RO_COMDAT_SELECT_ANY));
        assert(objfile_set_comdat(rodata, key, RO_COMDAT_SELECT_ANY));
        section_add_data(text, &zero, pointer_size);
        section_add_byte(text, text_marker);
        section_add_byte(rodata, data_marker);
        objfile_add_symbol(object, function_name, SYM_GLOBAL, BIND_CODE,
                           text_index, 0u, pointer_size + 1u);
        objfile_add_symbol(object, data_name, SYM_GLOBAL, BIND_DATA,
                           data_index, 0u, 1u);
        objfile_add_reloc(object, text_index, 0u, data_name,
                          pointer_reloc, 0);
    }

    assert(objfile_write(object, path));
    objfile_free(object);
}

static void assert_comdat_index_capacity(const char* path, uint16_t arch,
                                         unsigned int groups,
                                         size_t expected_capacity)
{
    Linker* linker;
    write_scaling_object(path, arch, 0u, groups, UINT8_C(0x20));
    linker = linker_new();
    memset(&g_linker_opts, 0, sizeof(g_linker_opts));
    assert(linker_add_object(linker, path));
    assert(linker->comdat_key_count == groups);
    assert(linker->comdat_key_capacity == expected_capacity);
    linker_free(linker);
}

static size_t parse_scaling_symbol_index(const char* name,
                                         const char* prefix)
{
    char* end = NULL;
    unsigned long parsed;
    assert(strncmp(name, prefix, strlen(prefix)) == 0);
    parsed = strtoul(name + strlen(prefix), &end, 10);
    assert(end != name + strlen(prefix) && *end == '\0');
    return (size_t)parsed;
}

static void assert_comdat_scaling(const char* first, const char* second,
                                  uint16_t arch)
{
    const unsigned int first_groups = 8192u;
    const unsigned int second_unique_groups = 2048u;
    const unsigned int group_count =
        first_groups + second_unique_groups;
    uint32_t pointer_size = arch == ARCH_X64 ? 8u : 4u;
    Linker* linker = linker_new();
    LinkedSection* text;
    LinkedSection* rodata;
    uint64_t* function_values = calloc(group_count, sizeof(*function_values));
    uint64_t* data_values = calloc(group_count, sizeof(*data_values));
    bool* functions_seen = calloc(group_count, sizeof(*functions_seen));
    bool* data_seen = calloc(group_count, sizeof(*data_seen));
    size_t function_count = 0u;
    size_t data_count = 0u;

    assert(function_values != NULL && data_values != NULL &&
           functions_seen != NULL && data_seen != NULL);
    write_scaling_object(first, arch, 0u, first_groups, UINT8_C(0x10));
    write_scaling_object(second, arch, 0u, group_count, UINT8_C(0x80));
    memset(&g_linker_opts, 0, sizeof(g_linker_opts));
    g_linker_opts.entry = "scale_func_00000000";
    assert(linker_add_object(linker, first));
    assert(linker_add_object(linker, second));
    assert(linker->comdat_key_count == group_count);
    assert(linker->comdat_key_capacity >= group_count);
    assert(linker->comdat_key_capacity < (size_t)group_count * 2u);
    assert(linker->comdat_key_count <=
           linker->comdat_key_capacity - linker->comdat_key_capacity / 4u);
    assert(linker->test_comdat_slot_probes > group_count);
    assert(linker->test_comdat_slot_probes < (size_t)group_count * 16u);
    printf("COMDAT scaling %s: %u groups, %zu slot probes, capacity %zu\n",
           arch == ARCH_X64 ? "x86_64" : "i686", group_count,
           linker->test_comdat_slot_probes, linker->comdat_key_capacity);

    assert(linker_merge_sections(linker));
    assert(linker->section_count == 2);
    assert(linker->reloc_count == (int)group_count);
    assert(linker_collect_symbols(linker));
    assert(linker->symbol_count == (int)(group_count * 2u));
    assert(linker_resolve_symbols(linker));
    assert(linker_layout(linker, UINT32_C(0x10000)));
    assert(linker_apply_relocations(linker));

    text = find_section(linker, ".text.scaling");
    rodata = find_section(linker, ".rodata.scaling");
    assert(text != NULL && rodata != NULL);
    assert(text->size == (uint64_t)group_count * (pointer_size + 1u));
    assert(rodata->size == group_count);
    for (GlobalSymbol* symbol = linker->symbols; symbol;
         symbol = symbol->next) {
        size_t index;
        if (strncmp(symbol->name, "scale_func_", 11u) == 0) {
            index = parse_scaling_symbol_index(symbol->name, "scale_func_");
            assert(index < group_count && !functions_seen[index]);
            functions_seen[index] = true;
            function_values[index] = symbol->value;
            assert(strcmp(symbol->source,
                          index < first_groups ? first : second) == 0);
            ++function_count;
        } else {
            index = parse_scaling_symbol_index(symbol->name, "scale_data_");
            assert(index < group_count && !data_seen[index]);
            data_seen[index] = true;
            data_values[index] = symbol->value;
            assert(strcmp(symbol->source,
                          index < first_groups ? first : second) == 0);
            ++data_count;
        }
    }
    assert(function_count == group_count && data_count == group_count);

    for (unsigned int group = 0u; group < group_count; ++group) {
        uint64_t text_offset = function_values[group] - text->vaddr;
        uint64_t data_offset = data_values[group] - rodata->vaddr;
        uint64_t relocated_target = 0u;
        uint8_t expected_base = group < first_groups
            ? UINT8_C(0x10) : UINT8_C(0x80);
        assert(functions_seen[group] && data_seen[group]);
        assert(text_offset + pointer_size + 1u <= text->size);
        assert(data_offset < rodata->size);
        memcpy(&relocated_target, text->data + (size_t)text_offset,
               pointer_size);
        assert(relocated_target == data_values[group]);
        assert(text->data[(size_t)text_offset + pointer_size] ==
               (uint8_t)(expected_base + group % 47u));
        assert(rodata->data[(size_t)data_offset] ==
               (uint8_t)(expected_base + 0x30u + group % 47u));
    }

    free(function_values);
    free(data_values);
    free(functions_seen);
    free(data_seen);
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
    assert_comdat_index_capacity(argv[1], arch, 48u, 64u);
    assert_comdat_index_capacity(argv[1], arch, 49u, 128u);
    assert_comdat_scaling(argv[1], argv[2], arch);
    return 0;
}
