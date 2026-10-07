#define _GNU_SOURCE

#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define RCC_TEST_SYSV __attribute__((sysv_abi))
#else
#include <sys/mman.h>
#include <unistd.h>
#define RCC_TEST_SYSV
#endif

static ObjSection* section_at(ObjectFile* object, int index)
{
    ObjSection* section = object->sections;
    while (section && index-- > 0) section = section->next;
    return section;
}

static void verify_alignment(ObjectFile* object, uint16_t architecture)
{
    ObjSection* data = objfile_get_section(object, ".data");
    ObjSection* bss = objfile_get_section(object, ".bss");
    ObjSymbol* aligned_global = objfile_find_symbol(object, "aligned_global");
    ObjSymbol* aligned_type_global =
        objfile_find_symbol(object, "aligned_type_global");
    ObjSymbol* entry = objfile_find_symbol(object, "main");
    ObjSection* global_section;
    ObjSection* type_section;

    assert(object->arch == architecture);
    assert((data != NULL && data->type == SECT_DATA) ||
           (bss != NULL && bss->type == SECT_BSS));
    assert(aligned_global != NULL && aligned_global->section >= 0);
    assert(aligned_type_global != NULL && aligned_type_global->section >= 0);
    assert(entry != NULL && entry->section >= 0);
    global_section = section_at(object, aligned_global->section);
    type_section = section_at(object, aligned_type_global->section);
    assert(global_section != NULL && type_section == global_section);
    assert(global_section->type == SECT_DATA || global_section->type == SECT_BSS);
    assert(global_section->align >= 16u);
    assert(aligned_global->value % 16u == 0u);
    assert(aligned_type_global->value % 8u == 0u);
}

static void verify_verified_function(const char* path, const char* name)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* symbol;
    size_t page_size;
    size_t mapping_size;
    void* memory;
    int (RCC_TEST_SYSV *function)(void);

    assert(object != NULL && object->arch == ARCH_X64);
    text = objfile_get_section(object, ".text");
    symbol = objfile_find_symbol(object, name);
    assert(text != NULL && symbol != NULL && symbol->section >= 0);
    assert((uint64_t)symbol->value <= text->size);
    assert(symbol->size <= text->size - symbol->value);
    for (ObjReloc* relocation = text->relocs; relocation;
         relocation = relocation->next) {
        assert(relocation->offset < symbol->value ||
               relocation->offset >= symbol->value + symbol->size);
    }
#ifdef _WIN32
    {
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        page_size = (size_t)system_info.dwPageSize;
    }
#else
    {
        long page = sysconf(_SC_PAGESIZE);
        assert(page > 0);
        page_size = (size_t)page;
    }
#endif
    mapping_size = ((size_t)text->size + page_size - 1u) / page_size *
        page_size;
#ifdef _WIN32
    memory = VirtualAlloc(NULL, mapping_size, MEM_RESERVE | MEM_COMMIT,
                          PAGE_READWRITE);
    assert(memory != NULL);
#else
    memory = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(memory != MAP_FAILED);
#endif
    memcpy(memory, text->data, (size_t)text->size);
#ifdef _WIN32
    {
        DWORD previous;
        assert(VirtualProtect(memory, mapping_size, PAGE_EXECUTE_READ,
                              &previous));
    }
#else
    assert(mprotect(memory, mapping_size, PROT_READ | PROT_EXEC) == 0);
#endif
    {
        void* address = (uint8_t*)memory + (size_t)symbol->value;
        memcpy(&function, &address, sizeof(function));
    }
    assert(function() == 0);
#ifdef _WIN32
    assert(VirtualFree(memory, 0, MEM_RELEASE));
#else
    assert(munmap(memory, mapping_size) == 0);
#endif
    objfile_free(object);
}

int main(int argc, char** argv)
{
    ObjectFile* x86;
    ObjectFile* x64;

    assert(argc == 4);
    x86 = objfile_read(argv[1]);
    x64 = objfile_read(argv[2]);
    assert(x86 != NULL && x64 != NULL);
    verify_alignment(x86, ARCH_X86);
    verify_alignment(x64, ARCH_X64);
    verify_verified_function(argv[3], "verified_local_alignment");
    objfile_free(x86);
    objfile_free(x64);
    puts("i686/x64 _Alignas object and verified-local execution tests passed");
    return 0;
}
