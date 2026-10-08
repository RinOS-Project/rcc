#include "objfile.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#if (defined(_WIN32) && (defined(_M_X64) || defined(_M_IX86) || \
                         defined(__x86_64__) || defined(__i386__))) || \
    defined(__x86_64__) || defined(__i386__)
#define RCC_OPTIMIZER_NATIVE_X86 1

static size_t execution_page_size(void)
{
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (size_t)info.dwPageSize;
#else
    long size = sysconf(_SC_PAGESIZE);
    return size > 0 ? (size_t)size : 0u;
#endif
}

static void* allocate_execution_memory(size_t size)
{
#if defined(_WIN32)
    return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* memory = mmap(NULL, size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return memory == MAP_FAILED ? NULL : memory;
#endif
}

static bool protect_execution_memory(void* memory, size_t size)
{
#if defined(_WIN32)
    DWORD old_protection;
    return VirtualProtect(memory, size, PAGE_EXECUTE_READ,
                          &old_protection) != 0;
#else
    return mprotect(memory, size, PROT_READ | PROT_EXEC) == 0;
#endif
}

static bool release_execution_memory(void* memory, size_t size)
{
#if defined(_WIN32)
    (void)size;
    return VirtualFree(memory, 0u, MEM_RELEASE) != 0;
#else
    return munmap(memory, size) == 0;
#endif
}
#endif

static ObjSection* code_section(ObjectFile* object)
{
    for (ObjSection* section = object->sections; section;
         section = section->next) {
        if (section->type == SECT_CODE) return section;
    }
    return NULL;
}

static uint64_t function_extent(ObjectFile* object, const char* name)
{
    ObjSection* code = code_section(object);
    ObjSymbol* function = objfile_find_symbol(object, name);
    uint64_t end;
    assert(code != NULL && function != NULL);
    end = code->size;
    for (ObjSymbol* symbol = object->symbols; symbol;
         symbol = symbol->next) {
        if (symbol->binding == BIND_CODE && symbol->section == function->section &&
            symbol->value > function->value && symbol->value < end &&
            strstr(symbol->name, "__rcc_label_") == NULL) {
            end = symbol->value;
        }
    }
    assert(end >= function->value);
    return end - function->value;
}

static size_t function_local_label_count(ObjectFile* object,
                                        const char* name)
{
    ObjSection* code = code_section(object);
    ObjSymbol* function = objfile_find_symbol(object, name);
    uint64_t end;
    size_t count = 0u;
    assert(code != NULL && function != NULL);
    end = function->value + function_extent(object, name);
    for (ObjSymbol* symbol = object->symbols; symbol; symbol = symbol->next) {
        if (symbol->binding == BIND_CODE &&
            symbol->section == function->section &&
            symbol->value > function->value && symbol->value < end &&
            strstr(symbol->name, "__rcc_label_") != NULL) {
            ++count;
        }
    }
    return count;
}

static bool is_bounded_loop_function(const char* name)
{
    return strncmp(name, "cxx_loop_", 9u) == 0 ||
        strcmp(name, "cxx_while_constant_sizeof") == 0 ||
        strcmp(name, "cxx_for_constant_alignof") == 0 ||
        strcmp(name, "cxx_for_constant_sizeof_bound") == 0;
}

static void verify_pair(const char* unoptimized_path,
                        const char* optimized_path, uint16_t architecture)
{
    static const char* const names[] = {
        "cxx_loop_for_two", "cxx_loop_while_two", "cxx_loop_do_two",
        "cxx_loop_for_eight", "cxx_loop_while_eight", "cxx_loop_do_eight",
        "cxx_loop_for_nine",
        "cxx_switch_constant_direct", "cxx_switch_constant_fallthrough",
        "cxx_switch_constant_no_match", "cxx_switch_constant_sizeof",
        "cxx_switch_constant_alignof", "cxx_if_constant_sizeof",
        "cxx_while_constant_sizeof", "cxx_for_constant_alignof",
        "cxx_for_constant_sizeof_bound", "cxx_expression_constant_alignof"
    };
    static const int expected[] = {
        146, 158, 166, 584, 632, 664, 801, 22, 8, 17, 71, 77, 83, 101,
        107, 109, 256
    };
    (void)expected;
    ObjectFile* unoptimized = objfile_read(unoptimized_path);
    ObjectFile* optimized = objfile_read(optimized_path);
    assert(unoptimized != NULL && optimized != NULL);
    assert(unoptimized->arch == architecture && optimized->arch == architecture);
    for (size_t index = 0u; index < sizeof(names) / sizeof(names[0]); ++index) {
        uint64_t unoptimized_extent;
        uint64_t optimized_extent;
        size_t unoptimized_labels;
        size_t optimized_labels;
        assert(function_extent(optimized, names[index]) > 0u);
        unoptimized_extent = function_extent(unoptimized, names[index]);
        optimized_extent = function_extent(optimized, names[index]);
        unoptimized_labels = function_local_label_count(
            unoptimized, names[index]);
        optimized_labels = function_local_label_count(
            optimized, names[index]);
        if (strcmp(names[index], "cxx_loop_for_nine") == 0) {
            if (optimized_labels != unoptimized_labels) {
                fprintf(stderr,
                        "%s changed loop labels: O0=%lu O1=%lu\n",
                        names[index], (unsigned long)unoptimized_labels,
                        (unsigned long)optimized_labels);
                assert(optimized_labels == unoptimized_labels);
            }
        } else if (is_bounded_loop_function(names[index])) {
            if (optimized_labels >= unoptimized_labels) {
                fprintf(stderr,
                        "%s retained loop labels: O0=%llu bytes/%lu labels, "
                        "O1=%llu bytes/%lu labels\n",
                        names[index], (unsigned long long)unoptimized_extent,
                        (unsigned long)unoptimized_labels,
                        (unsigned long long)optimized_extent,
                        (unsigned long)optimized_labels);
                assert(optimized_labels < unoptimized_labels);
            }
        } else {
            if (optimized_extent == unoptimized_extent) {
                fprintf(stderr,
                        "%s did not optimize: O0=O1=%llu\n", names[index],
                        (unsigned long long)optimized_extent);
                assert(optimized_extent != unoptimized_extent);
            }
        }
    }

#if defined(RCC_OPTIMIZER_NATIVE_X86)
#if defined(__i386__) || defined(_M_IX86)
    if (architecture == ARCH_X86)
#else
    if (architecture == ARCH_X64)
#endif
    {
        ObjSection* code = code_section(optimized);
        size_t page_size = execution_page_size();
        size_t mapping_size;
        uint8_t* mapping;
        assert(code != NULL && page_size > 0u && code->size > 0u);
        mapping_size = (((size_t)code->size + page_size - 1u) /
                        page_size) * page_size;
        mapping = allocate_execution_memory(mapping_size);
        assert(mapping != NULL);
        memcpy(mapping, code->data, (size_t)code->size);
        assert(protect_execution_memory(mapping, mapping_size));
        for (size_t index = 0u; index < sizeof(names) / sizeof(names[0]);
             ++index) {
            ObjSymbol* symbol = objfile_find_symbol(optimized, names[index]);
            int (*function)(void);
            void* address;
            assert(symbol != NULL);
            address = mapping + symbol->value;
            memcpy(&function, &address, sizeof(function));
            assert(function() == expected[index]);
        }
        assert(release_execution_memory(mapping, mapping_size));
    }
#endif
    objfile_free(unoptimized);
    objfile_free(optimized);
}

int main(int argc, char** argv)
{
    assert(argc == 5);
    verify_pair(argv[1], argv[2], ARCH_X86);
    verify_pair(argv[3], argv[4], ARCH_X64);
    return 0;
}
