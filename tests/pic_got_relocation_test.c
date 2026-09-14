#include "objfile.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

static void verify_got(const char* path, uint16_t arch)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    ObjSection* data;
    bool saw_code_got = false;
    bool saw_data_target = false;
    int got_slots = 0;

    assert(object != NULL);
    assert(object->arch == arch);
    text = objfile_get_section(object, ".text");
    data = objfile_get_section(object, ".data");
    assert(text != NULL && data != NULL);
    for (ObjSymbol* symbol = object->symbols; symbol; symbol = symbol->next) {
        if (strstr(symbol->name, "__rcc_got_") != NULL) {
            assert(symbol->type == SYM_LOCAL);
            assert(symbol->binding == BIND_DATA);
            assert(symbol->section >= 0);
            ++got_slots;
        }
    }
    for (ObjSection* section = object->sections; section;
         section = section->next) {
        for (ObjReloc* relocation = section->relocs; relocation;
             relocation = relocation->next) {
            if (section == text && relocation->type == RELOC_GOT32) {
                assert(strstr(relocation->symbol_name, "__rcc_got_") != NULL);
                assert(relocation->addend == 0);
                saw_code_got = true;
            }
            if (section == data &&
                (relocation->type == RELOC_ABS32U ||
                 relocation->type == RELOC_ABS64)) {
                assert(relocation->symbol_name != NULL);
                saw_data_target = true;
            }
        }
    }
    assert(got_slots >= 4);
    assert(saw_code_got);
    assert(saw_data_target);
    objfile_free(object);
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    verify_got(argv[1], ARCH_X86);
    verify_got(argv[2], ARCH_X64);
    return 0;
}
