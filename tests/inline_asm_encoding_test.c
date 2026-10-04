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

static bool internal_label(const char* name)
{
    return name && strstr(name, "__rcc_label_") != NULL;
}

static uint64_t function_end(ObjectFile* object, ObjSymbol* function)
{
    ObjSection* code = code_section(object);
    uint64_t end = code ? code->size : 0u;
    if (!object || !code || !function) return 0u;
    for (ObjSymbol* symbol = object->symbols; symbol; symbol = symbol->next) {
        if (symbol->binding != BIND_CODE || symbol->section != function->section ||
            internal_label(symbol->name) || symbol->value <= function->value ||
            symbol->value >= end) continue;
        end = symbol->value;
    }
    return end;
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
    ObjectFile* object = objfile_read(path);
    ObjSection* code;
    ObjSymbol* function;
    uint64_t end;
    static const uint8_t x86_move[] = {0x89u, 0xd8u};
    static const uint8_t x64_move[] = {0x48u, 0x89u, 0xd8u};

    assert(object != NULL && object->arch == architecture);
    code = code_section(object);
    function = objfile_find_symbol(object, "asm_placeholder_move");
    assert(code != NULL && function != NULL && function->section >= 0);
    end = function_end(object, function);
    assert(end > function->value && end <= code->size);
    assert(contains_bytes(code->data + function->value,
                          end - function->value,
                          architecture == ARCH_X64 ? x64_move : x86_move,
                          architecture == ARCH_X64 ? sizeof(x64_move)
                                                   : sizeof(x86_move)));
    objfile_free(object);
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    verify(argv[1], ARCH_X86);
    verify(argv[2], ARCH_X64);
    return 0;
}
