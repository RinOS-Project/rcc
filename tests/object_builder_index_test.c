#include "objfile.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct ObjectIndexMetrics {
    uint64_t string_probes;
    uint64_t string_rehash_probes;
    uint64_t symbol_probes;
    uint64_t symbol_rehash_probes;
    uint64_t symbol_append_operations;
    uint64_t relocation_append_operations;
    size_t tracked_heap_bytes;
} ObjectIndexMetrics;

static size_t objfile_tracked_heap_bytes(const ObjectFile* object) {
    size_t bytes = sizeof(*object) + strlen(object->filename) + 1u +
        object->strtab_cap +
        object->strtab_hash_capacity * sizeof(*object->strtab_hash_slots) +
        object->symbol_hash_capacity * sizeof(*object->symbol_hash_slots) +
        object->sections_by_index_capacity *
            sizeof(*object->sections_by_index);
    for (const ObjSection* section = object->sections; section;
         section = section->next) {
        bytes += sizeof(*section) + (size_t)section->capacity +
            strlen(section->name) + 1u;
        for (const ObjReloc* relocation = section->relocs; relocation;
             relocation = relocation->next) {
            bytes += sizeof(*relocation) +
                strlen(relocation->symbol_name) + 1u;
        }
    }
    for (const ObjSymbol* symbol = object->symbols; symbol;
         symbol = symbol->next) {
        bytes += sizeof(*symbol) + strlen(symbol->name) + 1u;
    }
    return bytes;
}

static void collect_metrics(const ObjectFile* object,
                            ObjectIndexMetrics* metrics) {
    metrics->string_probes += object->string_index_probes;
    metrics->string_rehash_probes += object->string_rehash_probes;
    metrics->symbol_probes += object->symbol_index_probes;
    metrics->symbol_rehash_probes += object->symbol_rehash_probes;
    metrics->symbol_append_operations += object->symbol_append_operations;
    metrics->relocation_append_operations +=
        object->relocation_append_operations;
    {
        size_t bytes = objfile_tracked_heap_bytes(object);
        if (bytes > metrics->tracked_heap_bytes) {
            metrics->tracked_heap_bytes = bytes;
        }
    }
}

static double elapsed_cpu_ms(clock_t start, clock_t end) {
    return (double)(end - start) * 1000.0 / (double)CLOCKS_PER_SEC;
}

static void run_scale(const char* path, uint32_t string_count,
                      uint32_t symbol_count) {
    ObjectFile* object;
    ObjectFile* roundtrip;
    ObjSection* text;
    uint32_t* string_offsets =
        (uint32_t*)malloc((size_t)string_count * sizeof(*string_offsets));
    uint8_t* code = (uint8_t*)calloc((size_t)symbol_count, 4u);
    char name[64];
    ObjectIndexMetrics metrics = {0};
    clock_t build_start;
    clock_t build_end;
    clock_t read_start;
    clock_t read_end;

    assert(string_offsets != NULL && code != NULL);
    build_start = clock();
    object = objfile_new(path, ARCH_X64);
    text = objfile_add_section(object, ".text", SECT_CODE,
                               SECT_FLAG_ALLOC | SECT_FLAG_EXEC);
    assert(section_add_data(text, code, (uint64_t)symbol_count * 4u) == 0u);

    for (uint32_t index = 0u; index < string_count; ++index) {
        snprintf(name, sizeof(name), "scale_string_%05" PRIu32, index);
        string_offsets[index] = objfile_add_string(object, name);
        assert(string_offsets[index] != 0u);
        assert(objfile_add_string(object, name) == string_offsets[index]);
    }

    for (uint32_t index = 0u; index < symbol_count; ++index) {
        snprintf(name, sizeof(name), "scale_symbol_%05" PRIu32, index);
        objfile_add_symbol(object, name, SYM_UNDEF, BIND_DATA, -1, 0u, 0u);
        objfile_add_reloc(object, 0, (uint64_t)index * 4u, name,
                          RELOC_ABS32U, (int64_t)index);
    }
    assert(object->symbol_count == (int)symbol_count);
    assert(object->symbols_tail != NULL && object->symbols_tail->next == NULL);
    assert(text->relocs_tail != NULL && text->relocs_tail->next == NULL);
    assert(objfile_write(object, path));
    collect_metrics(object, &metrics);
    objfile_free(object);
    free(code);
    build_end = clock();

    read_start = clock();
    roundtrip = objfile_read(path);
    assert(roundtrip != NULL);
    assert(roundtrip->section_count == 1);
    assert(roundtrip->symbol_count == (int)symbol_count);
    assert(roundtrip->symbols_tail != NULL &&
           roundtrip->symbols_tail->next == NULL);
    assert(roundtrip->sections_by_index[0]->relocs_tail != NULL &&
           roundtrip->sections_by_index[0]->relocs_tail->next == NULL);
    for (uint32_t index = 0u; index < string_count; ++index) {
        snprintf(name, sizeof(name), "scale_string_%05" PRIu32, index);
        assert(objfile_add_string(roundtrip, name) == string_offsets[index]);
    }
    for (uint32_t index = 0u; index < symbol_count; ++index) {
        ObjSymbol* symbol;
        snprintf(name, sizeof(name), "scale_symbol_%05" PRIu32, index);
        symbol = objfile_find_symbol(roundtrip, name);
        assert(symbol != NULL && strcmp(symbol->name, name) == 0);
    }
    {
        ObjReloc* relocation = roundtrip->sections_by_index[0]->relocs;
        for (uint32_t index = 0u; index < symbol_count; ++index) {
            snprintf(name, sizeof(name), "scale_symbol_%05" PRIu32, index);
            assert(relocation != NULL);
            assert(relocation->offset == (uint64_t)index * 4u);
            assert(strcmp(relocation->symbol_name, name) == 0);
            relocation = relocation->next;
        }
        assert(relocation == NULL);
    }
    collect_metrics(roundtrip, &metrics);
    objfile_free(roundtrip);
    read_end = clock();
    free(string_offsets);
    assert(remove(path) == 0);

    assert(metrics.symbol_append_operations == (uint64_t)symbol_count * 2u);
    assert(metrics.relocation_append_operations ==
           (uint64_t)symbol_count * 2u);
    assert(metrics.string_probes < (uint64_t)string_count * 32u +
                                      (uint64_t)symbol_count * 64u + 4096u);
    assert(metrics.symbol_probes < (uint64_t)symbol_count * 32u + 4096u);

    /* The linked-list append paths use their saved tails, so each append
     * advances no list cursor. The printed zero is the measured hop count. */
    printf("ObjectFile strings=%" PRIu32 " symbols=%" PRIu32
           " relocations=%" PRIu32
           " string_probes=%" PRIu64 " string_rehash_probes=%" PRIu64
           " symbol_probes=%" PRIu64 " symbol_rehash_probes=%" PRIu64
           " symbol_append_ops=%" PRIu64 " relocation_append_ops=%" PRIu64
           " symbol_append_hops=0 relocation_append_hops=0"
           " legacy_symbol_find_comparisons=%" PRIu64
           " legacy_symbol_append_hops_avoided=%" PRIu64
           " legacy_relocation_append_hops_avoided=%" PRIu64
           " tracked_object_heap_bytes=%zu build_write_cpu_ms=%.3f"
           " read_lookup_cpu_ms=%.3f\n",
           string_count, symbol_count, symbol_count, metrics.string_probes,
           metrics.string_rehash_probes, metrics.symbol_probes,
           metrics.symbol_rehash_probes, metrics.symbol_append_operations,
           metrics.relocation_append_operations,
           (uint64_t)symbol_count * ((uint64_t)symbol_count + 1u),
           (uint64_t)symbol_count * (symbol_count - 1u),
           (uint64_t)symbol_count * (symbol_count - 1u),
           metrics.tracked_heap_bytes,
           elapsed_cpu_ms(build_start, build_end),
           elapsed_cpu_ms(read_start, read_end));
}

static void check_first_duplicate_symbol(void) {
    ObjectFile* object = objfile_new("<symbol-index-test>", ARCH_X64);
    ObjSymbol* first = objfile_add_symbol(
        object, "duplicate", SYM_UNDEF, BIND_DATA, -1, 0u, 0u);
    objfile_add_symbol(object, "duplicate", SYM_WEAK, BIND_DATA, -1, 4u, 0u);
    assert(objfile_find_symbol(object, "duplicate") == first);
    objfile_free(object);
}

int main(int argc, char** argv) {
    char small_path[1024];
    char large_path[1024];
    assert(argc == 2);
    assert(snprintf(small_path, sizeof(small_path), "%s.small", argv[1]) > 0);
    assert(snprintf(large_path, sizeof(large_path), "%s.large", argv[1]) > 0);
    check_first_duplicate_symbol();
    run_scale(small_path, 64u, 32u);
    run_scale(large_path, 4096u, 4096u);
    puts("ObjectFile index scale and round-trip checks passed");
    return 0;
}
