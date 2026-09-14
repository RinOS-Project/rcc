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

static bool drv_relocation_source(const Module* mod,
                                  ModuleSymbolSection source_section,
                                  uint64_t rodata_rva, uint64_t data_rva,
                                  uint64_t init_array_rva,
                                  uint64_t fini_array_rva,
                                  uint64_t code_file_offset,
                                  uint64_t rodata_file_offset,
                                  uint64_t init_array_file_offset,
                                  uint64_t fini_array_file_offset,
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
        case MODULE_SYMBOL_INIT_ARRAY:
            *source_rva = init_array_rva;
            *source_file_offset = init_array_file_offset;
            *source_size = mod->init_array.size;
            return true;
        case MODULE_SYMBOL_FINI_ARRAY:
            *source_rva = fini_array_rva;
            *source_file_offset = fini_array_file_offset;
            *source_size = mod->fini_array.size;
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

static bool drv_add_got_slot_relocations(Module* mod, const char* outfile) {
    uint32_t pointer_size = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
    uint32_t relocation_type = pointer_size == 8u
        ? RIN_RELOC_ABS64 : RIN_RELOC_ABS32;

    for (ModuleGotEntry* entry = mod ? mod->got_entries : NULL;
         entry; entry = entry->next) {
        bool present = false;
        if (entry->offset > mod->data.size ||
            pointer_size > mod->data.size - entry->offset) {
            rcc_error((SourceLoc){outfile, 0, 0},
                      "PIC GOT slot exceeds NDRV data section");
            return false;
        }
        for (Reloc* relocation = mod->relocs; relocation;
             relocation = relocation->next) {
            if (relocation->source_section == MODULE_SYMBOL_DATA &&
                relocation->offset == entry->offset &&
                relocation->type == relocation_type) {
                present = true;
                break;
            }
        }
        if (!present) {
            add_reloc(mod, MODULE_SYMBOL_DATA, entry->offset,
                      relocation_type);
        }
    }
    return true;
}

static bool drv_patch_relative_relocations(
    Module* mod, uint8_t* output, const char* outfile,
    uint64_t rodata_rva, uint64_t init_array_rva, uint64_t fini_array_rva,
    uint64_t data_rva, uint64_t bss_rva, uint64_t code_file_offset,
    uint64_t rodata_file_offset, uint64_t init_array_file_offset,
    uint64_t fini_array_file_offset, uint64_t data_file_offset) {
    for (int index = 0; index < mod->reloc_count; ++index) {
        const ModuleReloc* relocation = &mod->relocs_arr[index];
        const ModuleSymbol* target;
        uint64_t source_rva;
        uint64_t source_file_offset;
        uint64_t source_size;
        uint64_t target_rva;
        uint64_t place;
        int64_t displacement;
        uint32_t encoded;

        if (!relocation->is_relative) continue;
        if (!drv_relocation_source(
                mod, relocation->source_section, rodata_rva, data_rva,
                init_array_rva, fini_array_rva, code_file_offset,
                rodata_file_offset, init_array_file_offset,
                fini_array_file_offset, data_file_offset, &source_rva,
                &source_file_offset, &source_size) || relocation->offset > source_size ||
            4u > source_size - relocation->offset ||
            !relocation->symbol_name) {
            rcc_error((SourceLoc){outfile, 0, 0},
                      "invalid relative relocation in direct NDRV v3 output");
            return false;
        }
        target = module_lookup_symbol(mod, relocation->symbol_name);
        if (!target || !target->is_defined) {
            rcc_error((SourceLoc){outfile, 0, 0},
                      "direct NDRV v3 output cannot contain unresolved relative relocation '%s'; emit .ro and link with rld",
                      relocation->symbol_name);
            return false;
        }
        switch (target->section) {
            case MODULE_SYMBOL_CODE: target_rva = target->offset; break;
            case MODULE_SYMBOL_RODATA: target_rva = rodata_rva + target->offset; break;
            case MODULE_SYMBOL_DATA: target_rva = data_rva + target->offset; break;
            case MODULE_SYMBOL_BSS: target_rva = bss_rva + target->offset; break;
            case MODULE_SYMBOL_INIT_ARRAY:
                target_rva = init_array_rva + target->offset;
                break;
            case MODULE_SYMBOL_FINI_ARRAY:
                target_rva = fini_array_rva + target->offset;
                break;
            default:
                rcc_error((SourceLoc){outfile, 0, 0},
                          "relative relocation target '%s' is not loadable",
                          relocation->symbol_name);
                return false;
        }
        if (target_rva > UINT64_MAX - relocation->target ||
            source_rva > UINT64_MAX - relocation->offset - 4u) {
            rcc_error((SourceLoc){outfile, 0, 0},
                      "relative relocation address overflow");
            return false;
        }
        target_rva += relocation->target;
        place = source_rva + relocation->offset + 4u;
        if (target_rva >= place) {
            uint64_t distance = target_rva - place;
            if (distance > (uint64_t)INT32_MAX) {
                rcc_error((SourceLoc){outfile, 0, 0},
                          "relative relocation overflow for '%s'",
                          relocation->symbol_name);
                return false;
            }
            displacement = (int64_t)distance;
        } else {
            uint64_t distance = place - target_rva;
            if (distance > (uint64_t)INT32_MAX + 1u) {
                rcc_error((SourceLoc){outfile, 0, 0},
                          "relative relocation underflow for '%s'",
                          relocation->symbol_name);
                return false;
            }
            displacement = -(int64_t)distance;
        }
        encoded = (uint32_t)(int32_t)displacement;
        memcpy(output + source_file_offset + relocation->offset,
               &encoded, sizeof(encoded));
    }
    return true;
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
    RinSectionV3 sections[7];
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
    uint32_t init_array_name = 0u;
    uint32_t fini_array_name = 0u;
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
    uint64_t init_array_file_offset = 0u;
    uint64_t fini_array_file_offset = 0u;
    uint64_t data_file_offset = 0u;
    uint64_t relocation_file_offset = 0u;
    uint64_t rodata_rva = 0u;
    uint64_t init_array_rva = 0u;
    uint64_t fini_array_rva = 0u;
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
        mod->data.size > UINT32_MAX || mod->bss.size > UINT32_MAX ||
        mod->tls.size > 0u) {
        rcc_error((SourceLoc){outfile, 0, 0}, "invalid module for NDRV v3 output");
        return false;
    }
    if (!drv_add_got_slot_relocations(mod, outfile)) return false;
    for (source_relocation = mod->relocs; source_relocation;
         source_relocation = source_relocation->next) ++relocation_count;
    if (mod->rodata.size > 0u) ++section_count;
    if (mod->init_array.size > 0u) ++section_count;
    if (mod->fini_array.size > 0u) ++section_count;
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
    if (mod->init_array.size > 0u) ADD_STRING(".init_array", init_array_name);
    if (mod->fini_array.size > 0u) ADD_STRING(".fini_array", fini_array_name);
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
    if (mod->init_array.size > 0u) {
        uint64_t pointer_size = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
        init_array_rva = drv_align(mapped_end, pointer_size);
        init_array_file_offset = code_file_offset + init_array_rva;
        payload_file_end = init_array_file_offset + mod->init_array.size;
        mapped_end = init_array_rva + mod->init_array.size;
    }
    if (mod->fini_array.size > 0u) {
        uint64_t pointer_size = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
        fini_array_rva = drv_align(mapped_end, pointer_size);
        fini_array_file_offset = code_file_offset + fini_array_rva;
        payload_file_end = fini_array_file_offset + mod->fini_array.size;
        mapped_end = fini_array_rva + mod->fini_array.size;
    }
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
    sections[0].file_size = mod->fini_array.size > 0u
        ? fini_array_rva + mod->fini_array.size
        : mod->init_array.size > 0u
            ? init_array_rva + mod->init_array.size : mod->code.size;
    sections[0].memory_size = sections[0].file_size;
    sections[0].name_offset = text_name;
    if (mod->init_array.size > 0u) {
        RinSectionV3* section = &sections[next_section++];
        section->type = RIN_IMAGE_SECTION_INIT_ARRAY;
        section->flags = RIN_IMAGE_SECTION_READ;
        section->alignment = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
        section->file_offset = init_array_file_offset;
        section->file_size = mod->init_array.size;
        section->virtual_address = init_array_rva;
        section->memory_size = mod->init_array.size;
        section->name_offset = init_array_name;
    }
    if (mod->fini_array.size > 0u) {
        RinSectionV3* section = &sections[next_section++];
        section->type = RIN_IMAGE_SECTION_FINI_ARRAY;
        section->flags = RIN_IMAGE_SECTION_READ;
        section->alignment = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
        section->file_offset = fini_array_file_offset;
        section->file_size = mod->fini_array.size;
        section->virtual_address = fini_array_rva;
        section->memory_size = mod->fini_array.size;
        section->name_offset = fini_array_name;
    }
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
            uint64_t source_rva;
            uint64_t source_file_offset;
            uint64_t source_size;
            if (!drv_relocation_source(mod, source_relocation->source_section,
                                       rodata_rva, data_rva, init_array_rva,
                                       fini_array_rva,
                                       code_file_offset, rodata_file_offset,
                                       init_array_file_offset,
                                       fini_array_file_offset, data_file_offset,
                                       &source_rva,
                                       &source_file_offset, &source_size) ||
                source_relocation->offset > source_size ||
                width > source_size - source_relocation->offset ||
                (g_opts.target_arch == ARCH_X86 && width != 4u)) {
                rcc_error((SourceLoc){outfile, 0, 0}, "invalid NDRV v3 relocation");
                rcc_free(relocations);
                return false;
            }
            (void)source_file_offset;
            relocations[relocation_index].virtual_address =
                source_rva + source_relocation->offset;
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
    memset(output, 0, (size_t)unsigned_size);
    memcpy(output, &header, sizeof(header));
    memcpy(output + section_table_offset, sections,
           (size_t)section_count * sizeof(RinSectionV3));
    memcpy(output + match_table_offset, &match, sizeof(match));
    memcpy(output + string_table_offset, strings, strings_size);
    memcpy(output + code_file_offset, mod->code.data, mod->code.size);
    if (mod->init_array.size > 0u) {
        memcpy(output + init_array_file_offset, mod->init_array.data,
               mod->init_array.size);
    }
    if (mod->fini_array.size > 0u) {
        memcpy(output + fini_array_file_offset, mod->fini_array.data,
               mod->fini_array.size);
    }
    if (mod->rodata.size > 0u) {
        memcpy(output + rodata_file_offset, mod->rodata.data,
               mod->rodata.size);
    }
    if (mod->data.size > 0u) memcpy(output + data_file_offset, mod->data.data, mod->data.size);
    if (!drv_patch_relative_relocations(
            mod, output, outfile, rodata_rva, init_array_rva, fini_array_rva,
            data_rva, bss_rva, code_file_offset, rodata_file_offset,
            init_array_file_offset, fini_array_file_offset, data_file_offset)) {
        rcc_free(relocations);
        rcc_free(output);
        return false;
    }
    for (source_relocation = mod->relocs; source_relocation;
         source_relocation = source_relocation->next) {
        uint64_t resolved;
        uint64_t source_rva;
        uint64_t source_file_offset;
        uint64_t source_size;
        bool is_64bit = source_relocation->type == RIN_RELOC_ABS64;
        if (!drv_relocation_source(mod, source_relocation->source_section,
                                   rodata_rva, data_rva, init_array_rva,
                                   fini_array_rva,
                                   code_file_offset, rodata_file_offset,
                                   init_array_file_offset,
                                   fini_array_file_offset, data_file_offset,
                                   &source_rva, &source_file_offset,
                                   &source_size) ||
            !module_resolve_image_relocation(mod,
                                             source_relocation->source_section,
                                             source_relocation->offset,
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
        (void)source_rva;
        (void)source_size;
        if (source_relocation->type == RIN_RELOC_ABS64) {
            memcpy(output + source_file_offset + source_relocation->offset,
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
            memcpy(output + source_file_offset + source_relocation->offset,
                   &value, 4u);
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
