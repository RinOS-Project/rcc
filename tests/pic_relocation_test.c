#include "objfile.h"

#include <assert.h>
#include <string.h>

static void verify_plt32(const char* path)
{
    ObjectFile* object = objfile_read(path);
    int seen = 0;

    assert(object != NULL);
    for (ObjSection* section = object->sections; section;
         section = section->next) {
        for (ObjReloc* relocation = section->relocs; relocation;
             relocation = relocation->next) {
            if (strcmp(relocation->symbol_name, "imported_function") != 0) {
                continue;
            }
            assert(relocation->type == RELOC_PLT32);
            assert(relocation->addend == 0);
            ++seen;
        }
    }
    assert(seen == 1);
    objfile_free(object);
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    verify_plt32(argv[1]);
    verify_plt32(argv[2]);
    return 0;
}
