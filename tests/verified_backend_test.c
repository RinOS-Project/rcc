#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "objfile.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

struct VerifiedSysvMixedAggregate {
    int integer;
    double floating;
};

struct VerifiedSysvIntegerAggregate {
    int first;
    int second;
};

struct VerifiedSysvVaLargeAggregate {
    long long first;
    long long second;
    long long third;
};

struct VerifiedSysvVaAlignedLargeAggregate {
    _Alignas(16) long long first;
    long long second;
    long long third;
    long long fourth;
};

static size_t verified_page_size(void)
{
#ifdef _WIN32
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return (size_t)system_info.dwPageSize;
#else
    long page = sysconf(_SC_PAGESIZE);
    return page > 0 ? (size_t)page : 0u;
#endif
}

static void* verified_map(size_t size)
{
#ifdef _WIN32
    return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT,
                        PAGE_READWRITE);
#else
    void* memory = mmap(NULL, size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return memory == MAP_FAILED ? NULL : memory;
#endif
}

static int verified_protect(void* memory, size_t size,
                            int writable, int executable)
{
#ifdef _WIN32
    DWORD protection;
    DWORD previous;
    if (executable) {
        protection = writable ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
    } else {
        protection = writable ? PAGE_READWRITE : PAGE_READONLY;
    }
    return VirtualProtect(memory, size, protection, &previous) ? 0 : -1;
#else
    int protection = PROT_READ;
    if (writable) protection |= PROT_WRITE;
    if (executable) protection |= PROT_EXEC;
    return mprotect(memory, size, protection);
#endif
}

static int verified_unmap(void* memory, size_t size)
{
#ifdef _WIN32
    (void)size;
    return VirtualFree(memory, 0, MEM_RELEASE) ? 0 : -1;
#else
    return munmap(memory, size);
#endif
}

struct VerifiedPair {
    int first;
    int second;
    int* pointer;
};

struct VerifiedArgument {
    int first;
    int second;
    int third;
};

struct VerifiedReturnPair {
    int first;
    int second;
};

struct VerifiedLargeReturn {
    int first;
    int second;
    int third;
    int fourth;
    int fifth;
    int sixth;
};

static void* map_text(ObjectFile* object, const ObjSection* text,
                      size_t* mapping_size)
{
    size_t page = verified_page_size();
    size_t text_size;
    size_t rodata_offset;
    size_t rodata_size = 0u;
    size_t size;
    int rodata_index = 0;
    ObjSection* rodata;
    void* memory;
    assert(text != NULL && text->size != 0u && page != 0u);
    assert(text->size <= (uint64_t)SIZE_MAX);
    text_size = (size_t)text->size;
    rodata_offset = text_size;
    rodata = objfile_get_section(object, ".rodata");
    if (rodata && rodata->size != 0u) {
        size_t alignment = rodata->align ? rodata->align : 1u;
        size_t remainder;
        assert(rodata->size <= (uint64_t)SIZE_MAX);
        assert((alignment & (alignment - 1u)) == 0u);
        remainder = text_size & (alignment - 1u);
        assert(text_size <= SIZE_MAX - remainder);
        rodata_offset = remainder ?
            text_size + alignment - remainder : text_size;
        rodata_size = (size_t)rodata->size;
        assert(rodata_offset <= SIZE_MAX - rodata_size);
        for (ObjSection* section = object->sections; section;
             section = section->next) {
            if (section == rodata) break;
            ++rodata_index;
        }
        assert(rodata_index < object->section_count);
    }
    assert(rodata_offset + rodata_size <= SIZE_MAX - ((size_t)page - 1u));
    size = ((rodata_offset + rodata_size + (size_t)page - 1u) / page) * page;
    memory = verified_map(size);
    assert(memory != NULL);
    memcpy(memory, text->data, text_size);
    if (rodata_size) memcpy((uint8_t*)memory + rodata_offset,
                            rodata->data, rodata_size);
    for (ObjReloc* relocation = text->relocs; relocation;
         relocation = relocation->next) {
        ObjSymbol* symbol = objfile_find_symbol(
            object, relocation->symbol_name);
        uint8_t* place;
        size_t target_offset;
        int64_t target;
        int64_t delta;
        int32_t encoded;
        assert(symbol != NULL &&
               relocation->offset <= text->size);
        if (symbol->section == 0) {
            target_offset = (size_t)symbol->value;
        } else {
            assert(symbol->section == rodata_index && rodata_size != 0u &&
                   symbol->value <= rodata_size);
            target_offset = rodata_offset + (size_t)symbol->value;
        }
        place = (uint8_t*)memory + relocation->offset;
        target = (int64_t)(uintptr_t)memory + (int64_t)target_offset +
            relocation->addend;
        if (relocation->type == RELOC_REL32) {
            assert(sizeof(encoded) <= text->size - relocation->offset);
            delta = target - (int64_t)(uintptr_t)(place + sizeof(encoded));
            encoded = (int32_t)delta;
            assert((int64_t)encoded == delta);
            memcpy(place, &encoded, sizeof(encoded));
        } else if (relocation->type == RELOC_ABS32U) {
            uint32_t absolute = (uint32_t)target;
            assert(sizeof(absolute) <= text->size - relocation->offset &&
                   (uint64_t)absolute == (uint64_t)target);
            memcpy(place, &absolute, sizeof(absolute));
        } else {
            uint64_t absolute = (uint64_t)target;
            assert(relocation->type == RELOC_ABS64 &&
                   sizeof(absolute) <= text->size - relocation->offset);
            memcpy(place, &absolute, sizeof(absolute));
        }
    }
    assert(verified_protect(memory, size, 0, 1) == 0);
    *mapping_size = size;
    return memory;
}

static void* symbol_address(void* text_memory, const ObjSymbol* symbol)
{
    assert(text_memory != NULL && symbol != NULL && symbol->section == 0);
    return (uint8_t*)text_memory + symbol->value;
}

static bool symbol_has_sysv_variadic_call_setup(
    const ObjectFile* object, const ObjSection* text,
    const ObjSymbol* symbol, uint8_t vector_argument_count)
{
    const ObjSymbol* candidate;
    uint64_t end;
    if (!object || !text || !symbol || symbol->section != 0 ||
        symbol->value >= text->size || text->size > SIZE_MAX) return false;
    end = text->size;
    for (candidate = object->symbols; candidate; candidate = candidate->next) {
        if (candidate->section == symbol->section &&
            candidate->value > symbol->value && candidate->value < end) {
            end = candidate->value;
        }
    }
    for (uint64_t offset = symbol->value;
         offset <= end && end - offset >= 3u; ++offset) {
        if (text->data[offset] == 0xb0u &&
            text->data[offset + 1u] == vector_argument_count &&
            text->data[offset + 2u] == 0xe8u) return true;
    }
    return false;
}

static void verify_va_list_pointer_cxx_object(
    const char* path, uint16_t arch, bool execute)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* symbol;
    assert(object != NULL && object->arch == arch);
    text = objfile_get_section(object, ".text");
    symbol = objfile_find_symbol(
        object, "verified_cpp_va_list_pointer_forward_call");
    assert(symbol != NULL && symbol->type == SYM_GLOBAL &&
           symbol->binding == BIND_CODE && symbol->section == 0);
    if (execute) {
        size_t mapping_size;
        void* memory;
        void* address;
        int (RINOS_ABI *function)(void);
        assert(arch == ARCH_X64 && sizeof(void*) == 8u);
        memory = map_text(object, text, &mapping_size);
        address = symbol_address(memory, symbol);
        memcpy(&function, &address, sizeof(function));
        assert(function() == 40);
        assert(verified_unmap(memory, mapping_size) == 0);
    }
    objfile_free(object);
}

static void verify_sysv_va_fp_object(const char* path, bool execute)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* first_symbol;
    ObjSymbol* second_symbol;
    ObjSymbol* ninth_symbol;
    ObjSymbol* named_first_symbol;
    ObjSymbol* named_mixed_symbol;
    ObjSymbol* named_float_symbol;
    ObjSymbol* named_ninth_symbol;
    ObjSymbol* named_mixed_stack_symbol;
    ObjSymbol* fp_call_double_symbol;
    ObjSymbol* fp_call_mixed_symbol;
    ObjSymbol* fp_call_ninth_symbol;
    ObjSymbol* fp_call_mixed_stack_symbol;
    ObjSymbol* fp_call_variadic_double_symbol;
    ObjSymbol* fp_call_variadic_float_symbol;
    ObjSymbol* fp_call_variadic_ninth_float_symbol;
    ObjSymbol* fp_call_variadic_int_symbol;
    ObjSymbol* fp_call_variadic_ninth_symbol;
    ObjSymbol* fp_call_variadic_named_double_symbol;
    ObjSymbol* fp_call_mixed_variadic_overflow_symbol;
    ObjSymbol* va_mixed_aggregate_integer_symbol;
    ObjSymbol* va_mixed_aggregate_floating_symbol;
    ObjSymbol* va_integer_aggregate_overflow_symbol;
    ObjSymbol* va_mixed_aggregate_overflow_symbol;
    ObjSymbol* va_mixed_aggregate_sse_overflow_symbol;
    ObjSymbol* va_large_aggregate_symbol;
    ObjSymbol* va_aligned_large_after_stack_symbol;
    assert(object != NULL && object->arch == ARCH_X64);
    text = objfile_get_section(object, ".text");
    first_symbol = objfile_find_symbol(
        object, "verified_sysv_va_double_first");
    second_symbol = objfile_find_symbol(
        object, "verified_sysv_va_double_second");
    ninth_symbol = objfile_find_symbol(
        object, "verified_sysv_va_double_ninth");
    named_first_symbol = objfile_find_symbol(
        object, "verified_sysv_named_double_first");
    named_mixed_symbol = objfile_find_symbol(
        object, "verified_sysv_named_double_after_int");
    named_float_symbol = objfile_find_symbol(
        object, "verified_sysv_named_float_first");
    named_ninth_symbol = objfile_find_symbol(
        object, "verified_sysv_named_double_ninth");
    named_mixed_stack_symbol = objfile_find_symbol(
        object, "verified_sysv_named_mixed_stack");
    fp_call_double_symbol = objfile_find_symbol(
        object, "verified_sysv_fp_call_double");
    fp_call_mixed_symbol = objfile_find_symbol(
        object, "verified_sysv_fp_call_mixed");
    fp_call_ninth_symbol = objfile_find_symbol(
        object, "verified_sysv_fp_call_ninth");
    fp_call_mixed_stack_symbol = objfile_find_symbol(
        object, "verified_sysv_fp_call_mixed_stack");
    fp_call_variadic_double_symbol = objfile_find_symbol(
        object, "verified_sysv_fp_call_variadic_double");
    fp_call_variadic_float_symbol = objfile_find_symbol(
        object, "verified_sysv_fp_call_variadic_float");
    fp_call_variadic_ninth_float_symbol = objfile_find_symbol(
        object, "verified_sysv_fp_call_variadic_ninth_float");
    fp_call_variadic_int_symbol = objfile_find_symbol(
        object, "verified_sysv_fp_call_variadic_int");
    fp_call_variadic_ninth_symbol = objfile_find_symbol(
        object, "verified_sysv_fp_call_variadic_ninth");
    fp_call_variadic_named_double_symbol = objfile_find_symbol(
        object, "verified_sysv_fp_call_variadic_named_double");
    fp_call_mixed_variadic_overflow_symbol = objfile_find_symbol(
        object, "verified_sysv_fp_call_mixed_variadic_overflow");
    va_mixed_aggregate_integer_symbol = objfile_find_symbol(
        object, "verified_sysv_va_mixed_aggregate_integer");
    va_mixed_aggregate_floating_symbol = objfile_find_symbol(
        object, "verified_sysv_va_mixed_aggregate_floating");
    va_integer_aggregate_overflow_symbol = objfile_find_symbol(
        object, "verified_sysv_va_integer_aggregate_overflow");
    va_mixed_aggregate_overflow_symbol = objfile_find_symbol(
        object, "verified_sysv_va_mixed_aggregate_overflow");
    va_mixed_aggregate_sse_overflow_symbol = objfile_find_symbol(
        object, "verified_sysv_va_mixed_aggregate_sse_overflow");
    va_large_aggregate_symbol = objfile_find_symbol(
        object, "verified_sysv_va_large_aggregate");
    va_aligned_large_after_stack_symbol = objfile_find_symbol(
        object, "verified_sysv_va_aligned_large_after_stack");
    assert(first_symbol != NULL && first_symbol->type == SYM_GLOBAL &&
           first_symbol->binding == BIND_CODE && first_symbol->section == 0);
    assert(second_symbol != NULL && second_symbol->type == SYM_GLOBAL &&
           second_symbol->binding == BIND_CODE && second_symbol->section == 0);
    assert(ninth_symbol != NULL && ninth_symbol->type == SYM_GLOBAL &&
           ninth_symbol->binding == BIND_CODE && ninth_symbol->section == 0);
    assert(named_first_symbol != NULL &&
           named_first_symbol->type == SYM_GLOBAL &&
           named_first_symbol->binding == BIND_CODE &&
           named_first_symbol->section == 0);
    assert(named_mixed_symbol != NULL &&
           named_mixed_symbol->type == SYM_GLOBAL &&
           named_mixed_symbol->binding == BIND_CODE &&
           named_mixed_symbol->section == 0);
    assert(named_float_symbol != NULL &&
           named_float_symbol->type == SYM_GLOBAL &&
           named_float_symbol->binding == BIND_CODE &&
           named_float_symbol->section == 0);
    assert(named_ninth_symbol != NULL &&
           named_ninth_symbol->type == SYM_GLOBAL &&
           named_ninth_symbol->binding == BIND_CODE &&
           named_ninth_symbol->section == 0);
    assert(named_mixed_stack_symbol != NULL &&
           named_mixed_stack_symbol->type == SYM_GLOBAL &&
           named_mixed_stack_symbol->binding == BIND_CODE &&
           named_mixed_stack_symbol->section == 0);
    assert(fp_call_double_symbol != NULL &&
           fp_call_double_symbol->type == SYM_GLOBAL &&
           fp_call_double_symbol->binding == BIND_CODE &&
           fp_call_double_symbol->section == 0);
    assert(fp_call_mixed_symbol != NULL &&
           fp_call_mixed_symbol->type == SYM_GLOBAL &&
           fp_call_mixed_symbol->binding == BIND_CODE &&
           fp_call_mixed_symbol->section == 0);
    assert(fp_call_ninth_symbol != NULL &&
           fp_call_ninth_symbol->type == SYM_GLOBAL &&
           fp_call_ninth_symbol->binding == BIND_CODE &&
           fp_call_ninth_symbol->section == 0);
    assert(fp_call_mixed_stack_symbol != NULL &&
           fp_call_mixed_stack_symbol->type == SYM_GLOBAL &&
           fp_call_mixed_stack_symbol->binding == BIND_CODE &&
           fp_call_mixed_stack_symbol->section == 0);
    assert(fp_call_variadic_double_symbol != NULL &&
           fp_call_variadic_double_symbol->type == SYM_GLOBAL &&
           fp_call_variadic_double_symbol->binding == BIND_CODE &&
           fp_call_variadic_double_symbol->section == 0);
    assert(fp_call_variadic_float_symbol != NULL &&
           fp_call_variadic_float_symbol->type == SYM_GLOBAL &&
           fp_call_variadic_float_symbol->binding == BIND_CODE &&
           fp_call_variadic_float_symbol->section == 0);
    assert(fp_call_variadic_ninth_float_symbol != NULL &&
           fp_call_variadic_ninth_float_symbol->type == SYM_GLOBAL &&
           fp_call_variadic_ninth_float_symbol->binding == BIND_CODE &&
           fp_call_variadic_ninth_float_symbol->section == 0);
    assert(fp_call_variadic_int_symbol != NULL &&
           fp_call_variadic_int_symbol->type == SYM_GLOBAL &&
           fp_call_variadic_int_symbol->binding == BIND_CODE &&
           fp_call_variadic_int_symbol->section == 0);
    assert(fp_call_variadic_ninth_symbol != NULL &&
           fp_call_variadic_ninth_symbol->type == SYM_GLOBAL &&
           fp_call_variadic_ninth_symbol->binding == BIND_CODE &&
           fp_call_variadic_ninth_symbol->section == 0);
    assert(fp_call_variadic_named_double_symbol != NULL &&
           fp_call_variadic_named_double_symbol->type == SYM_GLOBAL &&
           fp_call_variadic_named_double_symbol->binding == BIND_CODE &&
           fp_call_variadic_named_double_symbol->section == 0);
    assert(fp_call_mixed_variadic_overflow_symbol != NULL &&
           fp_call_mixed_variadic_overflow_symbol->type == SYM_GLOBAL &&
           fp_call_mixed_variadic_overflow_symbol->binding == BIND_CODE &&
           fp_call_mixed_variadic_overflow_symbol->section == 0);
    assert(va_mixed_aggregate_integer_symbol != NULL &&
           va_mixed_aggregate_integer_symbol->type == SYM_GLOBAL &&
           va_mixed_aggregate_integer_symbol->binding == BIND_CODE &&
           va_mixed_aggregate_integer_symbol->section == 0);
    assert(va_mixed_aggregate_floating_symbol != NULL &&
           va_mixed_aggregate_floating_symbol->type == SYM_GLOBAL &&
           va_mixed_aggregate_floating_symbol->binding == BIND_CODE &&
           va_mixed_aggregate_floating_symbol->section == 0);
    assert(va_integer_aggregate_overflow_symbol != NULL &&
           va_integer_aggregate_overflow_symbol->type == SYM_GLOBAL &&
           va_integer_aggregate_overflow_symbol->binding == BIND_CODE &&
           va_integer_aggregate_overflow_symbol->section == 0);
    assert(va_mixed_aggregate_overflow_symbol != NULL &&
           va_mixed_aggregate_overflow_symbol->type == SYM_GLOBAL &&
           va_mixed_aggregate_overflow_symbol->binding == BIND_CODE &&
           va_mixed_aggregate_overflow_symbol->section == 0);
    assert(va_mixed_aggregate_sse_overflow_symbol != NULL &&
           va_mixed_aggregate_sse_overflow_symbol->type == SYM_GLOBAL &&
           va_mixed_aggregate_sse_overflow_symbol->binding == BIND_CODE &&
           va_mixed_aggregate_sse_overflow_symbol->section == 0);
    assert(va_large_aggregate_symbol != NULL &&
           va_large_aggregate_symbol->type == SYM_GLOBAL &&
           va_large_aggregate_symbol->binding == BIND_CODE &&
           va_large_aggregate_symbol->section == 0);
    assert(va_aligned_large_after_stack_symbol != NULL &&
           va_aligned_large_after_stack_symbol->type == SYM_GLOBAL &&
           va_aligned_large_after_stack_symbol->binding == BIND_CODE &&
           va_aligned_large_after_stack_symbol->section == 0);
    assert(symbol_has_sysv_variadic_call_setup(
        object, text, fp_call_variadic_double_symbol, 1u));
    assert(symbol_has_sysv_variadic_call_setup(
        object, text, fp_call_variadic_float_symbol, 1u));
    assert(symbol_has_sysv_variadic_call_setup(
        object, text, fp_call_variadic_ninth_float_symbol, 8u));
    assert(symbol_has_sysv_variadic_call_setup(
        object, text, fp_call_variadic_int_symbol, 0u));
    assert(symbol_has_sysv_variadic_call_setup(
        object, text, fp_call_variadic_ninth_symbol, 8u));
    assert(symbol_has_sysv_variadic_call_setup(
        object, text, fp_call_variadic_named_double_symbol, 2u));
    assert(symbol_has_sysv_variadic_call_setup(
        object, text, fp_call_mixed_variadic_overflow_symbol, 8u));
    if (execute) {
        size_t mapping_size;
        void* memory = map_text(object, text, &mapping_size);
        void* address = symbol_address(memory, first_symbol);
        double (RINOS_ABI *first)(int, ...);
        double (RINOS_ABI *second)(int, ...);
        double (RINOS_ABI *ninth)(int, ...);
        double (RINOS_ABI *named_first)(double);
        double (RINOS_ABI *named_after_int)(int, double);
        float (RINOS_ABI *named_float)(float);
        double (RINOS_ABI *named_ninth)(
            double, double, double, double, double,
            double, double, double, double);
        double (RINOS_ABI *named_mixed_stack)(
            int, int, int, int, int, int, int,
            double, double, double, double, double,
            double, double, double, double);
        double (RINOS_ABI *fp_call_double)(double);
        double (RINOS_ABI *fp_call_mixed)(int, double);
        double (RINOS_ABI *fp_call_ninth)(
            double, double, double, double, double,
            double, double, double, double);
        double (RINOS_ABI *fp_call_mixed_stack)(
            int, int, int, int, int, int, int,
            double, double, double, double, double,
            double, double, double, double);
        double (RINOS_ABI *fp_call_variadic_double)(double);
        double (RINOS_ABI *fp_call_variadic_float)(float);
        double (RINOS_ABI *fp_call_variadic_ninth_float)(
            float, float, float, float, float, float,
            float, float, float, float, float, float,
            float, float, float, float, float);
        int (RINOS_ABI *fp_call_variadic_int)(int);
        double (RINOS_ABI *fp_call_variadic_ninth)(
            double, double, double, double, double,
            double, double, double, double);
        double (RINOS_ABI *fp_call_variadic_named_double)(double, double);
        double (RINOS_ABI *fp_call_mixed_variadic_overflow)(
            int, int, int, int, int, int,
            double, double, double, double, double, double, double, double,
            int, double);
        int (RINOS_ABI *va_mixed_aggregate_integer)(int, ...);
        double (RINOS_ABI *va_mixed_aggregate_floating)(int, ...);
        int (RINOS_ABI *va_integer_aggregate_overflow)(
            int, int, int, int, int, int, ...);
        double (RINOS_ABI *va_mixed_aggregate_overflow)(
            int, int, int, int, int, int, ...);
        double (RINOS_ABI *va_mixed_aggregate_sse_overflow)(
            double, double, double, double,
            double, double, double, double, ...);
        long long (RINOS_ABI *va_large_aggregate)(int, ...);
        long long (RINOS_ABI *va_aligned_large_after_stack)(
            int, int, int, int, int, int, ...);
        struct VerifiedSysvMixedAggregate mixed_aggregate = { 19, 4.125 };
        struct VerifiedSysvIntegerAggregate integer_aggregate = { 13, 7 };
        struct VerifiedSysvVaLargeAggregate large_aggregate = { 11, 23, 47 };
        struct VerifiedSysvVaAlignedLargeAggregate aligned_large = {
            3, 5, 7, 71
        };
        memcpy(&first, &address, sizeof(first));
        address = symbol_address(memory, second_symbol);
        memcpy(&second, &address, sizeof(second));
        address = symbol_address(memory, ninth_symbol);
        memcpy(&ninth, &address, sizeof(ninth));
        address = symbol_address(memory, named_first_symbol);
        memcpy(&named_first, &address, sizeof(named_first));
        address = symbol_address(memory, named_mixed_symbol);
        memcpy(&named_after_int, &address, sizeof(named_after_int));
        address = symbol_address(memory, named_float_symbol);
        memcpy(&named_float, &address, sizeof(named_float));
        address = symbol_address(memory, named_ninth_symbol);
        memcpy(&named_ninth, &address, sizeof(named_ninth));
        address = symbol_address(memory, named_mixed_stack_symbol);
        memcpy(&named_mixed_stack, &address, sizeof(named_mixed_stack));
        address = symbol_address(memory, fp_call_double_symbol);
        memcpy(&fp_call_double, &address, sizeof(fp_call_double));
        address = symbol_address(memory, fp_call_mixed_symbol);
        memcpy(&fp_call_mixed, &address, sizeof(fp_call_mixed));
        address = symbol_address(memory, fp_call_ninth_symbol);
        memcpy(&fp_call_ninth, &address, sizeof(fp_call_ninth));
        address = symbol_address(memory, fp_call_mixed_stack_symbol);
        memcpy(&fp_call_mixed_stack, &address, sizeof(fp_call_mixed_stack));
        address = symbol_address(memory, fp_call_variadic_double_symbol);
        memcpy(&fp_call_variadic_double, &address,
               sizeof(fp_call_variadic_double));
        address = symbol_address(memory, fp_call_variadic_float_symbol);
        memcpy(&fp_call_variadic_float, &address,
               sizeof(fp_call_variadic_float));
        address = symbol_address(
            memory, fp_call_variadic_ninth_float_symbol);
        memcpy(&fp_call_variadic_ninth_float, &address,
               sizeof(fp_call_variadic_ninth_float));
        address = symbol_address(memory, fp_call_variadic_int_symbol);
        memcpy(&fp_call_variadic_int, &address,
               sizeof(fp_call_variadic_int));
        address = symbol_address(memory, fp_call_variadic_ninth_symbol);
        memcpy(&fp_call_variadic_ninth, &address,
               sizeof(fp_call_variadic_ninth));
        address = symbol_address(memory, fp_call_variadic_named_double_symbol);
        memcpy(&fp_call_variadic_named_double, &address,
               sizeof(fp_call_variadic_named_double));
        address = symbol_address(memory, fp_call_mixed_variadic_overflow_symbol);
        memcpy(&fp_call_mixed_variadic_overflow, &address,
               sizeof(fp_call_mixed_variadic_overflow));
        address = symbol_address(memory, va_mixed_aggregate_integer_symbol);
        memcpy(&va_mixed_aggregate_integer, &address,
               sizeof(va_mixed_aggregate_integer));
        address = symbol_address(memory, va_mixed_aggregate_floating_symbol);
        memcpy(&va_mixed_aggregate_floating, &address,
               sizeof(va_mixed_aggregate_floating));
        address = symbol_address(memory, va_integer_aggregate_overflow_symbol);
        memcpy(&va_integer_aggregate_overflow, &address,
               sizeof(va_integer_aggregate_overflow));
        address = symbol_address(memory, va_mixed_aggregate_overflow_symbol);
        memcpy(&va_mixed_aggregate_overflow, &address,
               sizeof(va_mixed_aggregate_overflow));
        address = symbol_address(
            memory, va_mixed_aggregate_sse_overflow_symbol);
        memcpy(&va_mixed_aggregate_sse_overflow, &address,
               sizeof(va_mixed_aggregate_sse_overflow));
        address = symbol_address(memory, va_large_aggregate_symbol);
        memcpy(&va_large_aggregate, &address, sizeof(va_large_aggregate));
        address = symbol_address(
            memory, va_aligned_large_after_stack_symbol);
        memcpy(&va_aligned_large_after_stack, &address,
               sizeof(va_aligned_large_after_stack));
        assert(first(7, 3.25) == 3.25);
        assert(second(7, 1.25, 2.5) == 2.5);
        assert(ninth(7, 1.0, 2.0, 3.0, 4.0, 5.0,
                    6.0, 7.0, 8.0, 9.0) == 9.0);
        assert(named_first(3.75) == 3.75);
        assert(named_after_int(17, 6.5) == 6.5);
        assert(named_float(4.5f) == 4.5f);
        assert(named_ninth(1.0, 2.0, 3.0, 4.0, 5.0,
                           6.0, 7.0, 8.0, 9.0) == 9.0);
        assert(named_mixed_stack(
                   1, 2, 3, 4, 5, 6, 7,
                   1.0, 2.0, 3.0, 4.0, 5.0,
                   6.0, 7.0, 8.0, 9.0) == 9.0);
        assert(fp_call_double(12.5) == 12.5);
        assert(fp_call_mixed(23, 14.75) == 14.75);
        assert(fp_call_ninth(
                   1.0, 2.0, 3.0, 4.0, 5.0,
                   6.0, 7.0, 8.0, 9.0) == 9.0);
        assert(fp_call_mixed_stack(
                   1, 2, 3, 4, 5, 6, 7,
                   1.0, 2.0, 3.0, 4.0, 5.0,
                   6.0, 7.0, 8.0, 9.0) == 9.0);
        assert(fp_call_variadic_double(21.25) == 21.25);
        assert(fp_call_variadic_float(5.75f) == 5.75);
        assert(fp_call_variadic_ninth_float(
                   1.0f, 2.0f, 3.0f, 4.0f, 5.0f,
                   6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f,
                   12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f) == 9.0);
        assert(fp_call_variadic_int(52) == 52);
        assert(fp_call_variadic_ninth(
                   1.0, 2.0, 3.0, 4.0, 5.0,
                   6.0, 7.0, 8.0, 9.0) == 9.0);
        assert(fp_call_variadic_named_double(7.5, 23.125) == 23.125);
        assert(fp_call_mixed_variadic_overflow(
                   1, 2, 3, 4, 5, 6,
                   1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
                   91, 9.75) == 9.75);
        assert(va_mixed_aggregate_integer(7, mixed_aggregate) == 19);
        assert(va_mixed_aggregate_floating(7, mixed_aggregate) == 4.125);
        assert(va_integer_aggregate_overflow(
                   1, 2, 3, 4, 5, 6, integer_aggregate) == 137);
        assert(va_mixed_aggregate_overflow(
                   1, 2, 3, 4, 5, 6, mixed_aggregate) == 4.125);
        assert(va_mixed_aggregate_sse_overflow(
                   1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
                   mixed_aggregate) == 4.125);
        assert(va_large_aggregate(7, large_aggregate) == 47);
        assert(va_aligned_large_after_stack(
                   1, 2, 3, 4, 5, 6, 23, aligned_large) == 94);
        assert(verified_unmap(memory, mapping_size) == 0);
    }
    objfile_free(object);
}

static void verify_sysv_va_aggregate_object(const char* path)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* float_literal_symbol;
    ObjSymbol* double_literal_symbol;
    ObjSymbol* mixed_integer_symbol;
    ObjSymbol* mixed_floating_symbol;
    ObjSymbol* mixed_aggregate_integer_call_symbol;
    ObjSymbol* mixed_aggregate_floating_call_symbol;
    ObjSymbol* mixed_aggregate_stack_call_symbol;
    ObjSymbol* mixed_aggregate_stack_integer_call_symbol;
    ObjSymbol* named_mixed_aggregate_call_symbol;
    ObjSymbol* named_mixed_aggregate_integer_call_symbol;
    ObjSymbol* named_mixed_aggregate_stack_call_symbol;
    ObjSymbol* named_mixed_aggregate_stack_integer_call_symbol;
    ObjSymbol* named_mixed_va_gp_offset_call_symbol;
    ObjSymbol* named_mixed_va_sse_offset_call_symbol;
    ObjSymbol* named_mixed_va_stack_offset_call_symbol;
    ObjSymbol* integer_overflow_symbol;
    ObjSymbol* mixed_overflow_symbol;
    ObjSymbol* mixed_sse_overflow_symbol;
    ObjSymbol* large_aggregate_symbol;
    ObjSymbol* aligned32_aggregate_call_symbol;
    ObjSymbol* integer_aggregate_call_symbol;
    ObjSymbol* two_integer_aggregate_call_symbol;
    ObjSymbol* two_integer_aggregate_stack_call_symbol;
    ObjSymbol* double_aggregate_call_symbol;
    ObjSymbol* double_aggregate_last_xmm_call_symbol;
    ObjSymbol* double_aggregate_stack_call_symbol;
    ObjSymbol* float_aggregate_call_symbol;
    ObjSymbol* two_double_aggregate_call_symbol;
    ObjSymbol* two_double_aggregate_last_xmm_call_symbol;
    ObjSymbol* two_double_aggregate_stack_call_symbol;
    size_t mapping_size;
    void* memory;
    void* address;
    float (RINOS_ABI *float_literal)(void);
    double (RINOS_ABI *double_literal)(void);
    int (RINOS_ABI *mixed_integer)(int, ...);
    double (RINOS_ABI *mixed_floating)(int, ...);
    int (RINOS_ABI *mixed_aggregate_integer_call)(int, double);
    double (RINOS_ABI *mixed_aggregate_floating_call)(int, double);
    double (RINOS_ABI *mixed_aggregate_stack_call)(
        int, double, double, double, double, double,
        double, double, double, double);
    int (RINOS_ABI *mixed_aggregate_stack_integer_call)(
        int, double, double, double, double, double,
        double, double, double, double);
    double (RINOS_ABI *named_mixed_aggregate_call)(int, double);
    int (RINOS_ABI *named_mixed_aggregate_integer_call)(int, double);
    double (RINOS_ABI *named_mixed_aggregate_stack_call)(
        int, double, double, double, double, double,
        double, double, double, double);
    int (RINOS_ABI *named_mixed_aggregate_stack_integer_call)(
        int, double, double, double, double, double,
        double, double, double, double);
    int (RINOS_ABI *named_mixed_va_gp_offset_call)(int, double, int);
    double (RINOS_ABI *named_mixed_va_sse_offset_call)(
        int, double, double);
    int (RINOS_ABI *named_mixed_va_stack_offset_call)(
        double, int, double, int);
    int (RINOS_ABI *integer_overflow)(int, int, int, int, int, int, ...);
    double (RINOS_ABI *mixed_overflow)(int, int, int, int, int, int, ...);
    double (RINOS_ABI *mixed_sse_overflow)(
        double, double, double, double,
        double, double, double, double, ...);
    long long (RINOS_ABI *large_aggregate)(int, ...);
    long long (RINOS_ABI *aligned32_aggregate_call)(
        double, double, double, double, double, double, double, double,
        double, long long, long long, long long, long long, long long,
        long long, long long);
    int (RINOS_ABI *integer_aggregate_call)(int, int);
    long long (RINOS_ABI *two_integer_aggregate_call)(long long, long long);
    long long (RINOS_ABI *two_integer_aggregate_stack_call)(
        int, int, int, int, int, int, long long, long long);
    double (RINOS_ABI *double_aggregate_call)(double);
    double (RINOS_ABI *double_aggregate_last_xmm_call)(
        double, double, double, double, double, double, double, double);
    double (RINOS_ABI *double_aggregate_stack_call)(
        double, double, double, double, double, double, double, double, double);
    float (RINOS_ABI *float_aggregate_call)(float);
    double (RINOS_ABI *two_double_aggregate_call)(double, double);
    double (RINOS_ABI *two_double_aggregate_last_xmm_call)(
        double, double, double, double, double, double, double, double);
    double (RINOS_ABI *two_double_aggregate_stack_call)(
        double, double, double, double, double, double, double, double,
        double, double);
    struct VerifiedSysvMixedAggregate mixed = { 19, 4.125 };
    struct VerifiedSysvIntegerAggregate integer = { 13, 7 };
    struct VerifiedSysvVaLargeAggregate large = { 11, 23, 47 };
    assert(object != NULL && object->arch == ARCH_X64 && sizeof(void*) == 8u);
    text = objfile_get_section(object, ".text");
    float_literal_symbol = objfile_find_symbol(
        object, "verified_sysv_va_float_literal");
    double_literal_symbol = objfile_find_symbol(
        object, "verified_sysv_va_double_literal");
    mixed_integer_symbol = objfile_find_symbol(
        object, "verified_sysv_va_mixed_aggregate_integer");
    mixed_floating_symbol = objfile_find_symbol(
        object, "verified_sysv_va_mixed_aggregate_floating");
    mixed_aggregate_integer_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_mixed_aggregate_integer_call");
    mixed_aggregate_floating_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_mixed_aggregate_floating_call");
    mixed_aggregate_stack_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_mixed_aggregate_stack_call");
    mixed_aggregate_stack_integer_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_mixed_aggregate_stack_integer_call");
    named_mixed_aggregate_call_symbol = objfile_find_symbol(
        object, "verified_sysv_named_mixed_aggregate_call");
    named_mixed_aggregate_integer_call_symbol = objfile_find_symbol(
        object, "verified_sysv_named_mixed_aggregate_integer_call");
    named_mixed_aggregate_stack_call_symbol = objfile_find_symbol(
        object, "verified_sysv_named_mixed_aggregate_stack_call");
    named_mixed_aggregate_stack_integer_call_symbol = objfile_find_symbol(
        object, "verified_sysv_named_mixed_aggregate_stack_integer_call");
    named_mixed_va_gp_offset_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_named_mixed_aggregate_gp_offset_call");
    named_mixed_va_sse_offset_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_named_mixed_aggregate_sse_offset_call");
    named_mixed_va_stack_offset_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_named_mixed_aggregate_stack_offset_call");
    integer_overflow_symbol = objfile_find_symbol(
        object, "verified_sysv_va_integer_aggregate_overflow");
    mixed_overflow_symbol = objfile_find_symbol(
        object, "verified_sysv_va_mixed_aggregate_overflow");
    mixed_sse_overflow_symbol = objfile_find_symbol(
        object, "verified_sysv_va_mixed_aggregate_sse_overflow");
    large_aggregate_symbol = objfile_find_symbol(
        object, "verified_sysv_va_large_aggregate");
    aligned32_aggregate_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_aligned32_aggregate_call");
    integer_aggregate_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_integer_aggregate_call");
    two_integer_aggregate_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_two_integer_aggregate_call");
    two_integer_aggregate_stack_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_two_integer_aggregate_stack_call");
    double_aggregate_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_double_aggregate_call");
    double_aggregate_last_xmm_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_double_aggregate_last_xmm_call");
    double_aggregate_stack_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_double_aggregate_stack_call");
    float_aggregate_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_float_aggregate_call");
    two_double_aggregate_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_two_double_aggregate_call");
    two_double_aggregate_last_xmm_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_two_double_aggregate_last_xmm_call");
    two_double_aggregate_stack_call_symbol = objfile_find_symbol(
        object, "verified_sysv_va_two_double_aggregate_stack_call");
    assert(mixed_integer_symbol != NULL &&
           mixed_integer_symbol->type == SYM_GLOBAL &&
           mixed_integer_symbol->binding == BIND_CODE &&
           mixed_integer_symbol->section == 0);
    assert(mixed_floating_symbol != NULL &&
           mixed_floating_symbol->type == SYM_GLOBAL &&
           mixed_floating_symbol->binding == BIND_CODE &&
           mixed_floating_symbol->section == 0);
    assert(mixed_aggregate_integer_call_symbol != NULL &&
           mixed_aggregate_integer_call_symbol->type == SYM_GLOBAL &&
           mixed_aggregate_integer_call_symbol->binding == BIND_CODE &&
           mixed_aggregate_integer_call_symbol->section == 0);
    assert(mixed_aggregate_floating_call_symbol != NULL &&
           mixed_aggregate_floating_call_symbol->type == SYM_GLOBAL &&
           mixed_aggregate_floating_call_symbol->binding == BIND_CODE &&
           mixed_aggregate_floating_call_symbol->section == 0);
    assert(mixed_aggregate_stack_call_symbol != NULL &&
           mixed_aggregate_stack_call_symbol->type == SYM_GLOBAL &&
           mixed_aggregate_stack_call_symbol->binding == BIND_CODE &&
           mixed_aggregate_stack_call_symbol->section == 0);
    assert(mixed_aggregate_stack_integer_call_symbol != NULL &&
           mixed_aggregate_stack_integer_call_symbol->type == SYM_GLOBAL &&
           mixed_aggregate_stack_integer_call_symbol->binding == BIND_CODE &&
           mixed_aggregate_stack_integer_call_symbol->section == 0);
    assert(named_mixed_aggregate_call_symbol != NULL &&
           named_mixed_aggregate_call_symbol->type == SYM_GLOBAL &&
           named_mixed_aggregate_call_symbol->binding == BIND_CODE &&
           named_mixed_aggregate_call_symbol->section == 0);
    assert(named_mixed_aggregate_integer_call_symbol != NULL &&
           named_mixed_aggregate_integer_call_symbol->type == SYM_GLOBAL &&
           named_mixed_aggregate_integer_call_symbol->binding == BIND_CODE &&
           named_mixed_aggregate_integer_call_symbol->section == 0);
    assert(named_mixed_aggregate_stack_call_symbol != NULL &&
           named_mixed_aggregate_stack_call_symbol->type == SYM_GLOBAL &&
           named_mixed_aggregate_stack_call_symbol->binding == BIND_CODE &&
           named_mixed_aggregate_stack_call_symbol->section == 0);
    assert(named_mixed_aggregate_stack_integer_call_symbol != NULL &&
           named_mixed_aggregate_stack_integer_call_symbol->type == SYM_GLOBAL &&
           named_mixed_aggregate_stack_integer_call_symbol->binding == BIND_CODE &&
           named_mixed_aggregate_stack_integer_call_symbol->section == 0);
    assert(named_mixed_va_gp_offset_call_symbol != NULL &&
           named_mixed_va_gp_offset_call_symbol->type == SYM_GLOBAL &&
           named_mixed_va_gp_offset_call_symbol->binding == BIND_CODE &&
           named_mixed_va_gp_offset_call_symbol->section == 0);
    assert(named_mixed_va_sse_offset_call_symbol != NULL &&
           named_mixed_va_sse_offset_call_symbol->type == SYM_GLOBAL &&
           named_mixed_va_sse_offset_call_symbol->binding == BIND_CODE &&
           named_mixed_va_sse_offset_call_symbol->section == 0);
    assert(named_mixed_va_stack_offset_call_symbol != NULL &&
           named_mixed_va_stack_offset_call_symbol->type == SYM_GLOBAL &&
           named_mixed_va_stack_offset_call_symbol->binding == BIND_CODE &&
           named_mixed_va_stack_offset_call_symbol->section == 0);
    assert(float_literal_symbol != NULL &&
           float_literal_symbol->type == SYM_GLOBAL &&
           float_literal_symbol->binding == BIND_CODE &&
           float_literal_symbol->section == 0);
    assert(double_literal_symbol != NULL &&
           double_literal_symbol->type == SYM_GLOBAL &&
           double_literal_symbol->binding == BIND_CODE &&
           double_literal_symbol->section == 0);
    assert(integer_overflow_symbol != NULL &&
           integer_overflow_symbol->type == SYM_GLOBAL &&
           integer_overflow_symbol->binding == BIND_CODE &&
           integer_overflow_symbol->section == 0);
    assert(mixed_overflow_symbol != NULL &&
           mixed_overflow_symbol->type == SYM_GLOBAL &&
           mixed_overflow_symbol->binding == BIND_CODE &&
           mixed_overflow_symbol->section == 0);
    assert(mixed_sse_overflow_symbol != NULL &&
           mixed_sse_overflow_symbol->type == SYM_GLOBAL &&
           mixed_sse_overflow_symbol->binding == BIND_CODE &&
           mixed_sse_overflow_symbol->section == 0);
    assert(large_aggregate_symbol != NULL &&
           large_aggregate_symbol->type == SYM_GLOBAL &&
           large_aggregate_symbol->binding == BIND_CODE &&
           large_aggregate_symbol->section == 0);
    assert(aligned32_aggregate_call_symbol != NULL &&
           aligned32_aggregate_call_symbol->type == SYM_GLOBAL &&
           aligned32_aggregate_call_symbol->binding == BIND_CODE &&
           aligned32_aggregate_call_symbol->section == 0);
    assert(integer_aggregate_call_symbol != NULL &&
           integer_aggregate_call_symbol->type == SYM_GLOBAL &&
           integer_aggregate_call_symbol->binding == BIND_CODE &&
           integer_aggregate_call_symbol->section == 0);
    assert(two_integer_aggregate_call_symbol != NULL &&
           two_integer_aggregate_call_symbol->type == SYM_GLOBAL &&
           two_integer_aggregate_call_symbol->binding == BIND_CODE &&
           two_integer_aggregate_call_symbol->section == 0);
    assert(two_integer_aggregate_stack_call_symbol != NULL &&
           two_integer_aggregate_stack_call_symbol->type == SYM_GLOBAL &&
           two_integer_aggregate_stack_call_symbol->binding == BIND_CODE &&
           two_integer_aggregate_stack_call_symbol->section == 0);
    assert(double_aggregate_call_symbol != NULL &&
           double_aggregate_call_symbol->type == SYM_GLOBAL &&
           double_aggregate_call_symbol->binding == BIND_CODE &&
           double_aggregate_call_symbol->section == 0);
    assert(double_aggregate_last_xmm_call_symbol != NULL &&
           double_aggregate_last_xmm_call_symbol->type == SYM_GLOBAL &&
           double_aggregate_last_xmm_call_symbol->binding == BIND_CODE &&
           double_aggregate_last_xmm_call_symbol->section == 0);
    assert(double_aggregate_stack_call_symbol != NULL &&
           double_aggregate_stack_call_symbol->type == SYM_GLOBAL &&
           double_aggregate_stack_call_symbol->binding == BIND_CODE &&
           double_aggregate_stack_call_symbol->section == 0);
    assert(float_aggregate_call_symbol != NULL &&
           float_aggregate_call_symbol->type == SYM_GLOBAL &&
           float_aggregate_call_symbol->binding == BIND_CODE &&
           float_aggregate_call_symbol->section == 0);
    assert(two_double_aggregate_call_symbol != NULL &&
           two_double_aggregate_call_symbol->type == SYM_GLOBAL &&
           two_double_aggregate_call_symbol->binding == BIND_CODE &&
           two_double_aggregate_call_symbol->section == 0);
    assert(two_double_aggregate_last_xmm_call_symbol != NULL &&
           two_double_aggregate_last_xmm_call_symbol->type == SYM_GLOBAL &&
           two_double_aggregate_last_xmm_call_symbol->binding == BIND_CODE &&
           two_double_aggregate_last_xmm_call_symbol->section == 0);
    assert(two_double_aggregate_stack_call_symbol != NULL &&
           two_double_aggregate_stack_call_symbol->type == SYM_GLOBAL &&
           two_double_aggregate_stack_call_symbol->binding == BIND_CODE &&
           two_double_aggregate_stack_call_symbol->section == 0);
    memory = map_text(object, text, &mapping_size);
    address = symbol_address(memory, float_literal_symbol);
    memcpy(&float_literal, &address, sizeof(float_literal));
    address = symbol_address(memory, double_literal_symbol);
    memcpy(&double_literal, &address, sizeof(double_literal));
    address = symbol_address(memory, mixed_integer_symbol);
    memcpy(&mixed_integer, &address, sizeof(mixed_integer));
    address = symbol_address(memory, mixed_floating_symbol);
    memcpy(&mixed_floating, &address, sizeof(mixed_floating));
    address = symbol_address(memory, mixed_aggregate_integer_call_symbol);
    memcpy(&mixed_aggregate_integer_call, &address,
           sizeof(mixed_aggregate_integer_call));
    address = symbol_address(memory, mixed_aggregate_floating_call_symbol);
    memcpy(&mixed_aggregate_floating_call, &address,
           sizeof(mixed_aggregate_floating_call));
    address = symbol_address(memory, mixed_aggregate_stack_call_symbol);
    memcpy(&mixed_aggregate_stack_call, &address,
           sizeof(mixed_aggregate_stack_call));
    address = symbol_address(memory, mixed_aggregate_stack_integer_call_symbol);
    memcpy(&mixed_aggregate_stack_integer_call, &address,
           sizeof(mixed_aggregate_stack_integer_call));
    address = symbol_address(memory, named_mixed_aggregate_call_symbol);
    memcpy(&named_mixed_aggregate_call, &address,
           sizeof(named_mixed_aggregate_call));
    address = symbol_address(memory, named_mixed_aggregate_integer_call_symbol);
    memcpy(&named_mixed_aggregate_integer_call, &address,
           sizeof(named_mixed_aggregate_integer_call));
    address = symbol_address(memory, named_mixed_aggregate_stack_call_symbol);
    memcpy(&named_mixed_aggregate_stack_call, &address,
           sizeof(named_mixed_aggregate_stack_call));
    address = symbol_address(
        memory, named_mixed_aggregate_stack_integer_call_symbol);
    memcpy(&named_mixed_aggregate_stack_integer_call, &address,
           sizeof(named_mixed_aggregate_stack_integer_call));
    address = symbol_address(memory, named_mixed_va_gp_offset_call_symbol);
    memcpy(&named_mixed_va_gp_offset_call, &address,
           sizeof(named_mixed_va_gp_offset_call));
    address = symbol_address(memory, named_mixed_va_sse_offset_call_symbol);
    memcpy(&named_mixed_va_sse_offset_call, &address,
           sizeof(named_mixed_va_sse_offset_call));
    address = symbol_address(memory, named_mixed_va_stack_offset_call_symbol);
    memcpy(&named_mixed_va_stack_offset_call, &address,
           sizeof(named_mixed_va_stack_offset_call));
    address = symbol_address(memory, integer_overflow_symbol);
    memcpy(&integer_overflow, &address, sizeof(integer_overflow));
    address = symbol_address(memory, mixed_overflow_symbol);
    memcpy(&mixed_overflow, &address, sizeof(mixed_overflow));
    address = symbol_address(memory, mixed_sse_overflow_symbol);
    memcpy(&mixed_sse_overflow, &address, sizeof(mixed_sse_overflow));
    address = symbol_address(memory, large_aggregate_symbol);
    memcpy(&large_aggregate, &address, sizeof(large_aggregate));
    address = symbol_address(memory, aligned32_aggregate_call_symbol);
    memcpy(&aligned32_aggregate_call, &address,
           sizeof(aligned32_aggregate_call));
    address = symbol_address(memory, integer_aggregate_call_symbol);
    memcpy(&integer_aggregate_call, &address, sizeof(integer_aggregate_call));
    address = symbol_address(memory, two_integer_aggregate_call_symbol);
    memcpy(&two_integer_aggregate_call, &address,
           sizeof(two_integer_aggregate_call));
    address = symbol_address(memory, two_integer_aggregate_stack_call_symbol);
    memcpy(&two_integer_aggregate_stack_call, &address,
           sizeof(two_integer_aggregate_stack_call));
    address = symbol_address(memory, double_aggregate_call_symbol);
    memcpy(&double_aggregate_call, &address, sizeof(double_aggregate_call));
    address = symbol_address(memory, double_aggregate_last_xmm_call_symbol);
    memcpy(&double_aggregate_last_xmm_call, &address,
           sizeof(double_aggregate_last_xmm_call));
    address = symbol_address(memory, double_aggregate_stack_call_symbol);
    memcpy(&double_aggregate_stack_call, &address,
           sizeof(double_aggregate_stack_call));
    address = symbol_address(memory, float_aggregate_call_symbol);
    memcpy(&float_aggregate_call, &address, sizeof(float_aggregate_call));
    address = symbol_address(memory, two_double_aggregate_call_symbol);
    memcpy(&two_double_aggregate_call, &address,
           sizeof(two_double_aggregate_call));
    address = symbol_address(memory, two_double_aggregate_last_xmm_call_symbol);
    memcpy(&two_double_aggregate_last_xmm_call, &address,
           sizeof(two_double_aggregate_last_xmm_call));
    address = symbol_address(memory, two_double_aggregate_stack_call_symbol);
    memcpy(&two_double_aggregate_stack_call, &address,
           sizeof(two_double_aggregate_stack_call));
    assert(float_literal() == 1.25f);
    assert(double_literal() == 5.25);
    assert(mixed_integer(7, mixed) == 19);
    assert(mixed_floating(7, mixed) == 4.125);
    assert(mixed_aggregate_integer_call(19, 4.125) == 19);
    assert(mixed_aggregate_floating_call(19, 4.125) == 4.125);
    assert(mixed_aggregate_stack_call(
               19, 4.125, 1.0, 2.0, 3.0, 4.0,
               5.0, 6.0, 7.0, 8.0) == 4.125);
    assert(mixed_aggregate_stack_integer_call(
               19, 4.125, 1.0, 2.0, 3.0, 4.0,
               5.0, 6.0, 7.0, 8.0) == 19);
    assert(named_mixed_aggregate_call(19, 4.125) == 4.125);
    assert(named_mixed_aggregate_integer_call(19, 4.125) == 19);
    assert(named_mixed_aggregate_stack_call(
               19, 4.125, 1.0, 2.0, 3.0, 4.0,
               5.0, 6.0, 7.0, 8.0) == 4.125);
    assert(named_mixed_aggregate_stack_integer_call(
               19, 4.125, 1.0, 2.0, 3.0, 4.0,
               5.0, 6.0, 7.0, 8.0) == 19);
    assert(named_mixed_va_gp_offset_call(19, 4.125, 7) == 1907);
    assert(named_mixed_va_sse_offset_call(19, 4.125, 5.25) == 5.25);
    assert(named_mixed_va_stack_offset_call(1.0, 19, 4.125, 7) == 1907);
    assert(integer_overflow(1, 2, 3, 4, 5, 6, integer) == 137);
    assert(mixed_overflow(1, 2, 3, 4, 5, 6, mixed) == 4.125);
    assert(mixed_sse_overflow(
               1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
               mixed) == 4.125);
    assert(large_aggregate(7, large) == 47);
    assert(aligned32_aggregate_call(
               1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0,
               11, 23, 47, 53, 13, 29, 47) == 100);
    assert(integer_aggregate_call(13, 7) == 137);
    assert(two_integer_aggregate_call(11, 47) == 47);
    assert(two_integer_aggregate_stack_call(
               1, 2, 3, 4, 5, 6, 11, 47) == 47);
    assert(double_aggregate_call(6.25) == 6.25);
    assert(double_aggregate_last_xmm_call(
               1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 12.5) == 12.5);
    assert(double_aggregate_stack_call(
               1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 15.75) == 15.75);
    assert(float_aggregate_call(3.5f) == 3.5f);
    assert(two_double_aggregate_call(3.0, 4.5) == 4.5);
    assert(two_double_aggregate_last_xmm_call(
               1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.5, 8.5) == 8.5);
    assert(two_double_aggregate_stack_call(
               1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
               9.5, 10.5) == 10.5);
    assert(verified_unmap(memory, mapping_size) == 0);
    objfile_free(object);
}

static void verify_sysv_va_aggregate_straddle_object(const char* path)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* integer_symbol;
    ObjSymbol* sse_symbol;
    ObjSymbol* mixed_gp_symbol;
    ObjSymbol* mixed_sse_symbol;
    ObjSymbol* named_mixed_gp_symbol;
    ObjSymbol* named_mixed_sse_symbol;
    ObjSymbol* named_mixed_va_gp_symbol;
    ObjSymbol* named_mixed_va_sse_symbol;
    ObjSymbol* large_memory_aggregate_symbol;
    ObjSymbol* aligned_memory_aggregate_simple_symbol;
    ObjSymbol* aligned_memory_aggregate_symbol;
    size_t mapping_size;
    void* memory;
    void* address;
    long long (RINOS_ABI *integer_call)(
        int, int, int, int, int, long long, long long, int);
    double (RINOS_ABI *sse_call)(
        double, double, double, double, double, double, double,
        double, double);
    double (RINOS_ABI *mixed_gp_call)(
        int, int, int, int, int, int, int, double);
    double (RINOS_ABI *mixed_sse_call)(
        double, double, double, double, double, double, double, double,
        int, double);
    double (RINOS_ABI *named_mixed_gp_call)(
        int, int, int, int, int, int, int, double);
    double (RINOS_ABI *named_mixed_sse_call)(
        double, double, double, double, double, double, double, double,
        int, double);
    int (RINOS_ABI *named_mixed_va_gp_call)(int, double, int);
    double (RINOS_ABI *named_mixed_va_sse_call)(
        double, double, double, double, double, double, double, double,
        int, double, double);
    long long (RINOS_ABI *large_memory_aggregate_call)(
        long long, long long, long long);
    long long (RINOS_ABI *aligned_memory_aggregate_simple_call)(
        long long, long long, long long, long long);
    long long (RINOS_ABI *aligned_memory_aggregate_call)(
        double, double, double, double, double, double, double, double, double,
        long long, long long, long long, long long,
        long long, long long, long long);
    assert(object != NULL && object->arch == ARCH_X64 && sizeof(void*) == 8u);
    text = objfile_get_section(object, ".text");
    integer_symbol = objfile_find_symbol(
        object, "verified_sysv_va_two_integer_aggregate_straddle_call");
    sse_symbol = objfile_find_symbol(
        object, "verified_sysv_va_two_double_aggregate_straddle_call");
    mixed_gp_symbol = objfile_find_symbol(
        object, "verified_sysv_va_mixed_aggregate_gp_straddle_call");
    mixed_sse_symbol = objfile_find_symbol(
        object, "verified_sysv_va_mixed_aggregate_sse_straddle_call");
    named_mixed_gp_symbol = objfile_find_symbol(
        object, "verified_sysv_named_mixed_aggregate_gp_straddle_call");
    named_mixed_sse_symbol = objfile_find_symbol(
        object, "verified_sysv_named_mixed_aggregate_sse_straddle_call");
    named_mixed_va_gp_symbol = objfile_find_symbol(
        object, "verified_sysv_va_named_mixed_aggregate_gp_straddle_call");
    named_mixed_va_sse_symbol = objfile_find_symbol(
        object, "verified_sysv_va_named_mixed_aggregate_sse_straddle_call");
    large_memory_aggregate_symbol = objfile_find_symbol(
        object, "verified_sysv_va_large_memory_aggregate_call");
    aligned_memory_aggregate_simple_symbol = objfile_find_symbol(
        object, "verified_sysv_va_aligned_memory_aggregate_simple_call");
    aligned_memory_aggregate_symbol = objfile_find_symbol(
        object, "verified_sysv_va_aligned_memory_aggregate_call");
    assert(integer_symbol != NULL && integer_symbol->type == SYM_GLOBAL &&
           integer_symbol->binding == BIND_CODE && integer_symbol->section == 0);
    assert(sse_symbol != NULL && sse_symbol->type == SYM_GLOBAL &&
           sse_symbol->binding == BIND_CODE && sse_symbol->section == 0);
    assert(mixed_gp_symbol != NULL && mixed_gp_symbol->type == SYM_GLOBAL &&
           mixed_gp_symbol->binding == BIND_CODE &&
           mixed_gp_symbol->section == 0);
    assert(mixed_sse_symbol != NULL && mixed_sse_symbol->type == SYM_GLOBAL &&
           mixed_sse_symbol->binding == BIND_CODE &&
           mixed_sse_symbol->section == 0);
    assert(named_mixed_gp_symbol != NULL &&
           named_mixed_gp_symbol->type == SYM_GLOBAL &&
           named_mixed_gp_symbol->binding == BIND_CODE &&
           named_mixed_gp_symbol->section == 0);
    assert(named_mixed_sse_symbol != NULL &&
           named_mixed_sse_symbol->type == SYM_GLOBAL &&
           named_mixed_sse_symbol->binding == BIND_CODE &&
           named_mixed_sse_symbol->section == 0);
    assert(named_mixed_va_gp_symbol != NULL &&
           named_mixed_va_gp_symbol->type == SYM_GLOBAL &&
           named_mixed_va_gp_symbol->binding == BIND_CODE &&
           named_mixed_va_gp_symbol->section == 0);
    assert(named_mixed_va_sse_symbol != NULL &&
           named_mixed_va_sse_symbol->type == SYM_GLOBAL &&
           named_mixed_va_sse_symbol->binding == BIND_CODE &&
           named_mixed_va_sse_symbol->section == 0);
    assert(large_memory_aggregate_symbol != NULL &&
           large_memory_aggregate_symbol->type == SYM_GLOBAL &&
           large_memory_aggregate_symbol->binding == BIND_CODE &&
           large_memory_aggregate_symbol->section == 0);
    if (aligned_memory_aggregate_simple_symbol) {
        assert(aligned_memory_aggregate_simple_symbol->type == SYM_GLOBAL &&
               aligned_memory_aggregate_simple_symbol->binding == BIND_CODE &&
               aligned_memory_aggregate_simple_symbol->section == 0);
        assert(aligned_memory_aggregate_symbol != NULL &&
               aligned_memory_aggregate_symbol->type == SYM_GLOBAL &&
               aligned_memory_aggregate_symbol->binding == BIND_CODE &&
               aligned_memory_aggregate_symbol->section == 0);
    }
    memory = map_text(object, text, &mapping_size);
    address = symbol_address(memory, integer_symbol);
    memcpy(&integer_call, &address, sizeof(integer_call));
    address = symbol_address(memory, sse_symbol);
    memcpy(&sse_call, &address, sizeof(sse_call));
    address = symbol_address(memory, mixed_gp_symbol);
    memcpy(&mixed_gp_call, &address, sizeof(mixed_gp_call));
    address = symbol_address(memory, mixed_sse_symbol);
    memcpy(&mixed_sse_call, &address, sizeof(mixed_sse_call));
    address = symbol_address(memory, named_mixed_gp_symbol);
    memcpy(&named_mixed_gp_call, &address, sizeof(named_mixed_gp_call));
    address = symbol_address(memory, named_mixed_sse_symbol);
    memcpy(&named_mixed_sse_call, &address, sizeof(named_mixed_sse_call));
    address = symbol_address(memory, named_mixed_va_gp_symbol);
    memcpy(&named_mixed_va_gp_call, &address, sizeof(named_mixed_va_gp_call));
    address = symbol_address(memory, named_mixed_va_sse_symbol);
    memcpy(&named_mixed_va_sse_call, &address, sizeof(named_mixed_va_sse_call));
    address = symbol_address(memory, large_memory_aggregate_symbol);
    memcpy(&large_memory_aggregate_call, &address,
           sizeof(large_memory_aggregate_call));
    if (aligned_memory_aggregate_simple_symbol) {
        address = symbol_address(memory, aligned_memory_aggregate_simple_symbol);
        memcpy(&aligned_memory_aggregate_simple_call, &address,
               sizeof(aligned_memory_aggregate_simple_call));
    }
    if (aligned_memory_aggregate_symbol) {
        address = symbol_address(memory, aligned_memory_aggregate_symbol);
        memcpy(&aligned_memory_aggregate_call, &address,
               sizeof(aligned_memory_aggregate_call));
    }
    assert(integer_call(1, 2, 3, 4, 5, 11, 47, 13) == 60);
    assert(sse_call(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.5, 9.5) == 9.5);
    assert(mixed_gp_call(1, 2, 3, 4, 5, 6, 19, 4.125) == 23.125);
    assert(mixed_sse_call(
               1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
               19, 4.125) == 23.125);
    assert(named_mixed_gp_call(1, 2, 3, 4, 5, 6, 19, 4.125) == 23.125);
    assert(named_mixed_sse_call(
               1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
               19, 4.125) == 23.125);
    assert(named_mixed_va_gp_call(19, 4.125, 7) == 1907);
    assert(named_mixed_va_sse_call(
               1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
               19, 4.125, 5.25) == 5.25);
    assert(large_memory_aggregate_call(11, 23, 47) == 47);
    if (aligned_memory_aggregate_simple_symbol) {
        assert(aligned_memory_aggregate_simple_call(11, 23, 47, 53) == 53);
    }
    if (aligned_memory_aggregate_symbol) {
        assert(aligned_memory_aggregate_call(
                   1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0,
                   11, 23, 47, 53, 13, 29, 47) == 100);
    }
    assert(verified_unmap(memory, mapping_size) == 0);
    objfile_free(object);
}

static void verify_wide_scalar_object(const char* path, uint16_t arch)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* symbol;
    ObjSymbol* parameter_symbol;
    ObjSymbol* const_parameter_symbol;
    ObjSymbol* add_symbol;
    ObjSymbol* carry_symbol;
    ObjSymbol* subtract_symbol;
    ObjSymbol* local_symbol;
    ObjSymbol* const_local_symbol;
    ObjSymbol* narrow_symbol;
    ObjSymbol* equal_symbol;
    ObjSymbol* not_equal_symbol;
    ObjSymbol* unsigned_less_symbol;
    ObjSymbol* signed_less_symbol;
    ObjSymbol* unsigned_le_symbol;
    ObjSymbol* unsigned_ge_symbol;
    ObjSymbol* signed_le_symbol;
    ObjSymbol* signed_ge_symbol;
    ObjSymbol* lshift_symbol;
    ObjSymbol* lshr_symbol;
    ObjSymbol* ashr_symbol;
    ObjSymbol* conditional_symbol;
    ObjSymbol* conditional_assign_symbol;
    ObjSymbol* conditional_compound_symbol;
    ObjSymbol* pure_comma_compound_symbol;
    ObjSymbol* size_align_compound_symbol;
    ObjSymbol* truth_conditional_symbol;
    ObjSymbol* mul_symbol;
    ObjSymbol* call_symbol;
    ObjSymbol* call_local_symbol;
    ObjSymbol* indirect_call_symbol;
    ObjSymbol* variadic_call_symbol;
    ObjSymbol* variadic_target_symbol;
    ObjSymbol* variadic_scalar_call_symbol;
    ObjSymbol* variadic_scalar_target_symbol;
    ObjSymbol* variadic_overflow_call_symbol;
    ObjSymbol* variadic_named_overflow_call_symbol;
    ObjSymbol* va_list_forward_call_symbol;
    ObjSymbol* va_list_pointer_forward_call_symbol;
    ObjSymbol* expect_symbol;
    ObjSymbol* assignment_symbol;
    ObjSymbol* compound_symbol;
    ObjSymbol* postincrement_symbol;
    ObjSymbol* logical_not_symbol;
    ObjSymbol* logical_and_symbol;
    ObjSymbol* logical_or_symbol;
    ObjSymbol* comma_symbol;
    ObjSymbol* udiv_symbol;
    ObjSymbol* udiv_small_symbol;
    ObjSymbol* umod_symbol;
    ObjSymbol* sdiv_symbol;
    ObjSymbol* smod_symbol;
    ObjSymbol* branch_assign_symbol;
    ObjSymbol* branch_read_symbol;
    ObjSymbol* forward_goto_symbol;
    ObjSymbol* backward_goto_symbol;
    ObjSymbol* while_loop_symbol;
    ObjSymbol* for_loop_symbol;
    ObjSymbol* do_loop_symbol;
    ObjSymbol* nested_while_symbol;
    ObjSymbol* nested_for_symbol;
    ObjSymbol* nested_do_symbol;
    ObjSymbol* switch_symbol;
    ObjSymbol* switch_loop_symbol;
    ObjSymbol* switch_fallthrough_symbol;
    ObjSymbol* switch_if_symbol;
    ObjSymbol* switch_no_default_symbol;
    ObjSymbol* while_continue_symbol;
    ObjSymbol* for_break_symbol;
    ObjSymbol* do_control_symbol;
    ObjSymbol* loop_return_symbol;
    assert(object != NULL && object->arch == arch);
    text = objfile_get_section(object, ".text");
    symbol = objfile_find_symbol(
        object, "verified_wide_scalar_constant_return");
    parameter_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_parameter");
    const_parameter_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_const_parameter");
    add_symbol = objfile_find_symbol(object, "verified_wide_scalar_add");
    carry_symbol = objfile_find_symbol(object, "verified_wide_scalar_carry");
    subtract_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_subtract");
    local_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_local");
    const_local_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_const_local");
    narrow_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_narrow");
    equal_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_equal");
    not_equal_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_not_equal");
    unsigned_less_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_unsigned_less");
    signed_less_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_signed_less");
    unsigned_le_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_unsigned_le");
    unsigned_ge_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_unsigned_ge");
    signed_le_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_signed_le");
    signed_ge_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_signed_ge");
    lshift_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_lshift");
    lshr_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_lshr");
    ashr_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_ashr");
    conditional_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_conditional");
    conditional_assign_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_conditional_assign");
    conditional_compound_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_conditional_compound");
    pure_comma_compound_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_pure_comma_compound");
    size_align_compound_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_size_align_compound");
    truth_conditional_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_truth_conditional");
    mul_symbol = objfile_find_symbol(object, "verified_wide_scalar_mul");
    call_symbol = objfile_find_symbol(object, "verified_wide_scalar_call");
    call_local_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_call_local");
    indirect_call_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_indirect_call");
    variadic_call_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_variadic_call");
    variadic_target_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_variadic_target");
    variadic_scalar_call_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_variadic_scalar_call");
    variadic_scalar_target_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_variadic_scalar_target");
    variadic_overflow_call_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_variadic_overflow_call");
    variadic_named_overflow_call_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_variadic_named_overflow_call");
    va_list_forward_call_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_va_list_forward_call");
    va_list_pointer_forward_call_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_va_list_pointer_forward_call");
    expect_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_expect");
    assignment_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_assignment");
    compound_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_compound");
    postincrement_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_postincrement");
    logical_not_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_logical_not");
    logical_and_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_logical_and");
    logical_or_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_logical_or");
    comma_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_comma");
    udiv_symbol = objfile_find_symbol(object, "verified_wide_scalar_udiv");
    udiv_small_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_udiv_small");
    umod_symbol = objfile_find_symbol(object, "verified_wide_scalar_umod");
    sdiv_symbol = objfile_find_symbol(object, "verified_wide_scalar_sdiv");
    smod_symbol = objfile_find_symbol(object, "verified_wide_scalar_smod");
    branch_assign_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_branch_assign");
    branch_read_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_branch_read");
    forward_goto_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_forward_goto");
    backward_goto_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_backward_goto");
    while_loop_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_while_loop");
    for_loop_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_for_loop");
    do_loop_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_do_loop");
    nested_while_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_nested_while");
    nested_for_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_nested_for");
    nested_do_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_nested_do");
    switch_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_switch");
    switch_loop_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_switch_loop");
    switch_fallthrough_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_switch_fallthrough");
    switch_if_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_switch_if");
    switch_no_default_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_switch_no_default");
    while_continue_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_while_continue");
    for_break_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_for_break");
    do_control_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_do_control");
    loop_return_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_loop_return");
    assert(text != NULL && text->size != 0u &&
           (text->flags & (SECT_FLAG_ALLOC | SECT_FLAG_EXEC)) ==
               (SECT_FLAG_ALLOC | SECT_FLAG_EXEC));
    assert(symbol != NULL && symbol->type == SYM_GLOBAL &&
           symbol->binding == BIND_CODE && symbol->section == 0);
    assert(parameter_symbol != NULL && parameter_symbol->type == SYM_GLOBAL &&
           parameter_symbol->binding == BIND_CODE &&
           parameter_symbol->section == 0);
    assert(const_parameter_symbol != NULL &&
           const_parameter_symbol->type == SYM_GLOBAL &&
           const_parameter_symbol->binding == BIND_CODE &&
           const_parameter_symbol->section == 0);
    assert(add_symbol != NULL && add_symbol->type == SYM_GLOBAL &&
           add_symbol->binding == BIND_CODE && add_symbol->section == 0);
    assert(carry_symbol != NULL && carry_symbol->type == SYM_GLOBAL &&
           carry_symbol->binding == BIND_CODE && carry_symbol->section == 0);
    assert(subtract_symbol != NULL &&
           subtract_symbol->type == SYM_GLOBAL &&
           subtract_symbol->binding == BIND_CODE &&
           subtract_symbol->section == 0);
    assert(local_symbol != NULL && local_symbol->type == SYM_GLOBAL &&
           local_symbol->binding == BIND_CODE && local_symbol->section == 0);
    assert(const_local_symbol != NULL &&
           const_local_symbol->type == SYM_GLOBAL &&
           const_local_symbol->binding == BIND_CODE &&
           const_local_symbol->section == 0);
    assert(narrow_symbol != NULL && narrow_symbol->type == SYM_GLOBAL &&
           narrow_symbol->binding == BIND_CODE &&
           narrow_symbol->section == 0);
    assert(equal_symbol != NULL && equal_symbol->type == SYM_GLOBAL &&
           equal_symbol->binding == BIND_CODE && equal_symbol->section == 0);
    assert(not_equal_symbol != NULL &&
           not_equal_symbol->type == SYM_GLOBAL &&
           not_equal_symbol->binding == BIND_CODE &&
           not_equal_symbol->section == 0);
    assert(unsigned_less_symbol != NULL &&
           unsigned_less_symbol->type == SYM_GLOBAL &&
           unsigned_less_symbol->binding == BIND_CODE &&
           unsigned_less_symbol->section == 0);
    assert(signed_less_symbol != NULL &&
           signed_less_symbol->type == SYM_GLOBAL &&
           signed_less_symbol->binding == BIND_CODE &&
           signed_less_symbol->section == 0);
    assert(unsigned_le_symbol != NULL &&
           unsigned_le_symbol->type == SYM_GLOBAL &&
           unsigned_le_symbol->binding == BIND_CODE &&
           unsigned_le_symbol->section == 0);
    assert(unsigned_ge_symbol != NULL &&
           unsigned_ge_symbol->type == SYM_GLOBAL &&
           unsigned_ge_symbol->binding == BIND_CODE &&
           unsigned_ge_symbol->section == 0);
    assert(signed_le_symbol != NULL &&
           signed_le_symbol->type == SYM_GLOBAL &&
           signed_le_symbol->binding == BIND_CODE &&
           signed_le_symbol->section == 0);
    assert(signed_ge_symbol != NULL &&
           signed_ge_symbol->type == SYM_GLOBAL &&
           signed_ge_symbol->binding == BIND_CODE &&
           signed_ge_symbol->section == 0);
    assert(lshift_symbol != NULL && lshift_symbol->type == SYM_GLOBAL &&
           lshift_symbol->binding == BIND_CODE && lshift_symbol->section == 0);
    assert(lshr_symbol != NULL && lshr_symbol->type == SYM_GLOBAL &&
           lshr_symbol->binding == BIND_CODE && lshr_symbol->section == 0);
    assert(ashr_symbol != NULL && ashr_symbol->type == SYM_GLOBAL &&
           ashr_symbol->binding == BIND_CODE && ashr_symbol->section == 0);
    assert(conditional_symbol != NULL &&
           conditional_symbol->type == SYM_GLOBAL &&
           conditional_symbol->binding == BIND_CODE &&
           conditional_symbol->section == 0);
    assert(conditional_assign_symbol != NULL &&
           conditional_assign_symbol->type == SYM_GLOBAL &&
           conditional_assign_symbol->binding == BIND_CODE &&
           conditional_assign_symbol->section == 0);
    assert(conditional_compound_symbol != NULL &&
           conditional_compound_symbol->type == SYM_GLOBAL &&
           conditional_compound_symbol->binding == BIND_CODE &&
           conditional_compound_symbol->section == 0);
    assert(pure_comma_compound_symbol != NULL &&
           pure_comma_compound_symbol->type == SYM_GLOBAL &&
           pure_comma_compound_symbol->binding == BIND_CODE &&
           pure_comma_compound_symbol->section == 0);
    assert(size_align_compound_symbol != NULL &&
           size_align_compound_symbol->type == SYM_GLOBAL &&
           size_align_compound_symbol->binding == BIND_CODE &&
           size_align_compound_symbol->section == 0);
    assert(truth_conditional_symbol != NULL &&
           truth_conditional_symbol->type == SYM_GLOBAL &&
           truth_conditional_symbol->binding == BIND_CODE &&
           truth_conditional_symbol->section == 0);
    assert(mul_symbol != NULL && mul_symbol->type == SYM_GLOBAL &&
           mul_symbol->binding == BIND_CODE && mul_symbol->section == 0);
    assert(call_symbol != NULL && call_symbol->type == SYM_GLOBAL &&
           call_symbol->binding == BIND_CODE && call_symbol->section == 0);
    assert(call_local_symbol != NULL &&
           call_local_symbol->type == SYM_GLOBAL &&
           call_local_symbol->binding == BIND_CODE &&
           call_local_symbol->section == 0);
    assert(indirect_call_symbol != NULL &&
           indirect_call_symbol->type == SYM_GLOBAL &&
           indirect_call_symbol->binding == BIND_CODE &&
           indirect_call_symbol->section == 0);
    assert(variadic_target_symbol != NULL &&
           variadic_target_symbol->type == SYM_GLOBAL &&
           variadic_target_symbol->binding == BIND_CODE &&
           variadic_target_symbol->section == 0);
    assert(variadic_call_symbol != NULL &&
           variadic_call_symbol->type == SYM_GLOBAL &&
           variadic_call_symbol->binding == BIND_CODE &&
           variadic_call_symbol->section == 0);
    assert(variadic_scalar_target_symbol != NULL &&
           variadic_scalar_target_symbol->type == SYM_GLOBAL &&
           variadic_scalar_target_symbol->binding == BIND_CODE &&
           variadic_scalar_target_symbol->section == 0);
    assert(variadic_scalar_call_symbol != NULL &&
           variadic_scalar_call_symbol->type == SYM_GLOBAL &&
           variadic_scalar_call_symbol->binding == BIND_CODE &&
           variadic_scalar_call_symbol->section == 0);
    assert(variadic_overflow_call_symbol != NULL &&
           variadic_overflow_call_symbol->type == SYM_GLOBAL &&
           variadic_overflow_call_symbol->binding == BIND_CODE &&
           variadic_overflow_call_symbol->section == 0);
    assert(variadic_named_overflow_call_symbol != NULL &&
           variadic_named_overflow_call_symbol->type == SYM_GLOBAL &&
           variadic_named_overflow_call_symbol->binding == BIND_CODE &&
           variadic_named_overflow_call_symbol->section == 0);
    assert(va_list_forward_call_symbol != NULL &&
           va_list_forward_call_symbol->type == SYM_GLOBAL &&
           va_list_forward_call_symbol->binding == BIND_CODE &&
           va_list_forward_call_symbol->section == 0);
    assert(va_list_pointer_forward_call_symbol != NULL &&
           va_list_pointer_forward_call_symbol->type == SYM_GLOBAL &&
           va_list_pointer_forward_call_symbol->binding == BIND_CODE &&
           va_list_pointer_forward_call_symbol->section == 0);
    assert(expect_symbol != NULL && expect_symbol->type == SYM_GLOBAL &&
           expect_symbol->binding == BIND_CODE && expect_symbol->section == 0);
    assert(assignment_symbol != NULL &&
           assignment_symbol->type == SYM_GLOBAL &&
           assignment_symbol->binding == BIND_CODE &&
           assignment_symbol->section == 0);
    assert(compound_symbol != NULL && compound_symbol->type == SYM_GLOBAL &&
           compound_symbol->binding == BIND_CODE &&
           compound_symbol->section == 0);
    assert(postincrement_symbol != NULL &&
           postincrement_symbol->type == SYM_GLOBAL &&
           postincrement_symbol->binding == BIND_CODE &&
           postincrement_symbol->section == 0);
    assert(logical_not_symbol != NULL &&
           logical_not_symbol->type == SYM_GLOBAL &&
           logical_not_symbol->binding == BIND_CODE &&
           logical_not_symbol->section == 0);
    assert(logical_and_symbol != NULL &&
           logical_and_symbol->type == SYM_GLOBAL &&
           logical_and_symbol->binding == BIND_CODE &&
           logical_and_symbol->section == 0);
    assert(logical_or_symbol != NULL &&
           logical_or_symbol->type == SYM_GLOBAL &&
           logical_or_symbol->binding == BIND_CODE &&
           logical_or_symbol->section == 0);
    assert(comma_symbol != NULL && comma_symbol->type == SYM_GLOBAL &&
           comma_symbol->binding == BIND_CODE && comma_symbol->section == 0);
    assert(udiv_symbol != NULL && udiv_symbol->type == SYM_GLOBAL &&
           udiv_symbol->binding == BIND_CODE && udiv_symbol->section == 0);
    assert(udiv_small_symbol != NULL &&
           udiv_small_symbol->type == SYM_GLOBAL &&
           udiv_small_symbol->binding == BIND_CODE &&
           udiv_small_symbol->section == 0);
    assert(umod_symbol != NULL && umod_symbol->type == SYM_GLOBAL &&
           umod_symbol->binding == BIND_CODE && umod_symbol->section == 0);
    assert(sdiv_symbol != NULL && sdiv_symbol->type == SYM_GLOBAL &&
           sdiv_symbol->binding == BIND_CODE && sdiv_symbol->section == 0);
    assert(smod_symbol != NULL && smod_symbol->type == SYM_GLOBAL &&
           smod_symbol->binding == BIND_CODE && smod_symbol->section == 0);
    assert(branch_assign_symbol != NULL &&
           branch_assign_symbol->type == SYM_GLOBAL &&
           branch_assign_symbol->binding == BIND_CODE &&
           branch_assign_symbol->section == 0);
    assert(branch_read_symbol != NULL &&
           branch_read_symbol->type == SYM_GLOBAL &&
           branch_read_symbol->binding == BIND_CODE &&
           branch_read_symbol->section == 0);
    assert(forward_goto_symbol != NULL &&
           forward_goto_symbol->type == SYM_GLOBAL &&
           forward_goto_symbol->binding == BIND_CODE &&
           forward_goto_symbol->section == 0);
    assert(backward_goto_symbol != NULL &&
           backward_goto_symbol->type == SYM_GLOBAL &&
           backward_goto_symbol->binding == BIND_CODE &&
           backward_goto_symbol->section == 0);
    assert(while_loop_symbol != NULL &&
           while_loop_symbol->type == SYM_GLOBAL &&
           while_loop_symbol->binding == BIND_CODE &&
           while_loop_symbol->section == 0);
    assert(for_loop_symbol != NULL &&
           for_loop_symbol->type == SYM_GLOBAL &&
           for_loop_symbol->binding == BIND_CODE &&
           for_loop_symbol->section == 0);
    assert(do_loop_symbol != NULL &&
           do_loop_symbol->type == SYM_GLOBAL &&
           do_loop_symbol->binding == BIND_CODE &&
           do_loop_symbol->section == 0);
    assert(nested_while_symbol != NULL &&
           nested_while_symbol->type == SYM_GLOBAL &&
           nested_while_symbol->binding == BIND_CODE &&
           nested_while_symbol->section == 0);
    assert(nested_for_symbol != NULL &&
           nested_for_symbol->type == SYM_GLOBAL &&
           nested_for_symbol->binding == BIND_CODE &&
           nested_for_symbol->section == 0);
    assert(nested_do_symbol != NULL &&
           nested_do_symbol->type == SYM_GLOBAL &&
           nested_do_symbol->binding == BIND_CODE &&
           nested_do_symbol->section == 0);
    assert(switch_symbol != NULL &&
           switch_symbol->type == SYM_GLOBAL &&
           switch_symbol->binding == BIND_CODE &&
           switch_symbol->section == 0);
    assert(switch_loop_symbol != NULL &&
           switch_loop_symbol->type == SYM_GLOBAL &&
           switch_loop_symbol->binding == BIND_CODE &&
           switch_loop_symbol->section == 0);
    assert(switch_fallthrough_symbol != NULL &&
           switch_fallthrough_symbol->type == SYM_GLOBAL &&
           switch_fallthrough_symbol->binding == BIND_CODE &&
           switch_fallthrough_symbol->section == 0);
    assert(switch_if_symbol != NULL &&
           switch_if_symbol->type == SYM_GLOBAL &&
           switch_if_symbol->binding == BIND_CODE &&
           switch_if_symbol->section == 0);
    assert(switch_no_default_symbol != NULL &&
           switch_no_default_symbol->type == SYM_GLOBAL &&
           switch_no_default_symbol->binding == BIND_CODE &&
           switch_no_default_symbol->section == 0);
    assert(while_continue_symbol != NULL &&
           while_continue_symbol->type == SYM_GLOBAL &&
           while_continue_symbol->binding == BIND_CODE &&
           while_continue_symbol->section == 0);
    assert(for_break_symbol != NULL &&
           for_break_symbol->type == SYM_GLOBAL &&
           for_break_symbol->binding == BIND_CODE &&
           for_break_symbol->section == 0);
    assert(do_control_symbol != NULL &&
           do_control_symbol->type == SYM_GLOBAL &&
           do_control_symbol->binding == BIND_CODE &&
           do_control_symbol->section == 0);
    assert(loop_return_symbol != NULL &&
           loop_return_symbol->type == SYM_GLOBAL &&
           loop_return_symbol->binding == BIND_CODE &&
           loop_return_symbol->section == 0);
    if ((arch == ARCH_X86 && sizeof(void*) == 4u) ||
        (arch == ARCH_X64 && sizeof(void*) == 8u)) {
        size_t mapping_size;
        void* memory = map_text(object, text, &mapping_size);
        unsigned long long RINOS_ABI (*function)(void);
        unsigned long long RINOS_ABI (*parameter_function)(unsigned long long);
        unsigned long long RINOS_ABI (*const_parameter_function)(unsigned long long);
        unsigned long long RINOS_ABI (*add_function)(unsigned long long);
        unsigned long long RINOS_ABI (*carry_function)(unsigned long long);
        unsigned long long RINOS_ABI (*subtract_function)(unsigned long long);
        unsigned long long RINOS_ABI (*local_function)(unsigned long long);
        unsigned long long RINOS_ABI (*const_local_function)(unsigned long long);
        unsigned long long RINOS_ABI (*narrow_function)(unsigned int);
        int RINOS_ABI (*equal_function)(unsigned long long);
        int RINOS_ABI (*not_equal_function)(unsigned long long);
        int RINOS_ABI (*unsigned_less_function)(unsigned long long);
        int RINOS_ABI (*signed_less_function)(long long);
        int RINOS_ABI (*unsigned_le_function)(unsigned long long);
        int RINOS_ABI (*unsigned_ge_function)(unsigned long long);
        int RINOS_ABI (*signed_le_function)(long long);
        int RINOS_ABI (*signed_ge_function)(long long);
        unsigned long long RINOS_ABI (*lshift_function)(unsigned long long,
                                              unsigned int);
        unsigned long long RINOS_ABI (*lshr_function)(unsigned long long,
                                            unsigned int);
        long long RINOS_ABI (*ashr_function)(long long, unsigned int);
        unsigned long long RINOS_ABI (*conditional_function)(int);
        unsigned long long RINOS_ABI (*conditional_assign_function)(
            int, unsigned long long);
        unsigned long long RINOS_ABI (*conditional_compound_function)(
            int, unsigned long long);
        unsigned long long RINOS_ABI (*pure_comma_compound_function)(
            int, unsigned long long);
        unsigned long long RINOS_ABI (*size_align_compound_function)(
            unsigned long long);
        unsigned long long RINOS_ABI (*truth_conditional_function)(unsigned long long);
        unsigned long long RINOS_ABI (*mul_function)(unsigned long long);
        unsigned long long RINOS_ABI (*call_function)(unsigned long long);
        unsigned long long RINOS_ABI (*call_local_function)(unsigned long long);
        unsigned long long RINOS_ABI (*indirect_call_function)(
            unsigned long long RINOS_ABI (*)(unsigned long long),
            unsigned long long);
        unsigned long long RINOS_ABI (*variadic_call_function)(
            unsigned long long);
        unsigned long long RINOS_ABI (*variadic_scalar_call_function)(
            unsigned int, const void*);
        int RINOS_ABI (*variadic_overflow_call_function)(void);
        int RINOS_ABI (*variadic_named_overflow_call_function)(void);
        int RINOS_ABI (*va_list_forward_call_function)(void);
        int RINOS_ABI (*va_list_pointer_forward_call_function)(void);
        long long RINOS_ABI (*expect_function)(long long);
        unsigned long long RINOS_ABI (*assignment_function)(unsigned long long);
        unsigned long long RINOS_ABI (*compound_function)(unsigned long long);
        unsigned long long RINOS_ABI (*postincrement_function)(unsigned long long);
        int RINOS_ABI (*logical_not_function)(unsigned long long);
        int RINOS_ABI (*logical_and_function)(unsigned long long);
        int RINOS_ABI (*logical_or_function)(unsigned long long);
        unsigned long long RINOS_ABI (*comma_function)(unsigned long long);
        unsigned long long RINOS_ABI (*udiv_function)(unsigned long long);
        unsigned long long RINOS_ABI (*udiv_small_function)(unsigned long long);
        unsigned long long RINOS_ABI (*umod_function)(unsigned long long);
        long long RINOS_ABI (*sdiv_function)(long long);
        long long RINOS_ABI (*smod_function)(long long);
        unsigned long long RINOS_ABI (*branch_assign_function)(
            int, unsigned long long);
        unsigned long long RINOS_ABI (*branch_read_function)(
            int, unsigned long long);
        unsigned long long RINOS_ABI (*forward_goto_function)(
            int, unsigned long long);
        unsigned long long RINOS_ABI (*backward_goto_function)(
            unsigned int, unsigned long long);
        unsigned long long RINOS_ABI (*while_loop_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*for_loop_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*do_loop_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*nested_while_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*nested_for_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*nested_do_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*switch_function)(
            unsigned int, unsigned long long);
        unsigned long long RINOS_ABI (*switch_loop_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*switch_fallthrough_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*switch_if_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*switch_no_default_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*while_continue_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*for_break_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*do_control_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*loop_return_function)(
            unsigned long long, unsigned int);
        void* address = symbol_address(memory, symbol);
        memcpy(&function, &address, sizeof(function));
        assert(function() == 0x1122334455667788ULL);
        address = symbol_address(memory, parameter_symbol);
        memcpy(&parameter_function, &address, sizeof(parameter_function));
        assert(parameter_function(0x8899aabbccddeeffULL) ==
               0x8899aabbccddeeffULL);
        address = symbol_address(memory, const_parameter_symbol);
        memcpy(&const_parameter_function, &address,
               sizeof(const_parameter_function));
        assert(const_parameter_function(0x7766554433221100ULL) ==
               0x7766554433221100ULL);
        address = symbol_address(memory, add_symbol);
        memcpy(&add_function, &address, sizeof(add_function));
        assert(add_function(0x1020304050607080ULL) ==
               0x1122334455667788ULL);
        address = symbol_address(memory, indirect_call_symbol);
        memcpy(&indirect_call_function, &address,
               sizeof(indirect_call_function));
        assert(indirect_call_function(add_function,
                                      0x0000000200000002ULL) ==
               0x010203060506070aULL);
        address = symbol_address(memory, variadic_call_symbol);
        memcpy(&variadic_call_function, &address,
               sizeof(variadic_call_function));
        assert(variadic_call_function(0x0000000200000002ULL) ==
               0x0000000200000007ULL);
        address = symbol_address(memory, variadic_scalar_call_symbol);
        memcpy(&variadic_scalar_call_function, &address,
               sizeof(variadic_scalar_call_function));
        assert(variadic_scalar_call_function(0x1234u, object) == 60ULL);
        address = symbol_address(memory, variadic_overflow_call_symbol);
        memcpy(&variadic_overflow_call_function, &address,
               sizeof(variadic_overflow_call_function));
        assert(variadic_overflow_call_function() == 36);
        address = symbol_address(
            memory, variadic_named_overflow_call_symbol);
        memcpy(&variadic_named_overflow_call_function, &address,
               sizeof(variadic_named_overflow_call_function));
        assert(variadic_named_overflow_call_function() == 99);
        address = symbol_address(memory, va_list_forward_call_symbol);
        memcpy(&va_list_forward_call_function, &address,
               sizeof(va_list_forward_call_function));
        assert(va_list_forward_call_function() == 40);
        address = symbol_address(
            memory, va_list_pointer_forward_call_symbol);
        memcpy(&va_list_pointer_forward_call_function, &address,
               sizeof(va_list_pointer_forward_call_function));
        assert(va_list_pointer_forward_call_function() == 40);
        address = symbol_address(memory, carry_symbol);
        memcpy(&carry_function, &address, sizeof(carry_function));
        assert(carry_function(1ULL) == 0x0000000100000000ULL);
        address = symbol_address(memory, subtract_symbol);
        memcpy(&subtract_function, &address, sizeof(subtract_function));
        assert(subtract_function(0x0000000100000000ULL) ==
               0xffffffffULL);
        address = symbol_address(memory, local_symbol);
        memcpy(&local_function, &address, sizeof(local_function));
        assert(local_function(0x8899aabbccddeeffULL) ==
               0x8899aabbccddeeffULL);
        address = symbol_address(memory, const_local_symbol);
        memcpy(&const_local_function, &address,
               sizeof(const_local_function));
        assert(const_local_function(0x7766554433221100ULL) ==
               0x7766554433221100ULL);
        address = symbol_address(memory, narrow_symbol);
        memcpy(&narrow_function, &address, sizeof(narrow_function));
        assert(narrow_function(0xdeadbeefu) == 0x00000000deadbeefULL);
        address = symbol_address(memory, equal_symbol);
        memcpy(&equal_function, &address, sizeof(equal_function));
        assert(equal_function(0x1122334455667788ULL) == 1);
        assert(equal_function(0x1122334455667789ULL) == 0);
        address = symbol_address(memory, not_equal_symbol);
        memcpy(&not_equal_function, &address, sizeof(not_equal_function));
        assert(not_equal_function(0x1122334455667788ULL) == 0);
        assert(not_equal_function(0x1122334455667789ULL) == 1);
        address = symbol_address(memory, unsigned_less_symbol);
        memcpy(&unsigned_less_function, &address,
               sizeof(unsigned_less_function));
        assert(unsigned_less_function(0xffffffffULL) == 1);
        assert(unsigned_less_function(0x0000000100000000ULL) == 0);
        address = symbol_address(memory, signed_less_symbol);
        memcpy(&signed_less_function, &address, sizeof(signed_less_function));
        assert(signed_less_function(-1LL) == 1);
        assert(signed_less_function(1LL) == 0);
        address = symbol_address(memory, unsigned_le_symbol);
        memcpy(&unsigned_le_function, &address,
               sizeof(unsigned_le_function));
        assert(unsigned_le_function(0x0000000100000000ULL) == 1);
        assert(unsigned_le_function(0x0000000100000001ULL) == 0);
        address = symbol_address(memory, unsigned_ge_symbol);
        memcpy(&unsigned_ge_function, &address,
               sizeof(unsigned_ge_function));
        assert(unsigned_ge_function(0x00000000ffffffffULL) == 0);
        assert(unsigned_ge_function(0x0000000100000000ULL) == 1);
        address = symbol_address(memory, signed_le_symbol);
        memcpy(&signed_le_function, &address, sizeof(signed_le_function));
        assert(signed_le_function(-1LL) == 1);
        assert(signed_le_function(0LL) == 1);
        assert(signed_le_function(1LL) == 0);
        address = symbol_address(memory, signed_ge_symbol);
        memcpy(&signed_ge_function, &address, sizeof(signed_ge_function));
        assert(signed_ge_function(-1LL) == 0);
        assert(signed_ge_function(0LL) == 1);
        assert(signed_ge_function(1LL) == 1);
        address = symbol_address(memory, lshift_symbol);
        memcpy(&lshift_function, &address, sizeof(lshift_function));
        assert(lshift_function(1ULL, 0u) == 1ULL);
        assert(lshift_function(1ULL, 32u) == 0x0000000100000000ULL);
        assert(lshift_function(0x80000001ULL, 31u) ==
               0x4000000080000000ULL);
        address = symbol_address(memory, lshr_symbol);
        memcpy(&lshr_function, &address, sizeof(lshr_function));
        assert(lshr_function(0x8000000100000000ULL, 32u) == 0x80000001ULL);
        assert(lshr_function(0x8000000000000000ULL, 63u) == 1ULL);
        address = symbol_address(memory, ashr_symbol);
        memcpy(&ashr_function, &address, sizeof(ashr_function));
        assert(ashr_function(-0x0000000100000000LL, 32u) == -1LL);
        assert(ashr_function(0x4000000000000000LL, 62u) == 1LL);
        address = symbol_address(memory, conditional_symbol);
        memcpy(&conditional_function, &address, sizeof(conditional_function));
        assert(conditional_function(0) == 0x8877665544332211ULL);
        assert(conditional_function(1) == 0x1122334455667788ULL);
        address = symbol_address(memory, conditional_assign_symbol);
        memcpy(&conditional_assign_function, &address,
               sizeof(conditional_assign_function));
        assert(conditional_assign_function(0, 0ULL) == 7ULL);
        assert(conditional_assign_function(1, 0ULL) == 7ULL);
        assert(conditional_assign_function(1, 5ULL) == 6ULL);
        address = symbol_address(memory, conditional_compound_symbol);
        memcpy(&conditional_compound_function, &address,
               sizeof(conditional_compound_function));
        assert(conditional_compound_function(0, 5ULL) == 7ULL);
        assert(conditional_compound_function(1, 5ULL) == 6ULL);
        address = symbol_address(memory, pure_comma_compound_symbol);
        memcpy(&pure_comma_compound_function, &address,
               sizeof(pure_comma_compound_function));
        assert(pure_comma_compound_function(0, 5ULL) == 7ULL);
        assert(pure_comma_compound_function(1, 5ULL) == 6ULL);
        address = symbol_address(memory, size_align_compound_symbol);
        memcpy(&size_align_compound_function, &address,
               sizeof(size_align_compound_function));
        assert(size_align_compound_function(5ULL) == 7ULL);
        address = symbol_address(memory, truth_conditional_symbol);
        memcpy(&truth_conditional_function, &address,
               sizeof(truth_conditional_function));
        assert(truth_conditional_function(0ULL) ==
               0x8877665544332211ULL);
        assert(truth_conditional_function(1ULL) ==
               0x1122334455667788ULL);
        address = symbol_address(memory, mul_symbol);
        memcpy(&mul_function, &address, sizeof(mul_function));
        assert(mul_function(0ULL) == 0ULL);
        assert(mul_function(1ULL) == 0x0000000100000001ULL);
        assert(mul_function(0x1122334455667788ULL) ==
               0x6688aacc55667788ULL);
        assert(mul_function(0xffffffffffffffffULL) ==
               0xfffffffeffffffffULL);
        address = symbol_address(memory, call_symbol);
        memcpy(&call_function, &address, sizeof(call_function));
        assert(call_function(0x8899aabbccddeeffULL) ==
               0x8899aabbccddeeffULL);
        address = symbol_address(memory, call_local_symbol);
        memcpy(&call_local_function, &address, sizeof(call_local_function));
        assert(call_local_function(0x0000000200000002ULL) ==
               0x0000000400000004ULL);
        address = symbol_address(memory, expect_symbol);
        memcpy(&expect_function, &address, sizeof(expect_function));
        assert(expect_function(0x0000000100000005LL) ==
               0x0000000100000005LL);
        assert(expect_function(-0x0000000100000005LL) ==
               -0x0000000100000005LL);
        address = symbol_address(memory, assignment_symbol);
        memcpy(&assignment_function, &address, sizeof(assignment_function));
        assert(assignment_function(0x8899aabbccddeeffULL) ==
               0x8899aabbccddeeffULL);
        address = symbol_address(memory, compound_symbol);
        memcpy(&compound_function, &address, sizeof(compound_function));
        assert(compound_function(0x0000000200000002ULL) ==
               0x00000003fffffffcULL);
        address = symbol_address(memory, postincrement_symbol);
        memcpy(&postincrement_function, &address,
               sizeof(postincrement_function));
        assert(postincrement_function(0xffffffffffffffffULL) ==
               0xffffffffffffffffULL);
        address = symbol_address(memory, logical_not_symbol);
        memcpy(&logical_not_function, &address, sizeof(logical_not_function));
        assert(logical_not_function(0ULL) == 1);
        assert(logical_not_function(1ULL) == 0);
        address = symbol_address(memory, logical_and_symbol);
        memcpy(&logical_and_function, &address, sizeof(logical_and_function));
        assert(logical_and_function(0ULL) == 0);
        assert(logical_and_function(1ULL) == 1);
        address = symbol_address(memory, logical_or_symbol);
        memcpy(&logical_or_function, &address, sizeof(logical_or_function));
        assert(logical_or_function(0ULL) == 0);
        assert(logical_or_function(1ULL) == 1);
        address = symbol_address(memory, comma_symbol);
        memcpy(&comma_function, &address, sizeof(comma_function));
        assert(comma_function(0x8899aabbccddeeffULL) ==
               0x8899aabbccddeeffULL);
        address = symbol_address(memory, udiv_symbol);
        memcpy(&udiv_function, &address, sizeof(udiv_function));
        assert(udiv_function(0x0000000200000002ULL) == 2ULL);
        address = symbol_address(memory, udiv_small_symbol);
        memcpy(&udiv_small_function, &address,
               sizeof(udiv_small_function));
        assert(udiv_small_function(10ULL) == 3ULL);
        address = symbol_address(memory, umod_symbol);
        memcpy(&umod_function, &address, sizeof(umod_function));
        assert(umod_function(0x0000000200000003ULL) == 1ULL);
        address = symbol_address(memory, sdiv_symbol);
        memcpy(&sdiv_function, &address, sizeof(sdiv_function));
        assert(sdiv_function(-10LL) == -3LL);
        address = symbol_address(memory, smod_symbol);
        memcpy(&smod_function, &address, sizeof(smod_function));
        assert(smod_function(-10LL) == -1LL);
        address = symbol_address(memory, branch_assign_symbol);
        memcpy(&branch_assign_function, &address,
               sizeof(branch_assign_function));
        assert(branch_assign_function(1, 0x0000000200000002ULL) ==
               0x0000000300000003ULL);
        assert(branch_assign_function(0, 0x0000000200000002ULL) ==
               0x0000000200000001ULL);
        address = symbol_address(memory, branch_read_symbol);
        memcpy(&branch_read_function, &address,
               sizeof(branch_read_function));
        assert(branch_read_function(1, 10ULL) == 22ULL);
        assert(branch_read_function(0, 10ULL) == 17ULL);
        address = symbol_address(memory, forward_goto_symbol);
        memcpy(&forward_goto_function, &address,
               sizeof(forward_goto_function));
        assert(forward_goto_function(1, 0x0000000200000002ULL) ==
               0x000000020000000cULL);
        assert(forward_goto_function(0, 0x0000000200000002ULL) ==
               0x000000020000000eULL);
        address = symbol_address(memory, backward_goto_symbol);
        memcpy(&backward_goto_function, &address,
               sizeof(backward_goto_function));
        assert(backward_goto_function(0u, 0x0000000200000002ULL) ==
               0x0000000200000002ULL);
        assert(backward_goto_function(3u, 0x0000000200000002ULL) ==
               0x0000000500000005ULL);
        address = symbol_address(memory, while_loop_symbol);
        memcpy(&while_loop_function, &address, sizeof(while_loop_function));
        assert(while_loop_function(0x0000000200000002ULL, 3u) ==
               0x0000000500000005ULL);
        address = symbol_address(memory, for_loop_symbol);
        memcpy(&for_loop_function, &address, sizeof(for_loop_function));
        assert(for_loop_function(0x0000000200000002ULL, 3u) ==
               0x0000000500000005ULL);
        address = symbol_address(memory, do_loop_symbol);
        memcpy(&do_loop_function, &address, sizeof(do_loop_function));
        assert(do_loop_function(0x0000000200000002ULL, 3u) ==
               0x0000000500000005ULL);
        address = symbol_address(memory, nested_while_symbol);
        memcpy(&nested_while_function, &address,
               sizeof(nested_while_function));
        assert(nested_while_function(0x0000000200000002ULL, 3u) ==
               0x0000000200000008ULL);
        address = symbol_address(memory, nested_for_symbol);
        memcpy(&nested_for_function, &address, sizeof(nested_for_function));
        assert(nested_for_function(0x0000000200000002ULL, 3u) ==
               0x000000020000000eULL);
        address = symbol_address(memory, nested_do_symbol);
        memcpy(&nested_do_function, &address, sizeof(nested_do_function));
        assert(nested_do_function(0x0000000200000002ULL, 3u) ==
               0x0000000200000008ULL);
        address = symbol_address(memory, switch_symbol);
        memcpy(&switch_function, &address, sizeof(switch_function));
        assert(switch_function(0u, 0x0000000200000002ULL) ==
               0x0000000200000003ULL);
        assert(switch_function(1u, 0x0000000200000002ULL) ==
               0x0000000200000004ULL);
        assert(switch_function(2u, 0x0000000200000002ULL) ==
               0x0000000200000005ULL);
        address = symbol_address(memory, switch_loop_symbol);
        memcpy(&switch_loop_function, &address,
               sizeof(switch_loop_function));
        assert(switch_loop_function(0x0000000200000002ULL, 3u) ==
               0x0000000200000007ULL);
        assert(switch_loop_function(0x0000000200000002ULL, 1u) ==
               0x0000000200000003ULL);
        assert(switch_loop_function(0x0000000200000002ULL, 0u) ==
               0x0000000200000002ULL);
        address = symbol_address(memory, switch_fallthrough_symbol);
        memcpy(&switch_fallthrough_function, &address,
               sizeof(switch_fallthrough_function));
        assert(switch_fallthrough_function(0x0000000200000002ULL, 3u) ==
               0x0000000200000005ULL);
        assert(switch_fallthrough_function(0x0000000200000002ULL, 2u) ==
               0x0000000200000004ULL);
        assert(switch_fallthrough_function(0x0000000200000002ULL, 1u) ==
               0x0000000200000006ULL);
        address = symbol_address(memory, switch_if_symbol);
        memcpy(&switch_if_function, &address, sizeof(switch_if_function));
        assert(switch_if_function(0x0000000200000002ULL, 0u) ==
               0x0000000200000003ULL);
        assert(switch_if_function(0x0000000200000002ULL, 1u) ==
               0x0000000200000005ULL);
        address = symbol_address(memory, switch_no_default_symbol);
        memcpy(&switch_no_default_function, &address,
               sizeof(switch_no_default_function));
        assert(switch_no_default_function(0x0000000200000002ULL, 0u) ==
               0x0000000200000003ULL);
        assert(switch_no_default_function(0x0000000200000002ULL, 1u) ==
               0x0000000200000004ULL);
        assert(switch_no_default_function(0x0000000200000002ULL, 2u) ==
               0x0000000200000002ULL);
        address = symbol_address(memory, while_continue_symbol);
        memcpy(&while_continue_function, &address,
               sizeof(while_continue_function));
        assert(while_continue_function(0x0000000200000002ULL, 4u) ==
               0x0000000500000005ULL);
        address = symbol_address(memory, for_break_symbol);
        memcpy(&for_break_function, &address, sizeof(for_break_function));
        assert(for_break_function(0x0000000200000002ULL, 4u) ==
               0x0000000400000004ULL);
        address = symbol_address(memory, do_control_symbol);
        memcpy(&do_control_function, &address, sizeof(do_control_function));
        assert(do_control_function(0x0000000200000002ULL, 4u) ==
               0x0000000300000003ULL);
        address = symbol_address(memory, loop_return_symbol);
        memcpy(&loop_return_function, &address,
               sizeof(loop_return_function));
        assert(loop_return_function(0x0000000200000002ULL, 4u) ==
               0x0000000400000004ULL);
        assert(verified_unmap(memory, mapping_size) == 0);
    }
    objfile_free(object);
}

static void verify_object(const char* path, uint16_t arch)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* call;
    ObjSymbol* indirect_call;
    ObjSymbol* indirect_parameter;
    ObjSymbol* helper;
    ObjSymbol* load;
    ObjSymbol* control;
    ObjSymbol* index;
    ObjSymbol* local_array;
    ObjSymbol* local_pointer_array;
    ObjSymbol* local_string_array;
    ObjSymbol* nested_array;
    ObjSymbol* struct_symbol;
    ObjSymbol* struct_copy_pointer;
    ObjSymbol* nested_struct;
    ObjSymbol* union_symbol;
    ObjSymbol* compound_struct;
    ObjSymbol* compound_array;
    ObjSymbol* compound_scalar;
    ObjSymbol* struct_parameter;
    ObjSymbol* struct_argument_call;
    ObjSymbol* pair_return;
    ObjSymbol* pair_return_call;
    ObjSymbol* triple_return;
    ObjSymbol* triple_return_call;
    ObjSymbol* large_return;
    ObjSymbol* large_return_call;
    ObjSymbol* pointer_add;
    ObjSymbol* pointer_sub;
    ObjSymbol* conditional;
    ObjSymbol* logical_and;
    ObjSymbol* logical_or;
    ObjSymbol* pointer_compound;
    ObjSymbol* pointer_postincrement;
    ObjSymbol* lvalue_once;
    ObjSymbol* pointer_difference;
    ObjSymbol* common_subexpression;
    ObjSymbol* switch_symbol;
    ObjSymbol* nested_switch;
    ObjSymbol* switch_promotion;
    ObjSymbol* switch_skips_prefix;
    ObjSymbol* switch_nested_case;
    ObjSymbol* switch_while_case;
    ObjSymbol* switch_do_case;
    ObjSymbol* switch_for_case;
    ObjSymbol* switch_labeled_case;
    ObjSymbol* switch_do_return_case;
    ObjSymbol* switch_for_return_case;
    ObjSymbol* switch_do_continue_case;
    ObjSymbol* switch_for_continue_case;
    ObjSymbol* switch_do_break_case;
    ObjSymbol* switch_for_break_case;
    ObjReloc* relocation;
    assert(object != NULL && object->arch == arch);
    text = objfile_get_section(object, ".text");
    call = objfile_find_symbol(object, "verified_call");
    indirect_call = objfile_find_symbol(object, "verified_indirect_call");
    indirect_parameter = objfile_find_symbol(
        object, "verified_indirect_parameter");
    helper = objfile_find_symbol(
        object, "tests/verified_backend.c::verified_helper");
    load = objfile_find_symbol(object, "verified_load");
    control = objfile_find_symbol(object, "verified_control");
    index = objfile_find_symbol(object, "verified_index");
    local_array = objfile_find_symbol(object, "verified_local_array");
    local_pointer_array = objfile_find_symbol(
        object, "verified_local_pointer_array");
    local_string_array = objfile_find_symbol(
        object, "verified_local_string_array");
    nested_array = objfile_find_symbol(object, "verified_nested_array");
    struct_symbol = objfile_find_symbol(object, "verified_struct");
    struct_copy_pointer = objfile_find_symbol(
        object, "verified_struct_copy_pointer");
    nested_struct = objfile_find_symbol(
        object, "verified_nested_struct");
    union_symbol = objfile_find_symbol(object, "verified_union");
    compound_struct = objfile_find_symbol(
        object, "verified_compound_struct");
    compound_array = objfile_find_symbol(
        object, "verified_compound_array");
    compound_scalar = objfile_find_symbol(
        object, "verified_compound_scalar");
    struct_parameter = objfile_find_symbol(
        object, "verified_struct_parameter");
    struct_argument_call = objfile_find_symbol(
        object, "verified_struct_argument_call");
    pair_return = objfile_find_symbol(
        object, "verified_pair_return");
    pair_return_call = objfile_find_symbol(
        object, "verified_pair_return_call");
    triple_return = objfile_find_symbol(
        object, "verified_triple_return");
    triple_return_call = objfile_find_symbol(
        object, "verified_triple_return_call");
    large_return = objfile_find_symbol(
        object, "verified_large_return");
    large_return_call = objfile_find_symbol(
        object, "verified_large_return_call");
    pointer_add = objfile_find_symbol(object, "verified_pointer_add");
    pointer_sub = objfile_find_symbol(object, "verified_pointer_sub");
    conditional = objfile_find_symbol(object, "verified_conditional");
    logical_and = objfile_find_symbol(object, "verified_logical_and");
    logical_or = objfile_find_symbol(object, "verified_logical_or");
    pointer_compound = objfile_find_symbol(
        object, "verified_pointer_compound");
    pointer_postincrement = objfile_find_symbol(
        object, "verified_pointer_postincrement");
    lvalue_once = objfile_find_symbol(object, "verified_lvalue_once");
    pointer_difference = objfile_find_symbol(
        object, "verified_pointer_difference");
    common_subexpression = objfile_find_symbol(
        object, "verified_common_subexpression");
    switch_symbol = objfile_find_symbol(object, "verified_switch");
    nested_switch = objfile_find_symbol(object, "verified_nested_switch");
    switch_promotion = objfile_find_symbol(
        object, "verified_switch_promotion");
    switch_skips_prefix = objfile_find_symbol(
        object, "verified_switch_skips_prefix");
    switch_nested_case = objfile_find_symbol(
        object, "verified_switch_nested_case");
    switch_while_case = objfile_find_symbol(
        object, "verified_switch_while_case");
    switch_do_case = objfile_find_symbol(object, "verified_switch_do_case");
    switch_for_case = objfile_find_symbol(object, "verified_switch_for_case");
    switch_labeled_case = objfile_find_symbol(
        object, "verified_switch_labeled_case");
    switch_do_return_case = objfile_find_symbol(
        object, "verified_switch_do_return_case");
    switch_for_return_case = objfile_find_symbol(
        object, "verified_switch_for_return_case");
    switch_do_continue_case = objfile_find_symbol(
        object, "verified_switch_do_continue_case");
    switch_for_continue_case = objfile_find_symbol(
        object, "verified_switch_for_continue_case");
    switch_do_break_case = objfile_find_symbol(
        object, "verified_switch_do_break_case");
    switch_for_break_case = objfile_find_symbol(
        object, "verified_switch_for_break_case");
    assert(text != NULL && text->size != 0u && text->memory_size == text->size);
    assert((text->flags & (SECT_FLAG_ALLOC | SECT_FLAG_EXEC)) ==
           (SECT_FLAG_ALLOC | SECT_FLAG_EXEC));
    assert((text->flags & SECT_FLAG_WRITE) == 0u);
    assert(call != NULL && call->type == SYM_GLOBAL && call->section == 0);
    assert(indirect_call != NULL && indirect_call->type == SYM_GLOBAL &&
           indirect_call->section == 0);
    assert(indirect_parameter != NULL &&
           indirect_parameter->type == SYM_GLOBAL &&
           indirect_parameter->section == 0);
    assert(helper != NULL && helper->type == SYM_LOCAL && helper->section == 0);
    assert(load != NULL && load->type == SYM_GLOBAL && load->section == 0);
    assert(control != NULL && control->type == SYM_GLOBAL &&
           control->section == 0);
    assert(index != NULL && index->type == SYM_GLOBAL && index->section == 0);
    assert(local_array != NULL && local_array->type == SYM_GLOBAL &&
           local_array->section == 0);
    assert(local_pointer_array != NULL &&
           local_pointer_array->type == SYM_GLOBAL &&
           local_pointer_array->section == 0);
    assert(local_string_array != NULL &&
           local_string_array->type == SYM_GLOBAL &&
           local_string_array->section == 0);
    assert(nested_array != NULL && nested_array->type == SYM_GLOBAL &&
           nested_array->section == 0);
    assert(struct_symbol != NULL && struct_symbol->type == SYM_GLOBAL &&
           struct_symbol->section == 0);
    assert(struct_copy_pointer != NULL &&
           struct_copy_pointer->type == SYM_GLOBAL &&
           struct_copy_pointer->section == 0);
    assert(nested_struct != NULL && nested_struct->type == SYM_GLOBAL &&
           nested_struct->section == 0);
    assert(union_symbol != NULL && union_symbol->type == SYM_GLOBAL &&
           union_symbol->section == 0);
    assert(compound_struct != NULL &&
           compound_struct->type == SYM_GLOBAL &&
           compound_struct->section == 0);
    assert(compound_array != NULL &&
           compound_array->type == SYM_GLOBAL &&
           compound_array->section == 0);
    assert(compound_scalar != NULL &&
           compound_scalar->type == SYM_GLOBAL &&
           compound_scalar->section == 0);
    assert(struct_parameter != NULL &&
           struct_parameter->type == SYM_GLOBAL &&
           struct_parameter->section == 0);
    assert(struct_argument_call != NULL &&
           struct_argument_call->type == SYM_GLOBAL &&
           struct_argument_call->section == 0);
    assert(pair_return != NULL && pair_return->type == SYM_GLOBAL &&
           pair_return->section == 0);
    assert(pair_return_call != NULL &&
           pair_return_call->type == SYM_GLOBAL &&
           pair_return_call->section == 0);
    assert(triple_return != NULL && triple_return->type == SYM_GLOBAL &&
           triple_return->section == 0);
    assert(triple_return_call != NULL &&
           triple_return_call->type == SYM_GLOBAL &&
           triple_return_call->section == 0);
    assert(large_return != NULL && large_return->type == SYM_GLOBAL &&
           large_return->section == 0);
    assert(large_return_call != NULL &&
           large_return_call->type == SYM_GLOBAL &&
           large_return_call->section == 0);
    assert(pointer_add != NULL && pointer_add->type == SYM_GLOBAL &&
           pointer_add->section == 0);
    assert(pointer_sub != NULL && pointer_sub->type == SYM_GLOBAL &&
           pointer_sub->section == 0);
    assert(conditional != NULL && conditional->type == SYM_GLOBAL &&
           conditional->section == 0);
    assert(logical_and != NULL && logical_and->type == SYM_GLOBAL &&
           logical_and->section == 0);
    assert(logical_or != NULL && logical_or->type == SYM_GLOBAL &&
           logical_or->section == 0);
    assert(pointer_compound != NULL &&
           pointer_compound->type == SYM_GLOBAL &&
           pointer_compound->section == 0);
    assert(pointer_postincrement != NULL &&
           pointer_postincrement->type == SYM_GLOBAL &&
           pointer_postincrement->section == 0);
    assert(lvalue_once != NULL && lvalue_once->type == SYM_GLOBAL &&
           lvalue_once->section == 0);
    assert(pointer_difference != NULL &&
           pointer_difference->type == SYM_GLOBAL &&
           pointer_difference->section == 0);
    assert(common_subexpression != NULL &&
           common_subexpression->type == SYM_GLOBAL &&
           common_subexpression->section == 0);
    assert(switch_symbol != NULL && switch_symbol->type == SYM_GLOBAL &&
           switch_symbol->section == 0);
    assert(nested_switch != NULL && nested_switch->type == SYM_GLOBAL &&
           nested_switch->section == 0);
    assert(switch_promotion != NULL &&
           switch_promotion->type == SYM_GLOBAL &&
           switch_promotion->section == 0);
    assert(switch_skips_prefix != NULL &&
           switch_skips_prefix->type == SYM_GLOBAL &&
           switch_skips_prefix->section == 0);
    assert(switch_nested_case != NULL &&
           switch_nested_case->type == SYM_GLOBAL &&
           switch_nested_case->section == 0);
    assert(switch_while_case != NULL &&
           switch_while_case->type == SYM_GLOBAL &&
           switch_while_case->section == 0);
    assert(switch_do_case != NULL && switch_do_case->type == SYM_GLOBAL &&
           switch_do_case->section == 0);
    assert(switch_for_case != NULL && switch_for_case->type == SYM_GLOBAL &&
           switch_for_case->section == 0);
    assert(switch_labeled_case != NULL &&
           switch_labeled_case->type == SYM_GLOBAL &&
           switch_labeled_case->section == 0);
    assert(switch_do_return_case != NULL &&
           switch_do_return_case->type == SYM_GLOBAL &&
           switch_do_return_case->section == 0);
    assert(switch_for_return_case != NULL &&
           switch_for_return_case->type == SYM_GLOBAL &&
           switch_for_return_case->section == 0);
    assert(switch_do_continue_case != NULL &&
           switch_do_continue_case->type == SYM_GLOBAL &&
           switch_do_continue_case->section == 0);
    assert(switch_for_continue_case != NULL &&
           switch_for_continue_case->type == SYM_GLOBAL &&
           switch_for_continue_case->section == 0);
    assert(switch_do_break_case != NULL &&
           switch_do_break_case->type == SYM_GLOBAL &&
           switch_do_break_case->section == 0);
    assert(switch_for_break_case != NULL &&
           switch_for_break_case->type == SYM_GLOBAL &&
           switch_for_break_case->section == 0);
    assert(object->symbol_count == 51);
    {
        size_t relocation_count = 0u;
        size_t absolute_count = 0u;
        bool found_helper = false;
        bool found_indirect_target = false;
        bool found_struct_call = false;
        bool found_pair_return = false;
        bool found_triple_return = false;
        bool found_large_return = false;
        for (relocation = text->relocs; relocation;
             relocation = relocation->next) {
            if (relocation->type == RELOC_REL32) {
                ++relocation_count;
                if (strcmp(relocation->symbol_name,
                           "tests/verified_backend.c::verified_helper") == 0) {
                    found_helper = true;
                }
                if (strcmp(relocation->symbol_name,
                           "verified_struct_parameter") == 0) {
                    found_struct_call = true;
                }
                if (strcmp(relocation->symbol_name,
                           "verified_pair_return") == 0) {
                    found_pair_return = true;
                }
                if (strcmp(relocation->symbol_name,
                           "verified_triple_return") == 0) {
                    found_triple_return = true;
                }
                if (strcmp(relocation->symbol_name,
                           "verified_large_return") == 0) {
                    found_large_return = true;
                }
            } else {
                assert(relocation->type ==
                       (arch == ARCH_X64 ? RELOC_ABS64 : RELOC_ABS32U));
                ++absolute_count;
                if (strcmp(relocation->symbol_name,
                           "tests/verified_backend.c::verified_helper") == 0) {
                    found_indirect_target = true;
                }
            }
        }
        assert(relocation_count == 5u && absolute_count == 1u &&
               found_helper && found_indirect_target &&
               found_struct_call && found_pair_return &&
               found_triple_return && found_large_return);
    }
    objfile_free(object);
}

static void verify_float_binary_object(const char* path)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    size_t mapping_size;
    void* memory;
    void* address;
    float (RINOS_ABI *add_f32)(float, float);
    float (RINOS_ABI *sub_f32)(float, float);
    float (RINOS_ABI *mul_f32)(float, float);
    float (RINOS_ABI *div_f32)(float, float);
    double (RINOS_ABI *add_f64)(double, double);
    double (RINOS_ABI *sub_f64)(double, double);
    double (RINOS_ABI *mul_f64)(double, double);
    double (RINOS_ABI *div_f64)(double, double);
    double (RINOS_ABI *promote)(double, float);
    int (RINOS_ABI *predicates_f32)(float, float);
    int (RINOS_ABI *predicates_f64)(double, double);
    int (RINOS_ABI *truth_f32)(float);
    int (RINOS_ABI *truth_f64)(double);
    float (RINOS_ABI *neg_f32)(float);
    double (RINOS_ABI *neg_f64)(double);
    float (RINOS_ABI *compound_f32)(float, float);
    double (RINOS_ABI *compound_f64)(double, double);
    float (RINOS_ABI *incdec_f32)(float);
    double (RINOS_ABI *incdec_f64)(double);
    float (RINOS_ABI *from_i32)(int);
    double (RINOS_ABI *from_i64)(long long);
    float (RINOS_ABI *from_u32)(unsigned int);
    int (RINOS_ABI *to_i32)(float);
    long long (RINOS_ABI *to_i64)(double);
    unsigned int (RINOS_ABI *to_u32_f32)(float);
    unsigned int (RINOS_ABI *to_u32_f64)(double);
    float (RINOS_ABI *mixed_add_f32)(float, int);
    double (RINOS_ABI *mixed_add_f64)(double, unsigned int);
    int (RINOS_ABI *mixed_compare_i32)(double, int);
    int (RINOS_ABI *mixed_compare_u32)(float, unsigned int);
    float (RINOS_ABI *narrow_f64)(double);
    float (RINOS_ABI *from_u64_f32)(unsigned long long);
    double (RINOS_ABI *from_u64_f64)(unsigned long long);
    unsigned long long (RINOS_ABI *to_u64_f32)(float);
    unsigned long long (RINOS_ABI *to_u64_f64)(double);
    float (RINOS_ABI *pressure)(
        float, float, float, float, float, float, float, float,
        float, float, float, float, float, float, float, float,
        float, float, float, float, float, float, float, float,
        float, float, float, float, float, float, float, float);
    uint32_t nan_f32_bits = UINT32_C(0x7fc00000);
    uint64_t nan_f64_bits = UINT64_C(0x7ff8000000000000);
    float nan_f32;
    double nan_f64;
    assert(object != NULL && object->arch == ARCH_X64);
    text = objfile_get_section(object, ".text");
    memory = map_text(object, text, &mapping_size);
#define LOAD_FLOAT_FUNCTION(name, function) \
    do { \
        ObjSymbol* symbol = objfile_find_symbol(object, (name)); \
        assert(symbol != NULL && symbol->type == SYM_GLOBAL && \
               symbol->binding == BIND_CODE && symbol->section == 0); \
        address = symbol_address(memory, symbol); \
        memcpy(&(function), &address, sizeof(function)); \
    } while (0)
    LOAD_FLOAT_FUNCTION("verified_fp_add_f32", add_f32);
    LOAD_FLOAT_FUNCTION("verified_fp_sub_f32", sub_f32);
    LOAD_FLOAT_FUNCTION("verified_fp_mul_f32", mul_f32);
    LOAD_FLOAT_FUNCTION("verified_fp_div_f32", div_f32);
    LOAD_FLOAT_FUNCTION("verified_fp_add_f64", add_f64);
    LOAD_FLOAT_FUNCTION("verified_fp_sub_f64", sub_f64);
    LOAD_FLOAT_FUNCTION("verified_fp_mul_f64", mul_f64);
    LOAD_FLOAT_FUNCTION("verified_fp_div_f64", div_f64);
    LOAD_FLOAT_FUNCTION("verified_fp_promote_f32", promote);
    LOAD_FLOAT_FUNCTION("verified_fp_predicates_f32", predicates_f32);
    LOAD_FLOAT_FUNCTION("verified_fp_predicates_f64", predicates_f64);
    LOAD_FLOAT_FUNCTION("verified_fp_truth_f32", truth_f32);
    LOAD_FLOAT_FUNCTION("verified_fp_truth_f64", truth_f64);
    LOAD_FLOAT_FUNCTION("verified_fp_neg_f32", neg_f32);
    LOAD_FLOAT_FUNCTION("verified_fp_neg_f64", neg_f64);
    LOAD_FLOAT_FUNCTION("verified_fp_compound_f32", compound_f32);
    LOAD_FLOAT_FUNCTION("verified_fp_compound_f64", compound_f64);
    LOAD_FLOAT_FUNCTION("verified_fp_incdec_f32", incdec_f32);
    LOAD_FLOAT_FUNCTION("verified_fp_incdec_f64", incdec_f64);
    LOAD_FLOAT_FUNCTION("verified_fp_from_i32", from_i32);
    LOAD_FLOAT_FUNCTION("verified_fp_from_i64", from_i64);
    LOAD_FLOAT_FUNCTION("verified_fp_from_u32", from_u32);
    LOAD_FLOAT_FUNCTION("verified_fp_to_i32", to_i32);
    LOAD_FLOAT_FUNCTION("verified_fp_to_i64", to_i64);
    LOAD_FLOAT_FUNCTION("verified_fp_to_u32_f32", to_u32_f32);
    LOAD_FLOAT_FUNCTION("verified_fp_to_u32_f64", to_u32_f64);
    LOAD_FLOAT_FUNCTION("verified_fp_mixed_add_f32", mixed_add_f32);
    LOAD_FLOAT_FUNCTION("verified_fp_mixed_add_f64", mixed_add_f64);
    LOAD_FLOAT_FUNCTION("verified_fp_mixed_compare_i32", mixed_compare_i32);
    LOAD_FLOAT_FUNCTION("verified_fp_mixed_compare_u32", mixed_compare_u32);
    LOAD_FLOAT_FUNCTION("verified_fp_narrow_f64", narrow_f64);
    LOAD_FLOAT_FUNCTION("verified_fp_from_u64_f32", from_u64_f32);
    LOAD_FLOAT_FUNCTION("verified_fp_from_u64_f64", from_u64_f64);
    LOAD_FLOAT_FUNCTION("verified_fp_to_u64_f32", to_u64_f32);
    LOAD_FLOAT_FUNCTION("verified_fp_to_u64_f64", to_u64_f64);
    LOAD_FLOAT_FUNCTION("verified_fp_pressure", pressure);
#undef LOAD_FLOAT_FUNCTION
    memcpy(&nan_f32, &nan_f32_bits, sizeof(nan_f32));
    memcpy(&nan_f64, &nan_f64_bits, sizeof(nan_f64));
    assert(add_f32(1.25f, 2.5f) == 3.75f);
    assert(sub_f32(7.5f, 2.25f) == 5.25f);
    assert(mul_f32(1.5f, 2.0f) == 3.0f);
    assert(div_f32(9.0f, 2.0f) == 4.5f);
    assert(add_f64(1.25, 2.5) == 3.75);
    assert(sub_f64(7.5, 2.25) == 5.25);
    assert(mul_f64(1.5, 2.0) == 3.0);
    assert(div_f64(9.0, 2.0) == 4.5);
    assert(promote(2.5, 1.5f) == 4.0);
    assert(predicates_f32(1.0f, 2.0f) == 14);
    assert(predicates_f32(2.0f, 2.0f) == 41);
    assert(predicates_f32(-0.0f, 0.0f) == 41);
    assert(predicates_f32(nan_f32, 1.0f) == 2);
    assert(predicates_f32(1.0f, nan_f32) == 2);
    assert(predicates_f64(1.0, 2.0) == 14);
    assert(predicates_f64(2.0, 2.0) == 41);
    assert(predicates_f64(-0.0, 0.0) == 41);
    assert(predicates_f64(nan_f64, 1.0) == 2);
    assert(predicates_f64(1.0, nan_f64) == 2);
    assert(truth_f32(0.0f) == 0 && truth_f32(-0.0f) == 0);
    assert(truth_f32(1.0f) == 1 && truth_f32(nan_f32) == 1);
    assert(truth_f64(0.0) == 0 && truth_f64(-0.0) == 0);
    assert(truth_f64(1.0) == 1 && truth_f64(nan_f64) == 1);
    assert(neg_f32(3.5f) == -3.5f);
    assert(neg_f64(3.5) == -3.5);
    assert(neg_f32(0.0f) == 0.0f && signbit(neg_f32(0.0f)));
    assert(neg_f64(0.0) == 0.0 && signbit(neg_f64(0.0)));
    assert(compound_f32(8.0f, 2.0f) == 118.0f);
    assert(compound_f64(8.0, 2.0) == 118.0);
    assert(incdec_f32(3.0f) == 3553.0f);
    assert(incdec_f64(3.0) == 3553.0);
    assert(from_i32(16777217) == 16777216.0f);
    assert(from_i32(-16777217) == -16777216.0f);
    assert(from_i64(9007199254740993LL) == 9007199254740992.0);
    assert(from_u32(UINT32_MAX) == 4294967296.0f);
    assert(to_i32(-123.9f) == -123);
    assert(to_i64(-9007199254740991.0) == -9007199254740991LL);
    assert(to_u32_f32(4294967040.0f) == UINT32_C(4294967040));
    assert(to_u32_f64(4294967295.0) == UINT32_MAX);
    assert(mixed_add_f32(0.5f, 3) == 3.5f);
    assert(mixed_add_f64(0.5, UINT32_MAX) == 4294967295.5);
    assert(mixed_compare_i32(0.5, 1) == 1);
    assert(mixed_compare_i32(2.5, 1) == 0);
    assert(mixed_compare_u32(1.0f, 1u) == 0);
    assert(mixed_compare_u32(0.5f, 1u) == 1);
    assert(narrow_f64(1.1) == 1.1f);
    assert(from_u64_f32(UINT64_MAX) == 18446744073709551616.0f);
    assert(from_u64_f64(UINT64_MAX) == 18446744073709551616.0);
    assert(from_u64_f64(UINT64_C(0x8000000000000401)) ==
           9223372036854777856.0);
    assert(to_u64_f32(-0.0f) == 0u);
    assert(to_u64_f32(123.75f) == 123u);
    assert(to_u64_f32(18446742974197923840.0f) ==
           UINT64_C(0xffffff0000000000));
    assert(to_u64_f64(123.75) == 123u);
    assert(to_u64_f64(9223372036854774784.0) ==
           UINT64_C(0x7ffffffffffffc00));
    assert(to_u64_f64(9223372036854775808.0) ==
           UINT64_C(0x8000000000000000));
    assert(to_u64_f64(18446744073709549568.0) ==
           UINT64_C(0xfffffffffffff800));
    {
        static const uint64_t conversion_edges[] = {
            UINT64_C(0), UINT64_C(1), UINT64_C(2), UINT64_C(3),
            UINT64_C(0x0000000000ffffff), UINT64_C(0x0000000001000000),
            UINT64_C(0x0000000001000001), UINT64_C(0x001fffffffffffff),
            UINT64_C(0x0020000000000000), UINT64_C(0x0020000000000001),
            UINT64_C(0x7fffffffffffffff), UINT64_C(0x8000000000000000),
            UINT64_C(0x8000000000000001), UINT64_C(0xfffffffffffff800),
            UINT64_MAX
        };
        uint64_t random_state = UINT64_C(0x9e3779b97f4a7c15);
        for (size_t index = 0u;
             index < sizeof(conversion_edges) / sizeof(conversion_edges[0]);
             ++index) {
            float expected_f32 = (float)conversion_edges[index];
            float actual_f32 = from_u64_f32(conversion_edges[index]);
            double expected_f64 = (double)conversion_edges[index];
            double actual_f64 = from_u64_f64(conversion_edges[index]);
            uint32_t expected_f32_bits;
            uint32_t actual_f32_bits;
            uint64_t expected_f64_bits;
            uint64_t actual_f64_bits;
            memcpy(&expected_f32_bits, &expected_f32,
                   sizeof(expected_f32_bits));
            memcpy(&actual_f32_bits, &actual_f32,
                   sizeof(actual_f32_bits));
            memcpy(&expected_f64_bits, &expected_f64,
                   sizeof(expected_f64_bits));
            memcpy(&actual_f64_bits, &actual_f64,
                   sizeof(actual_f64_bits));
            assert(actual_f32_bits == expected_f32_bits);
            assert(actual_f64_bits == expected_f64_bits);
        }
        for (size_t index = 0u; index < 512u; ++index) {
            float input_f32;
            double input_f64;
            uint32_t input_f32_bits;
            uint64_t input_f64_bits;
            float expected_f32;
            float actual_f32;
            double expected_f64;
            double actual_f64;
            uint32_t expected_f32_bits;
            uint32_t actual_f32_bits;
            uint64_t expected_f64_bits;
            uint64_t actual_f64_bits;
            random_state ^= random_state >> 12u;
            random_state ^= random_state << 25u;
            random_state ^= random_state >> 27u;
            expected_f32 = (float)random_state;
            actual_f32 = from_u64_f32(random_state);
            expected_f64 = (double)random_state;
            actual_f64 = from_u64_f64(random_state);
            memcpy(&expected_f32_bits, &expected_f32,
                   sizeof(expected_f32_bits));
            memcpy(&actual_f32_bits, &actual_f32,
                   sizeof(actual_f32_bits));
            memcpy(&expected_f64_bits, &expected_f64,
                   sizeof(expected_f64_bits));
            memcpy(&actual_f64_bits, &actual_f64,
                   sizeof(actual_f64_bits));
            assert(actual_f32_bits == expected_f32_bits);
            assert(actual_f64_bits == expected_f64_bits);
            random_state ^= random_state >> 12u;
            random_state ^= random_state << 25u;
            random_state ^= random_state >> 27u;
            input_f32_bits = (uint32_t)(random_state %
                                         UINT32_C(0x5f800000));
            memcpy(&input_f32, &input_f32_bits, sizeof(input_f32));
            assert(to_u64_f32(input_f32) == (uint64_t)input_f32);
            random_state ^= random_state >> 12u;
            random_state ^= random_state << 25u;
            random_state ^= random_state >> 27u;
            input_f64_bits = random_state %
                UINT64_C(0x43f0000000000000);
            memcpy(&input_f64, &input_f64_bits, sizeof(input_f64));
            assert(to_u64_f64(input_f64) == (uint64_t)input_f64);
        }
    }
    assert(pressure(
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
        9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f,
        17.0f, 18.0f, 19.0f, 20.0f, 21.0f, 22.0f, 23.0f, 24.0f,
        25.0f, 26.0f, 27.0f, 28.0f, 29.0f, 30.0f, 31.0f, 32.0f)
        == 528.0f);
    assert(verified_unmap(memory, mapping_size) == 0);
    objfile_free(object);
}

static void verify_native_execution(const char* path, uint16_t arch)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* symbol;
    size_t mapping_size;
    void* memory;
    int values[] = {11, 22, 33, 44, 55};
    int side_effect = 10;
    int RINOS_ABI (*call_function)(int);
    int RINOS_ABI (*indirect_call_function)(int);
    typedef int (*host_unary_function)(int);
    int RINOS_ABI (*indirect_parameter_function)(
        host_unary_function, int);
    int RINOS_ABI (*pointer_function)(int*, int);
    int RINOS_ABI (*local_array_function)(int, int, int);
    int RINOS_ABI (*local_pointer_array_function)(int*, int*);
    int RINOS_ABI (*local_string_array_function)(int);
    int RINOS_ABI (*nested_array_function)(void);
    int RINOS_ABI (*struct_function)(int, int, int*);
    int RINOS_ABI (*struct_copy_pointer_function)(struct VerifiedPair*);
    int RINOS_ABI (*nested_struct_function)(int, int, int*);
    int RINOS_ABI (*union_function)(unsigned int);
    int RINOS_ABI (*compound_struct_function)(int);
    int RINOS_ABI (*compound_array_function)(int, int);
    int RINOS_ABI (*compound_scalar_function)(int);
    int RINOS_ABI (*struct_parameter_function)(struct VerifiedArgument, int);
    int RINOS_ABI (*struct_argument_call_function)(int, int, int);
    struct VerifiedReturnPair RINOS_ABI (*pair_return_function)(int, int);
    int RINOS_ABI (*pair_return_call_function)(int, int);
    struct VerifiedArgument RINOS_ABI (*triple_return_function)(int, int, int);
    int RINOS_ABI (*triple_return_call_function)(int, int, int);
    struct VerifiedLargeReturn RINOS_ABI (*large_return_function)(int, int, int);
    int RINOS_ABI (*large_return_call_function)(int, int, int);
    int RINOS_ABI (*conditional_function)(int, int*);
    int RINOS_ABI (*pointer_compound_function)(int**, int);
    int RINOS_ABI (*pointer_postincrement_function)(int**);
    long RINOS_ABI (*pointer_difference_function)(int*, int*);
    int RINOS_ABI (*switch_function)(int);
    int RINOS_ABI (*nested_switch_function)(int, int);
    int RINOS_ABI (*ternary_function)(int, int, int);
    int RINOS_ABI (*switch_promotion_function)(unsigned char);
    int RINOS_ABI (*switch_nested_case_function)(int);
    int RINOS_ABI (*switch_while_case_function)(int);
    int RINOS_ABI (*switch_do_case_function)(int);
    int RINOS_ABI (*switch_for_case_function)(int);
    int RINOS_ABI (*switch_labeled_case_function)(int);
    int RINOS_ABI (*switch_do_return_case_function)(int);
    int RINOS_ABI (*switch_for_return_case_function)(int);
    int RINOS_ABI (*switch_do_continue_case_function)(int);
    int RINOS_ABI (*switch_for_continue_case_function)(int);
    int RINOS_ABI (*switch_do_break_case_function)(int);
    int RINOS_ABI (*switch_for_break_case_function)(int);
    int* cursor;
    void* address;
    assert(object != NULL && object->arch == arch);
    text = objfile_get_section(object, ".text");
    memory = map_text(object, text, &mapping_size);

    symbol = objfile_find_symbol(object, "verified_call");
    address = symbol_address(memory, symbol);
    memcpy(&call_function, &address, sizeof(call_function));
    assert(call_function(7) == 22);

    symbol = objfile_find_symbol(object, "verified_indirect_call");
    address = symbol_address(memory, symbol);
    memcpy(&indirect_call_function, &address,
           sizeof(indirect_call_function));
    assert(indirect_call_function(7) == 23);

    symbol = objfile_find_symbol(
        object, "tests/verified_backend.c::verified_helper");
    address = symbol_address(memory, symbol);
    host_unary_function target_function;
    memcpy(&target_function, &address, sizeof(target_function));
    symbol = objfile_find_symbol(object, "verified_indirect_parameter");
    address = symbol_address(memory, symbol);
    memcpy(&indirect_parameter_function, &address,
           sizeof(indirect_parameter_function));
    assert(indirect_parameter_function(target_function, 7) == 24);

    symbol = objfile_find_symbol(object, "verified_index");
    address = symbol_address(memory, symbol);
    memcpy(&pointer_function, &address, sizeof(pointer_function));
    assert(pointer_function(values + 2, -1) == 22);

    symbol = objfile_find_symbol(object, "verified_local_array");
    address = symbol_address(memory, symbol);
    memcpy(&local_array_function, &address, sizeof(local_array_function));
    assert(local_array_function(2, 3, 4) == 20);

    symbol = objfile_find_symbol(object, "verified_local_pointer_array");
    address = symbol_address(memory, symbol);
    memcpy(&local_pointer_array_function, &address,
           sizeof(local_pointer_array_function));
    assert(local_pointer_array_function(values, values + 2) == 44);

    symbol = objfile_find_symbol(object, "verified_local_string_array");
    address = symbol_address(memory, symbol);
    memcpy(&local_string_array_function, &address,
           sizeof(local_string_array_function));
    assert(local_string_array_function(1) == 'i');
    assert(local_string_array_function(6) == 0);

    symbol = objfile_find_symbol(object, "verified_nested_array");
    address = symbol_address(memory, symbol);
    memcpy(&nested_array_function, &address,
           sizeof(nested_array_function));
    assert(nested_array_function() == 3);

    symbol = objfile_find_symbol(object, "verified_struct");
    address = symbol_address(memory, symbol);
    memcpy(&struct_function, &address, sizeof(struct_function));
    assert(struct_function(2, 3, values) == 214);

    {
        struct VerifiedPair pair = {4, 5, values};
        symbol = objfile_find_symbol(
            object, "verified_struct_copy_pointer");
        address = symbol_address(memory, symbol);
        memcpy(&struct_copy_pointer_function, &address,
               sizeof(struct_copy_pointer_function));
        assert(struct_copy_pointer_function(&pair) == 416);
    }

    symbol = objfile_find_symbol(object, "verified_nested_struct");
    address = symbol_address(memory, symbol);
    memcpy(&nested_struct_function, &address,
           sizeof(nested_struct_function));
    assert(nested_struct_function(2, 3, values) == 32413);

    symbol = objfile_find_symbol(object, "verified_union");
    address = symbol_address(memory, symbol);
    memcpy(&union_function, &address, sizeof(union_function));
    assert(union_function(0x00030002u) == 23);

    symbol = objfile_find_symbol(object, "verified_compound_struct");
    address = symbol_address(memory, symbol);
    memcpy(&compound_struct_function, &address,
           sizeof(compound_struct_function));
    assert(compound_struct_function(5) == 509);

    symbol = objfile_find_symbol(object, "verified_compound_array");
    address = symbol_address(memory, symbol);
    memcpy(&compound_array_function, &address,
           sizeof(compound_array_function));
    assert(compound_array_function(3, 8) == 8);

    symbol = objfile_find_symbol(object, "verified_compound_scalar");
    address = symbol_address(memory, symbol);
    memcpy(&compound_scalar_function, &address,
           sizeof(compound_scalar_function));
    assert(compound_scalar_function(9) == 11);

    {
        struct VerifiedArgument argument = {4, 5, 6};
        symbol = objfile_find_symbol(object, "verified_struct_parameter");
        address = symbol_address(memory, symbol);
        memcpy(&struct_parameter_function, &address,
               sizeof(struct_parameter_function));
        assert(struct_parameter_function(argument, 7) == 463);
    }

    symbol = objfile_find_symbol(object, "verified_struct_argument_call");
    address = symbol_address(memory, symbol);
    memcpy(&struct_argument_call_function, &address,
           sizeof(struct_argument_call_function));
    assert(struct_argument_call_function(4, 5, 6) == 460);

    {
        struct VerifiedReturnPair result;
        symbol = objfile_find_symbol(object, "verified_pair_return");
        address = symbol_address(memory, symbol);
        memcpy(&pair_return_function, &address,
               sizeof(pair_return_function));
        result = pair_return_function(7, 8);
        assert(result.first == 7 && result.second == 8);
    }

    symbol = objfile_find_symbol(object, "verified_pair_return_call");
    address = symbol_address(memory, symbol);
    memcpy(&pair_return_call_function, &address,
           sizeof(pair_return_call_function));
    assert(pair_return_call_function(7, 8) == 78);

    {
        struct VerifiedArgument result;
        symbol = objfile_find_symbol(object, "verified_triple_return");
        address = symbol_address(memory, symbol);
        memcpy(&triple_return_function, &address,
               sizeof(triple_return_function));
        result = triple_return_function(4, 5, 6);
        assert(result.first == 4 && result.second == 5 && result.third == 6);
    }

    symbol = objfile_find_symbol(object, "verified_triple_return_call");
    address = symbol_address(memory, symbol);
    memcpy(&triple_return_call_function, &address,
           sizeof(triple_return_call_function));
    assert(triple_return_call_function(4, 5, 6) == 456);

    {
        struct VerifiedLargeReturn result;
        symbol = objfile_find_symbol(object, "verified_large_return");
        address = symbol_address(memory, symbol);
        memcpy(&large_return_function, &address,
               sizeof(large_return_function));
        result = large_return_function(4, 5, 6);
        assert(result.first == 4 && result.second == 5 && result.third == 6 &&
               result.fourth == 5 && result.fifth == 6 && result.sixth == 7);
    }

    symbol = objfile_find_symbol(object, "verified_large_return_call");
    address = symbol_address(memory, symbol);
    memcpy(&large_return_call_function, &address,
           sizeof(large_return_call_function));
    assert(large_return_call_function(4, 5, 6) == 474);

    symbol = objfile_find_symbol(object, "verified_pointer_add");
    address = symbol_address(memory, symbol);
    memcpy(&pointer_function, &address, sizeof(pointer_function));
    assert(pointer_function(values, 3) == 44);

    symbol = objfile_find_symbol(object, "verified_pointer_sub");
    address = symbol_address(memory, symbol);
    memcpy(&pointer_function, &address, sizeof(pointer_function));
    assert(pointer_function(values + 4, 2) == 33);

    symbol = objfile_find_symbol(object, "verified_conditional");
    address = symbol_address(memory, symbol);
    memcpy(&conditional_function, &address, sizeof(conditional_function));
    assert(conditional_function(1, &side_effect) == 11);
    assert(side_effect == 11);
    assert(conditional_function(0, &side_effect) == 14);
    assert(side_effect == 14);

    symbol = objfile_find_symbol(object, "verified_logical_and");
    address = symbol_address(memory, symbol);
    memcpy(&conditional_function, &address, sizeof(conditional_function));
    assert(conditional_function(0, &side_effect) == 0);
    assert(side_effect == 14);
    assert(conditional_function(1, &side_effect) == 1);
    assert(side_effect == 15);

    symbol = objfile_find_symbol(object, "verified_logical_or");
    address = symbol_address(memory, symbol);
    memcpy(&conditional_function, &address, sizeof(conditional_function));
    assert(conditional_function(1, &side_effect) == 1);
    assert(side_effect == 15);
    assert(conditional_function(0, &side_effect) == 1);
    assert(side_effect == 16);

    cursor = values;
    symbol = objfile_find_symbol(object, "verified_pointer_compound");
    address = symbol_address(memory, symbol);
    memcpy(&pointer_compound_function, &address,
           sizeof(pointer_compound_function));
    assert(pointer_compound_function(&cursor, 2) == 33);
    assert(cursor == values + 2);

    cursor = values + 1;
    symbol = objfile_find_symbol(object,
                                 "verified_pointer_postincrement");
    address = symbol_address(memory, symbol);
    memcpy(&pointer_postincrement_function, &address,
           sizeof(pointer_postincrement_function));
    assert(pointer_postincrement_function(&cursor) == 55);
    assert(cursor == values + 2);

    symbol = objfile_find_symbol(object, "verified_lvalue_once");
    address = symbol_address(memory, symbol);
    memcpy(&pointer_function, &address, sizeof(pointer_function));
    assert(pointer_function(values, 1) == 227);
    assert(values[1] == 27 && values[2] == 33);

    symbol = objfile_find_symbol(object, "verified_pointer_difference");
    address = symbol_address(memory, symbol);
    memcpy(&pointer_difference_function, &address,
           sizeof(pointer_difference_function));
    assert(pointer_difference_function(values + 4, values + 1) == 3);
    assert(pointer_difference_function(values + 1, values + 4) == -3);

    symbol = objfile_find_symbol(
        object, "verified_common_subexpression");
    address = symbol_address(memory, symbol);
    memcpy(&ternary_function, &address, sizeof(ternary_function));
    assert(ternary_function(7, 5, 0) == 12);
    assert(ternary_function(7, 5, 1) == 12);

    symbol = objfile_find_symbol(object, "verified_switch");
    address = symbol_address(memory, symbol);
    memcpy(&switch_function, &address, sizeof(switch_function));
    assert(switch_function(-1) == 10);
    assert(switch_function(2) == 24);
    assert(switch_function(3) == 4);
    assert(switch_function(8) == 99);

    symbol = objfile_find_symbol(object, "verified_nested_switch");
    address = symbol_address(memory, symbol);
    memcpy(&nested_switch_function, &address,
           sizeof(nested_switch_function));
    assert(nested_switch_function(1, 4) == 114);
    assert(nested_switch_function(1, 7) == 119);
    assert(nested_switch_function(2, 4) == -1);

    symbol = objfile_find_symbol(object, "verified_switch_promotion");
    address = symbol_address(memory, symbol);
    memcpy(&switch_promotion_function, &address,
           sizeof(switch_promotion_function));
    assert(switch_promotion_function(255u) == 1);
    assert(switch_promotion_function(7u) == 0);

    side_effect = 5;
    symbol = objfile_find_symbol(object, "verified_switch_skips_prefix");
    address = symbol_address(memory, symbol);
    memcpy(&conditional_function, &address, sizeof(conditional_function));
    assert(conditional_function(1, &side_effect) == 5);
    assert(side_effect == 5);
    assert(conditional_function(7, &side_effect) == 9);
    assert(side_effect == 5);

    symbol = objfile_find_symbol(object, "verified_switch_nested_case");
    address = symbol_address(memory, symbol);
    memcpy(&switch_nested_case_function, &address,
           sizeof(switch_nested_case_function));
    assert(switch_nested_case_function(1) == 11);
    assert(switch_nested_case_function(7) == 22);

    symbol = objfile_find_symbol(object, "verified_switch_while_case");
    address = symbol_address(memory, symbol);
    memcpy(&switch_while_case_function, &address,
           sizeof(switch_while_case_function));
    assert(switch_while_case_function(1) == 3);
    assert(switch_while_case_function(7) == 0);

    symbol = objfile_find_symbol(object, "verified_switch_do_case");
    address = symbol_address(memory, symbol);
    memcpy(&switch_do_case_function, &address,
           sizeof(switch_do_case_function));
    assert(switch_do_case_function(1) == 3);
    assert(switch_do_case_function(7) == 0);

    symbol = objfile_find_symbol(object, "verified_switch_for_case");
    address = symbol_address(memory, symbol);
    memcpy(&switch_for_case_function, &address,
           sizeof(switch_for_case_function));
    assert(switch_for_case_function(1) == 3);
    assert(switch_for_case_function(7) == 0);

    symbol = objfile_find_symbol(object, "verified_switch_labeled_case");
    address = symbol_address(memory, symbol);
    memcpy(&switch_labeled_case_function, &address,
           sizeof(switch_labeled_case_function));
    assert(switch_labeled_case_function(-1) == 11);
    assert(switch_labeled_case_function(1) == 11);
    assert(switch_labeled_case_function(7) == 22);

    symbol = objfile_find_symbol(object, "verified_switch_do_return_case");
    address = symbol_address(memory, symbol);
    memcpy(&switch_do_return_case_function, &address,
           sizeof(switch_do_return_case_function));
    assert(switch_do_return_case_function(1) == 31);
    assert(switch_do_return_case_function(7) == 47);

    symbol = objfile_find_symbol(object, "verified_switch_for_return_case");
    address = symbol_address(memory, symbol);
    memcpy(&switch_for_return_case_function, &address,
           sizeof(switch_for_return_case_function));
    assert(switch_for_return_case_function(1) == 37);
    assert(switch_for_return_case_function(7) == 53);

    symbol = objfile_find_symbol(object, "verified_switch_do_continue_case");
    address = symbol_address(memory, symbol);
    memcpy(&switch_do_continue_case_function, &address,
           sizeof(switch_do_continue_case_function));
    assert(switch_do_continue_case_function(1) == 41);
    assert(switch_do_continue_case_function(7) == 47);

    symbol = objfile_find_symbol(object, "verified_switch_for_continue_case");
    address = symbol_address(memory, symbol);
    memcpy(&switch_for_continue_case_function, &address,
           sizeof(switch_for_continue_case_function));
    assert(switch_for_continue_case_function(1) == 43);
    assert(switch_for_continue_case_function(7) == 53);

    symbol = objfile_find_symbol(object, "verified_switch_do_break_case");
    address = symbol_address(memory, symbol);
    memcpy(&switch_do_break_case_function, &address,
           sizeof(switch_do_break_case_function));
    assert(switch_do_break_case_function(1) == 31);
    assert(switch_do_break_case_function(7) == 47);

    symbol = objfile_find_symbol(object, "verified_switch_for_break_case");
    address = symbol_address(memory, symbol);
    memcpy(&switch_for_break_case_function, &address,
           sizeof(switch_for_break_case_function));
    assert(switch_for_break_case_function(1) == 37);
    assert(switch_for_break_case_function(7) == 53);

    assert(verified_unmap(memory, mapping_size) == 0);
    objfile_free(object);
}

static void verify_cxx_object(const char* path, bool execute)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* symbol;
    ObjSymbol* indirect_target_symbol;
    ObjSymbol* indirect_parameter_symbol;
    ObjSymbol* overload_pointer_symbol;
    ObjSymbol* overload_assignment_symbol;
    ObjSymbol* overload_address_symbol;
    ObjSymbol* overload_parameter_symbol;
    ObjSymbol* overload_parameter_address_symbol;
    ObjSymbol* wide_symbol;
    ObjSymbol* wide_compound_symbol;
    ObjSymbol* wide_pure_comma_symbol;
    ObjSymbol* wide_noexcept_symbol;
    size_t mapping_size;
    void* memory;
    unsigned long long RINOS_ABI (*wide_function)(
        int, unsigned long long);
    unsigned long long RINOS_ABI (*wide_noexcept_function)(
        unsigned long long);
    typedef int RINOS_ABI (*CxxFunctionPointer)(int);
    CxxFunctionPointer indirect_target_function;
    int RINOS_ABI (*indirect_parameter_function)(CxxFunctionPointer, int);
    int RINOS_ABI (*overload_pointer_function)(int);
    int RINOS_ABI (*overload_assignment_function)(int);
    int RINOS_ABI (*overload_address_function)(int);
    int RINOS_ABI (*overload_parameter_function)(int);
    int RINOS_ABI (*overload_parameter_address_function)(int);
    void* address;
    assert(object != NULL && object->arch == ARCH_X64);
    text = objfile_get_section(object, ".text");
    symbol = objfile_find_symbol(object, "verified_cxx");
    indirect_target_symbol = objfile_find_symbol(
        object, "verified_cxx_indirect_target");
    indirect_parameter_symbol = objfile_find_symbol(
        object, "verified_cxx_indirect_parameter");
    overload_pointer_symbol = objfile_find_symbol(
        object, "verified_cxx_overload_pointer_call");
    overload_assignment_symbol = objfile_find_symbol(
        object, "verified_cxx_overload_pointer_assignment");
    overload_address_symbol = objfile_find_symbol(
        object, "verified_cxx_overload_address_of_call");
    overload_parameter_symbol = objfile_find_symbol(
        object, "verified_cxx_overload_parameter_call");
    overload_parameter_address_symbol = objfile_find_symbol(
        object, "verified_cxx_overload_parameter_address_call");
    wide_symbol = objfile_find_symbol(
        object, "verified_cxx_wide_scalar_conditional_assign");
    wide_compound_symbol = objfile_find_symbol(
        object, "verified_cxx_wide_scalar_conditional_compound");
    wide_pure_comma_symbol = objfile_find_symbol(
        object, "verified_cxx_wide_scalar_pure_comma_compound");
    wide_noexcept_symbol = objfile_find_symbol(
        object, "verified_cxx_wide_scalar_noexcept_compound");
    assert(symbol != NULL && symbol->type == SYM_GLOBAL &&
           symbol->binding == BIND_CODE && symbol->section == 0);
    assert(indirect_target_symbol != NULL &&
           indirect_target_symbol->type == SYM_GLOBAL &&
           indirect_target_symbol->binding == BIND_CODE &&
           indirect_target_symbol->section == 0);
    assert(indirect_parameter_symbol != NULL &&
           indirect_parameter_symbol->type == SYM_GLOBAL &&
           indirect_parameter_symbol->binding == BIND_CODE &&
           indirect_parameter_symbol->section == 0);
    assert(overload_pointer_symbol != NULL &&
           overload_pointer_symbol->type == SYM_GLOBAL &&
           overload_pointer_symbol->binding == BIND_CODE &&
           overload_pointer_symbol->section == 0);
    assert(overload_assignment_symbol != NULL &&
           overload_assignment_symbol->type == SYM_GLOBAL &&
           overload_assignment_symbol->binding == BIND_CODE &&
           overload_assignment_symbol->section == 0);
    assert(overload_address_symbol != NULL &&
           overload_address_symbol->type == SYM_GLOBAL &&
           overload_address_symbol->binding == BIND_CODE &&
           overload_address_symbol->section == 0);
    assert(overload_parameter_symbol != NULL &&
           overload_parameter_symbol->type == SYM_GLOBAL &&
           overload_parameter_symbol->binding == BIND_CODE &&
           overload_parameter_symbol->section == 0);
    assert(overload_parameter_address_symbol != NULL &&
           overload_parameter_address_symbol->type == SYM_GLOBAL &&
           overload_parameter_address_symbol->binding == BIND_CODE &&
           overload_parameter_address_symbol->section == 0);
    assert(wide_symbol != NULL && wide_symbol->type == SYM_GLOBAL &&
           wide_symbol->binding == BIND_CODE && wide_symbol->section == 0);
    assert(wide_compound_symbol != NULL &&
           wide_compound_symbol->type == SYM_GLOBAL &&
           wide_compound_symbol->binding == BIND_CODE &&
           wide_compound_symbol->section == 0);
    assert(wide_pure_comma_symbol != NULL &&
           wide_pure_comma_symbol->type == SYM_GLOBAL &&
           wide_pure_comma_symbol->binding == BIND_CODE &&
           wide_pure_comma_symbol->section == 0);
    assert(wide_noexcept_symbol != NULL &&
           wide_noexcept_symbol->type == SYM_GLOBAL &&
           wide_noexcept_symbol->binding == BIND_CODE &&
           wide_noexcept_symbol->section == 0);
    /* This fixture contains x86-64 machine code.  The i686 verifier still
     * validates the object format and exported code symbols, but only the
     * x86-64 host verifier can execute these function pointers. */
    if (!execute) {
        objfile_free(object);
        return;
    }
    memory = map_text(object, text, &mapping_size);
    address = symbol_address(memory, indirect_target_symbol);
    memcpy(&indirect_target_function, &address,
           sizeof(indirect_target_function));
    address = symbol_address(memory, indirect_parameter_symbol);
    memcpy(&indirect_parameter_function, &address,
           sizeof(indirect_parameter_function));
    assert(indirect_target_function(7) == 11);
    assert(indirect_parameter_function(indirect_target_function, 7) == 16);
    address = symbol_address(memory, overload_pointer_symbol);
    memcpy(&overload_pointer_function, &address,
           sizeof(overload_pointer_function));
    assert(overload_pointer_function(7) == 20);
    address = symbol_address(memory, overload_assignment_symbol);
    memcpy(&overload_assignment_function, &address,
           sizeof(overload_assignment_function));
    assert(overload_assignment_function(7) == 21);
    address = symbol_address(memory, overload_address_symbol);
    memcpy(&overload_address_function, &address,
           sizeof(overload_address_function));
    assert(overload_address_function(7) == 22);
    address = symbol_address(memory, overload_parameter_symbol);
    memcpy(&overload_parameter_function, &address,
           sizeof(overload_parameter_function));
    assert(overload_parameter_function(7) == 28);
    address = symbol_address(memory, overload_parameter_address_symbol);
    memcpy(&overload_parameter_address_function, &address,
           sizeof(overload_parameter_address_function));
    assert(overload_parameter_address_function(7) == 29);
    address = symbol_address(memory, wide_symbol);
    memcpy(&wide_function, &address, sizeof(wide_function));
    assert(wide_function(0, 0ULL) == 7ULL);
    assert(wide_function(1, 0ULL) == 7ULL);
    assert(wide_function(1, 5ULL) == 6ULL);
    address = symbol_address(memory, wide_compound_symbol);
    memcpy(&wide_function, &address, sizeof(wide_function));
    assert(wide_function(0, 5ULL) == 7ULL);
    assert(wide_function(1, 5ULL) == 6ULL);
    address = symbol_address(memory, wide_noexcept_symbol);
    memcpy(&wide_noexcept_function, &address,
           sizeof(wide_noexcept_function));
    assert(wide_noexcept_function(5ULL) == 6ULL);
    address = symbol_address(memory, wide_pure_comma_symbol);
    memcpy(&wide_function, &address, sizeof(wide_function));
    assert(wide_function(0, 5ULL) == 7ULL);
    assert(wide_function(1, 5ULL) == 6ULL);
    assert(verified_unmap(memory, mapping_size) == 0);
    objfile_free(object);
}

static void verify_cxx_reference_local_object(const char* path,
                                              uint16_t arch,
                                              bool execute)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* entry_symbol;
    size_t mapping_size;
    void* memory;
    void* address;
    int (RINOS_ABI *entry)(void);
    int result;
    assert(object != NULL && object->arch == arch);
    text = objfile_get_section(object, ".text");
    entry_symbol = objfile_find_symbol(object, "main");
    assert(text != NULL && entry_symbol != NULL &&
           entry_symbol->type == SYM_GLOBAL &&
           entry_symbol->binding == BIND_CODE &&
           entry_symbol->section == 0);
    if (!execute) {
        objfile_free(object);
        return;
    }
    memory = map_text(object, text, &mapping_size);
    address = symbol_address(memory, entry_symbol);
    memcpy(&entry, &address, sizeof(entry));
    result = entry();
    if (result != 0) {
        fprintf(stderr, "verified C++ reference object returned %d\n",
                result);
    }
    assert(result == 0);
    assert(verified_unmap(memory, mapping_size) == 0);
    objfile_free(object);
}

static void verify_member_methods_object(const char* path, uint16_t arch,
                                         bool execute)
{
    struct VerifiedMemberMethodObject {
        int value;
        _Alignas(8) unsigned long long wide_value;
    } instance;
    _Static_assert(offsetof(struct VerifiedMemberMethodObject, wide_value) ==
                       8u,
                   "verified member fixture must match RinOS 64-bit alignment");
    struct VerifiedMemberReleaseObject {
        unsigned int value;
    } release_instance;
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* read_symbol;
    ObjSymbol* add_symbol;
    ObjSymbol* equal_symbol;
    ObjSymbol* different_symbol;
    ObjSymbol* reference_symbol;
    ObjSymbol* wide_symbol;
    ObjSymbol* wide_equal_symbol;
    ObjSymbol* release_symbol;
    assert(object != NULL && object->arch == arch);
    text = objfile_get_section(object, ".text");
    read_symbol = objfile_find_symbol(object, "verified_member_method_read");
    add_symbol = objfile_find_symbol(object, "verified_member_method_add");
    equal_symbol = objfile_find_symbol(
        object, "verified_member_method_equals_seven");
    different_symbol = objfile_find_symbol(
        object, "verified_member_method_differs_from_seven");
    reference_symbol = objfile_find_symbol(
        object, "verified_member_method_reference");
    wide_symbol = objfile_find_symbol(object, "verified_member_method_wide");
    wide_equal_symbol = objfile_find_symbol(
        object, "verified_member_method_wide_equals_expected");
    release_symbol = objfile_find_symbol(
        object, "verified_member_method_release");
    assert(text != NULL && read_symbol != NULL &&
           read_symbol->binding == BIND_CODE && read_symbol->section == 0 &&
           add_symbol != NULL && add_symbol->binding == BIND_CODE &&
           add_symbol->section == 0 && equal_symbol != NULL &&
           equal_symbol->binding == BIND_CODE && equal_symbol->section == 0 &&
           different_symbol != NULL &&
           different_symbol->binding == BIND_CODE &&
           different_symbol->section == 0 && reference_symbol != NULL &&
           reference_symbol->binding == BIND_CODE &&
           reference_symbol->section == 0 && wide_symbol != NULL &&
           wide_symbol->binding == BIND_CODE && wide_symbol->section == 0 &&
           wide_equal_symbol != NULL &&
           wide_equal_symbol->binding == BIND_CODE &&
           wide_equal_symbol->section == 0 && release_symbol != NULL &&
           release_symbol->binding == BIND_CODE &&
           release_symbol->section == 0);
    if (execute) {
        size_t mapping_size;
        void* memory = map_text(object, text, &mapping_size);
        int (RINOS_ABI *read_function)(struct VerifiedMemberMethodObject*);
        int (RINOS_ABI *add_function)(struct VerifiedMemberMethodObject*, int);
        int (RINOS_ABI *equal_function)(struct VerifiedMemberMethodObject*);
        int (RINOS_ABI *different_function)(
            struct VerifiedMemberMethodObject*);
        const int* (RINOS_ABI *reference_function)(
            struct VerifiedMemberMethodObject*);
        unsigned long long (RINOS_ABI *wide_function)(
            struct VerifiedMemberMethodObject*);
        int (RINOS_ABI *wide_equal_function)(
            struct VerifiedMemberMethodObject*);
        unsigned int (RINOS_ABI *release_function)(
            struct VerifiedMemberReleaseObject*);
        void* address = symbol_address(memory, read_symbol);
        memcpy(&read_function, &address, sizeof(read_function));
        address = symbol_address(memory, add_symbol);
        memcpy(&add_function, &address, sizeof(add_function));
        address = symbol_address(memory, equal_symbol);
        memcpy(&equal_function, &address, sizeof(equal_function));
        address = symbol_address(memory, different_symbol);
        memcpy(&different_function, &address, sizeof(different_function));
        address = symbol_address(memory, reference_symbol);
        memcpy(&reference_function, &address, sizeof(reference_function));
        address = symbol_address(memory, wide_symbol);
        memcpy(&wide_function, &address, sizeof(wide_function));
        address = symbol_address(memory, wide_equal_symbol);
        memcpy(&wide_equal_function, &address, sizeof(wide_equal_function));
        address = symbol_address(memory, release_symbol);
        memcpy(&release_function, &address, sizeof(release_function));
        instance.value = 7;
        instance.wide_value = 0x1122334455667788ULL;
        assert(read_function(&instance) == 7);
        assert(add_function(&instance, 5) == 12);
        assert(equal_function(&instance) == 1);
        assert(different_function(&instance) == 0);
        assert(reference_function(&instance) == &instance.value);
        assert(wide_function(&instance) == 0x1122334455667788ULL);
        assert(wide_equal_function(&instance) == 1);
        release_instance.value = 0xabcdef01u;
        assert(release_function(&release_instance) == 0xabcdef01u);
        assert(release_instance.value == 0u);
        instance.value = 8;
        instance.wide_value = 42ULL;
        assert(equal_function(&instance) == 0);
        assert(different_function(&instance) == 1);
        assert(wide_function(&instance) == 42ULL);
        assert(wide_equal_function(&instance) == 0);
        assert(verified_unmap(memory, mapping_size) == 0);
    }
    objfile_free(object);
}

typedef struct {
    void** bases;
    size_t* sizes;
    size_t count;
} MappedObject;

static int mapped_external_value = 31;

static MappedObject map_object(ObjectFile* object)
{
    MappedObject mapping = {0};
    ObjSection* section;
    int section_index = 0;
    size_t page = verified_page_size();
    assert(object != NULL && object->section_count > 0 && page != 0u);
    mapping.count = (size_t)object->section_count;
    mapping.bases = calloc(mapping.count, sizeof(*mapping.bases));
    mapping.sizes = calloc(mapping.count, sizeof(*mapping.sizes));
    assert(mapping.bases != NULL && mapping.sizes != NULL);
    for (section = object->sections; section != NULL;
         section = section->next, ++section_index) {
        size_t size;
        if ((section->flags & SECT_FLAG_ALLOC) == 0u) continue;
        assert(section->memory_size != 0u &&
               section->memory_size <= (uint64_t)SIZE_MAX);
        size = ((size_t)section->memory_size + (size_t)page - 1u) &
            ~((size_t)page - 1u);
        mapping.bases[section_index] = verified_map(size);
        assert(mapping.bases[section_index] != NULL);
        mapping.sizes[section_index] = size;
        if (section->size != 0u) {
            memcpy(mapping.bases[section_index], section->data,
                   (size_t)section->size);
        }
    }
    section_index = 0;
    for (section = object->sections; section != NULL;
         section = section->next, ++section_index) {
        ObjReloc* relocation;
        for (relocation = section->relocs; relocation != NULL;
             relocation = relocation->next) {
            ObjSymbol* symbol = objfile_find_symbol(
                object, relocation->symbol_name);
            uint8_t* place;
            uint64_t target;
            assert(symbol != NULL);
            assert(mapping.bases[section_index] != NULL);
            place = (uint8_t*)mapping.bases[section_index] +
                relocation->offset;
            if (symbol->section < 0) {
                assert(strcmp(symbol->name, "verified_external_data") == 0);
                target = (uint64_t)(uintptr_t)&mapped_external_value;
            } else {
                assert((size_t)symbol->section < mapping.count &&
                       mapping.bases[symbol->section] != NULL);
                target =
                    (uint64_t)(uintptr_t)mapping.bases[symbol->section] +
                    symbol->value;
            }
            target += (uint64_t)relocation->addend;
            if (relocation->type == RELOC_REL32) {
                int64_t delta = (int64_t)target -
                    (int64_t)(uintptr_t)(place + 4u);
                int32_t value = (int32_t)delta;
                assert((int64_t)value == delta);
                memcpy(place, &value, sizeof(value));
            } else if (relocation->type == RELOC_ABS32U) {
                uint32_t value = (uint32_t)target;
                assert((uint64_t)value == target);
                memcpy(place, &value, sizeof(value));
            } else {
                assert(relocation->type == RELOC_ABS64);
                memcpy(place, &target, sizeof(target));
            }
        }
    }
    section_index = 0;
    for (section = object->sections; section != NULL;
         section = section->next, ++section_index) {
        int writable;
        int executable;
        if (mapping.bases[section_index] == NULL) continue;
        writable = (section->flags & SECT_FLAG_WRITE) != 0u;
        executable = (section->flags & SECT_FLAG_EXEC) != 0u;
        assert(verified_protect(mapping.bases[section_index],
                                mapping.sizes[section_index],
                                writable, executable) == 0);
    }
    return mapping;
}

static void unmap_object(MappedObject* mapping)
{
    size_t index;
    for (index = 0u; index < mapping->count; ++index) {
        if (mapping->bases[index] != NULL) {
            assert(verified_unmap(mapping->bases[index],
                                  mapping->sizes[index]) == 0);
        }
    }
    free(mapping->bases);
    free(mapping->sizes);
    memset(mapping, 0, sizeof(*mapping));
}

static void verify_virtual_dispatch_object(const char* path, uint16_t arch,
                                           bool execute)
{
    ObjectFile* object = objfile_read(path);
    ObjSymbol* dispatch_symbol;
    ObjSymbol* base_call_symbol;
    ObjSymbol* derived_call_symbol;
    ObjSymbol* secondary_dispatch_symbol;
    ObjSymbol* secondary_call_symbol;
    ObjSymbol* sret_dispatch_symbol;
    ObjSymbol* sret_base_call_symbol;
    ObjSymbol* sret_derived_call_symbol;
    assert(object != NULL && object->arch == arch);
    dispatch_symbol = objfile_find_symbol(
        object, "verified_virtual_dispatch");
    base_call_symbol = objfile_find_symbol(
        object, "verified_virtual_call_base");
    derived_call_symbol = objfile_find_symbol(
        object, "verified_virtual_call_derived");
    secondary_dispatch_symbol = objfile_find_symbol(
        object, "verified_virtual_secondary_dispatch");
    secondary_call_symbol = objfile_find_symbol(
        object, "verified_virtual_call_secondary");
    sret_dispatch_symbol = objfile_find_symbol(
        object, "verified_virtual_sret_dispatch");
    sret_base_call_symbol = objfile_find_symbol(
        object, "verified_virtual_sret_call_base");
    sret_derived_call_symbol = objfile_find_symbol(
        object, "verified_virtual_sret_call_derived");
    assert(dispatch_symbol != NULL && dispatch_symbol->type == SYM_GLOBAL &&
           dispatch_symbol->binding == BIND_CODE &&
           dispatch_symbol->section == 0 && base_call_symbol != NULL &&
           base_call_symbol->binding == BIND_CODE &&
           base_call_symbol->section == 0 && derived_call_symbol != NULL &&
           derived_call_symbol->binding == BIND_CODE &&
           derived_call_symbol->section == 0 &&
           secondary_dispatch_symbol != NULL &&
           secondary_dispatch_symbol->binding == BIND_CODE &&
           secondary_dispatch_symbol->section == 0 &&
           secondary_call_symbol != NULL &&
           secondary_call_symbol->binding == BIND_CODE &&
           secondary_call_symbol->section == 0 &&
           sret_dispatch_symbol != NULL &&
           sret_dispatch_symbol->binding == BIND_CODE &&
           sret_dispatch_symbol->section == 0 &&
           sret_base_call_symbol != NULL &&
           sret_base_call_symbol->binding == BIND_CODE &&
           sret_base_call_symbol->section == 0 &&
           sret_derived_call_symbol != NULL &&
           sret_derived_call_symbol->binding == BIND_CODE &&
           sret_derived_call_symbol->section == 0);
    if (execute) {
        MappedObject mapping = map_object(object);
        int (RINOS_ABI *base_call)(void);
        int (RINOS_ABI *derived_call)(void);
        int (RINOS_ABI *secondary_call)(void);
        int (RINOS_ABI *sret_base_call)(void);
        int (RINOS_ABI *sret_derived_call)(void);
        void* address = symbol_address(
            mapping.bases[0], base_call_symbol);
        memcpy(&base_call, &address, sizeof(base_call));
        address = symbol_address(mapping.bases[0], derived_call_symbol);
        memcpy(&derived_call, &address, sizeof(derived_call));
        address = symbol_address(mapping.bases[0], secondary_call_symbol);
        memcpy(&secondary_call, &address, sizeof(secondary_call));
        address = symbol_address(mapping.bases[0], sret_base_call_symbol);
        memcpy(&sret_base_call, &address, sizeof(sret_base_call));
        address = symbol_address(
            mapping.bases[0], sret_derived_call_symbol);
        memcpy(&sret_derived_call, &address, sizeof(sret_derived_call));
        assert(base_call() == 17);
        assert(derived_call() == 29);
        assert(secondary_call() == 47);
        assert(sret_base_call() == 24);
        assert(sret_derived_call() == 224);
        unmap_object(&mapping);
    }
    objfile_free(object);
}

static void verify_global_object(const char* path, uint16_t arch,
                                 bool execute)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* data;
    ObjSection* bss;
    ObjSection* text;
    ObjSection* rodata;
    ObjSymbol* global_data;
    ObjSymbol* global_zero;
    ObjSymbol* static_data;
    ObjSymbol* read_symbol;
    ObjSymbol* write_symbol;
    ObjSymbol* external_data;
    ObjSymbol* external_read;
    ObjSymbol* global_array;
    ObjSymbol* array_read;
    ObjSymbol* array_write;
    ObjSymbol* string_constant;
    ObjSymbol* string_read;
    ObjSymbol* global_pair;
    ObjSymbol* global_word;
    ObjSymbol* aggregate_read;
    ObjSymbol* aggregate_write;
    ObjReloc* relocation;
    size_t absolute_relocations = 0u;
    size_t relative_relocations = 0u;
    assert(object != NULL && object->arch == arch);
    data = objfile_get_section(object, ".data");
    bss = objfile_get_section(object, ".bss");
    text = objfile_get_section(object, ".text");
    rodata = objfile_get_section(object, ".rodata");
    global_data = objfile_find_symbol(object, "verified_global_data");
    global_zero = objfile_find_symbol(object, "verified_global_zero");
    static_data = objfile_find_symbol(
        object, "tests/verified_backend_globals.c::verified_static_data");
    read_symbol = objfile_find_symbol(object, "verified_global_read");
    write_symbol = objfile_find_symbol(object, "verified_global_write");
    external_data = objfile_find_symbol(object, "verified_external_data");
    external_read = objfile_find_symbol(object, "verified_external_read");
    global_array = objfile_find_symbol(object, "verified_global_array");
    array_read = objfile_find_symbol(
        object, "verified_global_array_read");
    array_write = objfile_find_symbol(
        object, "verified_global_array_write");
    string_constant = objfile_find_symbol(
        object,
        "tests/verified_backend_globals.c::verified_string_read::$rcc.constant.0");
    string_read = objfile_find_symbol(object, "verified_string_read");
    global_pair = objfile_find_symbol(object, "verified_global_pair");
    global_word = objfile_find_symbol(object, "verified_global_word");
    aggregate_read = objfile_find_symbol(
        object, "verified_global_aggregate_read");
    aggregate_write = objfile_find_symbol(
        object, "verified_global_aggregate_write");
    assert(data != NULL && data->size == 36u &&
           (data->flags & SECT_FLAG_WRITE) != 0u &&
           (data->flags & SECT_FLAG_EXEC) == 0u);
    assert(bss != NULL && bss->size == 0u && bss->memory_size == 4u);
    assert(text != NULL && (text->flags & SECT_FLAG_WRITE) == 0u);
    assert(rodata != NULL && rodata->size == 6u &&
           memcmp(rodata->data, "RinOS", 6u) == 0 &&
           (rodata->flags & SECT_FLAG_ALLOC) != 0u &&
           (rodata->flags & (SECT_FLAG_WRITE | SECT_FLAG_EXEC)) == 0u);
    assert(global_data != NULL && global_data->type == SYM_GLOBAL &&
           global_data->binding == BIND_DATA && global_data->section >= 0);
    assert(global_zero != NULL && global_zero->type == SYM_GLOBAL &&
           global_zero->binding == BIND_BSS && global_zero->section >= 0);
    assert(static_data != NULL && static_data->type == SYM_LOCAL &&
           static_data->binding == BIND_DATA && static_data->section >= 0);
    assert(read_symbol != NULL && read_symbol->binding == BIND_CODE &&
           read_symbol->section == 0);
    assert(write_symbol != NULL && write_symbol->binding == BIND_CODE &&
           write_symbol->section == 0);
    assert(external_data != NULL && external_data->type == SYM_UNDEF &&
           external_data->binding == BIND_DATA && external_data->section < 0);
    assert(external_read != NULL && external_read->binding == BIND_CODE &&
           external_read->section == 0);
    assert(global_array != NULL && global_array->type == SYM_GLOBAL &&
           global_array->binding == BIND_DATA && global_array->section >= 0);
    assert(array_read != NULL && array_read->binding == BIND_CODE &&
           array_read->section == 0);
    assert(array_write != NULL && array_write->binding == BIND_CODE &&
           array_write->section == 0);
    assert(string_constant != NULL && string_constant->type == SYM_LOCAL &&
           string_constant->binding == BIND_DATA &&
           string_constant->section >= 0 && string_constant->size == 6u);
    assert(string_read != NULL && string_read->binding == BIND_CODE &&
           string_read->section == 0);
    assert(global_pair != NULL && global_pair->type == SYM_GLOBAL &&
           global_pair->binding == BIND_DATA && global_pair->section >= 0);
    assert(global_word != NULL && global_word->type == SYM_GLOBAL &&
           global_word->binding == BIND_DATA && global_word->section >= 0);
    assert(aggregate_read != NULL &&
           aggregate_read->binding == BIND_CODE &&
           aggregate_read->section == 0);
    assert(aggregate_write != NULL &&
           aggregate_write->binding == BIND_CODE &&
           aggregate_write->section == 0);
    for (relocation = text->relocs; relocation != NULL;
         relocation = relocation->next) {
        if (relocation->type == RELOC_REL32) {
            ++relative_relocations;
        } else {
            assert(relocation->type ==
                   (arch == ARCH_X64 ? RELOC_ABS64 : RELOC_ABS32U));
            ++absolute_relocations;
        }
    }
    assert(absolute_relocations >= 11u && relative_relocations == 1u);
    if (execute) {
        MappedObject mapping = map_object(object);
        int RINOS_ABI (*read_function)(void);
        int RINOS_ABI (*write_function)(int);
        int RINOS_ABI (*external_function)(void);
        int RINOS_ABI (*array_read_function)(int);
        int RINOS_ABI (*array_write_function)(int, int);
        int RINOS_ABI (*string_function)(int);
        int RINOS_ABI (*aggregate_read_function)(int);
        int RINOS_ABI (*aggregate_write_function)(int);
        void* address = (uint8_t*)mapping.bases[0] + read_symbol->value;
        memcpy(&read_function, &address, sizeof(read_function));
        address = (uint8_t*)mapping.bases[0] + write_symbol->value;
        memcpy(&write_function, &address, sizeof(write_function));
        address = (uint8_t*)mapping.bases[0] + external_read->value;
        memcpy(&external_function, &address, sizeof(external_function));
        address = (uint8_t*)mapping.bases[0] + array_read->value;
        memcpy(&array_read_function, &address, sizeof(array_read_function));
        address = (uint8_t*)mapping.bases[0] + array_write->value;
        memcpy(&array_write_function, &address,
               sizeof(array_write_function));
        address = (uint8_t*)mapping.bases[0] + string_read->value;
        memcpy(&string_function, &address, sizeof(string_function));
        address = (uint8_t*)mapping.bases[0] + aggregate_read->value;
        memcpy(&aggregate_read_function, &address,
               sizeof(aggregate_read_function));
        address = (uint8_t*)mapping.bases[0] + aggregate_write->value;
        memcpy(&aggregate_write_function, &address,
               sizeof(aggregate_write_function));
        assert(read_function() == 12);
        assert(write_function(20) == 48);
        assert(read_function() == 48);
        assert(external_function() == mapped_external_value);
        assert(array_read_function(2) == 6);
        assert(array_write_function(1, 17) == 17);
        assert(array_read_function(1) == 17);
        assert(string_function(1) == 'i' * 2);
        assert(aggregate_read_function(1) == 813);
        assert(aggregate_write_function(12) == 929);
        assert(aggregate_read_function(0) == 817);
        unmap_object(&mapping);
    }
    objfile_free(object);
}

static void verify_typeinfo_object(const char* path, uint16_t arch)
{
    const char prefix[] = "__rcc_typeinfo_";
    ObjectFile* object = objfile_read(path);
    ObjSection* rodata;
    size_t typeinfo_count = 0u;
    size_t name_relocation_count = 0u;
    assert(object != NULL && object->arch == arch);
    rodata = objfile_get_section(object, ".rodata");
    assert(rodata != NULL && (rodata->flags & SECT_FLAG_ALLOC) != 0u &&
           (rodata->flags & (SECT_FLAG_WRITE | SECT_FLAG_EXEC)) == 0u);
    for (ObjSymbol* symbol = object->symbols; symbol;
         symbol = symbol->next) {
        const char* name = symbol->name;
        size_t length;
        if (!name || strncmp(name, prefix, sizeof(prefix) - 1u) != 0) {
            continue;
        }
        length = strlen(name);
        if (length >= 5u && strcmp(name + length - 5u, "_name") == 0) {
            continue;
        }
        assert(symbol->type == SYM_WEAK && symbol->binding == BIND_DATA &&
               symbol->section >= 0);
        ++typeinfo_count;
    }
    for (ObjReloc* relocation = rodata->relocs; relocation;
         relocation = relocation->next) {
        if (relocation->symbol_name &&
            strncmp(relocation->symbol_name, prefix,
                    sizeof(prefix) - 1u) == 0) {
            size_t length = strlen(relocation->symbol_name);
            if (length >= 5u &&
                strcmp(relocation->symbol_name + length - 5u, "_name") == 0) {
                ++name_relocation_count;
            }
        }
    }
    assert(typeinfo_count >= 2u && name_relocation_count >= typeinfo_count);
    objfile_free(object);
}

static void verify_wide_variadic_call_object(const char* path, uint16_t arch)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* caller;
    bool target_relocation = false;
    bool pointer_target_relocation = false;
    assert(object != NULL && object->arch == arch);
    text = objfile_get_section(object, ".text");
    caller = objfile_find_symbol(
        object, "verified_wide_scalar_variadic_call");
    assert(text != NULL && caller != NULL &&
           caller->type == SYM_GLOBAL && caller->binding == BIND_CODE &&
           caller->section == 0);
    for (ObjReloc* relocation = text->relocs; relocation;
         relocation = relocation->next) {
        if (relocation->symbol_name &&
            strcmp(relocation->symbol_name,
                   "verified_wide_scalar_variadic_target") == 0) {
            target_relocation = true;
        }
        if (relocation->symbol_name &&
            strcmp(relocation->symbol_name,
                   "verified_wide_scalar_variadic_pointer_target") == 0) {
            pointer_target_relocation = true;
        }
    }
    caller = objfile_find_symbol(
        object, "verified_wide_scalar_variadic_pointer_call");
    assert(caller != NULL && caller->type == SYM_GLOBAL &&
           caller->binding == BIND_CODE && caller->section == 0 &&
           target_relocation && pointer_target_relocation);
    objfile_free(object);
}

static void verify_optimized_switch_loop_labels(const char* path)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* while_symbol;
    ObjSymbol* do_symbol;
    ObjSymbol* for_symbol;
    ObjSymbol* labeled_symbol;
    ObjSymbol* do_return_symbol;
    ObjSymbol* for_return_symbol;
    ObjSymbol* do_continue_symbol;
    ObjSymbol* for_continue_symbol;
    ObjSymbol* do_break_symbol;
    ObjSymbol* for_break_symbol;
    int RINOS_ABI (*while_function)(int);
    int RINOS_ABI (*do_function)(int);
    int RINOS_ABI (*for_function)(int);
    int RINOS_ABI (*labeled_function)(int);
    int RINOS_ABI (*do_return_function)(int);
    int RINOS_ABI (*for_return_function)(int);
    int RINOS_ABI (*do_continue_function)(int);
    int RINOS_ABI (*for_continue_function)(int);
    int RINOS_ABI (*do_break_function)(int);
    int RINOS_ABI (*for_break_function)(int);
    size_t mapping_size;
    void* memory;
    void* address;
    assert(object != NULL && object->arch == ARCH_X64);
    text = objfile_get_section(object, ".text");
    while_symbol = objfile_find_symbol(
        object, "verified_switch_while_case");
    do_symbol = objfile_find_symbol(object, "verified_switch_do_case");
    for_symbol = objfile_find_symbol(object, "verified_switch_for_case");
    labeled_symbol = objfile_find_symbol(
        object, "verified_switch_labeled_case");
    do_return_symbol = objfile_find_symbol(
        object, "verified_switch_do_return_case");
    for_return_symbol = objfile_find_symbol(
        object, "verified_switch_for_return_case");
    do_continue_symbol = objfile_find_symbol(
        object, "verified_switch_do_continue_case");
    for_continue_symbol = objfile_find_symbol(
        object, "verified_switch_for_continue_case");
    do_break_symbol = objfile_find_symbol(
        object, "verified_switch_do_break_case");
    for_break_symbol = objfile_find_symbol(
        object, "verified_switch_for_break_case");
    assert(text != NULL && while_symbol != NULL &&
           while_symbol->type == SYM_GLOBAL && while_symbol->section == 0 &&
           do_symbol != NULL && do_symbol->type == SYM_GLOBAL &&
           do_symbol->section == 0 && for_symbol != NULL &&
           for_symbol->type == SYM_GLOBAL && for_symbol->section == 0 &&
           labeled_symbol != NULL && labeled_symbol->type == SYM_GLOBAL &&
           labeled_symbol->section == 0 && do_return_symbol != NULL &&
           do_return_symbol->type == SYM_GLOBAL &&
           do_return_symbol->section == 0 && for_return_symbol != NULL &&
           for_return_symbol->type == SYM_GLOBAL &&
           for_return_symbol->section == 0 && do_continue_symbol != NULL &&
           do_continue_symbol->type == SYM_GLOBAL &&
           do_continue_symbol->section == 0 && for_continue_symbol != NULL &&
           for_continue_symbol->type == SYM_GLOBAL &&
           for_continue_symbol->section == 0 && do_break_symbol != NULL &&
           do_break_symbol->type == SYM_GLOBAL &&
           do_break_symbol->section == 0 && for_break_symbol != NULL &&
           for_break_symbol->type == SYM_GLOBAL &&
           for_break_symbol->section == 0);
    memory = map_text(object, text, &mapping_size);
    address = symbol_address(memory, while_symbol);
    memcpy(&while_function, &address, sizeof(while_function));
    address = symbol_address(memory, do_symbol);
    memcpy(&do_function, &address, sizeof(do_function));
    address = symbol_address(memory, for_symbol);
    memcpy(&for_function, &address, sizeof(for_function));
    address = symbol_address(memory, labeled_symbol);
    memcpy(&labeled_function, &address, sizeof(labeled_function));
    address = symbol_address(memory, do_return_symbol);
    memcpy(&do_return_function, &address, sizeof(do_return_function));
    address = symbol_address(memory, for_return_symbol);
    memcpy(&for_return_function, &address, sizeof(for_return_function));
    address = symbol_address(memory, do_continue_symbol);
    memcpy(&do_continue_function, &address, sizeof(do_continue_function));
    address = symbol_address(memory, for_continue_symbol);
    memcpy(&for_continue_function, &address, sizeof(for_continue_function));
    address = symbol_address(memory, do_break_symbol);
    memcpy(&do_break_function, &address, sizeof(do_break_function));
    address = symbol_address(memory, for_break_symbol);
    memcpy(&for_break_function, &address, sizeof(for_break_function));
    assert(while_function(1) == 3 && while_function(7) == 0);
    assert(do_function(1) == 3 && do_function(7) == 0);
    assert(for_function(1) == 3 && for_function(7) == 0);
    assert(labeled_function(-1) == 11 && labeled_function(1) == 11 &&
           labeled_function(7) == 22);
    assert(do_return_function(1) == 31 && do_return_function(7) == 47);
    assert(for_return_function(1) == 37 && for_return_function(7) == 53);
    assert(do_continue_function(1) == 41 && do_continue_function(7) == 47);
    assert(for_continue_function(1) == 43 && for_continue_function(7) == 53);
    assert(do_break_function(1) == 31 && do_break_function(7) == 47);
    assert(for_break_function(1) == 37 && for_break_function(7) == 53);
    assert(verified_unmap(memory, mapping_size) == 0);
    objfile_free(object);
}

static void verify_tls_object(const char* path, uint16_t arch,
                              bool imported)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSection* tls;
    ObjSymbol* symbol;
    ObjSection* section_cursor;
    int tls_section_index = 0;
    size_t relocation_count = 0u;

    assert(object != NULL && object->arch == arch);
    text = objfile_get_section(object, ".text");
    tls = objfile_get_section(object, ".tls");
    symbol = objfile_find_symbol(object, "verified_fallback_tls");
    assert(text != NULL && text->type == SECT_CODE);
    assert((text->flags & (SECT_FLAG_ALLOC | SECT_FLAG_EXEC |
                           SECT_FLAG_WRITE)) ==
           (SECT_FLAG_ALLOC | SECT_FLAG_EXEC));
    if (imported) {
        assert(tls == NULL);
    } else {
        assert(tls != NULL && tls->type == SECT_TLS);
        assert((tls->flags & (SECT_FLAG_ALLOC | SECT_FLAG_WRITE)) ==
               (SECT_FLAG_ALLOC | SECT_FLAG_WRITE));
        for (section_cursor = object->sections;
             section_cursor && section_cursor != tls;
             section_cursor = section_cursor->next) {
            ++tls_section_index;
        }
        assert(section_cursor == tls);
    }
    assert(symbol != NULL && symbol->binding == BIND_TLS);
    if (imported) {
        assert(symbol->type == SYM_UNDEF && symbol->section == -1);
    } else {
        assert(symbol->type == SYM_GLOBAL &&
               symbol->section == tls_section_index);
    }

    for (ObjReloc* relocation = text->relocs; relocation;
         relocation = relocation->next) {
        size_t offset;
        uint8_t modrm;
        if (relocation->type != RELOC_TLSOFF32S) continue;
        assert(relocation->symbol_name != NULL &&
               strcmp(relocation->symbol_name,
                      "verified_fallback_tls") == 0);
        assert(relocation->offset <= text->size &&
               sizeof(uint32_t) <= text->size - relocation->offset);
        offset = (size_t)relocation->offset;
        assert(text->data[offset] == 0u && text->data[offset + 1u] == 0u &&
               text->data[offset + 2u] == 0u &&
               text->data[offset + 3u] == 0u);
        if (arch == ARCH_X64) {
            assert(offset >= 12u);
            assert(text->data[offset - 12u] == 0x64u);
            assert(text->data[offset - 10u] == 0x8bu);
            assert(text->data[offset - 8u] == 0x25u);
            assert(text->data[offset - 7u] == 0u &&
                   text->data[offset - 6u] == 0u &&
                   text->data[offset - 5u] == 0u &&
                   text->data[offset - 4u] == 0u);
            assert(text->data[offset - 3u] == 0x48u ||
                   text->data[offset - 3u] == 0x49u);
            assert(text->data[offset - 2u] == 0x81u);
            modrm = text->data[offset - 1u];
        } else {
            assert(arch == ARCH_X86 && offset >= 9u);
            assert(text->data[offset - 9u] == 0x65u);
            assert(text->data[offset - 8u] == 0x8bu);
            assert((text->data[offset - 7u] & 0xc7u) == 0x05u);
            assert(text->data[offset - 6u] == 0u &&
                   text->data[offset - 5u] == 0u &&
                   text->data[offset - 4u] == 0u &&
                   text->data[offset - 3u] == 0u);
            assert(text->data[offset - 2u] == 0x81u);
            modrm = text->data[offset - 1u];
        }
        assert((modrm & 0xf8u) == 0xc0u);
        ++relocation_count;
    }
    assert(relocation_count >= (imported ? 1u : 3u));
    objfile_free(object);
}

int main(int argc, char** argv)
{
    if (argc == 4 &&
        strcmp(argv[1], "--va-list-pointer-cxx-object") == 0) {
        uint16_t arch;
        bool execute;
        if (strcmp(argv[3], "x86") == 0) {
            arch = ARCH_X86;
            execute = false;
        } else {
            assert(strcmp(argv[3], "x64") == 0);
            arch = ARCH_X64;
            execute = true;
        }
        verify_va_list_pointer_cxx_object(argv[2], arch, execute);
        puts("Verified C++ pointer-based va_list object passed");
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "--sysv-va-fp-object") == 0) {
        verify_sysv_va_fp_object(argv[2], sizeof(void*) == 8u);
        puts("Verified x86-64 SysV floating va_arg object passed");
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "--float-binary-object") == 0) {
        verify_float_binary_object(argv[2]);
        puts("Verified typed floating binary object passed");
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "--sysv-va-aggregate-object") == 0) {
        verify_sysv_va_aggregate_object(argv[2]);
        puts("Verified x86-64 SysV aggregate va_arg object passed");
        return 0;
    }
    if (argc == 3 &&
        strcmp(argv[1], "--sysv-va-aggregate-straddle-object") == 0) {
        verify_sysv_va_aggregate_straddle_object(argv[2]);
        puts("Verified x86-64 SysV aggregate straddle object passed");
        return 0;
    }
    if (argc == 4 && strcmp(argv[1], "--cxx-reference-object") == 0) {
        uint16_t arch;
        if (strcmp(argv[3], "x86") == 0) {
            arch = ARCH_X86;
        } else {
            assert(strcmp(argv[3], "x64") == 0);
            arch = ARCH_X64;
        }
        verify_cxx_reference_local_object(
            argv[2], arch, arch == ARCH_X64 && sizeof(void*) == 8u);
        puts("Verified C++ local reference object passed");
        return 0;
    }
    if (argc == 4 && strcmp(argv[1], "--wide-scalar-object") == 0) {
        uint16_t arch;
        if (strcmp(argv[3], "x86") == 0) {
            arch = ARCH_X86;
        } else {
            assert(strcmp(argv[3], "x64") == 0);
            arch = ARCH_X64;
        }
        verify_wide_scalar_object(argv[2], arch);
        puts("Verified wide-scalar object passed");
        return 0;
    }
    if (argc == 4 &&
        (strcmp(argv[1], "--tls-object") == 0 ||
         strcmp(argv[1], "--tls-import-object") == 0)) {
        uint16_t arch;
        bool imported = strcmp(argv[1], "--tls-import-object") == 0;
        if (strcmp(argv[3], "x86") == 0) {
            arch = ARCH_X86;
        } else {
            assert(strcmp(argv[3], "x64") == 0);
            arch = ARCH_X64;
        }
        verify_tls_object(argv[2], arch, imported);
        puts("Verified local-exec TLS object passed");
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "--switch-loop-labels") == 0) {
        verify_optimized_switch_loop_labels(argv[2]);
        puts("Verified optimized switch loop-label execution passed");
        return 0;
    }
    assert(argc == 6 || argc == 8 || argc == 10 || argc == 12 ||
           argc == 14 || argc == 16);
    verify_object(argv[1], ARCH_X86);
    verify_object(argv[2], ARCH_X64);
    if (sizeof(void*) == 8u) {
        verify_native_execution(argv[2], ARCH_X64);
    } else {
        verify_native_execution(argv[1], ARCH_X86);
    }
    verify_cxx_object(argv[3], sizeof(void*) == 8u);
    verify_global_object(argv[4], ARCH_X86, sizeof(void*) == 4u);
    verify_global_object(argv[5], ARCH_X64, sizeof(void*) == 8u);
    if (argc == 8 || argc == 10 || argc == 12) {
        verify_wide_scalar_object(argv[6], ARCH_X86);
        verify_wide_scalar_object(argv[7], ARCH_X64);
    }
    if (argc == 10 || argc == 12) {
        verify_typeinfo_object(argv[8], ARCH_X86);
        verify_typeinfo_object(argv[9], ARCH_X64);
    }
    if (argc == 12 || argc == 14 || argc == 16) {
        verify_wide_variadic_call_object(argv[10], ARCH_X86);
        verify_wide_variadic_call_object(argv[11], ARCH_X64);
    }
    if (argc == 14 || argc == 16) {
        verify_member_methods_object(argv[12], ARCH_X86,
                                     sizeof(void*) == 4u);
        verify_member_methods_object(argv[13], ARCH_X64,
                                     sizeof(void*) == 8u);
    }
    if (argc == 16) {
        verify_virtual_dispatch_object(argv[14], ARCH_X86,
                                       sizeof(void*) == 4u);
        verify_virtual_dispatch_object(argv[15], ARCH_X64,
                                       sizeof(void*) == 8u);
    }
    puts("Verified typed-SSA production .ro bridge tests passed");
    return 0;
}
