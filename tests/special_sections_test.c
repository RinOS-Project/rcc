/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "linker.h"
#include "objfile.h"
#include "rin_formats_v3.h"

static void add_bytes(ObjSection* section, uint64_t count, uint8_t value)
{
    while (count-- != 0u) section_add_byte(section, value);
}

static void write_special_object(const char* path, uint16_t arch)
{
    uint32_t pointer_size = arch == ARCH_X64 ? 8u : 4u;
    RelocType pointer_reloc = arch == ARCH_X64 ? RELOC_ABS64 : RELOC_ABS32U;
    ObjectFile* object = objfile_new(path, arch);
    ObjSection* text = objfile_add_section(
        object, ".text", SECT_CODE, SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
    ObjSection* tls = objfile_add_section(
        object, ".tls", SECT_TLS, SECT_FLAG_WRITE | SECT_FLAG_ALLOC);
    ObjSection* unwind = objfile_add_section(
        object, ".unwind", SECT_UNWIND, SECT_FLAG_ALLOC);
    ObjSection* init = objfile_add_section(
        object, ".init_array", SECT_INIT_ARRAY, SECT_FLAG_ALLOC);
    ObjSection* fini = objfile_add_section(
        object, ".fini_array", SECT_FINI_ARRAY, SECT_FLAG_ALLOC);
    ObjSection* bss = objfile_add_section(
        object, ".bss", SECT_BSS, SECT_FLAG_WRITE | SECT_FLAG_ALLOC);
    ObjSection* signed_data = objfile_add_section(
        object, ".signed_data", SECT_DATA,
        SECT_FLAG_WRITE | SECT_FLAG_ALLOC);

    add_bytes(text, 16u, 0x90u);
    add_bytes(tls, pointer_size, 0x5au);
    add_bytes(unwind, 8u, 0x11u);
    add_bytes(init, pointer_size, 0u);
    add_bytes(fini, pointer_size, 0u);
    section_set_memory_size(tls, pointer_size * 2u);
    section_set_memory_size(bss, pointer_size * 4u);
    add_bytes(signed_data, 4u, 0u);
    tls->align = pointer_size;
    unwind->align = 4u;
    init->align = pointer_size;
    fini->align = pointer_size;
    bss->align = pointer_size;
    signed_data->align = 4u;

    objfile_add_symbol(object, "main", SYM_GLOBAL, BIND_CODE, 0, 0u, 16u);
    objfile_add_symbol(object, "tls_value", SYM_GLOBAL, BIND_TLS, 1, 0u,
                       pointer_size);
    objfile_add_symbol(object, "zero_data", SYM_GLOBAL, BIND_BSS, 5, 0u,
                       pointer_size * 4u);
    objfile_add_reloc(object, 0, 4u, "tls_value", RELOC_TLSOFF32S, 0);
    objfile_add_reloc(object, 3, 0u, "main", pointer_reloc, 0);
    objfile_add_reloc(object, 4, 0u, "main", pointer_reloc, 0);
    objfile_add_reloc(object, 6, 0u, "main", RELOC_ABS32S, 0);
    assert(objfile_write(object, path));
    objfile_free(object);
}

static void verify_object_sections(const char* path)
{
    static const SectionType expected[] = {
        SECT_CODE, SECT_TLS, SECT_UNWIND, SECT_INIT_ARRAY, SECT_FINI_ARRAY,
        SECT_BSS, SECT_DATA
    };
    ObjectFile* object = objfile_read(path);
    ObjSection* section;
    size_t index = 0u;
    assert(object != NULL);
    for (section = object->sections; section; section = section->next) {
        assert(index < sizeof(expected) / sizeof(expected[0]));
        assert(section->type == expected[index++]);
        if (section->type == SECT_TLS) assert(section->memory_size > section->size);
        if (section->type == SECT_BSS) {
            assert(section->size == 0u && section->memory_size > 0u);
        }
    }
    assert(index == sizeof(expected) / sizeof(expected[0]));
    objfile_free(object);
}

static void verify_image(const char* path, uint16_t expected_arch)
{
    FILE* file = fopen(path, "rb");
    RinHeaderV3 header;
    RinSectionV3* sections;
    unsigned seen = 0u;
    unsigned owner_count = 0u;
    RinSectionV3* code = NULL;
    RinSectionV3* data = NULL;
    RinSectionV3* bss = NULL;
    assert(file != NULL);
    assert(fread(&header, sizeof(header), 1, file) == 1);
    assert(header.magic == RIN_IMAGE_MAGIC);
    assert(header.architecture == expected_arch);
    assert((header.flags & RIN_IMAGE_USES_TLS) != 0u);
    assert(header.section_count == 8u);
    sections = calloc(header.section_count, sizeof(*sections));
    assert(sections != NULL);
    assert(fseek(file, (long)header.section_table_offset, SEEK_SET) == 0);
    assert(fread(sections, sizeof(*sections), header.section_count, file) ==
           header.section_count);
    for (uint32_t index = 0; index < header.section_count; ++index) {
        RinSectionV3* section = &sections[index];
        assert((section->flags &
                (RIN_IMAGE_SECTION_WRITE | RIN_IMAGE_SECTION_EXECUTE)) !=
               (RIN_IMAGE_SECTION_WRITE | RIN_IMAGE_SECTION_EXECUTE));
        switch (section->type) {
        case RIN_IMAGE_SECTION_CODE:
            assert(section->flags ==
                   (RIN_IMAGE_SECTION_READ | RIN_IMAGE_SECTION_EXECUTE));
            seen |= 1u << 0;
            code = section;
            break;
        case RIN_IMAGE_SECTION_TLS:
            assert(section->flags == RIN_IMAGE_SECTION_READ);
            assert(section->file_size < section->memory_size);
            seen |= 1u << 1;
            break;
        case RIN_IMAGE_SECTION_UNWIND:
            assert(section->flags == RIN_IMAGE_SECTION_READ);
            seen |= 1u << 2;
            break;
        case RIN_IMAGE_SECTION_INIT_ARRAY:
            assert(section->flags == RIN_IMAGE_SECTION_READ);
            seen |= 1u << 3;
            break;
        case RIN_IMAGE_SECTION_FINI_ARRAY:
            assert(section->flags == RIN_IMAGE_SECTION_READ);
            seen |= 1u << 4;
            break;
        case RIN_IMAGE_SECTION_RELOCATIONS:
        {
            RinRelocationV3 entries[4];
            unsigned signed_count = 0u;
            unsigned pointer_count = 0u;
            unsigned tls_count = 0u;
            assert(section->file_size == 4u * sizeof(RinRelocationV3));
            assert(fseek(file, (long)section->file_offset, SEEK_SET) == 0);
            assert(fread(entries, sizeof(entries), 1, file) == 1);
            for (size_t entry = 0u; entry < 4u; ++entry) {
                if (entries[entry].type == RIN_IMAGE_RELOCATION_ABS32S) {
                    ++signed_count;
                } else if (entries[entry].type ==
                           RIN_IMAGE_RELOCATION_TLSOFF32S) {
                    ++tls_count;
                } else if (entries[entry].type ==
                           (expected_arch == RIN_ARCH_X86
                                ? RIN_IMAGE_RELOCATION_ABS32U
                                : RIN_IMAGE_RELOCATION_ABS64)) {
                    ++pointer_count;
                }
            }
            assert(signed_count == 1u && pointer_count == 2u &&
                   tls_count == 1u);
            seen |= 1u << 5;
            break;
        }
        case RIN_IMAGE_SECTION_DATA:
            assert(section->flags ==
                   (RIN_IMAGE_SECTION_READ | RIN_IMAGE_SECTION_WRITE));
            ++owner_count;
            data = section;
            break;
        case RIN_IMAGE_SECTION_RODATA:
            assert(section->flags == RIN_IMAGE_SECTION_READ);
            ++owner_count;
            break;
        case RIN_IMAGE_SECTION_BSS:
            assert(section->flags ==
                   (RIN_IMAGE_SECTION_READ | RIN_IMAGE_SECTION_WRITE));
            assert(section->file_offset == 0u && section->file_size == 0u);
            assert(section->memory_size > 0u);
            seen |= 1u << 6;
            bss = section;
            break;
        default:
            assert(0 && "unexpected RIN v3 section type");
        }
    }
    assert(seen == 0x7fu);
    assert(owner_count == 1u);
    assert(code != NULL && data != NULL && bss != NULL);
    assert(code->virtual_address == 0u &&
           code->file_size == code->memory_size &&
           data->virtual_address == code->memory_size &&
           data->file_offset == code->file_offset + code->file_size &&
           data->file_size == data->memory_size &&
           bss->virtual_address == data->virtual_address + data->memory_size &&
           (bss->virtual_address & 4095u) == 0u &&
           header.image_size == bss->virtual_address + bss->memory_size);
    free(sections);
    assert(fclose(file) == 0);
}

static void link_and_verify(const char* object_path, const char* image_path,
                            uint16_t arch)
{
    Linker* linker;
    memset(&g_linker_opts, 0, sizeof(g_linker_opts));
    g_linker_opts.arch = arch;
    g_linker_opts.arch_explicit = true;
    g_linker_opts.entry = "main";
    g_linker_opts.base_addr = 0x10000u;
    linker = linker_new();
    assert(linker_add_object(linker, object_path));
    assert(linker_merge_sections(linker));
    assert(linker_collect_symbols(linker));
    assert(linker_resolve_symbols(linker));
    assert(linker_layout(linker, g_linker_opts.base_addr));
    assert(linker_apply_relocations(linker));
    assert(linker_emit_rin(linker, image_path));
    linker_free(linker);
    verify_image(image_path,
                 arch == ARCH_X64 ? RIN_ARCH_X86_64 : RIN_ARCH_X86);
}

static void verify_negative_objects(const char* wx_path,
                                    const char* array_path,
                                    const char* conflict_a,
                                    const char* conflict_b)
{
    ObjectFile* object = objfile_new(wx_path, ARCH_X64);
    ObjSection* section = objfile_add_section(
        object, ".text", SECT_CODE,
        SECT_FLAG_WRITE | SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
    section_add_byte(section, 0x90u);
    assert(objfile_write(object, wx_path));
    objfile_free(object);
    assert(objfile_read(wx_path) == NULL);

    object = objfile_new(array_path, ARCH_X64);
    section = objfile_add_section(
        object, ".init_array", SECT_INIT_ARRAY, SECT_FLAG_ALLOC);
    add_bytes(section, 9u, 0u);
    assert(objfile_write(object, array_path));
    objfile_free(object);
    assert(objfile_read(array_path) == NULL);

    object = objfile_new(conflict_a, ARCH_X64);
    section = objfile_add_section(
        object, ".same", SECT_RODATA, SECT_FLAG_ALLOC);
    section_add_byte(section, 1u);
    assert(objfile_write(object, conflict_a));
    objfile_free(object);
    object = objfile_new(conflict_b, ARCH_X64);
    section = objfile_add_section(
        object, ".same", SECT_DATA, SECT_FLAG_WRITE | SECT_FLAG_ALLOC);
    section_add_byte(section, 2u);
    assert(objfile_write(object, conflict_b));
    objfile_free(object);

    memset(&g_linker_opts, 0, sizeof(g_linker_opts));
    g_linker_opts.arch = ARCH_X64;
    g_linker_opts.arch_explicit = true;
    Linker* linker = linker_new();
    assert(linker_add_object(linker, conflict_a));
    assert(linker_add_object(linker, conflict_b));
    assert(!linker_merge_sections(linker));
    linker_free(linker);
}

int main(int argc, char** argv)
{
    assert(argc == 9);
    write_special_object(argv[1], ARCH_X86);
    verify_object_sections(argv[1]);
    link_and_verify(argv[1], argv[2], ARCH_X86);
    write_special_object(argv[3], ARCH_X64);
    verify_object_sections(argv[3]);
    link_and_verify(argv[3], argv[4], ARCH_X64);
    verify_negative_objects(argv[5], argv[6], argv[7], argv[8]);
    return 0;
}
