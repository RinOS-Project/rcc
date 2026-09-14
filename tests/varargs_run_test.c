#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef int (*nullary_int_function)(void);
typedef long long (*nullary_wide_function)(void);
typedef double (*nullary_double_function)(void);
typedef int (*variadic_int_function)(int, ...);
typedef long long (*variadic_wide_function)(int, ...);

static ObjSection* code_section(ObjectFile* object)
{
    ObjSection* section = object->sections;
    while (section && section->type != SECT_CODE) section = section->next;
    assert(section != NULL);
    return section;
}

#define LOAD_FUNCTION(target, object, mapping, symbol_name)                 \
    do {                                                                    \
        ObjSymbol* symbol = objfile_find_symbol((object), (symbol_name));    \
        void* address;                                                      \
        assert(symbol != NULL && symbol->section >= 0);                     \
        address = (mapping) + symbol->value;                                \
        memcpy(&(target), &address, sizeof(target));                        \
    } while (0)

int main(int argc, char** argv)
{
    ObjectFile* object;
    ObjSection* code;
    uint8_t* mapping;
    long page_size;
    size_t mapping_size;
    nullary_int_function generated_register_varargs;
    nullary_int_function generated_stack_varargs;
    nullary_wide_function generated_wide_varargs;
    nullary_double_function generated_floating_varargs;
    nullary_double_function generated_float_promotion_varargs;
    nullary_int_function generated_fixed_float_conversion;
    nullary_double_function generated_named_floating;
    nullary_double_function generated_stack_floating;
    nullary_int_function generated_copy_varargs;
    nullary_int_function generated_pointer_varargs;
    nullary_int_function generated_pair_varargs;
    nullary_double_function generated_mixed_varargs;
    variadic_int_function sum_values;
    variadic_wide_function select_wide_value;
    variadic_int_function copy_values;
    variadic_int_function pointer_value;

    assert(argc == 2);
    object = objfile_read(argv[1]);
    assert(object != NULL);
#if defined(__i386__)
    assert(object->arch == ARCH_X86);
#else
    assert(object->arch == ARCH_X64);
#endif
    code = code_section(object);
    page_size = sysconf(_SC_PAGESIZE);
    assert(page_size > 0);
    mapping_size = ((size_t)code->size + (size_t)page_size - 1u) /
                   (size_t)page_size * (size_t)page_size;
    mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(mapping != MAP_FAILED);
    memcpy(mapping, code->data, (size_t)code->size);
    assert(mprotect(mapping, mapping_size, PROT_READ | PROT_EXEC) == 0);

    LOAD_FUNCTION(generated_register_varargs, object, mapping,
                  "generated_register_varargs");
    LOAD_FUNCTION(generated_stack_varargs, object, mapping,
                  "generated_stack_varargs");
    LOAD_FUNCTION(generated_wide_varargs, object, mapping,
                  "generated_wide_varargs");
    LOAD_FUNCTION(generated_floating_varargs, object, mapping,
                  "generated_floating_varargs");
    LOAD_FUNCTION(generated_float_promotion_varargs, object, mapping,
                  "generated_float_promotion_varargs");
    LOAD_FUNCTION(generated_fixed_float_conversion, object, mapping,
                  "generated_fixed_float_conversion");
    LOAD_FUNCTION(generated_named_floating, object, mapping,
                  "generated_named_floating");
    LOAD_FUNCTION(generated_stack_floating, object, mapping,
                  "generated_stack_floating");
    LOAD_FUNCTION(generated_copy_varargs, object, mapping,
                  "generated_copy_varargs");
    LOAD_FUNCTION(generated_pointer_varargs, object, mapping,
                  "generated_pointer_varargs");
    LOAD_FUNCTION(generated_pair_varargs, object, mapping,
                  "generated_pair_varargs");
    LOAD_FUNCTION(generated_mixed_varargs, object, mapping,
                  "generated_mixed_varargs");
    LOAD_FUNCTION(sum_values, object, mapping, "sum_values");
    LOAD_FUNCTION(select_wide_value, object, mapping, "select_wide_value");
    LOAD_FUNCTION(copy_values, object, mapping, "copy_values");
    LOAD_FUNCTION(pointer_value, object, mapping, "pointer_value");

    assert(generated_register_varargs() == 15);
    assert(generated_stack_varargs() == 36);
    assert(generated_wide_varargs() == 0x1122334455667788LL);
    assert(generated_floating_varargs() > 7.4999 &&
           generated_floating_varargs() < 7.5001);
    assert(generated_float_promotion_varargs() > 7.4999 &&
           generated_float_promotion_varargs() < 7.5001);
    assert(generated_fixed_float_conversion() == 3);
    assert(generated_named_floating() > 6.9999 &&
           generated_named_floating() < 7.0001);
    assert(generated_stack_floating() > 44.9999 &&
           generated_stack_floating() < 45.0001);
    assert(generated_copy_varargs() == 447);
    assert(generated_pointer_varargs() == 73);
    assert(generated_pair_varargs() == 42);
    assert(generated_mixed_varargs() > 7.4999 &&
           generated_mixed_varargs() < 7.5001);
    assert(sum_values(8, 1, 2, 3, 4, 5, 6, 7, 8) == 36);
    assert(select_wide_value(0, 0x1020304050607080LL) ==
           0x1020304050607080LL);
    assert(copy_values(0, 3, 9) == 339);
    {
        int value = 91;
        assert(pointer_value(0, &value) == 91);
    }

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("C17 integer/pointer/floating varargs ABI test passed");
    return 0;
}
