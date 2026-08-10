/* RCC canonical unsigned NDRV v3 emitter.  Signing is performed by rinsign. */

#include "rcc.h"
#include "ast.h"
#include "codegen.h"
#include "rin_formats_v3.h"
#include <stdio.h>

static uint64_t drv_align(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static int drv_relocation_compare(const void* left, const void* right) {
    const RinRelocationV3* a = (const RinRelocationV3*)left;
    const RinRelocationV3* b = (const RinRelocationV3*)right;
    return a->virtual_address < b->virtual_address ? -1 :
           a->virtual_address > b->virtual_address ? 1 : 0;
}

static void driver_name(const char* path, char* output, size_t capacity) {
    const char* base = path;
    const char* cursor;
    size_t length = 0u;
    for (cursor = path; cursor && *cursor; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') base = cursor + 1;
    }
    while (base[length] && base[length] != '.' && length + 1u < capacity) {
        output[length] = base[length];
        ++length;
    }
    output[length] = '\0';
}

bool rcc_emit_drv(Module* mod, AST* ast, const char* outfile) {
    RinDriverHeaderV3 header;
    RinSectionV3 sections[5];
    RinDriverMatchV3 match;
    RinRelocationV3* relocations = NULL;
    Reloc* source_relocation;
    uint32_t relocation_count = 0u;
    uint32_t relocation_index = 0u;
    uint32_t section_count = 1u;
    uint32_t next_section = 1u;
    char strings[96] = {0};
    char name[48];
    uint32_t strings_size = 1u;
    uint32_t text_name;
    uint32_t rodata_name = 0u;
    uint32_t data_name = 0u;
    uint32_t bss_name = 0u;
    uint32_t relocation_name = 0u;
    uint32_t driver_name_offset;
    uint64_t section_table_offset = sizeof(RinDriverHeaderV3);
    uint64_t match_table_offset;
    uint64_t string_table_offset;
    uint64_t code_file_offset;
    uint64_t rodata_file_offset = 0u;
    uint64_t data_file_offset = 0u;
    uint64_t relocation_file_offset = 0u;
    uint64_t rodata_rva = 0u;
    uint64_t data_rva = 0u;
    uint64_t bss_rva = 0u;
    uint64_t unsigned_size;
    uint64_t payload_file_end;
    uint64_t mapped_end;
    uint64_t image_size;
    uint8_t* output;
    FILE* file;
    size_t written;
    int close_result;
    (void)ast;

    if (!mod || !outfile || mod->code.size == 0u ||
        mod->code.size > UINT32_MAX || mod->rodata.size > UINT32_MAX ||
        mod->data.size > UINT32_MAX || mod->bss.size > UINT32_MAX) {
        rcc_error((SourceLoc){outfile, 0, 0}, "invalid module for NDRV v3 output");
        return false;
    }
    for (source_relocation = mod->relocs; source_relocation;
         source_relocation = source_relocation->next) ++relocation_count;
    if (mod->rodata.size > 0u) ++section_count;
    if (mod->data.size > 0u) ++section_count;
    if (mod->bss.size > 0u) ++section_count;
    if (relocation_count > 0u) ++section_count;

#define ADD_STRING(value, result) do { \
    size_t _length = strlen(value) + 1u; \
    if (strings_size + _length > sizeof(strings)) { \
        rcc_error((SourceLoc){outfile, 0, 0}, "NDRV v3 string table overflow"); \
        return false; \
    } \
    result = strings_size; \
    memcpy(strings + strings_size, value, _length); \
    strings_size += (uint32_t)_length; \
} while (0)
    ADD_STRING(".text", text_name);
    if (mod->rodata.size > 0u) ADD_STRING(".rodata", rodata_name);
    if (mod->data.size > 0u) ADD_STRING(".data", data_name);
    if (mod->bss.size > 0u) ADD_STRING(".bss", bss_name);
    if (relocation_count > 0u) ADD_STRING(".reloc", relocation_name);
    driver_name(g_opts.input_file, name, sizeof(name));
    if (name[0] == '\0') strcpy(name, "rcc-driver");
    ADD_STRING(name, driver_name_offset);
#undef ADD_STRING

    match_table_offset = section_table_offset +
        (uint64_t)section_count * sizeof(RinSectionV3);
    string_table_offset = match_table_offset + sizeof(RinDriverMatchV3);
    code_file_offset = drv_align(string_table_offset + strings_size, 16u);
    payload_file_end = code_file_offset + mod->code.size;
    mapped_end = mod->code.size;
    if (mod->rodata.size > 0u) {
        rodata_file_offset = drv_align(payload_file_end, 16u);
        payload_file_end = rodata_file_offset + mod->rodata.size;
        rodata_rva = drv_align(mapped_end, 4096u);
        mapped_end = rodata_rva + mod->rodata.size;
    }
    if (mod->data.size > 0u) {
        data_file_offset = drv_align(payload_file_end, 16u);
        payload_file_end = data_file_offset + mod->data.size;
        data_rva = drv_align(mapped_end, 4096u);
        mapped_end = data_rva + mod->data.size;
    }
    if (mod->bss.size > 0u) {
        bss_rva = drv_align(mapped_end, 4096u);
        mapped_end = bss_rva + mod->bss.size;
    }
    if (relocation_count > 0u) {
        relocation_file_offset = drv_align(payload_file_end, 8u);
        unsigned_size = relocation_file_offset +
            (uint64_t)relocation_count * sizeof(RinRelocationV3);
    } else {
        unsigned_size = payload_file_end;
    }
    image_size = drv_align(mapped_end, 4096u);
    if (unsigned_size > SIZE_MAX || image_size == 0u ||
        (g_opts.target_arch == ARCH_X86 && image_size >= UINT64_C(0xC0000000))) {
        rcc_error((SourceLoc){outfile, 0, 0}, "NDRV v3 image exceeds target limits");
        return false;
    }

    memset(&header, 0, sizeof(header));
    memset(sections, 0, sizeof(sections));
    memset(&match, 0, sizeof(match));
    header.magic = RIN_DRIVER_IMAGE_MAGIC;
    header.version = RIN_DRIVER_IMAGE_VERSION_3;
    header.header_size = sizeof(RinDriverHeaderV3);
    header.architecture = g_opts.target_arch == ARCH_X64
        ? RIN_ARCH_X86_64 : RIN_ARCH_X86;
    header.abi_major = RIN_DRIVER_ABI_MAJOR;
    header.abi_minor = RIN_DRIVER_ABI_MINOR;
    header.driver_class = RIN_DRIVER_CLASS_CHARACTER;
    header.flags = RIN_DRIVER_IMAGE_RELOCATABLE;
    header.section_count = section_count;
    header.match_count = 1u;
    header.image_size = image_size;
    header.section_table_offset = section_table_offset;
    header.match_table_offset = match_table_offset;
    header.string_table_offset = string_table_offset;
    header.string_table_size = strings_size;
    header.start_rva = mod->entry_point;

    sections[0].type = RIN_IMAGE_SECTION_CODE;
    sections[0].flags = RIN_IMAGE_SECTION_READ | RIN_IMAGE_SECTION_EXECUTE;
    sections[0].alignment = 16u;
    sections[0].file_offset = code_file_offset;
    sections[0].file_size = mod->code.size;
    sections[0].memory_size = mod->code.size;
    sections[0].name_offset = text_name;
    if (mod->rodata.size > 0u) {
        RinSectionV3* section = &sections[next_section++];
        section->type = RIN_IMAGE_SECTION_RODATA;
        section->flags = RIN_IMAGE_SECTION_READ;
        section->alignment = 16u;
        section->file_offset = rodata_file_offset;
        section->file_size = mod->rodata.size;
        section->virtual_address = rodata_rva;
        section->memory_size = mod->rodata.size;
        section->name_offset = rodata_name;
    }
    if (mod->data.size > 0u) {
        RinSectionV3* section = &sections[next_section++];
        section->type = RIN_IMAGE_SECTION_DATA;
        section->flags = RIN_IMAGE_SECTION_READ | RIN_IMAGE_SECTION_WRITE;
        section->alignment = 16u;
        section->file_offset = data_file_offset;
        section->file_size = mod->data.size;
        section->virtual_address = data_rva;
        section->memory_size = mod->data.size;
        section->name_offset = data_name;
    }
    if (mod->bss.size > 0u) {
        RinSectionV3* section = &sections[next_section++];
        section->type = RIN_IMAGE_SECTION_BSS;
        section->flags = RIN_IMAGE_SECTION_READ | RIN_IMAGE_SECTION_WRITE;
        section->alignment = mod->bss.align;
        section->virtual_address = bss_rva;
        section->memory_size = mod->bss.size;
        section->name_offset = bss_name;
    }
    if (relocation_count > 0u) {
        RinSectionV3* section = &sections[next_section];
        section->type = RIN_IMAGE_SECTION_RELOCATIONS;
        section->flags = RIN_IMAGE_SECTION_DISCARDABLE;
        section->alignment = 8u;
        section->file_offset = relocation_file_offset;
        section->file_size =
            (uint64_t)relocation_count * sizeof(RinRelocationV3);
        section->name_offset = relocation_name;
        relocations = rcc_alloc((size_t)section->file_size);
        for (source_relocation = mod->relocs; source_relocation;
             source_relocation = source_relocation->next) {
            uint64_t width = source_relocation->type == RIN_RELOC_ABS64 ? 8u : 4u;
            if ((uint64_t)source_relocation->offset + width > mod->code.size ||
                (g_opts.target_arch == ARCH_X86 && width != 4u)) {
                rcc_error((SourceLoc){outfile, 0, 0}, "invalid NDRV v3 relocation");
                rcc_free(relocations);
                return false;
            }
            relocations[relocation_index].virtual_address = source_relocation->offset;
            relocations[relocation_index].type = width == 8u
                ? RIN_IMAGE_RELOCATION_ABS64 : RIN_IMAGE_RELOCATION_ABS32U;
            ++relocation_index;
        }
        qsort(relocations, relocation_count, sizeof(RinRelocationV3),
              drv_relocation_compare);
        for (relocation_index = 1u; relocation_index < relocation_count;
             ++relocation_index) {
            uint64_t previous_width = relocations[relocation_index - 1u].type ==
                RIN_IMAGE_RELOCATION_ABS64 ? 8u : 4u;
            if (relocations[relocation_index - 1u].virtual_address + previous_width >
                relocations[relocation_index].virtual_address) {
                rcc_error((SourceLoc){outfile, 0, 0}, "overlapping NDRV v3 relocations");
                rcc_free(relocations);
                return false;
            }
        }
    }

    match.bus_type = RIN_DRIVER_BUS_PLATFORM;
    match.flags = RIN_DRIVER_MATCH_CLASS;
    match.class_code = 1u;       /* RCC generic platform-driver ABI class. */
    match.class_mask = UINT32_MAX;
    match.name_offset = driver_name_offset;

    output = rcc_alloc((size_t)unsigned_size);
    memcpy(output, &header, sizeof(header));
    memcpy(output + section_table_offset, sections,
           (size_t)section_count * sizeof(RinSectionV3));
    memcpy(output + match_table_offset, &match, sizeof(match));
    memcpy(output + string_table_offset, strings, strings_size);
    memcpy(output + code_file_offset, mod->code.data, mod->code.size);
    if (mod->rodata.size > 0u) {
        memcpy(output + rodata_file_offset, mod->rodata.data,
               mod->rodata.size);
    }
    if (mod->data.size > 0u) memcpy(output + data_file_offset, mod->data.data, mod->data.size);
    for (source_relocation = mod->relocs; source_relocation;
         source_relocation = source_relocation->next) {
        uint64_t resolved;
        bool is_64bit = source_relocation->type == RIN_RELOC_ABS64;
        if (!module_resolve_image_relocation(mod, source_relocation->offset,
                                             is_64bit, rodata_rva, data_rva,
                                             bss_rva,
                                             &resolved)) {
            rcc_error((SourceLoc){outfile, 0, 0},
                      "unresolved NDRV relocation at code offset %u",
                      source_relocation->offset);
            rcc_free(relocations);
            rcc_free(output);
            return false;
        }
        if (source_relocation->type == RIN_RELOC_ABS64) {
            memcpy(output + code_file_offset + source_relocation->offset,
                   &resolved, sizeof(resolved));
        } else {
            uint32_t value;
            if (resolved > UINT32_MAX) {
                rcc_error((SourceLoc){outfile, 0, 0}, "NDRV ABS32U relocation overflow");
                rcc_free(relocations);
                rcc_free(output);
                return false;
            }
            value = (uint32_t)resolved;
            memcpy(output + code_file_offset + source_relocation->offset, &value, 4u);
        }
    }
    if (relocation_count > 0u) {
        memcpy(output + relocation_file_offset, relocations,
               (size_t)relocation_count * sizeof(RinRelocationV3));
    }

    file = fopen(outfile, "wb");
    if (!file) {
        rcc_error((SourceLoc){outfile, 0, 0}, "cannot create NDRV v3 output");
        rcc_free(relocations);
        rcc_free(output);
        return false;
    }
    written = fwrite(output, 1u, (size_t)unsigned_size, file);
    close_result = fclose(file);
    rcc_free(relocations);
    rcc_free(output);
    if (written != (size_t)unsigned_size || close_result != 0) {
        rcc_error((SourceLoc){outfile, 0, 0}, "cannot write NDRV v3 output");
        return false;
    }
    if (g_opts.verbose) {
        printf("Unsigned NDRV v3 stage: %s (%s, %u sections)\n", outfile,
               rcc_target_triple(g_opts.target_arch), section_count);
    }
    return true;
}
