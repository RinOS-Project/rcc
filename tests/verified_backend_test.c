#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static void* map_text(const ObjSection* text, size_t* mapping_size)
{
    long page = sysconf(_SC_PAGESIZE);
    size_t size;
    void* memory;
    assert(text != NULL && text->size != 0u && page > 0);
    assert(text->size <= (uint64_t)SIZE_MAX);
    size = ((size_t)text->size + (size_t)page - 1u) &
        ~((size_t)page - 1u);
    memory = mmap(NULL, size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(memory != MAP_FAILED);
    memcpy(memory, text->data, (size_t)text->size);
    assert(mprotect(memory, size, PROT_READ | PROT_EXEC) == 0);
    *mapping_size = size;
    return memory;
}

static void* symbol_address(void* text_memory, const ObjSymbol* symbol)
{
    assert(text_memory != NULL && symbol != NULL && symbol->section == 0);
    return (uint8_t*)text_memory + symbol->value;
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
    ObjSymbol* pointer_add;
    ObjSymbol* pointer_sub;
    ObjSymbol* conditional;
    ObjSymbol* logical_and;
    ObjSymbol* logical_or;
    ObjSymbol* pointer_compound;
    ObjSymbol* pointer_postincrement;
    ObjSymbol* lvalue_once;
    ObjSymbol* pointer_difference;
    ObjReloc* relocation;
    assert(object != NULL && object->arch == arch);
    text = objfile_get_section(object, ".text");
    call = objfile_find_symbol(object, "verified_call");
    helper = objfile_find_symbol(
        object, "tests/verified_backend.c::verified_helper");
    load = objfile_find_symbol(object, "verified_load");
    control = objfile_find_symbol(object, "verified_control");
    index = objfile_find_symbol(object, "verified_index");
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
    assert(object->symbol_count == 14);
    relocation = text->relocs;
    assert(relocation != NULL && relocation->next == NULL);
    assert(relocation->type == RELOC_REL32);
    assert(strcmp(relocation->symbol_name,
                  "tests/verified_backend.c::verified_helper") == 0);
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
    int (*pointer_function)(int*, int);
    int (*conditional_function)(int, int*);
    int (*pointer_compound_function)(int**, int);
    int (*pointer_postincrement_function)(int**);
    long (*pointer_difference_function)(int*, int*);
    int* cursor;
    void* address;
    assert(object != NULL && object->arch == arch);
    text = objfile_get_section(object, ".text");
    memory = map_text(text, &mapping_size);

    symbol = objfile_find_symbol(object, "verified_index");
    address = symbol_address(memory, symbol);
    memcpy(&pointer_function, &address, sizeof(pointer_function));
    assert(pointer_function(values + 2, -1) == 22);

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

    assert(munmap(memory, mapping_size) == 0);
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

static void verify_fallback_object(const char* path)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* data;
    ObjSymbol* symbol;
    assert(object != NULL && object->arch == ARCH_X64);
    data = objfile_get_section(object, ".data");
    symbol = objfile_find_symbol(object, "verified_fallback_data");
    assert(data != NULL && data->size == 4u);
    assert(symbol != NULL && symbol->binding == BIND_DATA &&
           symbol->section >= 0);
    objfile_free(object);
}

int main(int argc, char** argv)
{
    assert(argc == 5);
    verify_object(argv[1], ARCH_X86);
    verify_object(argv[2], ARCH_X64);
    if (sizeof(void*) == 8u) {
        verify_native_execution(argv[2], ARCH_X64);
    } else {
        verify_native_execution(argv[1], ARCH_X86);
    }
    verify_cxx_object(argv[3]);
    verify_fallback_object(argv[4]);
    puts("Verified typed-SSA production .ro bridge tests passed");
    return 0;
}
