/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"

int main(int argc, char** argv)
{
    FILE* file;
    RaHeader header;
    char* strings;
    int changed = 0;

    assert(argc == 2);
    file = fopen(argv[1], "rb+");
    assert(file != NULL);
    assert(fread(&header, sizeof(header), 1, file) == 1);
    assert(header.magic == RA_MAGIC && header.version == RA_VERSION);
    assert(header.member_count > 1u && header.strtab_size <= SIZE_MAX);

    strings = malloc((size_t)header.strtab_size);
    assert(strings != NULL);
    assert(fseek(file, (long)header.strtab_off, SEEK_SET) == 0);
    assert(fread(strings, (size_t)header.strtab_size, 1, file) == 1);

    for (uint32_t index = 0; index < header.symbol_count; ++index) {
        long position = (long)(header.symtab_off +
                               (uint64_t)index * sizeof(RaSymbol));
        RaSymbol symbol;
        assert(fseek(file, position, SEEK_SET) == 0);
        assert(fread(&symbol, sizeof(symbol), 1, file) == 1);
        assert(symbol.name < header.strtab_size);
        if (strcmp(strings + symbol.name, "archive_chosen") != 0) continue;
        symbol.member = 1u; /* unused.ro does not define archive_chosen. */
        assert(fseek(file, position, SEEK_SET) == 0);
        assert(fwrite(&symbol, sizeof(symbol), 1, file) == 1);
        changed = 1;
        break;
    }

    free(strings);
    assert(fclose(file) == 0);
    assert(changed);
    return 0;
}
