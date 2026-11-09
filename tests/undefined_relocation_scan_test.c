/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "linker.h"
#include "objfile.h"

enum {
    UNIQUE_TARGET_COUNT = 512,
    RELOCATION_COUNT = 8192
};

static void make_path(char* path, size_t capacity, const char* directory,
                      const char* name)
{
    int length = snprintf(path, capacity, "%s/%s", directory, name);
    assert(length > 0 && (size_t)length < capacity);
}

static uint32_t target_pointer_size(uint16_t arch)
{
    return arch == ARCH_X64 ? 8u : 4u;
}

static RelocType target_pointer_relocation(uint16_t arch)
{
    return arch == ARCH_X64 ? RELOC_ABS64 : RELOC_ABS32U;
}

static void write_scaling_objects(const char* references_path,
                                  const char* definitions_path,
                                  uint16_t arch)
{
    ObjectFile* references = objfile_new(references_path, arch);
    ObjectFile* definitions = objfile_new(definitions_path, arch);
    ObjSection* text = objfile_add_section(
        references, ".text.references", SECT_CODE,
        SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
    ObjSection* data = objfile_add_section(
        definitions, ".data.definitions", SECT_DATA,
        SECT_FLAG_WRITE | SECT_FLAG_ALLOC);
    uint32_t pointer_size = target_pointer_size(arch);
    RelocType pointer_reloc = target_pointer_relocation(arch);
    uint64_t zero = 0u;
    uint8_t entry_marker = UINT8_C(0x11);

    section_add_byte(text, entry_marker);
    objfile_add_symbol(references, "undefined_scan_entry", SYM_GLOBAL,
                       BIND_CODE, 0, 0u, 1u);
    for (unsigned int index = 0u; index < RELOCATION_COUNT; ++index) {
        char name[64];
        unsigned int target = index % UNIQUE_TARGET_COUNT;
        uint64_t offset;
        (void)snprintf(name, sizeof(name), "scale_target_%05u", target);
        offset = section_add_data(text, &zero, pointer_size);
        objfile_add_reloc(references, 0, offset, name, pointer_reloc, 0);
    }

    for (unsigned int index = 0u; index < UNIQUE_TARGET_COUNT; ++index) {
        char name[64];
        uint8_t marker = (uint8_t)(index % 251u);
        (void)snprintf(name, sizeof(name), "scale_target_%05u", index);
        objfile_add_symbol(references, name, SYM_UNDEF, BIND_CODE,
                           -1, 0u, 0u);
        /* A repeated undefined symbol entry still receives the same lookup
         * semantics as each retained relocation name. */
        objfile_add_symbol(references, name, SYM_UNDEF, BIND_CODE,
                           -1, 0u, 0u);
        section_add_byte(data, marker);
        objfile_add_symbol(definitions, name, SYM_GLOBAL, BIND_DATA,
                           0, index, 1u);
    }
    objfile_add_symbol(references, "unused_but_defined", SYM_UNDEF,
                       BIND_DATA, -1, 0u, 0u);
    section_add_byte(data, UINT8_C(0xee));
    objfile_add_symbol(definitions, "unused_but_defined", SYM_GLOBAL,
                       BIND_DATA, 0, UNIQUE_TARGET_COUNT, 1u);

    assert(objfile_write(references, references_path));
    assert(objfile_write(definitions, definitions_path));
    objfile_free(references);
    objfile_free(definitions);
}

static void write_comdat_owner(const char* path, uint16_t arch)
{
    ObjectFile* object = objfile_new(path, arch);
    ObjSection* section = objfile_add_section(
        object, ".text.discard-owner", SECT_CODE,
        SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
    uint8_t marker = UINT8_C(0x21);
    assert(objfile_set_comdat(section, "undefined.scan.discarded.group",
                              RO_COMDAT_SELECT_ANY));
    section_add_byte(section, marker);
    objfile_add_symbol(object, "discarded_group_owner", SYM_GLOBAL,
                       BIND_CODE, 0, 0u, 1u);
    assert(objfile_write(object, path));
    objfile_free(object);
}

static void write_discarded_references(const char* path, uint16_t arch,
                                      bool include_retained_reference)
{
    ObjectFile* object = objfile_new(path, arch);
    ObjSection* discarded = objfile_add_section(
        object, ".text.discarded", SECT_CODE,
        SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
    ObjSection* debug = objfile_add_section(
        object, ".debug_line", SECT_DEBUG_LINE, 0u);
    uint32_t pointer_size = target_pointer_size(arch);
    RelocType pointer_reloc = target_pointer_relocation(arch);
    uint64_t zero = 0u;
    uint64_t mixed_discarded_offset = 0u;
    uint8_t marker = UINT8_C(0x31);

    assert(objfile_set_comdat(discarded,
                              "undefined.scan.discarded.group",
                              RO_COMDAT_SELECT_ANY));
    section_add_data(discarded, &zero, pointer_size);
    section_add_byte(discarded, marker);
    objfile_add_symbol(object, "comdat_missing", SYM_UNDEF, BIND_CODE,
                       -1, 0u, 0u);
    objfile_add_reloc(object, 0, 0u, "comdat_missing", pointer_reloc, 0);
    section_add_data(debug, &zero, pointer_size);
    objfile_add_symbol(object, "debug_missing", SYM_UNDEF, BIND_CODE,
                       -1, 0u, 0u);
    objfile_add_reloc(object, 1, 0u, "debug_missing", pointer_reloc, 0);

    if (include_retained_reference) {
        ObjSection* retained = objfile_add_section(
            object, ".text.retained", SECT_CODE,
            SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
        mixed_discarded_offset =
            section_add_data(discarded, &zero, pointer_size);
        section_add_data(retained, &zero, pointer_size);
        objfile_add_symbol(object, "mixed_missing", SYM_UNDEF, BIND_CODE,
                           -1, 0u, 0u);
        objfile_add_reloc(object, 2, 0u, "mixed_missing", pointer_reloc, 0);
        /* The same undefined name also appears in a discarded group. The
         * retained reference must keep it unresolved. */
        objfile_add_reloc(object, 0, mixed_discarded_offset, "mixed_missing",
                          pointer_reloc, 0);
    }
    assert(objfile_write(object, path));
    objfile_free(object);
}

static void write_unreferenced_undefined(const char* path, uint16_t arch)
{
    ObjectFile* object = objfile_new(path, arch);
    ObjSection* text = objfile_add_section(
        object, ".text.unreferenced", SECT_CODE,
        SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
    uint8_t marker = UINT8_C(0x41);
    section_add_byte(text, marker);
    objfile_add_symbol(object, "unreferenced_missing", SYM_UNDEF,
                       BIND_CODE, -1, 0u, 0u);
    assert(objfile_write(object, path));
    objfile_free(object);
}

static bool resolve_object_paths(char** paths, int count,
                                 size_t* unresolved_count,
                                 size_t* definition_lookup_count)
{
    Linker* linker = linker_new();
    bool result;
    memset(&g_linker_opts, 0, sizeof(g_linker_opts));
    for (int index = 0; index < count; ++index)
        assert(linker_add_object(linker, paths[index]));
    assert(linker_merge_sections(linker));
    assert(linker_collect_symbols(linker));
    result = linker_resolve_symbols(linker);
    if (unresolved_count)
        *unresolved_count = linker->test_undefined_unresolved_entries;
    if (definition_lookup_count)
        *definition_lookup_count = linker->test_undefined_definition_lookups;
    linker_free(linker);
    return result;
}

static void assert_scaling(const char* directory, uint16_t arch)
{
    char references_path[RCC_MAX_PATH];
    char definitions_path[RCC_MAX_PATH];
    char* inputs[2];
    Linker* linker;
    make_path(references_path, sizeof(references_path), directory,
              "references.ro");
    make_path(definitions_path, sizeof(definitions_path), directory,
              "definitions.ro");
    write_scaling_objects(references_path, definitions_path, arch);
    inputs[0] = references_path;
    inputs[1] = definitions_path;
    linker = linker_new();
    memset(&g_linker_opts, 0, sizeof(g_linker_opts));
    assert(linker_add_object(linker, inputs[0]));
    assert(linker_add_object(linker, inputs[1]));
    assert(linker_merge_sections(linker));
    assert(linker->reloc_count == RELOCATION_COUNT);
    assert(linker_collect_symbols(linker));
    assert(linker_resolve_symbols(linker));
    assert(linker->test_undefined_relocation_unique_names ==
           UNIQUE_TARGET_COUNT);
    assert(linker->test_undefined_symbol_entries ==
           UNIQUE_TARGET_COUNT * 2u + 1u);
    assert(linker->test_undefined_definition_lookups ==
           UNIQUE_TARGET_COUNT * 2u + 1u);
    assert(linker->test_undefined_relocation_queries ==
           RELOCATION_COUNT + UNIQUE_TARGET_COUNT * 2u + 1u);
    assert(linker->test_undefined_relocation_slot_probes <
           linker->test_undefined_relocation_queries * 8u);
    printf("Undefined relocation scan %s: %u relocations, %u unique names, "
           "%zu index queries, %zu slot probes\n",
           arch == ARCH_X64 ? "x86_64" : "i686", RELOCATION_COUNT,
           UNIQUE_TARGET_COUNT,
           linker->test_undefined_relocation_queries,
           linker->test_undefined_relocation_slot_probes);
    linker_free(linker);
    assert(remove(references_path) == 0);
    assert(remove(definitions_path) == 0);
}

static void assert_discarded_references(const char* directory, uint16_t arch)
{
    char owner_path[RCC_MAX_PATH];
    char reference_path[RCC_MAX_PATH];
    char* inputs[2];
    size_t unresolved_count;
    size_t definition_lookup_count;
    make_path(owner_path, sizeof(owner_path), directory, "comdat-owner.ro");
    make_path(reference_path, sizeof(reference_path), directory,
              "discarded-references.ro");
    write_comdat_owner(owner_path, arch);
    write_discarded_references(reference_path, arch, false);
    inputs[0] = owner_path;
    inputs[1] = reference_path;
    assert(resolve_object_paths(inputs, 2, &unresolved_count,
                                &definition_lookup_count));
    assert(unresolved_count == 0u && definition_lookup_count == 0u);
    assert(remove(owner_path) == 0);
    assert(remove(reference_path) == 0);
}

static void assert_retained_reference_is_not_hidden(const char* directory,
                                                    uint16_t arch)
{
    char owner_path[RCC_MAX_PATH];
    char reference_path[RCC_MAX_PATH];
    char* inputs[2];
    size_t unresolved_count;
    size_t definition_lookup_count;
    make_path(owner_path, sizeof(owner_path), directory, "mixed-owner.ro");
    make_path(reference_path, sizeof(reference_path), directory,
              "mixed-references.ro");
    write_comdat_owner(owner_path, arch);
    write_discarded_references(reference_path, arch, true);
    inputs[0] = owner_path;
    inputs[1] = reference_path;
    assert(!resolve_object_paths(inputs, 2, &unresolved_count,
                                 &definition_lookup_count));
    assert(unresolved_count == 1u && definition_lookup_count == 1u);
    assert(remove(owner_path) == 0);
    assert(remove(reference_path) == 0);
}

static void assert_unreferenced_undefined_fails(const char* directory,
                                                uint16_t arch)
{
    char path[RCC_MAX_PATH];
    char* input[1];
    size_t unresolved_count;
    size_t definition_lookup_count;
    make_path(path, sizeof(path), directory, "unreferenced.ro");
    write_unreferenced_undefined(path, arch);
    input[0] = path;
    assert(!resolve_object_paths(input, 1, &unresolved_count,
                                 &definition_lookup_count));
    assert(unresolved_count == 1u && definition_lookup_count == 1u);
    assert(remove(path) == 0);
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
    assert_scaling(argv[1], arch);
    assert_discarded_references(argv[1], arch);
    assert_retained_reference_is_not_hidden(argv[1], arch);
    assert_unreferenced_undefined_fails(argv[1], arch);
    return 0;
}
