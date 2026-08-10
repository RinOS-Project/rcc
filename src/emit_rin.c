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

static uint32_t append_name(char* strings, uint32_t* size, const char* name) {
    uint32_t offset = *size;
    size_t length = strlen(name) + 1u;
    memcpy(strings + offset, name, length);
    *size += (uint32_t)length;
    return offset;
}

bool rcc_emit(Module* mod, const char* outfile) {
    RinHeaderV3 header;
    RinSectionV3 sections[3];
    RinRelocationV3* relocations = NULL;
    uint32_t relocation_count = 0u;
    uint32_t section_count = 1u;
    uint32_t relocation_index = 0u;
    uint32_t strings_size = 1u;
    uint32_t text_name;
    uint32_t data_name = 0u;
    uint32_t reloc_name = 0u;
    char strings[32] = {0};
    uint64_t section_table_offset = sizeof(RinHeaderV3);
    uint64_t dependency_table_offset;
    uint64_t string_table_offset;
    uint64_t code_file_offset;
    uint64_t data_file_offset = 0u;
    uint64_t relocation_file_offset = 0u;
    uint64_t unsigned_size;
    uint64_t data_rva = 0u;
    uint64_t image_size;
    uint8_t* output;
    FILE* file;
    Reloc* relocation;

    if (!mod || !outfile || mod->code.size == 0u ||
        mod->code.size > UINT32_MAX || mod->data.size > UINT32_MAX) {
        rcc_error((SourceLoc){outfile, 0, 0}, "invalid module for RIN v3 output");
        return false;
    }
    for (relocation = mod->relocs; relocation; relocation = relocation->next) {
        ++relocation_count;
    }
    if (mod->data.size > 0u) ++section_count;
    if (relocation_count > 0u) ++section_count;

    text_name = append_name(strings, &strings_size, ".text");
    if (mod->data.size > 0u) data_name = append_name(strings, &strings_size, ".data");
    if (relocation_count > 0u) reloc_name = append_name(strings, &strings_size, ".reloc");

    dependency_table_offset = section_table_offset +
        (uint64_t)section_count * sizeof(RinSectionV3);
    string_table_offset = dependency_table_offset;
    code_file_offset = align_up_u64(string_table_offset + strings_size, 16u);
    if (mod->data.size > 0u) {
        data_file_offset = align_up_u64(code_file_offset + mod->code.size, 16u);
        data_rva = align_up_u64(mod->code.size, 4096u);
    }
    if (relocation_count > 0u) {
        uint64_t payload_end = mod->data.size > 0u
            ? data_file_offset + mod->data.size
            : code_file_offset + mod->code.size;
        relocation_file_offset = align_up_u64(payload_end, 8u);
        unsigned_size = relocation_file_offset +
            (uint64_t)relocation_count * sizeof(RinRelocationV3);
    } else {
        unsigned_size = mod->data.size > 0u
            ? data_file_offset + mod->data.size
            : code_file_offset + mod->code.size;
    }
    image_size = align_up_u64(
        mod->data.size > 0u ? data_rva + mod->data.size : mod->code.size,
        4096u);
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
            if ((uint64_t)relocation->offset + width > mod->code.size ||
                (g_opts.target_arch == ARCH_X86 && width != 4u)) {
                rcc_error((SourceLoc){outfile, 0, 0}, "invalid target relocation in RIN v3 output");
                rcc_free(relocations);
                return false;
            }
            relocations[relocation_index].virtual_address = relocation->offset;
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
    if (mod->data.size > 0u) {
        memcpy(output + data_file_offset, mod->data.data, mod->data.size);
    }
    for (relocation = mod->relocs; relocation; relocation = relocation->next) {
        uint64_t resolved;
        bool is_64bit = relocation->type == RIN_RELOC_ABS64;
        if (!module_resolve_image_relocation(mod, relocation->offset,
                                             is_64bit, data_rva,
                                             &resolved)) {
            rcc_error((SourceLoc){outfile, 0, 0},
                      "unresolved direct-image relocation at code offset %u",
                      relocation->offset);
            rcc_free(relocations);
            rcc_free(output);
            return false;
        }
        if (relocation->type == RIN_RELOC_ABS64) {
            memcpy(output + code_file_offset + relocation->offset,
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
            memcpy(output + code_file_offset + relocation->offset, &value, sizeof(value));
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
