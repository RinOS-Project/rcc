#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static ObjSection* code_section(ObjectFile* object)
{
    for (ObjSection* section = object ? object->sections : NULL;
         section; section = section->next) {
        if (section->type == SECT_CODE) return section;
    }
    return NULL;
}

static bool contains_bytes(const uint8_t* data, uint64_t size,
                           const uint8_t* pattern, size_t pattern_size)
{
    if (!data || !pattern || pattern_size == 0u || size < pattern_size) {
        return false;
    }
    for (uint64_t offset = 0u; offset <= size - pattern_size; ++offset) {
        if (memcmp(data + offset, pattern, pattern_size) == 0) return true;
    }
    return false;
}

static void verify(const char* path, uint16_t architecture)
{
    static const uint8_t cpuid[] = {0x0Fu, 0xA2u};
    static const uint8_t rdtsc[] = {0x0Fu, 0x31u};
    static const uint8_t rdtscp[] = {0x0Fu, 0x01u, 0xF9u};
    static const uint8_t rdmsr[] = {0x0Fu, 0x32u};
    static const uint8_t wrmsr[] = {0x0Fu, 0x30u};
    static const uint8_t fence[] = {0xF0u, 0x83u, 0x0Cu, 0x24u, 0x00u};
    static const uint8_t inb[] = {0xECu};
    static const uint8_t outb[] = {0xEEu};
    static const uint8_t read_cr0_x86[] = {0x0Fu, 0x20u, 0xC1u};
    static const uint8_t write_cr0_x86[] = {0x0Fu, 0x22u, 0xC1u};
    static const uint8_t read_cr0_x64[] = {0x41u, 0x0Fu, 0x20u, 0xC2u};
    static const uint8_t write_cr0_x64[] = {0x41u, 0x0Fu, 0x22u, 0xC2u};
    ObjectFile* object = objfile_read(path);
    ObjSection* code;

    assert(object != NULL && object->arch == architecture);
    code = code_section(object);
    assert(code != NULL && code->size > 0u);
#define REQUIRE(pattern) \
    assert(contains_bytes(code->data, code->size, (pattern), sizeof(pattern)))
    REQUIRE(cpuid);
    REQUIRE(rdtsc);
    REQUIRE(rdtscp);
    REQUIRE(rdmsr);
    REQUIRE(wrmsr);
    REQUIRE(fence);
    REQUIRE(inb);
    REQUIRE(outb);
    if (architecture == ARCH_X64) {
        REQUIRE(read_cr0_x64);
        REQUIRE(write_cr0_x64);
    } else {
        REQUIRE(read_cr0_x86);
        REQUIRE(write_cr0_x86);
    }
#undef REQUIRE
    objfile_free(object);
}

int main(int argc, char** argv)
{
    assert(argc == 4);
    verify(argv[1], ARCH_X86);
    verify(argv[2], ARCH_X64);
    verify(argv[3], ARCH_X64);
    return 0;
}
