#include "objfile.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(_WIN64) && defined(__GNUC__)
typedef int (__attribute__((sysv_abi)) *EntryFunction)(void);
#else
typedef int (*EntryFunction)(void);
#endif

static size_t align_up(size_t value, size_t alignment)
{
    size_t remainder;
    assert(alignment != 0u);
    remainder = value % alignment;
    assert(remainder == 0u || value <= SIZE_MAX - (alignment - remainder));
    return remainder == 0u ? value : value + alignment - remainder;
}

static void resolve_local_relocations(ObjectFile* object,
                                      size_t* section_offsets,
                                      uint8_t* mapping, size_t image_size)
{
    ObjSection* source_section;
    int source_index = 0;

    for (source_section = object->sections; source_section;
         source_section = source_section->next, ++source_index) {
        ObjReloc* relocation;
        if (section_offsets[source_index] == SIZE_MAX) continue;
        for (relocation = source_section->relocs; relocation;
             relocation = relocation->next) {
            ObjSymbol* symbol = objfile_find_symbol(
                object, relocation->symbol_name);
            uint64_t target_offset;
            uint8_t* patch;

            assert(symbol != NULL && symbol->section >= 0 &&
                   symbol->section < object->section_count);
            assert(section_offsets[symbol->section] != SIZE_MAX);
            assert(section_offsets[symbol->section] <= UINT64_MAX -
                   symbol->value);
            target_offset = (uint64_t)section_offsets[symbol->section] +
                symbol->value;
            if (relocation->addend >= 0) {
                assert((uint64_t)relocation->addend <=
                       UINT64_MAX - target_offset);
                target_offset += (uint64_t)relocation->addend;
            } else {
                uint64_t magnitude = UINT64_C(0) -
                    (uint64_t)relocation->addend;
                assert(magnitude <= target_offset);
                target_offset -= magnitude;
            }
            assert(target_offset < image_size);
            patch = mapping + section_offsets[source_index] +
                (size_t)relocation->offset;
            switch (relocation->type) {
                case RELOC_REL32:
                case RELOC_PLT32: {
                    int64_t displacement;
                    int32_t encoded_displacement;
                    assert(relocation->offset <= source_section->size);
                    assert(sizeof(encoded_displacement) <=
                           source_section->size -
                           (size_t)relocation->offset);
                    displacement = (int64_t)target_offset -
                        (int64_t)(section_offsets[source_index] +
                                  (size_t)relocation->offset +
                                  sizeof(encoded_displacement));
                    assert(displacement >= INT32_MIN &&
                           displacement <= INT32_MAX);
                    encoded_displacement = (int32_t)displacement;
                    memcpy(patch, &encoded_displacement,
                           sizeof(encoded_displacement));
                    break;
                }
                case RELOC_REL8: {
                    int64_t displacement;
                    int8_t encoded_displacement;
                    assert(relocation->offset < source_section->size);
                    displacement = (int64_t)target_offset -
                        (int64_t)(section_offsets[source_index] +
                                  (size_t)relocation->offset + 1u);
                    assert(displacement >= INT8_MIN &&
                           displacement <= INT8_MAX);
                    encoded_displacement = (int8_t)displacement;
                    memcpy(patch, &encoded_displacement,
                           sizeof(encoded_displacement));
                    break;
                }
                case RELOC_ABS64: {
                    uint64_t address = (uint64_t)(uintptr_t)(mapping +
                        (size_t)target_offset);
                    assert(relocation->offset <= source_section->size);
                    assert(sizeof(address) <= source_section->size -
                           (size_t)relocation->offset);
                    memcpy(patch, &address, sizeof(address));
                    break;
                }
                case RELOC_ABS32:
                case RELOC_ABS32U:
                case RELOC_ABS32S: {
                    uintptr_t address = (uintptr_t)(mapping +
                        (size_t)target_offset);
                    uint32_t encoded_address;
                    assert(relocation->offset <= source_section->size);
                    assert(sizeof(encoded_address) <= source_section->size -
                           (size_t)relocation->offset);
                    if (relocation->type == RELOC_ABS32S) {
                        assert(address <= INT32_MAX);
                    } else {
                        assert(address <= UINT32_MAX);
                    }
                    encoded_address = (uint32_t)address;
                    memcpy(patch, &encoded_address, sizeof(encoded_address));
                    break;
                }
                default:
                    assert(!"unexpected relocation in variadic execution test");
            }
        }
    }
}

int main(int argc, char** argv)
{
    ObjectFile* object;
    ObjSection* section;
    ObjSymbol* entry_symbol;
    uint8_t* mapping;
    size_t* section_offsets;
    size_t mapping_size;
    size_t image_size = 0u;
    int section_index;
    long page_size;
    EntryFunction entry;
    int result;

    assert(argc == 2);
    object = objfile_read(argv[1]);
    assert(object != NULL);
    entry_symbol = objfile_find_symbol(object, "main");
    assert(entry_symbol != NULL && entry_symbol->section >= 0 &&
           entry_symbol->section < object->section_count);
    section_offsets = (size_t*)malloc((size_t)object->section_count *
                                      sizeof(*section_offsets));
    assert(section_offsets != NULL);
    for (section_index = 0; section_index < object->section_count;
         ++section_index) {
        section_offsets[section_index] = SIZE_MAX;
    }
    section_index = 0;
    for (section = object->sections; section; section = section->next,
         ++section_index) {
        size_t alignment;
        size_t section_size;
        if ((section->flags & SECT_FLAG_ALLOC) == 0u) continue;
        alignment = section->align ? section->align : 1u;
        image_size = align_up(image_size, alignment);
        section_offsets[section_index] = image_size;
        section_size = (size_t)(section->memory_size > section->size
            ? section->memory_size : section->size);
        assert(section_size <= SIZE_MAX - image_size);
        image_size += section_size;
    }
    assert(section_index == object->section_count);
    assert(section_offsets[entry_symbol->section] != SIZE_MAX);
    assert(entry_symbol->value < SIZE_MAX -
           section_offsets[entry_symbol->section]);
    page_size =
#if defined(_WIN32)
        4096;
#else
        sysconf(_SC_PAGESIZE);
#endif
    assert(page_size > 0);
    mapping_size = align_up(image_size, (size_t)page_size);

#if defined(_WIN32)
    mapping = VirtualAlloc(NULL, mapping_size, MEM_COMMIT | MEM_RESERVE,
                          PAGE_READWRITE);
    assert(mapping != NULL);
#else
    mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(mapping != MAP_FAILED);
#endif
    section_index = 0;
    for (section = object->sections; section; section = section->next,
         ++section_index) {
        size_t stored_size;
        size_t memory_size;
        if (section_offsets[section_index] == SIZE_MAX) continue;
        stored_size = (size_t)section->size;
        memory_size = (size_t)(section->memory_size > section->size
            ? section->memory_size : section->size);
        if (stored_size != 0u) {
            assert(section->data != NULL);
            memcpy(mapping + section_offsets[section_index], section->data,
                   stored_size);
        }
        if (memory_size > stored_size) {
            memset(mapping + section_offsets[section_index] + stored_size,
                   0, memory_size - stored_size);
        }
    }
    resolve_local_relocations(object, section_offsets, mapping, image_size);
#if defined(_WIN32)
    {
        DWORD old_protection;
        assert(VirtualProtect(mapping, mapping_size, PAGE_EXECUTE_READ,
                              &old_protection));
    }
#else
    assert(mprotect(mapping, mapping_size, PROT_READ | PROT_EXEC) == 0);
#endif
    {
        void* address = mapping + section_offsets[entry_symbol->section] +
            (size_t)entry_symbol->value;
        memcpy(&entry, &address, sizeof(entry));
    }
    result = entry();
#if defined(_WIN32)
    assert(VirtualFree(mapping, 0u, MEM_RELEASE));
#else
    assert(munmap(mapping, mapping_size) == 0);
#endif
    free(section_offsets);
    objfile_free(object);
    if (result != 0) {
        fprintf(stderr, "verified variadic floating-point entry returned %d\n",
                result);
        return 1;
    }
    puts("verified variadic floating-point object execution passed");
    return 0;
}
