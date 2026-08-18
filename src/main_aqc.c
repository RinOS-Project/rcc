// SPDX-License-Identifier: MIT
#include "../include/aqc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE* stream) {
    fputs("usage: aqc [-o output.rsh] [--dump-ir] input.aq\n", stream);
}

static const char* opcode_name(uint16_t opcode) {
    static const char* const names[] = {
        "nop", "const.i32", "mov", "add.i32", "mul.i32", "min.i32",
        "max.i32", "load.input.i32", "store.output.i32",
        "load.storage.i32", "store.storage.i32", "jump", "jump.if",
        "return", "sample.i32", "sample.compare.i32", "const.f32",
        "sub.i32", "div.i32", "mod.i32", "add.f32", "sub.f32",
        "mul.f32", "div.f32", "min.f32", "max.f32", "and.i32",
        "or.i32", "xor.i32", "shl.i32", "shr.i32", "cmp.eq.i32",
        "cmp.ne.i32", "cmp.lt.i32", "cmp.le.i32", "cmp.gt.i32",
        "cmp.ge.i32", "cmp.eq.f32", "cmp.ne.f32", "cmp.lt.f32",
        "cmp.le.f32", "cmp.gt.f32", "cmp.ge.f32", "i32.to.f32",
        "f32.to.i32", "load.input.f32", "store.output.f32",
        "load.storage.f32", "store.storage.f32", "sample.f32",
        "sample.compare.f32", "load.builtin.i32", "load.builtin.f32",
        "discard"
    };
    return opcode < sizeof(names) / sizeof(names[0]) ? names[opcode]
                                                     : "unknown";
}

static void dump_ir(const uint8_t* bytes) {
    RinShaderHeaderV1 header;
    memcpy(&header, bytes, sizeof(header));
    printf("RSH1 stage=%u instructions=%u registers=%u inputs=%u "
           "outputs=%u resources=%u workgroup=%u,%u,%u\n",
           header.stage, header.instruction_count, header.register_count,
           header.input_count, header.output_count, header.resource_count,
           header.workgroup_x, header.workgroup_y, header.workgroup_z);
    for (uint32_t index = 0u; index < header.instruction_count; index++) {
        RinShaderInstructionV1 instruction;
        memcpy(&instruction,
               bytes + sizeof(header) +
                   (size_t)index * sizeof(instruction),
               sizeof(instruction));
        printf("%5u  %-20s d=%u s0=%u s1=%u r=%u imm=0x%08x\n",
               index, opcode_name(instruction.opcode),
               instruction.destination, instruction.source0,
               instruction.source1, instruction.resource,
               instruction.immediate);
    }
}

static int has_aq_extension(const char* path) {
    size_t length = strlen(path);
    return length >= 3u && path[length - 3u] == '.' &&
           (path[length - 2u] == 'a' || path[length - 2u] == 'A') &&
           (path[length - 1u] == 'q' || path[length - 1u] == 'Q');
}

static char* default_output_path(const char* input) {
    size_t length = strlen(input);
    size_t stem = has_aq_extension(input) ? length - 3u : length;
    char* output = (char*)malloc(stem + 5u);
    if (!output) return NULL;
    memcpy(output, input, stem);
    memcpy(output + stem, ".rsh", 5u);
    return output;
}

static int read_source(const char* path, char** source, size_t* size) {
    FILE* file = fopen(path, "rb");
    long length;
    size_t read_size;
    char* bytes;
    if (!file) return 0;
    if (fseek(file, 0, SEEK_END) != 0 ||
        (length = ftell(file)) < 0 ||
        (unsigned long)length > AQC_MAX_SOURCE_SIZE ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    bytes = (char*)malloc((size_t)length + 1u);
    if (!bytes) {
        fclose(file);
        return 0;
    }
    read_size = fread(bytes, 1u, (size_t)length, file);
    if (read_size != (size_t)length || fclose(file) != 0) {
        free(bytes);
        return 0;
    }
    bytes[read_size] = '\0';
    *source = bytes;
    *size = read_size;
    return 1;
}

static int write_binary(const char* path, const void* bytes, size_t size) {
    FILE* file = fopen(path, "wb");
    int ok;
    if (!file) return 0;
    ok = fwrite(bytes, 1u, size, file) == size && fflush(file) == 0;
    if (fclose(file) != 0) ok = 0;
    return ok;
}

int main(int argc, char** argv) {
    const char* input = NULL;
    const char* output_option = NULL;
    char* output_owned = NULL;
    const char* output;
    char* source = NULL;
    size_t source_size = 0u;
    uint8_t* binary = NULL;
    size_t binary_size = 0u;
    AqcDiagnostic diagnostic;
    int dump = 0;
    int result;

    for (int index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--help") == 0 ||
            strcmp(argv[index], "-h") == 0) {
            usage(stdout);
            return 0;
        }
        if (strcmp(argv[index], "--dump-ir") == 0) {
            dump = 1;
            continue;
        }
        if (strcmp(argv[index], "-o") == 0) {
            if (++index >= argc || output_option) {
                usage(stderr);
                return 2;
            }
            output_option = argv[index];
            continue;
        }
        if (argv[index][0] == '-' || input) {
            usage(stderr);
            return 2;
        }
        input = argv[index];
    }
    if (!input) {
        usage(stderr);
        return 2;
    }
    if (!has_aq_extension(input)) {
        fprintf(stderr, "aqc: input must use the .aq extension: %s\n", input);
        return 2;
    }
    output_owned = output_option ? NULL : default_output_path(input);
    output = output_option ? output_option : output_owned;
    if (!output) {
        fputs("aqc: unable to allocate output path\n", stderr);
        return 1;
    }
    if (!read_source(input, &source, &source_size)) {
        fprintf(stderr, "aqc: unable to read bounded source: %s\n", input);
        free(output_owned);
        return 1;
    }
    binary = (uint8_t*)malloc(AQC_MAX_OUTPUT_SIZE);
    if (!binary) {
        fputs("aqc: unable to allocate RSH1 output buffer\n", stderr);
        free(source);
        free(output_owned);
        return 1;
    }
    result = aqc_compile(source, source_size, binary, AQC_MAX_OUTPUT_SIZE,
                         &binary_size, &diagnostic);
    if (result != AQC_OK) {
        fprintf(stderr, "%s:%u:%u: error: %s\n", input, diagnostic.line,
                diagnostic.column, diagnostic.message);
        free(binary);
        free(source);
        free(output_owned);
        return 1;
    }
    if (!write_binary(output, binary, binary_size)) {
        fprintf(stderr, "aqc: unable to write %s\n", output);
        free(binary);
        free(source);
        free(output_owned);
        return 1;
    }
    if (dump) dump_ir(binary);
    free(binary);
    free(source);
    free(output_owned);
    return 0;
}
