#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef uint64_t (*wide_nullary_fn)(void);
typedef uint64_t (*wide_unary_fn)(uint64_t);
typedef uint64_t (*wide_binary_fn)(uint64_t, uint64_t);
typedef int64_t (*signed_wide_unary_fn)(int64_t);
typedef uint64_t (*unsigned_widen_fn)(uint32_t);
typedef int64_t (*signed_widen_fn)(int32_t);
typedef uint64_t (*wide_narrow_binary_fn)(uint64_t, int32_t);
typedef int (*wide_compare_fn)(uint64_t, uint64_t);
typedef int (*signed_wide_compare_fn)(int64_t, int64_t);
typedef int64_t (*signed_wide_binary_fn)(int64_t, int64_t);
typedef uint64_t (*wide_shift_fn)(uint64_t, int32_t);
typedef int64_t (*signed_wide_shift_fn)(int64_t, int32_t);
typedef uint64_t (*atomic_wide_load_fn)(volatile uint64_t*);
typedef void (*atomic_wide_store_fn)(volatile uint64_t*, uint64_t);
typedef uint64_t (*atomic_wide_exchange_fn)(volatile uint64_t*, uint64_t);
typedef int (*atomic_wide_compare_fn)(volatile uint64_t*, uint64_t*,
                                      uint64_t);

static ObjSymbol* required_function(ObjectFile* object, const char* name) {
    ObjSymbol* symbol = objfile_find_symbol(object, name);
    assert(symbol != NULL && symbol->section >= 0);
    assert(symbol->binding == BIND_CODE);
    return symbol;
}

#define LOAD_FUNCTION(target, object, mapping, symbol_name)                 \
    do {                                                                    \
        ObjSymbol* symbol = required_function((object), (symbol_name));      \
        void* address = (mapping) + symbol->value;                           \
        memcpy(&(target), &address, sizeof(target));                         \
    } while (0)

int main(int argc, char** argv) {
    ObjectFile* object;
    ObjSection* code;
    ObjSection* data;
    ObjSymbol* global;
    ObjReloc* relocation;
    long page_size;
    size_t data_offset;
    size_t mapping_size;
    uint8_t* mapping;
    wide_nullary_fn literal;
    wide_unary_fn identity;
    wide_binary_fn add;
    wide_binary_fn sub;
    wide_binary_fn bits;
    signed_wide_unary_fn negate;
    wide_unary_fn local;
    wide_unary_fn assign;
    signed_widen_fn assign_narrow;
    wide_narrow_binary_fn add_narrow;
    wide_compare_fn equal;
    wide_compare_fn less_unsigned;
    signed_wide_compare_fn greater_signed;
    signed_wide_compare_fn less_equal_signed;
    wide_shift_fn shift_left;
    wide_shift_fn shift_right;
    signed_wide_shift_fn shift_right_signed;
    wide_binary_fn multiply;
    signed_wide_binary_fn multiply_signed;
    wide_binary_fn divide;
    wide_binary_fn modulo;
    signed_wide_binary_fn divide_signed;
    signed_wide_binary_fn modulo_signed;
    wide_nullary_fn call;
    wide_nullary_fn call_promoted;
    unsigned_widen_fn widen_unsigned;
    signed_widen_fn widen_signed;
    wide_nullary_fn global_load;
    wide_unary_fn global_store;
    atomic_wide_load_fn atomic_load;
    atomic_wide_store_fn atomic_store;
    atomic_wide_exchange_fn atomic_exchange;
    atomic_wide_compare_fn atomic_compare;

    assert(argc == 2);
    object = objfile_read(argv[1]);
    assert(object != NULL && object->arch == ARCH_X86);
    code = objfile_get_section(object, ".text");
    data = objfile_get_section(object, ".data");
    global = objfile_find_symbol(object, "abi_wide_global");
    assert(code != NULL && code->size > 0u);
    assert(data != NULL && data->size >= sizeof(uint64_t));
    assert(global != NULL && global->binding == BIND_DATA);

    page_size = sysconf(_SC_PAGESIZE);
    assert(page_size > 0);
    data_offset = (((size_t)code->size + (size_t)page_size - 1u) /
                   (size_t)page_size) * (size_t)page_size;
    mapping_size = data_offset +
        ((((size_t)data->size + (size_t)page_size - 1u) /
          (size_t)page_size) * (size_t)page_size);
    mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(mapping != MAP_FAILED);
    memcpy(mapping, code->data, (size_t)code->size);
    memcpy(mapping + data_offset, data->data, (size_t)data->size);

    for (relocation = code->relocs; relocation;
         relocation = relocation->next) {
        uint32_t address;
        assert(strcmp(relocation->symbol_name, "abi_wide_global") == 0);
        assert(relocation->type == RELOC_ABS32U ||
               relocation->type == RELOC_ABS32);
        assert(relocation->offset + sizeof(address) <= code->size);
        address = (uint32_t)(uintptr_t)(mapping + data_offset +
                                        global->value + relocation->addend);
        memcpy(mapping + relocation->offset, &address, sizeof(address));
    }
    assert(mprotect(mapping, data_offset, PROT_READ | PROT_EXEC) == 0);

    LOAD_FUNCTION(literal, object, mapping, "abi_wide_literal");
    LOAD_FUNCTION(identity, object, mapping, "abi_wide_identity");
    LOAD_FUNCTION(add, object, mapping, "abi_wide_add");
    LOAD_FUNCTION(sub, object, mapping, "abi_wide_sub");
    LOAD_FUNCTION(bits, object, mapping, "abi_wide_bits");
    LOAD_FUNCTION(negate, object, mapping, "abi_wide_negate");
    LOAD_FUNCTION(local, object, mapping, "abi_wide_local");
    LOAD_FUNCTION(assign, object, mapping, "abi_wide_assign");
    LOAD_FUNCTION(assign_narrow, object, mapping,
                  "abi_wide_assign_narrow");
    LOAD_FUNCTION(add_narrow, object, mapping, "abi_wide_add_narrow");
    LOAD_FUNCTION(equal, object, mapping, "abi_wide_equal");
    LOAD_FUNCTION(less_unsigned, object, mapping,
                  "abi_wide_less_unsigned");
    LOAD_FUNCTION(greater_signed, object, mapping,
                  "abi_wide_greater_signed");
    LOAD_FUNCTION(less_equal_signed, object, mapping,
                  "abi_wide_less_equal_signed");
    LOAD_FUNCTION(shift_left, object, mapping, "abi_wide_shift_left");
    LOAD_FUNCTION(shift_right, object, mapping, "abi_wide_shift_right");
    LOAD_FUNCTION(shift_right_signed, object, mapping,
                  "abi_wide_shift_right_signed");
    LOAD_FUNCTION(multiply, object, mapping, "abi_wide_multiply");
    LOAD_FUNCTION(multiply_signed, object, mapping,
                  "abi_wide_multiply_signed");
    LOAD_FUNCTION(divide, object, mapping, "abi_wide_divide");
    LOAD_FUNCTION(modulo, object, mapping, "abi_wide_modulo");
    LOAD_FUNCTION(divide_signed, object, mapping,
                  "abi_wide_divide_signed");
    LOAD_FUNCTION(modulo_signed, object, mapping,
                  "abi_wide_modulo_signed");
    LOAD_FUNCTION(call, object, mapping, "abi_wide_call");
    LOAD_FUNCTION(call_promoted, object, mapping,
                  "abi_wide_call_promoted");
    LOAD_FUNCTION(widen_unsigned, object, mapping,
                  "abi_wide_widen_unsigned");
    LOAD_FUNCTION(widen_signed, object, mapping,
                  "abi_wide_widen_signed");
    LOAD_FUNCTION(global_load, object, mapping, "abi_wide_global_load");
    LOAD_FUNCTION(global_store, object, mapping, "abi_wide_global_store");
    LOAD_FUNCTION(atomic_load, object, mapping, "abi_atomic_u64_load");
    LOAD_FUNCTION(atomic_store, object, mapping, "abi_atomic_u64_store");
    LOAD_FUNCTION(atomic_exchange, object, mapping,
                  "abi_atomic_u64_exchange");
    LOAD_FUNCTION(atomic_compare, object, mapping,
                  "abi_atomic_u64_compare");

    assert(literal() == UINT64_C(0x1234567889abcdef));
    assert(identity(UINT64_C(0xfedcba9876543210)) ==
           UINT64_C(0xfedcba9876543210));
    assert(add(UINT64_C(0x00000001ffffffff), UINT64_C(2)) ==
           UINT64_C(0x0000000200000001));
    assert(sub(UINT64_C(0x0000000200000000), UINT64_C(1)) ==
           UINT64_C(0x00000001ffffffff));
    assert(bits(UINT64_C(0x0f0ff0f05555aaaa),
                UINT64_C(0x33333333cccccccc)) ==
           (UINT64_C(0x0f0ff0f05555aaaa) |
            UINT64_C(0x33333333cccccccc)));
    assert(negate(INT64_C(0x0000000100000001)) ==
           -INT64_C(0x0000000100000001));
    assert(local(UINT64_C(0xabcdef0123456789)) ==
           UINT64_C(0xabcdef0123456789));
    assert(assign(UINT64_C(0x8899aabbccddeeff)) ==
           UINT64_C(0x8899aabbccddeeff));
    assert(assign_narrow(-11) == INT64_C(-11));
    assert(add_narrow(UINT64_C(0x0000000200000000), -1) ==
           UINT64_C(0x00000001ffffffff));
    assert(equal(UINT64_C(0x1234567889abcdef),
                 UINT64_C(0x1234567889abcdef)) == 1);
    assert(equal(UINT64_C(0x1234567889abcdef),
                 UINT64_C(0x1234567989abcdef)) == 0);
    assert(less_unsigned(UINT64_C(0x7fffffffffffffff),
                         UINT64_C(0x8000000000000000)) == 1);
    assert(greater_signed(INT64_C(7), INT64_C(-9)) == 1);
    assert(less_equal_signed(INT64_C(-9), INT64_C(-9)) == 1);
    assert(shift_left(UINT64_C(0x0000000080000001), 1) ==
           UINT64_C(0x0000000100000002));
    assert(shift_left(UINT64_C(3), 33) == UINT64_C(0x0000000600000000));
    assert(shift_right(UINT64_C(0x8000000100000000), 1) ==
           UINT64_C(0x4000000080000000));
    assert(shift_right(UINT64_C(0x8000000100000000), 33) ==
           UINT64_C(0x0000000040000000));
    assert(shift_right_signed(-INT64_C(0x100000000), 33) == INT64_C(-1));
    assert(multiply(UINT64_C(0x0000000100000003), UINT64_C(0x100000005)) ==
           UINT64_C(0x000000080000000f));
    assert((int64_t)multiply_signed(INT64_C(-1234567), INT64_C(7654321)) ==
           INT64_C(-9449772114007));
    assert(divide(UINT64_C(0xfedcba9876543210), UINT64_C(0x100000003)) ==
           UINT64_C(0xfedcba95));
    assert(modulo(UINT64_C(0xfedcba9876543210), UINT64_C(0x100000003)) ==
           UINT64_C(0x79be0251));
    assert(divide(UINT64_C(7), UINT64_C(11)) == UINT64_C(0));
    assert(modulo(UINT64_C(7), UINT64_C(11)) == UINT64_C(7));
    assert(divide(UINT64_MAX, UINT64_C(1)) == UINT64_MAX);
    assert(divide_signed(INT64_C(-9449772114007), INT64_C(7654321)) ==
           INT64_C(-1234567));
    assert(divide_signed(INT64_C(-21), INT64_C(-4)) == INT64_C(5));
    assert(divide_signed(INT64_C(21), INT64_C(-4)) == INT64_C(-5));
    assert(modulo_signed(INT64_C(-9449772114010), INT64_C(7654321)) ==
           INT64_C(-3));
    assert(modulo_signed(INT64_C(21), INT64_C(-4)) == INT64_C(1));
    {
        uint64_t state = UINT64_C(0x9e3779b97f4a7c15);
        unsigned iteration;
        for (iteration = 0; iteration < 512u; ++iteration) {
            uint64_t dividend;
            uint64_t divisor;
            int64_t signed_dividend;
            int64_t signed_divisor;
            state = state * UINT64_C(6364136223846793005) + UINT64_C(1);
            dividend = state;
            state = state * UINT64_C(6364136223846793005) + UINT64_C(1);
            divisor = state | UINT64_C(1);
            assert(divide(dividend, divisor) == dividend / divisor);
            assert(modulo(dividend, divisor) == dividend % divisor);
            signed_dividend = (int64_t)dividend;
            signed_divisor = (int64_t)divisor;
            if (signed_divisor == 0) signed_divisor = 1;
            if (signed_dividend == INT64_MIN && signed_divisor == -1) {
                signed_divisor = 1;
            }
            assert(divide_signed(signed_dividend, signed_divisor) ==
                   signed_dividend / signed_divisor);
            assert(modulo_signed(signed_dividend, signed_divisor) ==
                   signed_dividend % signed_divisor);
        }
    }
    assert(call() == UINT64_C(0x0000000200000001));
    assert(call_promoted() == UINT64_C(7));
    assert(widen_unsigned(UINT32_C(0xf1234567)) ==
           UINT64_C(0x00000000f1234567));
    assert(widen_signed(-7) == INT64_C(-7));
    assert(global_load() == UINT64_C(0x1234567889abcdef));
    assert(global_store(UINT64_C(0xcafebabedeadbeef)) ==
           UINT64_C(0xcafebabedeadbeef));
    assert(global_load() == UINT64_C(0xcafebabedeadbeef));
    {
        volatile uint64_t atomic_value = UINT64_C(0x100000005);
        uint64_t expected;
        assert(atomic_load(&atomic_value) == UINT64_C(0x100000005));
        atomic_store(&atomic_value, UINT64_C(0x200000007));
        assert(atomic_value == UINT64_C(0x200000007));
        assert(atomic_exchange(&atomic_value, UINT64_C(0x30000000b)) ==
               UINT64_C(0x200000007));
        assert(atomic_value == UINT64_C(0x30000000b));
        expected = UINT64_C(0x30000000b);
        assert(atomic_compare(&atomic_value, &expected,
                              UINT64_C(0x800000011)) == 1);
        assert(expected == UINT64_C(0x30000000b));
        assert(atomic_value == UINT64_C(0x800000011));
        expected = UINT64_C(7);
        assert(atomic_compare(&atomic_value, &expected, UINT64_C(9)) == 0);
        assert(expected == UINT64_C(0x800000011));
        assert(atomic_value == UINT64_C(0x800000011));
    }

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("i686 EDX:EAX scalar ABI execution test passed");
    return 0;
}
