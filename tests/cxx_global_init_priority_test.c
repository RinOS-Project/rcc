#include "objfile.h"
#include "rin_formats_v3.h"

#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ExpectedInitializer {
    size_t object_index;
    uint16_t priority;
} ExpectedInitializer;

static uint64_t read_le(const uint8_t* data, size_t width) {
    uint64_t value = 0u;
    for (size_t index = 0u; index < width; ++index) {
        value |= (uint64_t)data[index] << (index * 8u);
    }
    return value;
}

static ObjSymbol* find_priority_symbol(ObjectFile* object,
                                       uint16_t priority) {
    char local_name[64];
    int length = snprintf(local_name, sizeof(local_name),
                          "__rcc_global_init_priority_%05u",
                          (unsigned)priority);
    if (length < 0 || (size_t)length >= sizeof(local_name)) return NULL;
    for (ObjSymbol* symbol = object ? object->symbols : NULL;
         symbol; symbol = symbol->next) {
        const char* local = symbol->name;
        for (const char* scan = symbol->name; scan && *scan; ++scan) {
            if (scan[0] == ':' && scan[1] == ':') local = scan + 2;
        }
        if (symbol->type != SYM_UNDEF && strcmp(local, local_name) == 0) {
            return symbol;
        }
    }
    return NULL;
}

static uint64_t text_section_offset(ObjectFile* const* objects,
                                    size_t object_index) {
    uint64_t offset = 0u;
    for (size_t index = 0u; index <= object_index; ++index) {
        ObjSection* text = objfile_get_section(objects[index], ".text");
        if (!text || text->type != SECT_CODE || text->align == 0u ||
            (text->align & (text->align - 1u)) != 0u) {
            fprintf(stderr, "init-priority test: missing aligned .text\n");
            return UINT64_MAX;
        }
        uint64_t mask = (uint64_t)text->align - 1u;
        if (offset > UINT64_MAX - mask) return UINT64_MAX;
        offset = (offset + mask) & ~mask;
        if (index == object_index) return offset;
        if (text->memory_size > UINT64_MAX - offset) return UINT64_MAX;
        offset += text->memory_size;
    }
    return UINT64_MAX;
}

static bool verify_linked_order(const char* image_path,
                                const char* const* object_paths,
                                size_t object_count) {
    static const ExpectedInitializer expected[] = {
        {0u, 101u}, {2u, 101u}, {1u, 150u}, {0u, 200u}, {0u, 65535u}
    };
    ObjectFile* objects[3] = {NULL, NULL, NULL};
    uint8_t* bytes = NULL;
    RinHeaderV3 header;
    RinSectionV3 init_array;
    RinSectionV3 code;
    FILE* file = NULL;
    long file_length;
    bool success = false;

    if (object_count != sizeof(objects) / sizeof(objects[0]) ||
        object_count != 3u) {
        fprintf(stderr, "init-priority test: expected three input objects\n");
        return false;
    }
    for (size_t index = 0u; index < object_count; ++index) {
        objects[index] = objfile_read(object_paths[index]);
        if (!objects[index]) {
            fprintf(stderr, "init-priority test: cannot read %s\n",
                    object_paths[index]);
            goto cleanup;
        }
    }

    file = fopen(image_path, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0 ||
        (file_length = ftell(file)) < (long)sizeof(header) ||
        fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "init-priority test: cannot read %s\n", image_path);
        goto cleanup;
    }
    bytes = malloc((size_t)file_length);
    if (!bytes || fread(bytes, 1u, (size_t)file_length, file) !=
                      (size_t)file_length) {
        fprintf(stderr, "init-priority test: short image read\n");
        goto cleanup;
    }
    memcpy(&header, bytes, sizeof(header));
    if (header.magic != RIN_IMAGE_MAGIC ||
        header.version != RIN_IMAGE_VERSION_3 ||
        header.header_size != sizeof(header) ||
        header.section_table_offset > (uint64_t)file_length ||
        header.section_count >
            ((uint64_t)file_length - header.section_table_offset) /
                sizeof(RinSectionV3)) {
        fprintf(stderr, "init-priority test: malformed RIN v3 header\n");
        goto cleanup;
    }
    memset(&init_array, 0, sizeof(init_array));
    memset(&code, 0, sizeof(code));
    bool have_init_array = false;
    bool have_code = false;
    for (uint32_t index = 0u; index < header.section_count; ++index) {
        RinSectionV3 section;
        uint64_t offset = header.section_table_offset +
                          (uint64_t)index * sizeof(section);
        memcpy(&section, bytes + (size_t)offset, sizeof(section));
        if (section.file_offset > (uint64_t)file_length ||
            section.file_size > (uint64_t)file_length - section.file_offset) {
            fprintf(stderr, "init-priority test: section exceeds image\n");
            goto cleanup;
        }
        if (section.type == RIN_IMAGE_SECTION_INIT_ARRAY) {
            init_array = section;
            have_init_array = true;
        } else if (section.type == RIN_IMAGE_SECTION_CODE) {
            code = section;
            have_code = true;
        }
    }
    if (!have_init_array || !have_code || code.virtual_address != 0u) {
        fprintf(stderr, "init-priority test: missing canonical code/init array\n");
        goto cleanup;
    }
    size_t pointer_size = header.architecture == RIN_ARCH_X86_64 ? 8u :
                          header.architecture == RIN_ARCH_X86 ? 4u : 0u;
    if (pointer_size == 0u || init_array.file_size !=
            sizeof(expected) / sizeof(expected[0]) * pointer_size) {
        fprintf(stderr, "init-priority test: unexpected init-array size\n");
        goto cleanup;
    }

    for (size_t index = 0u;
         index < sizeof(expected) / sizeof(expected[0]); ++index) {
        const ExpectedInitializer* entry = &expected[index];
        ObjSymbol* symbol = find_priority_symbol(
            objects[entry->object_index], entry->priority);
        uint64_t section_offset = text_section_offset(
            objects, entry->object_index);
        if (!symbol || symbol->section < 0 ||
            section_offset == UINT64_MAX ||
            symbol->value > UINT64_MAX - section_offset ||
            header.preferred_base > UINT64_MAX - code.virtual_address ||
            header.preferred_base + code.virtual_address >
                UINT64_MAX - section_offset - symbol->value) {
            fprintf(stderr,
                    "init-priority test: cannot resolve priority %u callback\n",
                    (unsigned)entry->priority);
            goto cleanup;
        }
        uint64_t expected_address = header.preferred_base +
            code.virtual_address + section_offset + symbol->value;
        uint64_t actual_address = read_le(
            bytes + (size_t)init_array.file_offset + index * pointer_size,
            pointer_size);
        if (actual_address != expected_address) {
            fprintf(stderr,
                    "init-priority test: entry %zu is 0x%" PRIx64
                    ", expected priority %u callback at 0x%" PRIx64 "\n",
                    index, actual_address, (unsigned)entry->priority,
                    expected_address);
            goto cleanup;
        }
    }
    success = true;

cleanup:
    if (file) fclose(file);
    free(bytes);
    for (size_t index = 0u; index < object_count; ++index) {
        objfile_free(objects[index]);
    }
    return success;
}

int main(int argc, char** argv) {
    if (argc != 5 ||
        !verify_linked_order(argv[1],
                             (const char* const*)&argv[2], 3u)) {
        fprintf(stderr,
                "usage: init-priority-test IMAGE.RIN A.RO B.RO C.RO\n");
        return 1;
    }
    return 0;
}
