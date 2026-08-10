/* RCC canonical unsigned RIN v3 emitter.  Final signing is a separate process. */

#include "rcc.h"
#include "codegen.h"
#include "rin_formats_v3.h"
#include <stdio.h>

static uint64_t align_up_u64(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static int compare_relocation(const void* left, const void* right) {
    const RinRelocationV3* a = (const RinRelocationV3*)left;
    const RinRelocationV3* b = (const RinRelocationV3*)right;
    if (a->virtual_address < b->virtual_address) return -1;
    if (a->virtual_address > b->virtual_address) return 1;
    return 0;
}

static bool relocation_source(const Module* mod,
                              ModuleSymbolSection source_section,
                              uint64_t rodata_rva, uint64_t data_rva,
                              uint64_t code_file_offset,
                              uint64_t rodata_file_offset,
                              uint64_t data_file_offset,
                              uint64_t* source_rva,
                              uint64_t* source_file_offset,
                              uint64_t* source_size) {
    switch (source_section) {
        case MODULE_SYMBOL_CODE:
            *source_rva = 0u;
            *source_file_offset = code_file_offset;
            *source_size = mod->code.size;
            return true;
        case MODULE_SYMBOL_RODATA:
            *source_rva = rodata_rva;
            *source_file_offset = rodata_file_offset;
            *source_size = mod->rodata.size;
            return true;
        case MODULE_SYMBOL_DATA:
            *source_rva = data_rva;
            *source_file_offset = data_file_offset;
            *source_size = mod->data.size;
            return true;
        case MODULE_SYMBOL_BSS:
        default:
            return false;
    }
}

static uint32_t append_name(char* strings, uint32_t* size, const char* name) {
    uint32_t offset = *size;
    size_t length = strlen(name) + 1u;
    memcpy(strings + offset, name, length);
    *size += (uint32_t)length;
    return offset;
}

bool rcc_emit(Module* mod, const char* outfile) {
    RinHeaderV3 header;
    RinSectionV3 sections[5];
    RinRelocationV3* relocations = NULL;
    uint32_t relocation_count = 0u;
    uint32_t section_count = 1u;
    uint32_t relocation_index = 0u;
    uint32_t strings_size = 1u;
    uint32_t text_name;
    uint32_t rodata_name = 0u;
    uint32_t data_name = 0u;
    uint32_t bss_name = 0u;
    uint32_t reloc_name = 0u;
    char strings[40] = {0};
    uint64_t section_table_offset = sizeof(RinHeaderV3);
    uint64_t dependency_table_offset;
    uint64_t string_table_offset;
    uint64_t code_file_offset;
    uint64_t rodata_file_offset = 0u;
    uint64_t data_file_offset = 0u;
    uint64_t relocation_file_offset = 0u;
    uint64_t unsigned_size;
    uint64_t payload_file_end;
    uint64_t mapped_end;
    uint64_t rodata_rva = 0u;
    uint64_t data_rva = 0u;
    uint64_t bss_rva = 0u;
    uint64_t image_size;
    uint8_t* output;
    FILE* file;
    Reloc* relocation;

    if (!mod || !outfile || mod->code.size == 0u ||
        mod->code.size > UINT32_MAX || mod->rodata.size > UINT32_MAX ||
        mod->data.size > UINT32_MAX || mod->bss.size > UINT32_MAX) {
        rcc_error((SourceLoc){outfile, 0, 0}, "invalid module for RIN v3 output");
        return false;
    }
    for (relocation = mod->relocs; relocation; relocation = relocation->next) {
        ++relocation_count;
    }
    if (mod->rodata.size > 0u) ++section_count;
    if (mod->data.size > 0u) ++section_count;
    if (mod->bss.size > 0u) ++section_count;
    if (relocation_count > 0u) ++section_count;

    text_name = append_name(strings, &strings_size, ".text");
    if (mod->rodata.size > 0u) {
        rodata_name = append_name(strings, &strings_size, ".rodata");
    }
    if (mod->data.size > 0u) data_name = append_name(strings, &strings_size, ".data");
    if (mod->bss.size > 0u) bss_name = append_name(strings, &strings_size, ".bss");
    if (relocation_count > 0u) reloc_name = append_name(strings, &strings_size, ".reloc");

    dependency_table_offset = section_table_offset +
        (uint64_t)section_count * sizeof(RinSectionV3);
    string_table_offset = dependency_table_offset;
    code_file_offset = align_up_u64(string_table_offset + strings_size, 16u);
    payload_file_end = code_file_offset + mod->code.size;
    mapped_end = mod->code.size;
    if (mod->rodata.size > 0u) {
        rodata_file_offset = align_up_u64(payload_file_end, 16u);
        payload_file_end = rodata_file_offset + mod->rodata.size;
        rodata_rva = align_up_u64(mapped_end, 4096u);
        mapped_end = rodata_rva + mod->rodata.size;
    }
    if (mod->data.size > 0u) {
        data_file_offset = align_up_u64(payload_file_end, 16u);
        payload_file_end = data_file_offset + mod->data.size;
        data_rva = align_up_u64(mapped_end, 4096u);
        mapped_end = data_rva + mod->data.size;
    }
    if (mod->bss.size > 0u) {
        bss_rva = align_up_u64(mapped_end, 4096u);
        mapped_end = bss_rva + mod->bss.size;
    }
    if (relocation_count > 0u) {
        relocation_file_offset = align_up_u64(payload_file_end, 8u);
        unsigned_size = relocation_file_offset +
            (uint64_t)relocation_count * sizeof(RinRelocationV3);
    } else {
        unsigned_size = payload_file_end;
    }
    image_size = align_up_u64(mapped_end, 4096u);
    if (unsigned_size > SIZE_MAX || image_size == 0u ||
        (g_opts.target_arch == ARCH_X86 && image_size >= UINT64_C(0xC0000000))) {
        rcc_error((SourceLoc){outfile, 0, 0}, "RIN v3 image exceeds target limits");
        return false;
    }

    memset(&header, 0, sizeof(header));
    memset(sections, 0, sizeof(sections));
    header.magic = RIN_IMAGE_MAGIC;
    header.version = RIN_IMAGE_VERSION_3;
    header.header_size = sizeof(RinHeaderV3);
    header.architecture = g_opts.target_arch == ARCH_X64
        ? RIN_ARCH_X86_64 : RIN_ARCH_X86;
    header.abi_major = RIN_IMAGE_ABI_MAJOR;
    header.abi_minor = RIN_IMAGE_ABI_MINOR;
    header.flags = RIN_IMAGE_EXECUTABLE | RIN_IMAGE_GUI |
                   RIN_IMAGE_RELOCATABLE | RIN_IMAGE_ASLR;
    header.section_count = section_count;
    header.entry_rva = mod->entry_point;
    header.image_size = image_size;
    header.section_table_offset = section_table_offset;
    header.dependency_table_offset = dependency_table_offset;
    header.string_table_offset = string_table_offset;
    header.string_table_size = strings_size;

    sections[0].type = RIN_IMAGE_SECTION_CODE;
    sections[0].flags = RIN_IMAGE_SECTION_READ | RIN_IMAGE_SECTION_EXECUTE;
    sections[0].alignment = 16u;
    sections[0].file_offset = code_file_offset;
    sections[0].file_size = mod->code.size;
    sections[0].memory_size = mod->code.size;
    sections[0].name_offset = text_name;
    uint32_t next_section = 1u;
    if (mod->rodata.size > 0u) {
        RinSectionV3* rodata_section = &sections[next_section++];
        rodata_section->type = RIN_IMAGE_SECTION_RODATA;
        rodata_section->flags = RIN_IMAGE_SECTION_READ;
        rodata_section->alignment = 16u;
        rodata_section->file_offset = rodata_file_offset;
        rodata_section->file_size = mod->rodata.size;
        rodata_section->virtual_address = rodata_rva;
        rodata_section->memory_size = mod->rodata.size;
        rodata_section->name_offset = rodata_name;
    }
    if (mod->data.size > 0u) {
        RinSectionV3* data_section = &sections[next_section++];
        data_section->type = RIN_IMAGE_SECTION_DATA;
        data_section->flags = RIN_IMAGE_SECTION_READ | RIN_IMAGE_SECTION_WRITE;
        data_section->alignment = 16u;
        data_section->file_offset = data_file_offset;
        data_section->file_size = mod->data.size;
        data_section->virtual_address = data_rva;
        data_section->memory_size = mod->data.size;
        data_section->name_offset = data_name;
    }
    if (mod->bss.size > 0u) {
        RinSectionV3* bss_section = &sections[next_section++];
        bss_section->type = RIN_IMAGE_SECTION_BSS;
        bss_section->flags = RIN_IMAGE_SECTION_READ | RIN_IMAGE_SECTION_WRITE;
        bss_section->alignment = mod->bss.align;
        bss_section->virtual_address = bss_rva;
        bss_section->memory_size = mod->bss.size;
        bss_section->name_offset = bss_name;
    }
    if (relocation_count > 0u) {
        RinSectionV3* relocation_section = &sections[next_section];
        relocation_section->type = RIN_IMAGE_SECTION_RELOCATIONS;
        relocation_section->flags = RIN_IMAGE_SECTION_DISCARDABLE;
        relocation_section->alignment = 8u;
        relocation_section->file_offset = relocation_file_offset;
        relocation_section->file_size =
            (uint64_t)relocation_count * sizeof(RinRelocationV3);
        relocation_section->name_offset = reloc_name;
        relocations = rcc_alloc((size_t)relocation_section->file_size);
        for (relocation = mod->relocs; relocation; relocation = relocation->next) {
            uint64_t width = relocation->type == RIN_RELOC_ABS64 ? 8u : 4u;
            uint64_t source_rva;
            uint64_t source_file_offset;
            uint64_t source_size;
            if (!relocation_source(mod, relocation->source_section,
                                   rodata_rva, data_rva, code_file_offset,
                                   rodata_file_offset, data_file_offset,
                                   &source_rva, &source_file_offset,
                                   &source_size) ||
                relocation->offset > source_size ||
                width > source_size - relocation->offset ||
                (g_opts.target_arch == ARCH_X86 && width != 4u)) {
                rcc_error((SourceLoc){outfile, 0, 0}, "invalid target relocation in RIN v3 output");
                rcc_free(relocations);
                return false;
            }
            (void)source_file_offset;
            relocations[relocation_index].virtual_address =
                source_rva + relocation->offset;
            relocations[relocation_index].type = width == 8u
                ? RIN_IMAGE_RELOCATION_ABS64 : RIN_IMAGE_RELOCATION_ABS32U;
            ++relocation_index;
        }
        qsort(relocations, relocation_count, sizeof(RinRelocationV3), compare_relocation);
        for (relocation_index = 1u; relocation_index < relocation_count; ++relocation_index) {
            if (relocations[relocation_index - 1u].virtual_address +
                    (relocations[relocation_index - 1u].type ==
                     RIN_IMAGE_RELOCATION_ABS64 ? 8u : 4u) >
                relocations[relocation_index].virtual_address) {
                rcc_error((SourceLoc){outfile, 0, 0}, "overlapping RIN v3 relocations");
                rcc_free(relocations);
                return false;
            }
        }
    }

    output = rcc_alloc((size_t)unsigned_size);
    memcpy(output, &header, sizeof(header));
    memcpy(output + section_table_offset, sections,
           (size_t)section_count * sizeof(RinSectionV3));
    memcpy(output + string_table_offset, strings, strings_size);
    memcpy(output + code_file_offset, mod->code.data, mod->code.size);
    if (mod->rodata.size > 0u) {
        memcpy(output + rodata_file_offset, mod->rodata.data,
               mod->rodata.size);
    }
    if (mod->data.size > 0u) {
        memcpy(output + data_file_offset, mod->data.data, mod->data.size);
    }
    for (relocation = mod->relocs; relocation; relocation = relocation->next) {
        uint64_t resolved;
        uint64_t source_rva;
        uint64_t source_file_offset;
        uint64_t source_size;
        bool is_64bit = relocation->type == RIN_RELOC_ABS64;
        if (!relocation_source(mod, relocation->source_section,
                               rodata_rva, data_rva, code_file_offset,
                               rodata_file_offset, data_file_offset,
                               &source_rva, &source_file_offset,
                               &source_size) ||
            !module_resolve_image_relocation(mod,
                                             relocation->source_section,
                                             relocation->offset,
                                             is_64bit, rodata_rva, data_rva,
                                             bss_rva,
                                             &resolved)) {
            rcc_error((SourceLoc){outfile, 0, 0},
                      "unresolved direct-image relocation at section offset %u",
                      relocation->offset);
            rcc_free(relocations);
            rcc_free(output);
            return false;
        }
        (void)source_rva;
        (void)source_size;
        if (relocation->type == RIN_RELOC_ABS64) {
            memcpy(output + source_file_offset + relocation->offset,
                   &resolved, sizeof(resolved));
        } else {
            uint32_t value;
            if (resolved > UINT32_MAX) {
                rcc_error((SourceLoc){outfile, 0, 0}, "ABS32U relocation overflow");
                rcc_free(relocations);
                rcc_free(output);
                return false;
            }
            value = (uint32_t)resolved;
            memcpy(output + source_file_offset + relocation->offset,
                   &value, sizeof(value));
        }
    }
    if (relocation_count > 0u) {
        memcpy(output + relocation_file_offset, relocations,
               (size_t)relocation_count * sizeof(RinRelocationV3));
    }

    file = fopen(outfile, "wb");
    if (!file) {
        rcc_error((SourceLoc){outfile, 0, 0}, "cannot write unsigned RIN v3 output");
        rcc_free(relocations);
        rcc_free(output);
        return false;
    }
    size_t written = fwrite(output, 1u, (size_t)unsigned_size, file);
    int close_result = fclose(file);
    if (written != (size_t)unsigned_size || close_result != 0) {
        rcc_error((SourceLoc){outfile, 0, 0}, "cannot write unsigned RIN v3 output");
        rcc_free(relocations);
        rcc_free(output);
        return false;
    }
    if (g_opts.verbose) {
        printf("Unsigned RIN v3 stage: %s (%s, %u sections, %u relocations)\n",
               outfile, rcc_target_triple(g_opts.target_arch), section_count,
               relocation_count);
    }
    rcc_free(relocations);
    rcc_free(output);
    return true;
}
