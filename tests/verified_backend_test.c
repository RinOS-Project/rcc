#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

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
    ObjSymbol* switch_symbol;
    ObjSymbol* nested_switch;
    ObjSymbol* switch_promotion;
    ObjSymbol* switch_skips_prefix;
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
    switch_symbol = objfile_find_symbol(object, "verified_switch");
    nested_switch = objfile_find_symbol(object, "verified_nested_switch");
    switch_promotion = objfile_find_symbol(
        object, "verified_switch_promotion");
    switch_skips_prefix = objfile_find_symbol(
        object, "verified_switch_skips_prefix");
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
    assert(object->symbol_count == 35);
    {
        size_t relocation_count = 0u;
        bool found_helper = false;
        bool found_struct_call = false;
        bool found_pair_return = false;
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
                       "verified_large_return") == 0) {
                found_large_return = true;
            }
        }
        assert(relocation_count == 4u && found_helper && found_struct_call &&
               found_pair_return && found_large_return);
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
    int (*call_function)(int);
    int (*pointer_function)(int*, int);
    int (*local_array_function)(int, int, int);
    int (*local_pointer_array_function)(int*, int*);
    int (*local_string_array_function)(int);
    int (*nested_array_function)(void);
    int (*struct_function)(int, int, int*);
    int (*struct_copy_pointer_function)(struct VerifiedPair*);
    int (*nested_struct_function)(int, int, int*);
    int (*union_function)(unsigned int);
    int (*compound_struct_function)(int);
    int (*compound_array_function)(int, int);
    int (*compound_scalar_function)(int);
    int (*struct_parameter_function)(struct VerifiedArgument, int);
    int (*struct_argument_call_function)(int, int, int);
    struct VerifiedReturnPair (*pair_return_function)(int, int);
    int (*pair_return_call_function)(int, int);
    struct VerifiedLargeReturn (*large_return_function)(int, int, int);
    int (*large_return_call_function)(int, int, int);
    int (*conditional_function)(int, int*);
    int (*pointer_compound_function)(int**, int);
    int (*pointer_postincrement_function)(int**);
    long (*pointer_difference_function)(int*, int*);
    int (*switch_function)(int);
    int (*nested_switch_function)(int, int);
    int (*switch_promotion_function)(unsigned char);
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
    long page = sysconf(_SC_PAGESIZE);
    assert(object != NULL && object->section_count > 0 && page > 0);
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
        mapping.bases[section_index] = mmap(
            NULL, size, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(mapping.bases[section_index] != MAP_FAILED);
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
        int protection = PROT_READ;
        if (mapping.bases[section_index] == NULL) continue;
        if ((section->flags & SECT_FLAG_WRITE) != 0u) protection |= PROT_WRITE;
        if ((section->flags & SECT_FLAG_EXEC) != 0u) protection |= PROT_EXEC;
        assert(mprotect(mapping.bases[section_index],
                        mapping.sizes[section_index], protection) == 0);
    }
    return mapping;
}

static void unmap_object(MappedObject* mapping)
{
    size_t index;
    for (index = 0u; index < mapping->count; ++index) {
        if (mapping->bases[index] != NULL) {
            assert(munmap(mapping->bases[index],
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
        int (*read_function)(void);
        int (*write_function)(int);
        int (*external_function)(void);
        int (*array_read_function)(int);
        int (*array_write_function)(int, int);
        int (*string_function)(int);
        int (*aggregate_read_function)(int);
        int (*aggregate_write_function)(int);
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

int main(int argc, char** argv)
{
    assert(argc == 6);
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
    puts("Verified typed-SSA production .ro bridge tests passed");
    return 0;
}
