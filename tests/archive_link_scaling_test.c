/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"
#include "linker.h"
#include "objfile.h"

enum {
    ARCHIVE_UNUSED_MEMBER_COUNT = 4096,
    ARCHIVE_CHAIN_LENGTH = 128
};

static void make_path(char* path, size_t capacity, const char* directory,
                      const char* name)
{
    int length = snprintf(path, capacity, "%s/%s", directory, name);
    assert(length > 0 && (size_t)length < capacity);
}

static void write_archive_member(const char* path, uint16_t arch,
                                 const char* symbol_name, SymbolType type,
                                 uint8_t marker, const char* dependency,
                                 const char* comdat_key)
{
    ObjectFile* object = objfile_new(path, arch);
    ObjSection* text = objfile_add_section(
        object, ".text.archive", SECT_CODE,
        SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
    uint32_t pointer_size = arch == ARCH_X64 ? 8u : 4u;
    uint64_t zero = 0u;

    if (comdat_key)
        assert(objfile_set_comdat(text, comdat_key, RO_COMDAT_SELECT_ANY));
    section_add_data(text, &zero, pointer_size);
    section_add_byte(text, marker);
    objfile_add_symbol(object, symbol_name, type, BIND_CODE,
                       0, 0u, pointer_size + 1u);
    if (dependency) {
        objfile_add_symbol(object, dependency, SYM_UNDEF, BIND_CODE,
                           -1, 0u, 0u);
        objfile_add_reloc(object, 0, 0u, dependency,
                          arch == ARCH_X64 ? RELOC_ABS64 : RELOC_ABS32U, 0);
    }
    assert(objfile_write(object, path));
    objfile_free(object);
}

static void append_archive_member(Archive* archive, const char* directory,
                                  const char* member_name, uint16_t arch,
                                  const char* symbol_name, SymbolType type,
                                  uint8_t marker, const char* dependency,
                                  const char* comdat_key)
{
    char path[RCC_MAX_PATH];
    make_path(path, sizeof(path), directory, member_name);
    write_archive_member(path, arch, symbol_name, type, marker, dependency,
                         comdat_key);
    assert(archive_add_member(archive, path));
    assert(remove(path) == 0);
}

static void write_root_object(const char* path, uint16_t arch)
{
    static const char* const needed_symbols[] = {
        "chain_0000", "weak_pick", "ordered_pick", "comdat_needed"
    };
    ObjectFile* object = objfile_new(path, arch);
    ObjSection* text = objfile_add_section(
        object, ".text.root", SECT_CODE,
        SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
    ObjSection* data = objfile_add_section(
        object, ".data.root", SECT_DATA,
        SECT_FLAG_WRITE | SECT_FLAG_ALLOC);
    uint32_t pointer_size = arch == ARCH_X64 ? 8u : 4u;
    RelocType pointer_reloc = arch == ARCH_X64 ? RELOC_ABS64 : RELOC_ABS32U;
    uint64_t zero = 0u;
    uint8_t entry_marker = UINT8_C(0x41);
    ObjSection* shared = objfile_add_section(
        object, ".text.shared", SECT_CODE,
        SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
    uint8_t shared_marker = UINT8_C(0x42);

    section_add_byte(text, entry_marker);
    objfile_add_symbol(object, "archive_test_entry", SYM_GLOBAL, BIND_CODE,
                       0, 0u, 1u);
    for (size_t index = 0u;
         index < sizeof(needed_symbols) / sizeof(needed_symbols[0]);
         ++index) {
        uint64_t offset = section_add_data(data, &zero, pointer_size);
        objfile_add_symbol(object, needed_symbols[index], SYM_UNDEF,
                           BIND_CODE, -1, 0u, 0u);
        objfile_add_reloc(object, 1, offset, needed_symbols[index],
                          pointer_reloc, 0);
    }
    assert(objfile_set_comdat(shared, "archive.shared.group",
                              RO_COMDAT_SELECT_ANY));
    section_add_byte(shared, shared_marker);
    objfile_add_symbol(object, "shared_owner", SYM_GLOBAL, BIND_CODE,
                       2, 0u, 1u);
    assert(objfile_write(object, path));
    objfile_free(object);
}

static void write_strong_override(const char* path, uint16_t arch)
{
    write_archive_member(path, arch, "weak_pick", SYM_GLOBAL,
                         UINT8_C(0x23), NULL, NULL);
}

static void write_scaling_archive(const char* directory, const char* path,
                                  uint16_t arch)
{
    Archive* archive = archive_new(path);
    char member_name[64];
    char symbol_name[64];

    for (unsigned int index = 0u;
         index < ARCHIVE_UNUSED_MEMBER_COUNT; ++index) {
        (void)snprintf(member_name, sizeof(member_name),
                       "unused_%05u.ro", index);
        (void)snprintf(symbol_name, sizeof(symbol_name),
                       "unused_symbol_%05u", index);
        append_archive_member(archive, directory, member_name, arch,
                              symbol_name, SYM_GLOBAL,
                              (uint8_t)(index % 251u), NULL, NULL);
    }
    for (unsigned int index = ARCHIVE_CHAIN_LENGTH; index > 0u; --index) {
        unsigned int group = index - 1u;
        const char* dependency = NULL;
        char next_symbol[64];
        (void)snprintf(member_name, sizeof(member_name),
                       "chain_%04u.ro", group);
        (void)snprintf(symbol_name, sizeof(symbol_name),
                       "chain_%04u", group);
        if (group + 1u < ARCHIVE_CHAIN_LENGTH) {
            (void)snprintf(next_symbol, sizeof(next_symbol),
                           "chain_%04u", group + 1u);
            dependency = next_symbol;
        }
        append_archive_member(archive, directory, member_name, arch,
                              symbol_name, SYM_GLOBAL,
                              (uint8_t)(0x50u + group % 31u), dependency, NULL);
    }
    append_archive_member(archive, directory, "weak_first.ro", arch,
                          "weak_pick", SYM_WEAK, UINT8_C(0x21), NULL, NULL);
    append_archive_member(archive, directory, "weak_second.ro", arch,
                          "weak_pick", SYM_GLOBAL, UINT8_C(0x22), NULL, NULL);
    append_archive_member(archive, directory, "ordered_first.ro", arch,
                          "ordered_pick", SYM_GLOBAL, UINT8_C(0x31), NULL,
                          NULL);
    append_archive_member(archive, directory, "ordered_second.ro", arch,
                          "ordered_pick", SYM_GLOBAL, UINT8_C(0x32), NULL,
                          NULL);
    append_archive_member(archive, directory, "comdat_duplicate.ro", arch,
                          "comdat_needed", SYM_GLOBAL, UINT8_C(0x71), NULL,
                          "archive.shared.group");
    append_archive_member(archive, directory, "comdat_fallback.ro", arch,
                          "comdat_needed", SYM_GLOBAL, UINT8_C(0x72), NULL,
                          NULL);
    assert(archive_write(archive, path));
    archive_free(archive);
}

static GlobalSymbol* find_global(Linker* linker, const char* name)
{
    for (GlobalSymbol* symbol = linker->symbols; symbol;
         symbol = symbol->next) {
        if (strcmp(symbol->name, name) == 0) return symbol;
    }
    return NULL;
}

static LinkedSection* find_section(Linker* linker, const char* name)
{
    for (LinkedSection* section = linker->sections; section;
         section = section->next) {
        if (strcmp(section->name, name) == 0) return section;
    }
    return NULL;
}

static bool linker_has_member(Linker* linker, const char* member_name)
{
    for (int index = 0; index < linker->object_count; ++index) {
        if (strstr(linker->objects[index]->filename, member_name) != NULL)
            return true;
    }
    return false;
}

static uint64_t read_target(const uint8_t* bytes, uint32_t pointer_size)
{
    uint64_t value = 0u;
    memcpy(&value, bytes, pointer_size);
    return value;
}

static void assert_symbol_marker(const LinkedSection* section,
                                 const GlobalSymbol* symbol,
                                 uint32_t pointer_size, uint8_t marker)
{
    uint64_t offset = symbol->value - section->vaddr;
    assert(offset + pointer_size + 1u <= section->size);
    assert(section->data[(size_t)offset + pointer_size] == marker);
}

static void assert_archive_resolution(const char* directory,
                                     const char* arch_name, uint16_t arch)
{
    char archive_path[RCC_MAX_PATH];
    char root_path[RCC_MAX_PATH];
    char strong_path[RCC_MAX_PATH];
    char expected_source[RCC_MAX_PATH * 2u];
    char* inputs[3];
    uint32_t pointer_size = arch == ARCH_X64 ? 8u : 4u;
    Linker* linker;
    LinkedSection* text;
    LinkedSection* data;

    make_path(archive_path, sizeof(archive_path), directory, "libscale.ra");
    make_path(root_path, sizeof(root_path), directory, "root.ro");
    make_path(strong_path, sizeof(strong_path), directory, "strong.ro");
    write_scaling_archive(directory, archive_path, arch);
    write_root_object(root_path, arch);
    write_strong_override(strong_path, arch);

    inputs[0] = root_path;
    inputs[1] = archive_path;
    inputs[2] = strong_path;
    linker = linker_new();
    memset(&g_linker_opts, 0, sizeof(g_linker_opts));
    g_linker_opts.entry = "archive_test_entry";
    assert(linker_add_objects(linker, inputs, 3));
    assert(linker->test_archive_selected_members ==
           ARCHIVE_CHAIN_LENGTH + 4u);
    assert(linker->test_archive_candidate_pops <=
           linker->test_archive_selected_members + 8u);
    assert(linker->test_archive_needed_symbol_probes ==
           linker->test_archive_selected_members);
    assert(linker->test_archive_heap_comparisons <
           linker->test_archive_selected_members * 32u);
    printf("Archive scaling %s: %u members, %zu selected, %zu candidate pops, "
           "%zu heap comparisons, %zu needed-symbol probes\n",
           arch_name, ARCHIVE_UNUSED_MEMBER_COUNT + ARCHIVE_CHAIN_LENGTH + 6u,
           linker->test_archive_selected_members,
           linker->test_archive_candidate_pops,
           linker->test_archive_heap_comparisons,
           linker->test_archive_needed_symbol_probes);

    assert(linker->object_count == ARCHIVE_CHAIN_LENGTH + 6);
    assert(linker_has_member(linker, "weak_first.ro"));
    assert(!linker_has_member(linker, "weak_second.ro"));
    assert(linker_has_member(linker, "ordered_first.ro"));
    assert(!linker_has_member(linker, "ordered_second.ro"));
    assert(linker_has_member(linker, "comdat_duplicate.ro"));
    assert(linker_has_member(linker, "comdat_fallback.ro"));
    assert(!linker_has_member(linker, "unused_"));

    assert(linker_merge_sections(linker));
    assert(linker->reloc_count == ARCHIVE_CHAIN_LENGTH - 1 + 4);
    assert(linker_collect_symbols(linker));
    assert(linker_resolve_symbols(linker));
    assert(linker_layout(linker, UINT32_C(0x10000)));
    assert(linker_apply_relocations(linker));
    text = find_section(linker, ".text.archive");
    data = find_section(linker, ".data.root");
    assert(text != NULL && data != NULL);

    for (unsigned int index = 0u; index < ARCHIVE_CHAIN_LENGTH; ++index) {
        char name[64];
        GlobalSymbol* symbol;
        uint64_t offset;
        (void)snprintf(name, sizeof(name), "chain_%04u", index);
        symbol = find_global(linker, name);
        assert(symbol != NULL);
        (void)snprintf(expected_source, sizeof(expected_source),
                       "%s(chain_%04u.ro)", archive_path, index);
        assert(strcmp(symbol->source, expected_source) == 0);
        offset = symbol->value - text->vaddr;
        assert(offset + pointer_size + 1u <= text->size);
        assert(text->data[(size_t)offset + pointer_size] ==
               (uint8_t)(0x50u + index % 31u));
        if (index + 1u < ARCHIVE_CHAIN_LENGTH) {
            char next_name[64];
            GlobalSymbol* next;
            (void)snprintf(next_name, sizeof(next_name), "chain_%04u",
                           index + 1u);
            next = find_global(linker, next_name);
            assert(next != NULL);
            assert(read_target(text->data + (size_t)offset,
                               pointer_size) == next->value);
        } else {
            assert(read_target(text->data + (size_t)offset,
                               pointer_size) == 0u);
        }
    }

    {
        GlobalSymbol* weak_pick = find_global(linker, "weak_pick");
        GlobalSymbol* ordered_pick = find_global(linker, "ordered_pick");
        GlobalSymbol* comdat_needed = find_global(linker, "comdat_needed");
        GlobalSymbol* shared_owner = find_global(linker, "shared_owner");
        assert(weak_pick != NULL && weak_pick->type == SYM_GLOBAL);
        assert(strcmp(weak_pick->source, strong_path) == 0);
        assert(ordered_pick != NULL);
        (void)snprintf(expected_source, sizeof(expected_source),
                       "%s(ordered_first.ro)", archive_path);
        assert(strcmp(ordered_pick->source, expected_source) == 0);
        assert(comdat_needed != NULL);
        (void)snprintf(expected_source, sizeof(expected_source),
                       "%s(comdat_fallback.ro)", archive_path);
        assert(strcmp(comdat_needed->source, expected_source) == 0);
        assert(shared_owner != NULL &&
               strcmp(shared_owner->source, root_path) == 0);
        assert_symbol_marker(text, weak_pick, pointer_size, UINT8_C(0x23));
        assert_symbol_marker(text, ordered_pick, pointer_size, UINT8_C(0x31));
        assert_symbol_marker(text, comdat_needed, pointer_size,
                             UINT8_C(0x72));
    }

    assert(read_target(data->data, pointer_size) ==
           find_global(linker, "chain_0000")->value);
    assert(read_target(data->data + pointer_size, pointer_size) ==
           find_global(linker, "weak_pick")->value);
    assert(read_target(data->data + pointer_size * 2u, pointer_size) ==
           find_global(linker, "ordered_pick")->value);
    assert(read_target(data->data + pointer_size * 3u, pointer_size) ==
           find_global(linker, "comdat_needed")->value);

    for (int object_index = 0; object_index < linker->object_count;
         ++object_index) {
        ObjectFile* object = linker->objects[object_index];
        if (strstr(object->filename, "comdat_duplicate.ro") != NULL) {
            ObjSection* section = object->sections;
            assert(section != NULL && !section->comdat_selected);
        }
    }
    linker_free(linker);
    assert(remove(root_path) == 0);
    assert(remove(strong_path) == 0);
}

int main(int argc, char** argv)
{
    uint16_t arch;
    assert(argc == 3);
    if (strcmp(argv[2], "x86") == 0) arch = ARCH_X86;
    else {
        assert(strcmp(argv[2], "x64") == 0);
        arch = ARCH_X64;
    }
    assert_archive_resolution(argv[1], argv[2], arch);
    return 0;
}
