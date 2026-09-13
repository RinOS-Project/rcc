/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <string.h>

#include "objfile.h"

static ObjSymbol* find_symbol(ObjectFile* object, const char* name)
{
    ObjSymbol* symbol;
    for (symbol = object->symbols; symbol; symbol = symbol->next) {
        if (strcmp(symbol->name, name) == 0) return symbol;
    }
    return NULL;
}

static unsigned relocation_count(ObjSection* text, const char* name)
{
    unsigned count = 0u;
    ObjReloc* relocation;
    for (relocation = text->relocs; relocation;
         relocation = relocation->next) {
        if (strcmp(relocation->symbol_name, name) == 0) ++count;
    }
    return count;
}

static void verify_inline_wrappers(const char* path, uint16_t architecture)
{
    static const char mutable_name[] = "_ZN3rin5sliceEPvy";
    static const char const_name[] = "_ZN3rin5sliceEPKvy";
    static const char reference_name[] =
        "_ZN3rin14copy_referenceERK10RinSliceV1";
    static const char reference_overload_name[] =
        "_ZN3rin6chooseERK10RinSliceV1";
    static const char pointer_overload_name[] =
        "_ZN3rin6chooseEPK10RinSliceV1";
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSymbol* mutable_symbol;
    ObjSymbol* const_symbol;
    ObjSymbol* reference_symbol;
    ObjSymbol* reference_overload_symbol;
    ObjSymbol* pointer_overload_symbol;

    assert(object != NULL && object->arch == architecture);
    text = objfile_get_section(object, ".text");
    assert(text != NULL);
    mutable_symbol = find_symbol(object, mutable_name);
    const_symbol = find_symbol(object, const_name);
    reference_symbol = find_symbol(object, reference_name);
    reference_overload_symbol = find_symbol(object, reference_overload_name);
    pointer_overload_symbol = find_symbol(object, pointer_overload_name);
    assert(mutable_symbol != NULL && mutable_symbol->type == SYM_WEAK);
    assert(const_symbol != NULL && const_symbol->type == SYM_WEAK);
    assert(reference_symbol != NULL && reference_symbol->type == SYM_WEAK);
    assert(reference_overload_symbol != NULL &&
           reference_overload_symbol->type == SYM_WEAK);
    assert(pointer_overload_symbol != NULL &&
           pointer_overload_symbol->type == SYM_WEAK);
    assert(mutable_symbol->section == 0);
    assert(const_symbol->section == 0);
    assert(reference_symbol->section == 0);
    assert(reference_overload_symbol->section == 0);
    assert(pointer_overload_symbol->section == 0);
    assert(relocation_count(text, mutable_name) == 0u);
    assert(relocation_count(text, const_name) == 0u);
    objfile_free(object);
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    verify_inline_wrappers(argv[1], ARCH_X86);
    verify_inline_wrappers(argv[2], ARCH_X64);
    return 0;
}
