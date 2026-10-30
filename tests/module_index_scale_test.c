#include "codegen.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MODULE_SCALE_RUNS 5u

static double elapsed_cpu_ms(clock_t start, clock_t end) {
    return (double)(end - start) * 1000.0 / (double)CLOCKS_PER_SEC;
}

static void sort_times(double* samples, size_t count) {
    for (size_t index = 1u; index < count; ++index) {
        double value = samples[index];
        size_t insertion = index;
        while (insertion > 0u && samples[insertion - 1u] > value) {
            samples[insertion] = samples[insertion - 1u];
            --insertion;
        }
        samples[insertion] = value;
    }
}

static uint64_t module_tracked_bytes(const Module* mod) {
    uint64_t bytes = sizeof(*mod) +
        mod->code.capacity + mod->rodata.capacity +
        mod->init_array.capacity + mod->fini_array.capacity +
        mod->data.capacity + mod->tls.capacity +
        (uint64_t)mod->symbol_capacity * sizeof(*mod->symbols) +
        (uint64_t)mod->symbol_hash_capacity * sizeof(*mod->symbol_hash_slots) +
        (uint64_t)mod->reloc_capacity * sizeof(*mod->relocs_arr) +
        (uint64_t)mod->reloc_capacity * sizeof(*mod->relocation_lookup_next) +
        (uint64_t)mod->relocation_lookup_capacity *
            sizeof(*mod->relocation_lookup_entries) +
        (uint64_t)mod->relocation_lookup_slot_capacity *
            sizeof(*mod->relocation_lookup_slots);
    for (int index = 0; index < mod->symbol_count; ++index) {
        bytes += strlen(mod->symbols[index].name) + 1u;
    }
    for (int index = 0; index < mod->reloc_count; ++index) {
        if (mod->relocs_arr[index].symbol_name) {
            bytes += strlen(mod->relocs_arr[index].symbol_name) + 1u;
        }
    }
    for (const Reloc* relocation = mod->relocs; relocation;
         relocation = relocation->next) {
        bytes += sizeof(*relocation);
        if (relocation->symbol) bytes += strlen(relocation->symbol) + 1u;
    }
    return bytes;
}

static void reset_stats(Module* mod) {
    mod->stats_symbol_lookup_probes = 0u;
    mod->stats_symbol_insert_probes = 0u;
    mod->stats_symbol_rehash_probes = 0u;
    mod->stats_relocation_lookup_probes = 0u;
    mod->stats_relocation_rehash_probes = 0u;
    mod->stats_resolver_candidate_visits = 0u;
    mod->stats_image_resolver_calls = 0u;
    mod->stats_tls_resolver_calls = 0u;
    mod->stats_relative_patch_visits = 0u;
}

static Module* build_module(uint32_t symbol_count) {
    Module* mod = codegen_new();
    char name[64];
    assert(mod != NULL);

    for (uint32_t index = 0u; index < symbol_count; ++index) {
        emit_dword(mod, 0u);
        emit_dword(mod, 0u);
    }
    for (uint32_t index = 0u; index < symbol_count; ++index) {
        uint32_t relative_offset = index * 8u;
        uint32_t absolute_offset = relative_offset + 4u;
        snprintf(name, sizeof(name), "scale_target_%05" PRIu32, index);
        module_add_symbol(mod, name, relative_offset, true,
                          MODULE_SYMBOL_CODE, false);
        module_add_relocation(mod, MODULE_SYMBOL_CODE, relative_offset, 0u,
                              true, false, name);
        module_add_relocation(mod, MODULE_SYMBOL_CODE, absolute_offset, 0u,
                              false, false, name);
        add_reloc(mod, MODULE_SYMBOL_CODE, absolute_offset, RIN_RELOC_ABS32);
    }
    assert(mod->symbol_count == (int)symbol_count);
    assert(mod->reloc_count == (int)(symbol_count * 2u));
    assert(mod->relocation_lookup_count == (size_t)symbol_count * 2u);
    mod->entry_point = 0u;
    return mod;
}

static uint64_t output_file_bytes(const char* path) {
    FILE* file = fopen(path, "rb");
    long end;
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    end = ftell(file);
    assert(end >= 0);
    assert(fclose(file) == 0);
    return (uint64_t)end;
}

static bool emit_rll_for_scale(Module* mod, const char* path) {
    return rcc_emit_rll(mod, NULL, path);
}

static bool emit_drv_for_scale(Module* mod, const char* path) {
    return rcc_emit_drv(mod, NULL, path);
}

static void report_emit(Module* mod, const char* directory,
                        const char* scale_name, const char* format_name,
                        const char* extension,
                        bool (*emit)(Module*, const char*),
                        uint32_t symbol_count, uint64_t tracked_bytes) {
    char path[512];
    double samples[MODULE_SCALE_RUNS];
    uint64_t artifact_bytes = 0u;
    assert(snprintf(path, sizeof(path), "%s/module-index-%s-%s.%s",
                    directory, scale_name, format_name, extension) > 0);
    for (size_t run = 0u; run < MODULE_SCALE_RUNS; ++run) {
        clock_t start;
        clock_t end;
        uint64_t current_bytes;
        reset_stats(mod);
        start = clock();
        assert(emit(mod, path));
        end = clock();
        samples[run] = elapsed_cpu_ms(start, end);
        current_bytes = output_file_bytes(path);
        if (run == 0u) artifact_bytes = current_bytes;
        else assert(current_bytes == artifact_bytes);
    }
    sort_times(samples, MODULE_SCALE_RUNS);
    assert(mod->stats_relative_patch_visits == symbol_count);
    assert(mod->stats_image_resolver_calls == symbol_count);
    assert(mod->stats_resolver_candidate_visits == symbol_count);
    assert(mod->stats_symbol_lookup_probes < (uint64_t)symbol_count * 8u);
    assert(mod->stats_relocation_lookup_probes <
           (uint64_t)symbol_count * 8u);
    printf("module_scale format=%s scale=%s symbols=%" PRIu32
           " module_relocations=%d abs_image_relocations=%" PRIu32
           " symbol_lookup_probes=%" PRIu64
           " symbol_insert_probes=%" PRIu64
           " symbol_rehash_probes=%" PRIu64
           " relocation_lookup_probes=%" PRIu64
           " relocation_rehash_probes=%" PRIu64
           " resolver_calls=%" PRIu64
           " resolver_candidates=%" PRIu64
           " relative_patch_visits=%" PRIu64
           " module_tracked_bytes=%" PRIu64
           " artifact_bytes=%" PRIu64
           " runs=%u emit_cpu_median_ms=%.3f\n",
           format_name, scale_name, symbol_count, mod->reloc_count,
           symbol_count, mod->stats_symbol_lookup_probes,
           mod->stats_symbol_insert_probes, mod->stats_symbol_rehash_probes,
           mod->stats_relocation_lookup_probes,
           mod->stats_relocation_rehash_probes,
           mod->stats_image_resolver_calls + mod->stats_tls_resolver_calls,
           mod->stats_resolver_candidate_visits,
           mod->stats_relative_patch_visits, tracked_bytes, artifact_bytes,
           MODULE_SCALE_RUNS, samples[MODULE_SCALE_RUNS / 2u]);
}

static void run_scale(const char* directory, const char* scale_name,
                      uint32_t symbol_count) {
    Module* mod = NULL;
    double build_samples[MODULE_SCALE_RUNS];
    uint64_t tracked_bytes;
    uint64_t build_symbol_probes;
    uint64_t build_symbol_insert_probes;
    uint64_t build_symbol_rehash_probes;
    uint64_t build_relocation_probes;
    uint64_t build_relocation_rehash_probes;

    for (size_t run = 0u; run < MODULE_SCALE_RUNS; ++run) {
        clock_t build_start = clock();
        Module* candidate = build_module(symbol_count);
        clock_t build_end = clock();
        build_samples[run] = elapsed_cpu_ms(build_start, build_end);
        if (run + 1u == MODULE_SCALE_RUNS) mod = candidate;
        else codegen_free(candidate);
    }
    sort_times(build_samples, MODULE_SCALE_RUNS);
    tracked_bytes = module_tracked_bytes(mod);
    build_symbol_probes = mod->stats_symbol_lookup_probes;
    build_symbol_insert_probes = mod->stats_symbol_insert_probes;
    build_symbol_rehash_probes = mod->stats_symbol_rehash_probes;
    build_relocation_probes = mod->stats_relocation_lookup_probes;
    build_relocation_rehash_probes = mod->stats_relocation_rehash_probes;
    assert(build_symbol_probes < (uint64_t)symbol_count * 16u);
    assert(build_relocation_probes < (uint64_t)symbol_count * 32u);
    printf("module_scale phase=build scale=%s symbols=%" PRIu32
           " module_relocations=%d symbol_lookup_probes=%" PRIu64
           " symbol_insert_probes=%" PRIu64
           " symbol_rehash_probes=%" PRIu64
           " relocation_lookup_probes=%" PRIu64
           " relocation_rehash_probes=%" PRIu64
           " module_tracked_bytes=%" PRIu64 " runs=%u build_cpu_median_ms=%.3f\n",
           scale_name, symbol_count, mod->reloc_count,
           build_symbol_probes, build_symbol_insert_probes,
           build_symbol_rehash_probes, build_relocation_probes,
           build_relocation_rehash_probes, tracked_bytes, MODULE_SCALE_RUNS,
           build_samples[MODULE_SCALE_RUNS / 2u]);

    report_emit(mod, directory, scale_name, "rin", "rin", rcc_emit,
                symbol_count, tracked_bytes);
    report_emit(mod, directory, scale_name, "rll", "rll",
                emit_rll_for_scale, symbol_count, tracked_bytes);
    report_emit(mod, directory, scale_name, "drv", "drv",
                emit_drv_for_scale, symbol_count, tracked_bytes);
    codegen_free(mod);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s OUTPUT_DIRECTORY\n", argv[0]);
        return 2;
    }
    g_opts.target_arch = ARCH_X64;
    assert(rcc_copy_path(g_opts.input_file, sizeof(g_opts.input_file),
                         "module-index-scale.c"));
    run_scale(argv[1], "small", 32u);
    run_scale(argv[1], "large", 4096u);
    return 0;
}
