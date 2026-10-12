/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "objfile.h"

int main(int argc, char** argv)
{
    ObjectFile* object;
    ObjSection* text;
    ObjSection* data;
    unsigned module_relocations = 0u;
    unsigned offset_relocations = 0u;
    unsigned static_relocations = 0u;
    if (argc != 2) return 2;
    object = objfile_read(argv[1]);
    assert(object != NULL && object->arch == ARCH_X64);
    text = objfile_get_section(object, ".text");
    data = objfile_get_section(object, ".data");
    assert(text != NULL && data != NULL && data->size >= 16u);
    for (ObjReloc* relocation = text->relocs; relocation;
         relocation = relocation->next) {
        if (relocation->type == RELOC_TLSOFF32S) ++static_relocations;
    }
    for (ObjReloc* relocation = data->relocs; relocation;
         relocation = relocation->next) {
        if (relocation->type == RELOC_TLSINDEX_MODULE64) {
            ++module_relocations;
            assert(relocation->offset + 8u <= data->size);
        } else if (relocation->type == RELOC_TLSINDEX_OFFSET64) {
            ++offset_relocations;
            assert(relocation->offset + 8u <= data->size);
        }
    }
    assert(module_relocations == 1u && offset_relocations == 1u);
    assert(static_relocations == 0u);
    objfile_free(object);
    puts("dynamic TLS compiler relocation contract passed");
    return 0;
}
