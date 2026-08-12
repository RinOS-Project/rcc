#include "objfile.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void verify_object(const char* path, uint16_t arch)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* call;
    ObjSymbol* helper;
    ObjSymbol* load;
    ObjSymbol* control;
    ObjReloc* relocation;
    assert(object != NULL && object->arch == arch);
    text = objfile_get_section(object, ".text");
    call = objfile_find_symbol(object, "verified_call");
    helper = objfile_find_symbol(
        object, "tests/verified_backend.c::verified_helper");
    load = objfile_find_symbol(object, "verified_load");
    control = objfile_find_symbol(object, "verified_control");
    assert(text != NULL && text->size != 0u && text->memory_size == text->size);
    assert((text->flags & (SECT_FLAG_ALLOC | SECT_FLAG_EXEC)) ==
           (SECT_FLAG_ALLOC | SECT_FLAG_EXEC));
    assert((text->flags & SECT_FLAG_WRITE) == 0u);
    assert(call != NULL && call->type == SYM_GLOBAL && call->section == 0);
    assert(helper != NULL && helper->type == SYM_LOCAL && helper->section == 0);
    assert(load != NULL && load->type == SYM_GLOBAL && load->section == 0);
    assert(control != NULL && control->type == SYM_GLOBAL &&
           control->section == 0);
    assert(object->symbol_count == 4);
    relocation = text->relocs;
    assert(relocation != NULL && relocation->next == NULL);
    assert(relocation->type == RELOC_REL32);
    assert(strcmp(relocation->symbol_name,
                  "tests/verified_backend.c::verified_helper") == 0);
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

static void verify_fallback_object(const char* path)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* data;
    ObjSymbol* symbol;
    assert(object != NULL && object->arch == ARCH_X64);
    data = objfile_get_section(object, ".data");
    symbol = objfile_find_symbol(object, "verified_fallback_data");
    assert(data != NULL && data->size == 4u);
    assert(symbol != NULL && symbol->binding == BIND_DATA &&
           symbol->section >= 0);
    objfile_free(object);
}

int main(int argc, char** argv)
{
    assert(argc == 5);
    verify_object(argv[1], ARCH_X86);
    verify_object(argv[2], ARCH_X64);
    verify_cxx_object(argv[3]);
    verify_fallback_object(argv[4]);
    puts("Verified typed-SSA production .ro bridge tests passed");
    return 0;
}
