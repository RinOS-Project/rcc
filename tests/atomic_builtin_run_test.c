#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(__x86_64__) && !defined(_WIN32)
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

#if defined(__x86_64__) && !defined(_WIN32)
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

typedef struct {
    atomic_binary_fn fetch_add;
    volatile uint32_t* counter;
    unsigned iterations;
} AtomicWorker;

static void* atomic_worker(void* argument) {
    AtomicWorker* worker = argument;
    unsigned i;
    for (i = 0; i < worker->iterations; ++i) {
        worker->fetch_add(worker->counter, 1u);
    }
    return NULL;
}

#define LOAD_FUNCTION(target, object, mapping, symbol_name)                  \
    do {                                                                     \
        ObjSymbol* load_symbol = function_symbol((object), (symbol_name));    \
        void* load_address = (mapping) + load_symbol->value;                  \
        memcpy(&(target), &load_address, sizeof(target));                     \
    } while (0)
#endif

int main(int argc, char** argv) {
    assert(argc == 2);
#if defined(__x86_64__) && !defined(_WIN32)
    ObjectFile* object = objfile_read(argv[1]);
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
    atomic_compare_bool_fn compare_bool;
    atomic_compare_value_fn compare_value;
    atomic_binary_fn sync_exchange;
    atomic_release_fn sync_release;
    atomic_binary_fn sync_fetch_add;
    atomic_binary_fn sync_fetch_sub;
    atomic_binary_fn sync_add_fetch;
    atomic_binary_fn sync_sub_fetch;
    atomic_fence_fn atomic_fence;
    atomic_fence_fn sync_fence;
    atomic_store_fn standard_init;
    atomic_load_fn standard_load;
    atomic_store_fn standard_store;
    atomic_binary_fn standard_exchange;
    atomic_binary_fn standard_fetch_add;
    standard_atomic_compare_fn standard_compare;
    atomic_load_fn standard_flag_test_and_set;
    atomic_release_fn standard_flag_clear;
    atomic_load_fn standard_is_lock_free;
    atomic_fence_fn standard_signal_fence;
    volatile uint32_t value = 5u;
    volatile uint32_t counter = 0u;
    pthread_t threads[4];
    AtomicWorker workers[4];
    unsigned i;

    assert(object != NULL);
    assert(object->arch == ARCH_X64);
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
    LOAD_FUNCTION(compare_bool, object, mapping, "atomic_compare_exchange_bool");
    LOAD_FUNCTION(compare_value, object, mapping,
                  "atomic_compare_exchange_value");
    LOAD_FUNCTION(sync_exchange, object, mapping, "sync_exchange_value");
    LOAD_FUNCTION(sync_release, object, mapping, "sync_release_value");
    LOAD_FUNCTION(sync_fetch_add, object, mapping, "sync_fetch_add_value");
    LOAD_FUNCTION(sync_fetch_sub, object, mapping, "sync_fetch_sub_value");
    LOAD_FUNCTION(sync_add_fetch, object, mapping, "sync_add_fetch_value");
    LOAD_FUNCTION(sync_sub_fetch, object, mapping, "sync_sub_fetch_value");
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

    assert(atomic_load(&value) == 5u);
    assert(atomic_dynamic_load(&value, 2u) == 5u);
    atomic_store(&value, 7u);
    assert(value == 7u);
    assert(atomic_exchange(&value, 11u) == 7u && value == 11u);
    assert(atomic_fetch_add(&value, 4u) == 11u && value == 15u);
    assert(atomic_add_fetch(&value, 5u) == 20u && value == 20u);
    assert(atomic_fetch_sub(&value, 3u) == 20u && value == 17u);
    assert(atomic_sub_fetch(&value, 2u) == 15u && value == 15u);
    assert(compare_bool(&value, 15u, 99u) == 1 && value == 99u);
    assert(compare_bool(&value, 15u, 7u) == 0 && value == 99u);
    assert(compare_value(&value, 99u, 3u) == 99u && value == 3u);
    assert(compare_value(&value, 99u, 8u) == 3u && value == 3u);
    assert(sync_exchange(&value, 10u) == 3u && value == 10u);
    assert(sync_fetch_add(&value, 4u) == 10u && value == 14u);
    assert(sync_add_fetch(&value, 6u) == 20u && value == 20u);
    assert(sync_fetch_sub(&value, 5u) == 20u && value == 15u);
    assert(sync_sub_fetch(&value, 5u) == 10u && value == 10u);
    sync_release(&value);
    assert(value == 0u);
    atomic_fence();
    sync_fence();

    standard_init(&value, 12u);
    assert(standard_load(&value) == 12u);
    standard_store(&value, 14u);
    assert(standard_exchange(&value, 20u) == 14u && value == 20u);
    assert(standard_fetch_add(&value, 2u) == 20u && value == 22u);
    {
        uint32_t expected = 22u;
        assert(standard_compare(&value, &expected, 30u) == 1);
        assert(expected == 22u && value == 30u);
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
    puts("atomic builtin execution test skipped on non-x86_64 host");
#endif
    return 0;
}
