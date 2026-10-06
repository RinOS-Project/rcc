#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "objfile.h"

#include <assert.h>
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
    size_t size;
    void* memory;
    assert(text != NULL && text->size != 0u && page != 0u);
    assert(text->size <= (uint64_t)SIZE_MAX);
    size = ((size_t)text->size + (size_t)page - 1u) &
        ~((size_t)page - 1u);
    memory = verified_map(size);
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
               symbol->section == 0 && relocation->offset <= text->size &&
               sizeof(encoded) <= text->size - relocation->offset);
        place = (uint8_t*)memory + relocation->offset;
        target = (int64_t)(uintptr_t)memory + (int64_t)symbol->value +
            relocation->addend;
        delta = target - (int64_t)(uintptr_t)(place + sizeof(encoded));
        encoded = (int32_t)delta;
        assert((int64_t)encoded == delta);
        memcpy(place, &encoded, sizeof(encoded));
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
    ObjSymbol* truth_conditional_symbol;
    ObjSymbol* mul_symbol;
    ObjSymbol* call_symbol;
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
    ObjSymbol* while_loop_symbol;
    ObjSymbol* for_loop_symbol;
    ObjSymbol* do_loop_symbol;
    ObjSymbol* while_continue_symbol;
    ObjSymbol* for_break_symbol;
    ObjSymbol* do_control_symbol;
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
    truth_conditional_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_truth_conditional");
    mul_symbol = objfile_find_symbol(object, "verified_wide_scalar_mul");
    call_symbol = objfile_find_symbol(object, "verified_wide_scalar_call");
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
    while_loop_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_while_loop");
    for_loop_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_for_loop");
    do_loop_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_do_loop");
    while_continue_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_while_continue");
    for_break_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_for_break");
    do_control_symbol = objfile_find_symbol(
        object, "verified_wide_scalar_do_control");
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
    assert(truth_conditional_symbol != NULL &&
           truth_conditional_symbol->type == SYM_GLOBAL &&
           truth_conditional_symbol->binding == BIND_CODE &&
           truth_conditional_symbol->section == 0);
    assert(mul_symbol != NULL && mul_symbol->type == SYM_GLOBAL &&
           mul_symbol->binding == BIND_CODE && mul_symbol->section == 0);
    assert(call_symbol != NULL && call_symbol->type == SYM_GLOBAL &&
           call_symbol->binding == BIND_CODE && call_symbol->section == 0);
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
        unsigned long long RINOS_ABI (*truth_conditional_function)(unsigned long long);
        unsigned long long RINOS_ABI (*mul_function)(unsigned long long);
        unsigned long long RINOS_ABI (*call_function)(unsigned long long);
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
        unsigned long long RINOS_ABI (*while_loop_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*for_loop_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*do_loop_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*while_continue_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*for_break_function)(
            unsigned long long, unsigned int);
        unsigned long long RINOS_ABI (*do_control_function)(
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
        assert(verified_unmap(memory, mapping_size) == 0);
    }
    objfile_free(object);
}

static void verify_object(const char* path, uint16_t arch)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* call;
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
    ObjReloc* relocation;
    assert(object != NULL && object->arch == arch);
    text = objfile_get_section(object, ".text");
    call = objfile_find_symbol(object, "verified_call");
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
    assert(text != NULL && text->size != 0u && text->memory_size == text->size);
    assert((text->flags & (SECT_FLAG_ALLOC | SECT_FLAG_EXEC)) ==
           (SECT_FLAG_ALLOC | SECT_FLAG_EXEC));
    assert((text->flags & SECT_FLAG_WRITE) == 0u);
    assert(call != NULL && call->type == SYM_GLOBAL && call->section == 0);
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
    assert(object->symbol_count == 39);
    {
        size_t relocation_count = 0u;
        bool found_helper = false;
        bool found_struct_call = false;
        bool found_pair_return = false;
        bool found_triple_return = false;
        bool found_large_return = false;
        for (relocation = text->relocs; relocation;
             relocation = relocation->next) {
            assert(relocation->type == RELOC_REL32);
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
        }
        assert(relocation_count == 5u && found_helper && found_struct_call &&
               found_pair_return && found_triple_return &&
               found_large_return);
    }
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
    int* cursor;
    void* address;
    assert(object != NULL && object->arch == arch);
    text = objfile_get_section(object, ".text");
    memory = map_text(object, text, &mapping_size);

    symbol = objfile_find_symbol(object, "verified_call");
    address = symbol_address(memory, symbol);
    memcpy(&call_function, &address, sizeof(call_function));
    assert(call_function(7) == 22);

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

    assert(verified_unmap(memory, mapping_size) == 0);
    objfile_free(object);
}

static void verify_cxx_object(const char* path)
{
    ObjectFile* object = objfile_read(path);
    ObjSymbol* symbol;
    assert(object != NULL && object->arch == ARCH_X64);
    symbol = objfile_find_symbol(object, "verified_cxx");
    assert(symbol != NULL && symbol->type == SYM_GLOBAL &&
           symbol->binding == BIND_CODE && symbol->section == 0);
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

int main(int argc, char** argv)
{
    assert(argc == 6 || argc == 8 || argc == 10);
    verify_object(argv[1], ARCH_X86);
    verify_object(argv[2], ARCH_X64);
    if (sizeof(void*) == 8u) {
        verify_native_execution(argv[2], ARCH_X64);
    } else {
        verify_native_execution(argv[1], ARCH_X86);
    }
    verify_cxx_object(argv[3]);
    verify_global_object(argv[4], ARCH_X86, sizeof(void*) == 4u);
    verify_global_object(argv[5], ARCH_X64, sizeof(void*) == 8u);
    if (argc == 8) {
        verify_wide_scalar_object(argv[6], ARCH_X86);
        verify_wide_scalar_object(argv[7], ARCH_X64);
    }
    if (argc == 10) {
        verify_wide_scalar_object(argv[6], ARCH_X86);
        verify_wide_scalar_object(argv[7], ARCH_X64);
        verify_typeinfo_object(argv[8], ARCH_X86);
        verify_typeinfo_object(argv[9], ARCH_X64);
    }
    puts("Verified typed-SSA production .ro bridge tests passed");
    return 0;
}
