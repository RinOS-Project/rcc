/* SPDX-License-Identifier: MIT */
#define _GNU_SOURCE

#include "rin_formats_v3.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef int (*RccEntry)(int argc, char** argv);

static void fail(const char* message)
{
    fprintf(stderr, "bootstrap-stage-runner: %s\n", message);
    exit(2);
}

static int range_valid(uint64_t offset, uint64_t size, uint64_t capacity)
{
    return offset <= capacity && size <= capacity - offset;
}

static FILE* rin_stderr_adapter(void)
{
    return stderr;
}

static uintptr_t symbol_address(const char* name)
{
    void* address;
    if (strcmp(name, "__rin_stderr") == 0) {
        FILE* (*function)(void) = rin_stderr_adapter;
        uintptr_t result = 0u;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    if (strcmp(name, "atexit") == 0) {
        int (*function)(void (*)(void)) = atexit;
        uintptr_t result = 0u;
        memcpy(&result, &function, sizeof(function));
        return result;
    }
    dlerror();
    address = dlsym(RTLD_DEFAULT, name);
    if (!address || dlerror() != NULL) {
        fprintf(stderr, "bootstrap-stage-runner: unresolved import '%s'\n",
                name);
        exit(2);
    }
    return (uintptr_t)address;
}

static void protect_range(uint8_t* image, uint64_t image_size,
                          uint64_t offset, uint64_t size, int protection)
{
    long page_size = sysconf(_SC_PAGESIZE);
    uintptr_t start;
    uintptr_t end;
    if (page_size <= 0 || !range_valid(offset, size, image_size)) {
        fail("invalid protection range");
    }
    start = ((uintptr_t)image + (uintptr_t)offset) &
            ~((uintptr_t)page_size - 1u);
    end = ((uintptr_t)image + (uintptr_t)(offset + size) +
           (uintptr_t)page_size - 1u) & ~((uintptr_t)page_size - 1u);
    if (end > start && mprotect((void*)start, end - start, protection) != 0) {
        fail("mprotect failed");
    }
}

static uint8_t* map_image(const uint8_t* file_bytes, size_t file_size,
                          const RinHeaderV3* header,
                          const RinSectionV3* sections)
{
    uint8_t* image;
    void* hint = header->architecture == RIN_ARCH_X86
        ? (void*)(uintptr_t)0x20000000u : NULL;
    image = mmap(hint, (size_t)header->image_size,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (image == MAP_FAILED) fail("mmap failed");
    if (header->architecture == RIN_ARCH_X86 &&
        (uintptr_t)image + header->image_size >= UINT64_C(0xc0000000)) {
        fail("i686 image was not mapped below 3 GiB");
    }
    for (uint32_t index = 0u; index < header->section_count; ++index) {
        const RinSectionV3* section = &sections[index];
        if (section->type < RIN_IMAGE_SECTION_CODE ||
            section->type > RIN_IMAGE_SECTION_FINI_ARRAY ||
            section->file_size == 0u || section->memory_size == 0u) {
            continue;
        }
        if (!range_valid(section->file_offset, section->file_size, file_size) ||
            !range_valid(section->virtual_address, section->memory_size,
                         header->image_size) ||
            section->file_size > section->memory_size) {
            fail("invalid load section");
        }
        memcpy(image + section->virtual_address,
               file_bytes + section->file_offset,
               (size_t)section->file_size);
    }
    return image;
}

static void apply_relocations(uint8_t* image, const uint8_t* file_bytes,
                              size_t file_size, const RinHeaderV3* header,
                              const RinSectionV3* sections)
{
    uint64_t runtime_base = (uint64_t)(uintptr_t)image;
    for (uint32_t index = 0u; index < header->section_count; ++index) {
        const RinSectionV3* section = &sections[index];
        uint64_t count;
        const RinRelocationV3* relocations;
        if (section->type != RIN_IMAGE_SECTION_RELOCATIONS) continue;
        if (!range_valid(section->file_offset, section->file_size, file_size) ||
            section->file_size % sizeof(RinRelocationV3) != 0u) {
            fail("invalid relocation section");
        }
        relocations = (const RinRelocationV3*)(file_bytes +
                                               section->file_offset);
        count = section->file_size / sizeof(*relocations);
        for (uint64_t relocation_index = 0u;
             relocation_index < count; ++relocation_index) {
            const RinRelocationV3* relocation =
                &relocations[relocation_index];
            uint64_t preferred_target;
            uint64_t runtime_target;
            uint32_t width = relocation->type ==
                RIN_IMAGE_RELOCATION_ABS64 ? 8u : 4u;
            if (!range_valid(relocation->virtual_address, width,
                             header->image_size)) {
                fail("relocation target is outside image");
            }
            if (relocation->type == RIN_IMAGE_RELOCATION_ABS64) {
                memcpy(&preferred_target,
                       image + relocation->virtual_address, 8u);
            } else if (relocation->type == RIN_IMAGE_RELOCATION_ABS32U ||
                       relocation->type == RIN_IMAGE_RELOCATION_ABS32S) {
                uint32_t value;
                memcpy(&value, image + relocation->virtual_address, 4u);
                preferred_target = value;
            } else {
                fail("unsupported bootstrap relocation");
            }
            if (preferred_target < header->preferred_base ||
                preferred_target - header->preferred_base >=
                    header->image_size) {
                fail("absolute relocation points outside image");
            }
            runtime_target = runtime_base +
                (preferred_target - header->preferred_base);
            if (width == 8u) {
                memcpy(image + relocation->virtual_address,
                       &runtime_target, 8u);
            } else {
                uint32_t value;
                if (runtime_target > UINT32_MAX) {
                    fail("i686 relocation overflow");
                }
                value = (uint32_t)runtime_target;
                memcpy(image + relocation->virtual_address, &value, 4u);
            }
        }
    }
}

static void bind_imports(uint8_t* image, const uint8_t* file_bytes,
                         size_t file_size, const RinHeaderV3* header,
                         const RinSectionV3* sections)
{
    const char* strings;
    if (!range_valid(header->string_table_offset, header->string_table_size,
                     file_size)) {
        fail("invalid string table");
    }
    strings = (const char*)(file_bytes + header->string_table_offset);
    for (uint32_t index = 0u; index < header->section_count; ++index) {
        const RinSectionV3* section = &sections[index];
        const RinImportV3* imports;
        uint64_t count;
        uint32_t pointer_width = header->architecture == RIN_ARCH_X86
            ? 4u : 8u;
        if (section->type != RIN_IMAGE_SECTION_IMPORTS) continue;
        if (!range_valid(section->file_offset, section->file_size, file_size) ||
            section->file_size % sizeof(RinImportV3) != 0u) {
            fail("invalid import section");
        }
        imports = (const RinImportV3*)(file_bytes + section->file_offset);
        count = section->file_size / sizeof(*imports);
        for (uint64_t import_index = 0u; import_index < count; ++import_index) {
            const RinImportV3* imported = &imports[import_index];
            uintptr_t address;
            if (imported->name_offset >= header->string_table_size ||
                imported->kind != RIN_SYMBOL_FUNCTION ||
                imported->dependency_index >= header->dependency_count ||
                !range_valid(imported->target_rva, pointer_width,
                             header->image_size)) {
                fail("invalid bootstrap import");
            }
            address = symbol_address(strings + imported->name_offset);
            if (pointer_width == 4u) {
                uint32_t value;
                if (address > UINT32_MAX) fail("i686 import overflow");
                value = (uint32_t)address;
                memcpy(image + imported->target_rva, &value, 4u);
            } else {
                uint64_t value = (uint64_t)address;
                memcpy(image + imported->target_rva, &value, 8u);
            }
        }
    }
}

static void enforce_wx(uint8_t* image, const RinHeaderV3* header,
                       const RinSectionV3* sections)
{
    for (uint32_t index = 0u; index < header->section_count; ++index) {
        const RinSectionV3* section = &sections[index];
        if (section->memory_size == 0u) continue;
        if (section->type == RIN_IMAGE_SECTION_CODE) {
            protect_range(image, header->image_size,
                          section->virtual_address, section->memory_size,
                          PROT_READ | PROT_EXEC);
        } else if (section->type == RIN_IMAGE_SECTION_DATA ||
                   section->type == RIN_IMAGE_SECTION_BSS) {
            protect_range(image, header->image_size,
                          section->virtual_address, section->memory_size,
                          PROT_READ | PROT_WRITE);
        }
    }
}

int main(int argc, char** argv)
{
    FILE* file;
    long end;
    uint8_t* bytes;
    RinHeaderV3* header;
    RinSectionV3* sections;
    uint8_t* image;
    RccEntry entry;
    void* entry_address;
    int result;

    if (argc < 4) {
        fprintf(stderr,
                "usage: %s IMAGE TOOL-NAME [RCC-ARGUMENT ...]\n", argv[0]);
        return 2;
    }
    file = fopen(argv[1], "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0 ||
        (end = ftell(file)) < (long)sizeof(RinHeaderV3) ||
        fseek(file, 0, SEEK_SET) != 0) {
        fail("cannot read image");
    }
    bytes = malloc((size_t)end);
    if (!bytes || fread(bytes, 1u, (size_t)end, file) != (size_t)end ||
        fclose(file) != 0) {
        fail("cannot load image bytes");
    }
    header = (RinHeaderV3*)bytes;
    if (header->magic != RIN_IMAGE_MAGIC ||
        header->version != RIN_IMAGE_VERSION_3 ||
        header->header_size != sizeof(*header) ||
        !range_valid(header->section_table_offset,
                     (uint64_t)header->section_count * sizeof(RinSectionV3),
                     (uint64_t)end) ||
        header->entry_rva >= header->image_size ||
        header->image_size == 0u || header->image_size > SIZE_MAX) {
        fail("invalid RIN v3 header");
    }
#if defined(__i386__)
    if (header->architecture != RIN_ARCH_X86) fail("runner architecture mismatch");
#else
    if (header->architecture != RIN_ARCH_X86_64) fail("runner architecture mismatch");
#endif
    sections = (RinSectionV3*)(bytes + header->section_table_offset);
    image = map_image(bytes, (size_t)end, header, sections);
    apply_relocations(image, bytes, (size_t)end, header, sections);
    bind_imports(image, bytes, (size_t)end, header, sections);
    enforce_wx(image, header, sections);
    entry_address = image + header->entry_rva;
    memcpy(&entry, &entry_address, sizeof(entry));
    result = entry(argc - 2, &argv[2]);
    if (result != 0) {
        fprintf(stderr, "bootstrap-stage-runner: stage returned %d\n", result);
    }
    /* Keep the image mapped: stage1 registers generated atexit callbacks. */
    free(bytes);
    return result;
}
