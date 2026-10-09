#include "objfile.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    ObjSymbol** items;
    size_t count;
} SymbolList;

typedef struct {
    ObjReloc** items;
    size_t count;
} RelocList;

static int compare_symbols(const void* left, const void* right)
{
    const ObjSymbol* a = *(ObjSymbol* const*)left;
    const ObjSymbol* b = *(ObjSymbol* const*)right;
    if (a->value < b->value) return -1;
    if (a->value > b->value) return 1;
    return strcmp(a->name, b->name);
}

static int compare_relocations(const void* left, const void* right)
{
    const ObjReloc* a = *(ObjReloc* const*)left;
    const ObjReloc* b = *(ObjReloc* const*)right;
    if (a->offset < b->offset) return -1;
    if (a->offset > b->offset) return 1;
    return 0;
}

static bool format_symbol_name(const char* name, char* buffer,
                              size_t buffer_size)
{
    static const char hex[] = "0123456789abcdef";
    bool needs_encoding = false;
    size_t length;
    if (!name || !buffer || buffer_size == 0u) return false;
    length = strlen(name);
    for (size_t index = 0u; index < length; ++index) {
        unsigned char ch = (unsigned char)name[index];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '_' || ch == '.')) {
            needs_encoding = true;
            break;
        }
    }
    if (!needs_encoding) {
        if (length >= buffer_size) return false;
        memcpy(buffer, name, length + 1u);
        return true;
    }
    if (length > (buffer_size - 1u) / 2u ||
        18u + length * 2u >= buffer_size) return false;
    memcpy(buffer, "__rcc_test_symbol_", 18u);
    for (size_t index = 0u; index < length; ++index) {
        unsigned char ch = (unsigned char)name[index];
        buffer[18u + index * 2u] = hex[ch >> 4];
        buffer[19u + index * 2u] = hex[ch & 0x0fu];
    }
    buffer[18u + length * 2u] = '\0';
    return true;
}

static bool emit_bytes(FILE* output, const uint8_t* bytes, size_t count)
{
    if (!output || (!bytes && count != 0u)) return false;
    for (size_t index = 0u; index < count; ++index) {
        if (index % 12u == 0u && fputs("\t.byte ", output) == EOF) {
            return false;
        }
        if (fprintf(output, "0x%02x", bytes[index]) < 0) return false;
        if (index % 12u == 11u || index + 1u == count) {
            if (fputc('\n', output) == EOF) return false;
        } else if (fputs(", ", output) == EOF) {
            return false;
        }
    }
    return true;
}

static bool emit_symbol(FILE* output, const ObjSymbol* symbol)
{
    char name[1024];
    if (!output || !symbol || !format_symbol_name(
            symbol->name, name, sizeof(name))) return false;
    if (symbol->type == SYM_WEAK) {
        if (fprintf(output, "\t.weak %s\n", name) < 0) return false;
    } else if (symbol->type == SYM_GLOBAL) {
        if (fprintf(output, "\t.globl %s\n", name) < 0) return false;
#if !defined(_WIN32)
    } else if (fprintf(output, "\t.local %s\n", name) < 0) {
        return false;
#endif
    }
    return fprintf(output, "%s:\n", name) >= 0;
}

static bool emit_relocation(FILE* output, const ObjReloc* relocation)
{
    char name[1024];
    if (!output || !relocation || !format_symbol_name(
            relocation->symbol_name, name, sizeof(name))) return false;
    switch (relocation->type) {
        case RELOC_REL32:
        case RELOC_PLT32: {
            int64_t addend;
            if (relocation->addend < INT64_MIN + 4) return false;
            addend = relocation->addend - 4;
            return fprintf(output, "\t.long %s%+lld - .\n",
                           name, (long long)addend) >= 0;
        }
        case RELOC_ABS32:
        case RELOC_ABS32U:
        case RELOC_ABS32S:
            return fprintf(output, "\t.long %s%+lld\n",
                           name,
                           (long long)relocation->addend) >= 0;
        default:
            fprintf(stderr, "unsupported i686 test relocation %u\n",
                    (unsigned)relocation->type);
            return false;
    }
}

static int section_index(const ObjectFile* object,
                         const ObjSection* target)
{
    int index = 0;
    if (!object || !target) return -1;
    for (const ObjSection* section = object->sections; section;
         section = section->next, ++index) {
        if (section == target) return index;
    }
    return -1;
}

static bool collect_symbols(ObjectFile* object, ObjSection* section,
                            int section_number, SymbolList* list)
{
    size_t count = 0u;
    if (!object || !section || section_number < 0 || !list) return false;
    for (ObjSymbol* symbol = object->symbols; symbol;
         symbol = symbol->next) {
        if (symbol->section == section_number &&
            symbol->binding != BIND_ABS) {
            if (!symbol->name) return false;
            ++count;
        }
    }
    list->items = count ? (ObjSymbol**)malloc(count * sizeof(*list->items))
                        : NULL;
    if (count && !list->items) return false;
    list->count = 0u;
    for (ObjSymbol* symbol = object->symbols; symbol;
         symbol = symbol->next) {
        if (symbol->section != section_number ||
            symbol->binding == BIND_ABS) continue;
        if (!symbol->name || symbol->value > section->size) return false;
        list->items[list->count++] = symbol;
    }
    qsort(list->items, list->count, sizeof(*list->items), compare_symbols);
    return true;
}

static bool collect_relocations(ObjSection* text, RelocList* list)
{
    size_t count = 0u;
    if (!text || !list) return false;
    for (ObjReloc* relocation = text->relocs; relocation;
         relocation = relocation->next) ++count;
    list->items = count ? (ObjReloc**)malloc(count * sizeof(*list->items))
                        : NULL;
    if (count && !list->items) return false;
    list->count = 0u;
    for (ObjReloc* relocation = text->relocs; relocation;
         relocation = relocation->next) {
        if (text->size < 4u || relocation->offset > text->size ||
            relocation->offset > text->size - 4u) return false;
        list->items[list->count++] = relocation;
    }
    qsort(list->items, list->count, sizeof(*list->items), compare_relocations);
    return true;
}

static bool emit_section(FILE* output, ObjectFile* object,
                         ObjSection* section, int section_number)
{
    SymbolList symbols = {0};
    RelocList relocations = {0};
    uint64_t offset = 0u;
    size_t symbol_index = 0u;
    size_t relocation_index = 0u;
    bool success = false;

    if (!section || section->size == 0u || section_number < 0 ||
        !collect_symbols(object, section, section_number, &symbols) ||
        !collect_relocations(section, &relocations)) {
        goto done;
    }
    if (section->type == SECT_CODE) {
        if (fputs("\t.text\n", output) == EOF) goto done;
    } else if (section->type == SECT_RODATA) {
        if (fputs("\t.section .rodata\n", output) == EOF) goto done;
    } else {
        goto done;
    }
    if (section->align > 1u &&
        fprintf(output, "\t.balign %u\n", section->align) < 0) goto done;
    while (offset <= section->size) {
        bool emitted_event = false;
        while (symbol_index < symbols.count &&
               symbols.items[symbol_index]->value == offset) {
            if (!emit_symbol(output, symbols.items[symbol_index++])) {
                goto done;
            }
            emitted_event = true;
        }
        while (relocation_index < relocations.count &&
               relocations.items[relocation_index]->offset == offset) {
            if (!emit_relocation(output,
                                 relocations.items[relocation_index++])) {
                goto done;
            }
            offset += 4u;
            emitted_event = true;
        }
        if (offset == section->size) break;
        if (emitted_event) continue;

        uint64_t next = section->size;
        if (symbol_index < symbols.count &&
            symbols.items[symbol_index]->value < next) {
            next = symbols.items[symbol_index]->value;
        }
        if (relocation_index < relocations.count &&
            relocations.items[relocation_index]->offset < next) {
            next = relocations.items[relocation_index]->offset;
        }
        if (next <= offset || next - offset > SIZE_MAX ||
            !emit_bytes(output, section->data + (size_t)offset,
                        (size_t)(next - offset))) {
            goto done;
        }
        offset = next;
    }
    success = symbol_index == symbols.count &&
        relocation_index == relocations.count &&
        ferror(output) == 0;

done:
    free(symbols.items);
    free(relocations.items);
    return success;
}

static bool emit_object(FILE* output, ObjectFile* object)
{
    ObjSection* text = objfile_get_section(object, ".text");
    ObjSection* rodata = objfile_get_section(object, ".rodata");
    int text_index = section_index(object, text);
    int rodata_index = rodata ? section_index(object, rodata) : -1;

    if (!text || text->size == 0u ||
        (text->flags & (SECT_FLAG_ALLOC | SECT_FLAG_EXEC)) !=
            (SECT_FLAG_ALLOC | SECT_FLAG_EXEC) ||
        (rodata &&
         (rodata->type != SECT_RODATA || rodata->size == 0u ||
          (rodata->flags & SECT_FLAG_ALLOC) == 0u ||
          (rodata->flags & (SECT_FLAG_WRITE | SECT_FLAG_EXEC)) != 0u))) {
        return false;
    }
    for (ObjSection* section = object->sections; section;
         section = section->next) {
        if (section != text && section != rodata &&
            (section->size != 0u || section->relocs != NULL)) return false;
    }
    return emit_section(output, object, text, text_index) &&
        (!rodata || emit_section(
            output, object, rodata, rodata_index)) &&
        ferror(output) == 0;
}

int main(int argc, char** argv)
{
    ObjectFile* object;
    FILE* output;
    bool success;
    if (argc != 3) {
        fprintf(stderr, "usage: %s input.ro output.s\n", argv[0]);
        return 2;
    }
    object = objfile_read(argv[1]);
    if (!object || object->arch != ARCH_X86) {
        fprintf(stderr, "input is not an i686 RinOS object: %s\n", argv[1]);
        objfile_free(object);
        return 1;
    }
    output = fopen(argv[2], "wb");
    if (!output) {
        perror(argv[2]);
        objfile_free(object);
        return 1;
    }
    success = emit_object(output, object);
    if (fclose(output) != 0) success = false;
    objfile_free(object);
    if (!success) {
        remove(argv[2]);
        fprintf(stderr, "failed to emit bounded i686 test assembly\n");
        return 1;
    }
    return 0;
}
