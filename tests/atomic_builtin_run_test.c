#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if (defined(__x86_64__) || defined(__i386__)) && !defined(_WIN32)
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

static ObjSymbol* function_symbol(ObjectFile* object, const char* name) {
    ObjSymbol* symbol = objfile_find_symbol(object, name);
    assert(symbol != NULL);
    assert(symbol->section >= 0);
    assert(symbol->binding == BIND_CODE);
    return symbol;
}

static int section_contains(const ObjSection* section,
                            const uint8_t* sequence, size_t size) {
    uint64_t offset;
    if (!section || !sequence || size == 0u || section->size < size) return 0;
    for (offset = 0u; offset + size <= section->size; ++offset) {
        if (memcmp(section->data + offset, sequence, size) == 0) return 1;
    }
    return 0;
}

#if (defined(__x86_64__) || defined(__i386__)) && !defined(_WIN32)
typedef uint32_t (*atomic_load_fn)(volatile uint32_t*);
typedef void (*atomic_store_fn)(volatile uint32_t*, uint32_t);
typedef uint32_t (*atomic_binary_fn)(volatile uint32_t*, uint32_t);
typedef int (*atomic_compare_bool_fn)(volatile uint32_t*, uint32_t, uint32_t);
typedef uint32_t (*atomic_compare_value_fn)(volatile uint32_t*, uint32_t,
                                            uint32_t);
typedef int (*standard_atomic_compare_fn)(volatile uint32_t*, uint32_t*,
                                          uint32_t);
typedef void (*atomic_release_fn)(volatile uint32_t*);
typedef void (*atomic_fence_fn)(void);
typedef uint32_t (*atomic_u8_load_fn)(volatile uint8_t*);
typedef void (*atomic_u8_store_fn)(volatile uint8_t*, uint32_t);
typedef uint32_t (*atomic_u8_binary_fn)(volatile uint8_t*, uint32_t);
typedef int (*atomic_u8_compare_fn)(volatile uint8_t*, uint8_t*, uint32_t);
typedef uint32_t (*atomic_u8_sync_compare_fn)(volatile uint8_t*, uint32_t,
                                              uint32_t);
typedef void (*atomic_u8_release_fn)(volatile uint8_t*);
typedef uint32_t (*atomic_u16_load_fn)(volatile uint16_t*);
typedef void (*atomic_u16_store_fn)(volatile uint16_t*, uint32_t);
typedef uint32_t (*atomic_u16_binary_fn)(volatile uint16_t*, uint32_t);
typedef int (*atomic_u16_compare_fn)(volatile uint16_t*, uint16_t*,
                                     uint32_t);
typedef int32_t (*atomic_i8_binary_fn)(volatile int8_t*, int32_t);
typedef int32_t (*atomic_i16_binary_fn)(volatile int16_t*, int32_t);
typedef int (*atomic_bool_binary_fn)(volatile _Bool*, int);
#if defined(__x86_64__)
typedef uint64_t (*atomic_u32_wide_binary_fn)(volatile uint32_t*, uint32_t);
typedef int64_t (*atomic_i32_wide_binary_fn)(volatile int32_t*, int32_t);
typedef uint64_t (*atomic_u64_load_fn)(volatile uint64_t*);
typedef void (*atomic_u64_store_fn)(volatile uint64_t*, uint64_t);
typedef uint64_t (*atomic_u64_binary_fn)(volatile uint64_t*, uint64_t);
typedef int (*atomic_u64_compare_fn)(volatile uint64_t*, uint64_t*,
                                     uint64_t);
#else
typedef long (*atomic_long_binary_fn)(volatile long*, long);
#endif

typedef struct {
    atomic_binary_fn fetch_add;
    volatile uint32_t* counter;
    unsigned iterations;
} AtomicWorker;

typedef struct {
    atomic_u16_binary_fn fetch_add;
    volatile uint16_t* counter;
    unsigned iterations;
} AtomicWorker16;

typedef struct {
    atomic_binary_fn fetch_or;
    volatile uint32_t* value;
    uint32_t operand;
    unsigned iterations;
} AtomicBitwiseWorker;

#if defined(__x86_64__)
typedef struct {
    atomic_u64_binary_fn fetch_add;
    volatile uint64_t* counter;
    unsigned iterations;
} AtomicWorker64;
#endif

static void* atomic_worker(void* argument) {
    AtomicWorker* worker = argument;
    unsigned i;
    for (i = 0; i < worker->iterations; ++i) {
        worker->fetch_add(worker->counter, 1u);
    }
    return NULL;
}

static void* atomic_worker16(void* argument) {
    AtomicWorker16* worker = argument;
    unsigned i;
    for (i = 0; i < worker->iterations; ++i) {
        worker->fetch_add(worker->counter, 1u);
    }
    return NULL;
}

static void* atomic_bitwise_worker(void* argument) {
    AtomicBitwiseWorker* worker = argument;
    unsigned i;
    for (i = 0; i < worker->iterations; ++i) {
        worker->fetch_or(worker->value, worker->operand);
    }
    return NULL;
}

#if defined(__x86_64__)
static void* atomic_worker64(void* argument) {
    AtomicWorker64* worker = argument;
    unsigned i;
    for (i = 0; i < worker->iterations; ++i) {
        worker->fetch_add(worker->counter, UINT64_C(1));
    }
    return NULL;
}
#endif

#define LOAD_FUNCTION(target, object, mapping, symbol_name)                  \
    do {                                                                     \
        ObjSymbol* load_symbol = function_symbol((object), (symbol_name));    \
        void* load_address = (mapping) + load_symbol->value;                  \
        memcpy(&(target), &load_address, sizeof(target));                     \
    } while (0)
#endif

int main(int argc, char** argv) {
    ObjectFile* x86_object;
    ObjSection* x86_code;
    static const uint8_t byte_xadd[] = {0xF0, 0x0F, 0xC0};
    static const uint8_t word_xadd[] = {0x66, 0xF0, 0x0F, 0xC1};
    static const uint8_t byte_cmpxchg[] = {0xF0, 0x0F, 0xB0};
    static const uint8_t word_cmpxchg[] = {0x66, 0xF0, 0x0F, 0xB1};
    assert(argc == 3);
    x86_object = objfile_read(argv[1]);
    assert(x86_object != NULL && x86_object->arch == ARCH_X86);
    x86_code = objfile_get_section(x86_object, ".text");
    assert(x86_code != NULL);
    assert(section_contains(x86_code, byte_xadd, sizeof(byte_xadd)));
    assert(section_contains(x86_code, word_xadd, sizeof(word_xadd)));
    assert(section_contains(x86_code, byte_cmpxchg, sizeof(byte_cmpxchg)));
    assert(section_contains(x86_code, word_cmpxchg, sizeof(word_cmpxchg)));
    (void)function_symbol(x86_object,
                          "standard_atomic_long_fetch_xor_value");
    objfile_free(x86_object);
#if (defined(__x86_64__) || defined(__i386__)) && !defined(_WIN32)
#if defined(__x86_64__)
    ObjectFile* object = objfile_read(argv[2]);
#else
    ObjectFile* object = objfile_read(argv[1]);
#endif
    ObjSection* code;
    long page_size;
    size_t mapping_size;
    uint8_t* mapping;
    atomic_load_fn atomic_load;
    atomic_binary_fn atomic_dynamic_load;
    atomic_store_fn atomic_store;
    atomic_binary_fn atomic_exchange;
    atomic_binary_fn atomic_fetch_add;
    atomic_binary_fn atomic_fetch_sub;
    atomic_binary_fn atomic_add_fetch;
    atomic_binary_fn atomic_sub_fetch;
    atomic_binary_fn atomic_fetch_and;
    atomic_binary_fn atomic_and_fetch;
    atomic_binary_fn atomic_fetch_or;
    atomic_binary_fn atomic_or_fetch;
    atomic_binary_fn atomic_fetch_xor;
    atomic_binary_fn atomic_xor_fetch;
    atomic_binary_fn atomic_fetch_nand;
    atomic_binary_fn atomic_nand_fetch;
#if defined(__x86_64__)
    atomic_u32_wide_binary_fn atomic_nand_fetch_widened;
    atomic_i32_wide_binary_fn atomic_i32_xor_fetch_widened;
    atomic_u64_load_fn u64_load;
    atomic_u64_store_fn u64_store;
    atomic_u64_binary_fn u64_exchange;
    atomic_u64_binary_fn u64_fetch_add;
    atomic_u64_binary_fn u64_add_fetch;
    atomic_u64_binary_fn u64_fetch_sub;
    atomic_u64_binary_fn u64_sub_fetch;
    atomic_u64_binary_fn u64_fetch_xor;
    atomic_u64_binary_fn u64_or_fetch;
    atomic_u64_compare_fn u64_compare;
    atomic_u64_binary_fn standard_ullong_fetch_add;
    atomic_u64_load_fn standard_ullong_is_lock_free;
#else
    atomic_long_binary_fn standard_long_fetch_xor;
#endif
    atomic_compare_bool_fn compare_bool;
    atomic_compare_value_fn compare_value;
    atomic_binary_fn sync_exchange;
    atomic_release_fn sync_release;
    atomic_binary_fn sync_fetch_add;
    atomic_binary_fn sync_fetch_sub;
    atomic_binary_fn sync_add_fetch;
    atomic_binary_fn sync_sub_fetch;
    atomic_binary_fn sync_fetch_or;
    atomic_binary_fn sync_xor_fetch;
    atomic_fence_fn atomic_fence;
    atomic_fence_fn sync_fence;
    atomic_store_fn standard_init;
    atomic_load_fn standard_load;
    atomic_store_fn standard_store;
    atomic_binary_fn standard_exchange;
    atomic_binary_fn standard_fetch_add;
    atomic_binary_fn standard_fetch_and;
    atomic_binary_fn standard_fetch_or;
    atomic_binary_fn standard_fetch_xor;
    standard_atomic_compare_fn standard_compare;
    atomic_load_fn standard_flag_test_and_set;
    atomic_release_fn standard_flag_clear;
    atomic_load_fn standard_is_lock_free;
    atomic_fence_fn standard_signal_fence;
    atomic_u8_load_fn u8_load;
    atomic_u8_store_fn u8_store;
    atomic_u8_binary_fn u8_exchange;
    atomic_u8_binary_fn u8_fetch_add;
    atomic_u8_binary_fn u8_add_fetch;
    atomic_u8_binary_fn u8_fetch_sub;
    atomic_u8_binary_fn u8_sub_fetch;
    atomic_u8_binary_fn u8_fetch_nand;
    atomic_u8_compare_fn u8_compare;
    atomic_u16_load_fn u16_load;
    atomic_u16_store_fn u16_store;
    atomic_u16_binary_fn u16_exchange;
    atomic_u16_binary_fn u16_fetch_add;
    atomic_u16_binary_fn u16_add_fetch;
    atomic_u16_binary_fn u16_fetch_sub;
    atomic_u16_binary_fn u16_sub_fetch;
    atomic_u16_binary_fn u16_xor_fetch;
    atomic_u16_compare_fn u16_compare;
    atomic_i8_binary_fn i8_fetch_add;
    atomic_i16_binary_fn i16_fetch_sub;
    atomic_u8_sync_compare_fn sync_u8_compare;
    atomic_u8_release_fn sync_u8_release;
    atomic_u8_binary_fn standard_uchar_fetch_add;
    atomic_u16_binary_fn standard_ushort_exchange;
    atomic_bool_binary_fn standard_bool_exchange;
    volatile uint32_t value = 5u;
    volatile uint32_t counter = 0u;
    pthread_t threads[4];
    AtomicWorker workers[4];
    AtomicWorker16 workers16[4];
    AtomicBitwiseWorker bitwise_workers[4];
#if defined(__x86_64__)
    AtomicWorker64 workers64[4];
#endif
    unsigned i;

    assert(object != NULL);
#if defined(__x86_64__)
    assert(object->arch == ARCH_X64);
#else
    assert(object->arch == ARCH_X86);
#endif
    code = objfile_get_section(object, ".text");
    assert(code != NULL && code->size > 0u);
    assert(code->relocs == NULL);
    page_size = sysconf(_SC_PAGESIZE);
    assert(page_size > 0);
    mapping_size = (((size_t)code->size + (size_t)page_size - 1u) /
                    (size_t)page_size) * (size_t)page_size;
    mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(mapping != MAP_FAILED);
    memcpy(mapping, code->data, (size_t)code->size);
    assert(mprotect(mapping, mapping_size, PROT_READ | PROT_EXEC) == 0);

    LOAD_FUNCTION(atomic_load, object, mapping, "atomic_load_value");
    LOAD_FUNCTION(atomic_dynamic_load, object, mapping,
                  "atomic_dynamic_load_value");
    LOAD_FUNCTION(atomic_store, object, mapping, "atomic_store_value");
    LOAD_FUNCTION(atomic_exchange, object, mapping, "atomic_exchange_value");
    LOAD_FUNCTION(atomic_fetch_add, object, mapping, "atomic_fetch_add_value");
    LOAD_FUNCTION(atomic_fetch_sub, object, mapping, "atomic_fetch_sub_value");
    LOAD_FUNCTION(atomic_add_fetch, object, mapping, "atomic_add_fetch_value");
    LOAD_FUNCTION(atomic_sub_fetch, object, mapping, "atomic_sub_fetch_value");
    LOAD_FUNCTION(atomic_fetch_and, object, mapping, "atomic_fetch_and_value");
    LOAD_FUNCTION(atomic_and_fetch, object, mapping, "atomic_and_fetch_value");
    LOAD_FUNCTION(atomic_fetch_or, object, mapping, "atomic_fetch_or_value");
    LOAD_FUNCTION(atomic_or_fetch, object, mapping, "atomic_or_fetch_value");
    LOAD_FUNCTION(atomic_fetch_xor, object, mapping, "atomic_fetch_xor_value");
    LOAD_FUNCTION(atomic_xor_fetch, object, mapping, "atomic_xor_fetch_value");
    LOAD_FUNCTION(atomic_fetch_nand, object, mapping,
                  "atomic_fetch_nand_value");
    LOAD_FUNCTION(atomic_nand_fetch, object, mapping,
                  "atomic_nand_fetch_value");
#if defined(__x86_64__)
    LOAD_FUNCTION(atomic_nand_fetch_widened, object, mapping,
                  "atomic_nand_fetch_widened_value");
    LOAD_FUNCTION(atomic_i32_xor_fetch_widened, object, mapping,
                  "atomic_i32_xor_fetch_widened_value");
    LOAD_FUNCTION(u64_load, object, mapping, "atomic_u64_load_value");
    LOAD_FUNCTION(u64_store, object, mapping, "atomic_u64_store_value");
    LOAD_FUNCTION(u64_exchange, object, mapping,
                  "atomic_u64_exchange_value");
    LOAD_FUNCTION(u64_fetch_add, object, mapping,
                  "atomic_u64_fetch_add_value");
    LOAD_FUNCTION(u64_add_fetch, object, mapping,
                  "atomic_u64_add_fetch_value");
    LOAD_FUNCTION(u64_fetch_sub, object, mapping,
                  "atomic_u64_fetch_sub_value");
    LOAD_FUNCTION(u64_sub_fetch, object, mapping,
                  "atomic_u64_sub_fetch_value");
    LOAD_FUNCTION(u64_fetch_xor, object, mapping,
                  "atomic_u64_fetch_xor_value");
    LOAD_FUNCTION(u64_or_fetch, object, mapping,
                  "atomic_u64_or_fetch_value");
    LOAD_FUNCTION(u64_compare, object, mapping,
                  "atomic_u64_compare_exchange_value");
    LOAD_FUNCTION(standard_ullong_fetch_add, object, mapping,
                  "standard_atomic_ullong_fetch_add_value");
    LOAD_FUNCTION(standard_ullong_is_lock_free, object, mapping,
                  "standard_atomic_ullong_is_lock_free_value");
#else
    LOAD_FUNCTION(standard_long_fetch_xor, object, mapping,
                  "standard_atomic_long_fetch_xor_value");
#endif
    LOAD_FUNCTION(compare_bool, object, mapping, "atomic_compare_exchange_bool");
    LOAD_FUNCTION(compare_value, object, mapping,
                  "atomic_compare_exchange_value");
    LOAD_FUNCTION(sync_exchange, object, mapping, "sync_exchange_value");
    LOAD_FUNCTION(sync_release, object, mapping, "sync_release_value");
    LOAD_FUNCTION(sync_fetch_add, object, mapping, "sync_fetch_add_value");
    LOAD_FUNCTION(sync_fetch_sub, object, mapping, "sync_fetch_sub_value");
    LOAD_FUNCTION(sync_add_fetch, object, mapping, "sync_add_fetch_value");
    LOAD_FUNCTION(sync_sub_fetch, object, mapping, "sync_sub_fetch_value");
    LOAD_FUNCTION(sync_fetch_or, object, mapping, "sync_fetch_or_value");
    LOAD_FUNCTION(sync_xor_fetch, object, mapping, "sync_xor_fetch_value");
    LOAD_FUNCTION(atomic_fence, object, mapping, "atomic_thread_fence_value");
    LOAD_FUNCTION(sync_fence, object, mapping, "sync_synchronize_value");
    LOAD_FUNCTION(standard_init, object, mapping,
                  "standard_atomic_init_value");
    LOAD_FUNCTION(standard_load, object, mapping,
                  "standard_atomic_load_value");
    LOAD_FUNCTION(standard_store, object, mapping,
                  "standard_atomic_store_value");
    LOAD_FUNCTION(standard_exchange, object, mapping,
                  "standard_atomic_exchange_value");
    LOAD_FUNCTION(standard_fetch_add, object, mapping,
                  "standard_atomic_fetch_add_value");
    LOAD_FUNCTION(standard_fetch_and, object, mapping,
                  "standard_atomic_fetch_and_value");
    LOAD_FUNCTION(standard_fetch_or, object, mapping,
                  "standard_atomic_fetch_or_value");
    LOAD_FUNCTION(standard_fetch_xor, object, mapping,
                  "standard_atomic_fetch_xor_value");
    LOAD_FUNCTION(standard_compare, object, mapping,
                  "standard_atomic_compare_exchange_value");
    LOAD_FUNCTION(standard_flag_test_and_set, object, mapping,
                  "standard_atomic_flag_test_and_set_value");
    LOAD_FUNCTION(standard_flag_clear, object, mapping,
                  "standard_atomic_flag_clear_value");
    LOAD_FUNCTION(standard_is_lock_free, object, mapping,
                  "standard_atomic_is_lock_free_value");
    LOAD_FUNCTION(standard_signal_fence, object, mapping,
                  "standard_atomic_signal_fence_value");
    LOAD_FUNCTION(u8_load, object, mapping, "atomic_u8_load_value");
    LOAD_FUNCTION(u8_store, object, mapping, "atomic_u8_store_value");
    LOAD_FUNCTION(u8_exchange, object, mapping, "atomic_u8_exchange_value");
    LOAD_FUNCTION(u8_fetch_add, object, mapping, "atomic_u8_fetch_add_value");
    LOAD_FUNCTION(u8_add_fetch, object, mapping, "atomic_u8_add_fetch_value");
    LOAD_FUNCTION(u8_fetch_sub, object, mapping, "atomic_u8_fetch_sub_value");
    LOAD_FUNCTION(u8_sub_fetch, object, mapping, "atomic_u8_sub_fetch_value");
    LOAD_FUNCTION(u8_fetch_nand, object, mapping,
                  "atomic_u8_fetch_nand_value");
    LOAD_FUNCTION(u8_compare, object, mapping,
                  "atomic_u8_compare_exchange_value");
    LOAD_FUNCTION(u16_load, object, mapping, "atomic_u16_load_value");
    LOAD_FUNCTION(u16_store, object, mapping, "atomic_u16_store_value");
    LOAD_FUNCTION(u16_exchange, object, mapping, "atomic_u16_exchange_value");
    LOAD_FUNCTION(u16_fetch_add, object, mapping,
                  "atomic_u16_fetch_add_value");
    LOAD_FUNCTION(u16_add_fetch, object, mapping,
                  "atomic_u16_add_fetch_value");
    LOAD_FUNCTION(u16_fetch_sub, object, mapping,
                  "atomic_u16_fetch_sub_value");
    LOAD_FUNCTION(u16_sub_fetch, object, mapping,
                  "atomic_u16_sub_fetch_value");
    LOAD_FUNCTION(u16_xor_fetch, object, mapping,
                  "atomic_u16_xor_fetch_value");
    LOAD_FUNCTION(u16_compare, object, mapping,
                  "atomic_u16_compare_exchange_value");
    LOAD_FUNCTION(i8_fetch_add, object, mapping,
                  "atomic_i8_fetch_add_value");
    LOAD_FUNCTION(i16_fetch_sub, object, mapping,
                  "atomic_i16_fetch_sub_value");
    LOAD_FUNCTION(sync_u8_compare, object, mapping,
                  "sync_u8_compare_exchange_value");
    LOAD_FUNCTION(sync_u8_release, object, mapping,
                  "sync_u8_release_value");
    LOAD_FUNCTION(standard_uchar_fetch_add, object, mapping,
                  "standard_atomic_uchar_fetch_add_value");
    LOAD_FUNCTION(standard_ushort_exchange, object, mapping,
                  "standard_atomic_ushort_exchange_value");
    LOAD_FUNCTION(standard_bool_exchange, object, mapping,
                  "standard_atomic_bool_exchange_value");

    assert(atomic_load(&value) == 5u);
    assert(atomic_dynamic_load(&value, 2u) == 5u);
    atomic_store(&value, 7u);
    assert(value == 7u);
    assert(atomic_exchange(&value, 11u) == 7u && value == 11u);
    assert(atomic_fetch_add(&value, 4u) == 11u && value == 15u);
    assert(atomic_add_fetch(&value, 5u) == 20u && value == 20u);
    assert(atomic_fetch_sub(&value, 3u) == 20u && value == 17u);
    assert(atomic_sub_fetch(&value, 2u) == 15u && value == 15u);
    assert(atomic_fetch_and(&value, 12u) == 15u && value == 12u);
    assert(atomic_and_fetch(&value, 10u) == 8u && value == 8u);
    assert(atomic_fetch_or(&value, 5u) == 8u && value == 13u);
    assert(atomic_or_fetch(&value, 2u) == 15u && value == 15u);
    assert(atomic_fetch_xor(&value, 6u) == 15u && value == 9u);
    assert(atomic_xor_fetch(&value, 3u) == 10u && value == 10u);
    assert(atomic_fetch_nand(&value, 15u) == 10u &&
           value == UINT32_C(0xfffffff5));
#if defined(__x86_64__)
    assert(atomic_nand_fetch_widened(&value, UINT32_C(0xffffffff)) ==
           UINT64_C(10) && value == 10u);
    value = UINT32_C(0xfffffff5);
#endif
    assert(atomic_nand_fetch(&value, UINT32_C(0xffffffff)) == 10u &&
           value == 10u);
#if defined(__x86_64__)
    {
        volatile int32_t signed32 = -1;
        assert(atomic_i32_xor_fetch_widened(&signed32, 255) ==
               INT64_C(-256) && signed32 == -256);
    }
    {
        volatile uint64_t wide = UINT64_C(0x100000005);
        uint64_t expected64;
        assert(u64_load(&wide) == UINT64_C(0x100000005));
        u64_store(&wide, UINT64_C(0x200000007));
        assert(wide == UINT64_C(0x200000007));
        assert(u64_exchange(&wide, UINT64_C(0x30000000b)) ==
               UINT64_C(0x200000007) && wide == UINT64_C(0x30000000b));
        assert(u64_fetch_add(&wide, UINT64_C(0x100000000)) ==
               UINT64_C(0x30000000b) && wide == UINT64_C(0x40000000b));
        assert(u64_add_fetch(&wide, UINT64_C(3)) == UINT64_C(0x40000000e));
        assert(u64_fetch_sub(&wide, UINT64_C(0x100000000)) ==
               UINT64_C(0x40000000e) && wide == UINT64_C(0x30000000e));
        assert(u64_sub_fetch(&wide, UINT64_C(4)) == UINT64_C(0x30000000a));
        assert(u64_fetch_xor(&wide, UINT64_C(0x700000000)) ==
               UINT64_C(0x30000000a) && wide == UINT64_C(0x40000000a));
        assert(u64_or_fetch(&wide, UINT64_C(0x00000f000)) ==
               UINT64_C(0x40000f00a));
        expected64 = UINT64_C(0x40000f00a);
        assert(u64_compare(&wide, &expected64, UINT64_C(0x800000011)) == 1);
        assert(expected64 == UINT64_C(0x40000f00a) &&
               wide == UINT64_C(0x800000011));
        expected64 = UINT64_C(7);
        assert(u64_compare(&wide, &expected64, UINT64_C(9)) == 0);
        assert(expected64 == UINT64_C(0x800000011) &&
               wide == UINT64_C(0x800000011));
        assert(standard_ullong_fetch_add(&wide, UINT64_C(0x100000000)) ==
               UINT64_C(0x800000011) && wide == UINT64_C(0x900000011));
        assert(standard_ullong_is_lock_free(&wide) == UINT64_C(1));
    }
#else
    {
        volatile long long_value = 0x55;
        assert(standard_long_fetch_xor(&long_value, 0x0f) == 0x55 &&
               long_value == 0x5a);
    }
#endif
    value = 15u;
    assert(compare_bool(&value, 15u, 99u) == 1 && value == 99u);
    assert(compare_bool(&value, 15u, 7u) == 0 && value == 99u);
    assert(compare_value(&value, 99u, 3u) == 99u && value == 3u);
    assert(compare_value(&value, 99u, 8u) == 3u && value == 3u);
    assert(sync_exchange(&value, 10u) == 3u && value == 10u);
    assert(sync_fetch_add(&value, 4u) == 10u && value == 14u);
    assert(sync_add_fetch(&value, 6u) == 20u && value == 20u);
    assert(sync_fetch_sub(&value, 5u) == 20u && value == 15u);
    assert(sync_sub_fetch(&value, 5u) == 10u && value == 10u);
    assert(sync_fetch_or(&value, 5u) == 10u && value == 15u);
    assert(sync_xor_fetch(&value, 3u) == 12u && value == 12u);
    sync_release(&value);
    assert(value == 0u);
    atomic_fence();
    sync_fence();

    standard_init(&value, 12u);
    assert(standard_load(&value) == 12u);
    standard_store(&value, 14u);
    assert(standard_exchange(&value, 20u) == 14u && value == 20u);
    assert(standard_fetch_add(&value, 2u) == 20u && value == 22u);
    assert(standard_fetch_and(&value, 15u) == 22u && value == 6u);
    assert(standard_fetch_or(&value, 8u) == 6u && value == 14u);
    assert(standard_fetch_xor(&value, 3u) == 14u && value == 13u);
    {
        uint32_t expected = 13u;
        assert(standard_compare(&value, &expected, 30u) == 1);
        assert(expected == 13u && value == 30u);
        expected = 7u;
        assert(standard_compare(&value, &expected, 40u) == 0);
        assert(expected == 30u && value == 30u);
    }
    value = 0u;
    assert(standard_flag_test_and_set(&value) == 0u && value == 1u);
    assert(standard_flag_test_and_set(&value) == 1u && value == 1u);
    standard_flag_clear(&value);
    assert(value == 0u);
    assert(standard_is_lock_free(&value) == 1u);
    standard_signal_fence();

    {
        volatile uint8_t small = 250u;
        uint8_t expected8;
        assert(u8_load(&small) == 250u);
        u8_store(&small, 248u);
        assert(small == 248u);
        assert(u8_exchange(&small, 250u) == 248u && small == 250u);
        assert(u8_fetch_add(&small, 10u) == 250u && small == 4u);
        assert(u8_add_fetch(&small, 252u) == 0u && small == 0u);
        small = 3u;
        assert(u8_fetch_sub(&small, 5u) == 3u && small == 254u);
        assert(u8_sub_fetch(&small, 255u) == 255u && small == 255u);
        assert(u8_fetch_nand(&small, 15u) == 255u && small == 240u);
        small = 10u;
        expected8 = 10u;
        assert(u8_compare(&small, &expected8, 300u) == 1);
        assert(expected8 == 10u && small == 44u);
        expected8 = 7u;
        assert(u8_compare(&small, &expected8, 1u) == 0);
        assert(expected8 == 44u && small == 44u);
        assert(sync_u8_compare(&small, 44u, 260u) == 44u && small == 4u);
        sync_u8_release(&small);
        assert(small == 0u);
        small = 250u;
        assert(standard_uchar_fetch_add(&small, 10u) == 250u &&
               small == 4u);
    }
    {
        volatile uint16_t small = 65530u;
        uint16_t expected16;
        assert(u16_load(&small) == 65530u);
        u16_store(&small, 65520u);
        assert(small == 65520u);
        assert(u16_exchange(&small, 65530u) == 65520u && small == 65530u);
        assert(u16_fetch_add(&small, 10u) == 65530u && small == 4u);
        assert(u16_add_fetch(&small, 65535u) == 3u && small == 3u);
        assert(u16_fetch_sub(&small, 5u) == 3u && small == 65534u);
        assert(u16_sub_fetch(&small, 65535u) == 65535u &&
               small == 65535u);
        assert(u16_xor_fetch(&small, 0x00ffu) == 0xff00u &&
               small == 0xff00u);
        small = 10u;
        expected16 = 10u;
        assert(u16_compare(&small, &expected16, 70000u) == 1);
        assert(expected16 == 10u && small == 4464u);
        expected16 = 7u;
        assert(u16_compare(&small, &expected16, 1u) == 0);
        assert(expected16 == 4464u && small == 4464u);
        assert(standard_ushort_exchange(&small, 9u) == 4464u &&
               small == 9u);
    }
    {
        volatile int8_t signed8 = -5;
        volatile int16_t signed16 = -300;
        volatile _Bool boolean = 0;
        assert(i8_fetch_add(&signed8, 2) == -5 && signed8 == -3);
        assert(i16_fetch_sub(&signed16, 10) == -300 && signed16 == -310);
        assert(standard_bool_exchange(&boolean, 7) == 0 && boolean == 1);
        assert(standard_bool_exchange(&boolean, 0) == 1 && boolean == 0);
    }

    value = 0u;
    for (i = 0; i < 4u; ++i) {
        bitwise_workers[i].fetch_or = atomic_fetch_or;
        bitwise_workers[i].value = &value;
        bitwise_workers[i].operand = 1u << i;
        bitwise_workers[i].iterations = 10000u;
        assert(pthread_create(&threads[i], NULL, atomic_bitwise_worker,
                              &bitwise_workers[i]) == 0);
    }
    for (i = 0; i < 4u; ++i) {
        assert(pthread_join(threads[i], NULL) == 0);
    }
    assert(value == 15u);

#if defined(__x86_64__)
    {
        volatile uint64_t counter64 = UINT64_C(0x100000000);
        for (i = 0; i < 4u; ++i) {
            workers64[i].fetch_add = u64_fetch_add;
            workers64[i].counter = &counter64;
            workers64[i].iterations = 25000u;
            assert(pthread_create(&threads[i], NULL, atomic_worker64,
                                  &workers64[i]) == 0);
        }
        for (i = 0; i < 4u; ++i) {
            assert(pthread_join(threads[i], NULL) == 0);
        }
        assert(counter64 == UINT64_C(0x1000186a0));
    }
#endif

    {
        volatile uint16_t counter16 = 0u;
        for (i = 0; i < 4u; ++i) {
            workers16[i].fetch_add = u16_fetch_add;
            workers16[i].counter = &counter16;
            workers16[i].iterations = 10000u;
            assert(pthread_create(&threads[i], NULL, atomic_worker16,
                                  &workers16[i]) == 0);
        }
        for (i = 0; i < 4u; ++i) {
            assert(pthread_join(threads[i], NULL) == 0);
        }
        assert(counter16 == 40000u);
    }

    for (i = 0; i < 4u; ++i) {
        workers[i].fetch_add = atomic_fetch_add;
        workers[i].counter = &counter;
        workers[i].iterations = 25000u;
        assert(pthread_create(&threads[i], NULL, atomic_worker,
                              &workers[i]) == 0);
    }
    for (i = 0; i < 4u; ++i) {
        assert(pthread_join(threads[i], NULL) == 0);
    }
    assert(counter == 100000u);

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
#else
    (void)argv;
    puts("atomic builtin execution test skipped on unsupported host");
#endif
    return 0;
}
