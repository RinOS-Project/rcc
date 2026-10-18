#define _GNU_SOURCE

#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(_WIN64)
#define RINOS_ABI __attribute__((sysv_abi))
#else
#define RINOS_ABI
#endif

typedef int RINOS_ABI (*EntryFunction)(void);

int main(int argc, char** argv)
{
    ObjectFile* object;
    ObjSection* text;
    ObjSymbol* entry_symbol;
    size_t page_size;
    size_t mapping_size;
    void* memory;
    void* entry_address;
    EntryFunction entry;
#ifdef _WIN32
    SYSTEM_INFO system_info;
    DWORD previous;
#else
    long host_page_size;
#endif

    assert(argc == 2);
    object = objfile_read(argv[1]);
    assert(object != NULL && object->arch == ARCH_X64);
    text = objfile_get_section(object, ".text");
    entry_symbol = objfile_find_symbol(object, "main");
    assert(text != NULL && text->data != NULL && text->size != 0u &&
           entry_symbol != NULL && entry_symbol->section == 0);
#ifdef _WIN32
    GetSystemInfo(&system_info);
    page_size = (size_t)system_info.dwPageSize;
#else
    host_page_size = sysconf(_SC_PAGESIZE);
    page_size = host_page_size > 0 ? (size_t)host_page_size : 0u;
#endif
    assert(page_size != 0u && text->size <= SIZE_MAX - page_size + 1u);
    mapping_size = ((size_t)text->size + page_size - 1u) & ~(page_size - 1u);
#ifdef _WIN32
    memory = VirtualAlloc(NULL, mapping_size, MEM_RESERVE | MEM_COMMIT,
                          PAGE_READWRITE);
#else
    memory = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) memory = NULL;
#endif
    assert(memory != NULL);
    memcpy(memory, text->data, (size_t)text->size);
    for (ObjReloc* relocation = text->relocs; relocation;
         relocation = relocation->next) {
        ObjSymbol* symbol = objfile_find_symbol(
            object, relocation->symbol_name);
        uint8_t* place;
        int64_t target;
        int64_t delta;
        int32_t encoded;
        assert(relocation->type == RELOC_REL32 && symbol != NULL &&
               symbol->section == 0 &&
               relocation->offset <= text->size &&
               sizeof(encoded) <= text->size - relocation->offset);
        place = (uint8_t*)memory + relocation->offset;
        target = (int64_t)(uintptr_t)memory +
            (int64_t)symbol->value + relocation->addend;
        delta = target -
            (int64_t)(uintptr_t)(place + sizeof(encoded));
        encoded = (int32_t)delta;
        assert((int64_t)encoded == delta);
        memcpy(place, &encoded, sizeof(encoded));
    }
#ifdef _WIN32
    assert(VirtualProtect(memory, mapping_size, PAGE_EXECUTE_READ, &previous));
#else
    assert(mprotect(memory, mapping_size, PROT_READ | PROT_EXEC) == 0);
#endif
    entry_address = (uint8_t*)memory + entry_symbol->value;
    memcpy(&entry, &entry_address, sizeof(entry));
    assert(entry() == 0);
#ifdef _WIN32
    assert(VirtualFree(memory, 0, MEM_RELEASE));
#else
    assert(munmap(memory, mapping_size) == 0);
#endif
    objfile_free(object);
    puts("Verified backend goto/label execution passed");
    return 0;
}
